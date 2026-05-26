#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <list>

#include "frame/test_macros.h"
#include "hiredis_happ.h"
#include "test_redis_reply_helper.h"

namespace hiredis {
namespace happ {
struct connection_unit_test_access {
  static void push_reply(connection& conn, cmd_exec* cmd) { conn.reply_list_.push_back(cmd); }

  static size_t reply_list_size(const connection& conn) { return conn.reply_list_.size(); }
};
}  // namespace happ
}  // namespace hiredis

template <typename T>
struct AutoPtr {
  typedef T value_type;
  T* v;

  AutoPtr(T* v_) : v(v_) {}
  ~AutoPtr() {
    if (nullptr != v) {
      delete v;
    }
  }

  AutoPtr(AutoPtr& other) {
    v = other.v;
    other.v = nullptr;
  }

  AutoPtr& operator=(AutoPtr& other) {
    v = other.v;
    other.v = nullptr;
    return (*this);
  }

  T* get() { return v; }
  const T* get() const { return v; }

  T* operator->() { return v; }
  const T* operator->() const { return v; }

  T& operator*() { return *v; }
  const T& operator*() const { return *v; }
};

struct ConnectionCallbackState {
  int call_count = 0;
  int last_error_code = hiredis::happ::error_code::REDIS_HAPP_OK;
  bool last_reply_is_null = false;
};

static void happ_connection_queue_cb(hiredis::happ::cmd_exec* cmd, struct redisAsyncContext*, void* r, void* pridata) {
  ConnectionCallbackState* state = reinterpret_cast<ConnectionCallbackState*>(pridata);
  CASE_EXPECT_NE(nullptr, state);
  ++state->call_count;
  state->last_error_code = cmd->get_error_code();
  state->last_reply_is_null = (nullptr == r);
}

CASE_TEST(happ_connection, basic) {
  AutoPtr<hiredis::happ::connection> conn1(new hiredis::happ::connection());
  AutoPtr<hiredis::happ::connection> conn2(new hiredis::happ::connection());

  CASE_EXPECT_EQ(conn1->get_sequence() + 1, conn2->get_sequence());

  hiredis::happ::holder_t h;
  redisAsyncContext vir_context;

  h.clu = nullptr;
  conn1->init(h, "127.0.0.2", 1234);
  conn2->init(h, "127.0.0.3", 1234);

  CASE_EXPECT_TRUE("127.0.0.2" == conn1->get_key().ip);
  CASE_EXPECT_TRUE(1234 == conn1->get_key().port);
  CASE_EXPECT_TRUE("127.0.0.2:1234" == conn1->get_key().name);
  CASE_EXPECT_TRUE("127.0.0.1:0" == hiredis::happ::connection::make_name("127.0.0.1", 0));

  CASE_EXPECT_EQ(hiredis::happ::connection::status::DISCONNECTED, conn1->get_status());
  hiredis::happ::cmd_exec* cmd = hiredis::happ::cmd_exec::create(h, nullptr, &vir_context, 0);

  int res = conn1->redis_cmd(cmd, nullptr);
  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_CREATE, res);

  conn1->set_connecting(&vir_context);

  CASE_EXPECT_EQ(conn1->get_context(), &vir_context);
  CASE_EXPECT_EQ(hiredis::happ::connection::status::CONNECTING, conn1->get_status());
  conn1->set_connected();
  CASE_EXPECT_EQ(hiredis::happ::connection::status::CONNECTED, conn1->get_status());

  CASE_EXPECT_EQ(conn1->get_context(), &vir_context);

  cmd->private_data(conn2.get());
  conn2->set_connecting(&vir_context);

  conn1->release(false);
  conn2->release(false);

  CASE_EXPECT_EQ(conn1->get_context(), nullptr);
  CASE_EXPECT_EQ(conn2->get_context(), nullptr);

  hiredis::happ::cmd_exec::destroy(cmd);
}

CASE_TEST(happ_connection, pick_name_and_reply_queue) {
  std::string ip;
  uint16_t port = 0;
  CASE_EXPECT_TRUE(hiredis::happ::connection::pick_name(" 127.0.0.1:6380", ip, port));
  CASE_EXPECT_TRUE("127.0.0.1" == ip);
  CASE_EXPECT_EQ(static_cast<uint16_t>(6380), port);

  CASE_EXPECT_TRUE(hiredis::happ::connection::pick_name(":6381", ip, port));
  CASE_EXPECT_TRUE(ip.empty());
  CASE_EXPECT_EQ(static_cast<uint16_t>(6381), port);

  CASE_EXPECT_FALSE(hiredis::happ::connection::pick_name("127.0.0.1", ip, port));
  CASE_EXPECT_FALSE(hiredis::happ::connection::pick_name("127.0.0.1:", ip, port));

  hiredis::happ::holder_t h;
  redisAsyncContext vir_context;
  memset(&vir_context, 0, sizeof(vir_context));
  h.clu = nullptr;

  hiredis::happ::connection conn;
  conn.init(h, "127.0.0.1", 6379);
  conn.set_connecting(&vir_context);
  conn.set_connected();

  ConnectionCallbackState states[3];
  hiredis::happ::cmd_exec* cmd1 = hiredis::happ::cmd_exec::create(h, happ_connection_queue_cb, &states[0], 0);
  hiredis::happ::cmd_exec* cmd2 = hiredis::happ::cmd_exec::create(h, happ_connection_queue_cb, &states[1], 0);
  hiredis::happ::cmd_exec* cmd3 = hiredis::happ::cmd_exec::create(h, happ_connection_queue_cb, &states[2], 0);
  CASE_EXPECT_NE(nullptr, cmd1);
  CASE_EXPECT_NE(nullptr, cmd2);
  CASE_EXPECT_NE(nullptr, cmd3);

  hiredis::happ::connection_unit_test_access::push_reply(conn, cmd1);
  hiredis::happ::connection_unit_test_access::push_reply(conn, cmd2);
  hiredis::happ::connection_unit_test_access::push_reply(conn, cmd3);
  CASE_EXPECT_EQ(static_cast<size_t>(3), hiredis::happ::connection_unit_test_access::reply_list_size(conn));

  hiredis_happ_test::redis_reply_ptr reply = hiredis_happ_test::adopt_reply(hiredis_happ_test::make_status_reply("OK"));
  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_OK, conn.call_reply(cmd2, reply.get()));
  CASE_EXPECT_EQ(1, states[0].call_count);
  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_TIMEOUT, states[0].last_error_code);
  CASE_EXPECT_TRUE(states[0].last_reply_is_null);

  CASE_EXPECT_EQ(1, states[1].call_count);
  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_OK, states[1].last_error_code);
  CASE_EXPECT_FALSE(states[1].last_reply_is_null);

  CASE_EXPECT_EQ(static_cast<size_t>(1), hiredis::happ::connection_unit_test_access::reply_list_size(conn));

  conn.release(false);
  CASE_EXPECT_EQ(1, states[2].call_count);
  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_CONNECTION, states[2].last_error_code);
  CASE_EXPECT_TRUE(states[2].last_reply_is_null);
  CASE_EXPECT_EQ(hiredis::happ::connection::status::DISCONNECTED, conn.get_status());
}
