#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "frame/test_macros.h"
#include "hiredis_happ.h"

#if defined(HIREDIS_HAPP_ENABLE_LIBUV)
#  include "hiredis/adapters/libuv.h"
#elif defined(HIREDIS_HAPP_ENABLE_LIBEVENT)
#  include "hiredis/adapters/libevent.h"
#endif

#if defined(HIREDIS_HAPP_ENABLE_LIBUV) || defined(HIREDIS_HAPP_ENABLE_LIBEVENT)
namespace {

std::string get_env_or_default(const char *name, const char *fallback) {
  const char *value = getenv(name);
  if (nullptr == value || 0 == value[0]) {
    return fallback;
  }

  return value;
}

uint16_t get_env_port(const char *name, uint16_t fallback) {
  const char *value = getenv(name);
  if (nullptr == value || 0 == value[0]) {
    return fallback;
  }

  long port = strtol(value, nullptr, 10);
  if (port <= 0 || port > 65535) {
    return fallback;
  }

  return static_cast<uint16_t>(port);
}

std::string make_test_key(const std::string &prefix) {
  static size_t sequence = 0;
  std::ostringstream oss;
  oss << prefix << ":" << ++sequence;
  return oss.str();
}

struct ReplyCapture {
  bool done = false;
  int error_code = hiredis::happ::error_code::REDIS_HAPP_UNKNOWD;
  int reply_type = -1;
  std::string string_value;
  long long integer_value = 0;
  std::vector<std::string> array_values;
};

static void capture_reply(hiredis::happ::cmd_exec *cmd, struct redisAsyncContext *, void *reply_ptr, void *pridata) {
  ReplyCapture *state = reinterpret_cast<ReplyCapture *>(pridata);
  CASE_EXPECT_NE(nullptr, state);
  if (nullptr == state) {
    return;
  }

  state->done = true;
  state->error_code = cmd->result();
  if (nullptr == reply_ptr) {
    return;
  }

  redisReply *reply = reinterpret_cast<redisReply *>(reply_ptr);
  state->reply_type = reply->type;
  switch (reply->type) {
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_STRING:
    case REDIS_REPLY_ERROR:
      if (nullptr != reply->str) {
        state->string_value = reply->str;
      }
      break;

    case REDIS_REPLY_INTEGER:
      state->integer_value = reply->integer;
      break;

    case REDIS_REPLY_ARRAY:
      for (size_t i = 0; i < reply->elements; ++i) {
        redisReply *element = reply->element[i];
        if (nullptr == element) {
          state->array_values.push_back("(null)");
          continue;
        }

        switch (element->type) {
          case REDIS_REPLY_STATUS:
          case REDIS_REPLY_STRING:
          case REDIS_REPLY_ERROR:
            state->array_values.push_back(nullptr != element->str ? element->str : "");
            break;

          case REDIS_REPLY_INTEGER: {
            std::ostringstream oss;
            oss << element->integer;
            state->array_values.push_back(oss.str());
            break;
          }

          case REDIS_REPLY_NIL:
            state->array_values.push_back("(nil)");
            break;

          default:
            state->array_values.push_back("(unsupported)");
            break;
        }
      }
      break;

    default:
      break;
  }
}

#  if defined(HIREDIS_HAPP_ENABLE_LIBUV)
class EventLoopHarness {
 public:
  EventLoopHarness() : closed_(false), sec_(1), usec_(0) { uv_loop_init(&loop_); }

  ~EventLoopHarness() { close(); }

  void attach(redisAsyncContext *ctx) {
    CASE_EXPECT_NE(nullptr, ctx);
    if (nullptr != ctx) {
      redisLibuvAttach(ctx, &loop_);
    }
  }

  template <class Client>
  void pump_once(Client &client, time_t step_usec = 10000) {
    usec_ += step_usec;
    while (usec_ >= 1000000) {
      ++sec_;
      usec_ -= 1000000;
    }

    client.proc(sec_, usec_);
    uv_run(&loop_, UV_RUN_NOWAIT);
    CASE_THREAD_SLEEP_MS(1);
  }

  template <class Client, class Predicate>
  bool pump_until(Client &client, Predicate predicate, int timeout_ms) {
    int rounds = timeout_ms / 10;
    if (rounds <= 0) {
      rounds = 1;
    }

    for (int i = 0; i < rounds && !predicate(); ++i) {
      pump_once(client);
    }

    return predicate();
  }

  template <class Client>
  void pump_for(Client &client, int total_ms) {
    int rounds = total_ms / 10;
    if (rounds <= 0) {
      rounds = 1;
    }

    for (int i = 0; i < rounds; ++i) {
      pump_once(client);
    }
  }

  bool close() {
    if (closed_) {
      return true;
    }

    for (int i = 0; i < 200 && uv_loop_alive(&loop_) != 0; ++i) {
      uv_run(&loop_, UV_RUN_NOWAIT);
      CASE_THREAD_SLEEP_MS(1);
    }

    closed_ = (0 == uv_loop_close(&loop_));
    return closed_;
  }

 private:
  uv_loop_t loop_;
  bool closed_;
  time_t sec_;
  time_t usec_;
};
#  elif defined(HIREDIS_HAPP_ENABLE_LIBEVENT)
class EventLoopHarness {
 public:
  EventLoopHarness() : loop_(event_base_new()), closed_(false), sec_(1), usec_(0) {}

  ~EventLoopHarness() { close(); }

  void attach(redisAsyncContext *ctx) {
    CASE_EXPECT_NE(nullptr, ctx);
    if (nullptr != ctx) {
      redisLibeventAttach(ctx, loop_);
    }
  }

  template <class Client>
  void pump_once(Client &client, time_t step_usec = 10000) {
    usec_ += step_usec;
    while (usec_ >= 1000000) {
      ++sec_;
      usec_ -= 1000000;
    }

    client.proc(sec_, usec_);
    event_base_loop(loop_, EVLOOP_NONBLOCK);
    CASE_THREAD_SLEEP_MS(1);
  }

  template <class Client, class Predicate>
  bool pump_until(Client &client, Predicate predicate, int timeout_ms) {
    int rounds = timeout_ms / 10;
    if (rounds <= 0) {
      rounds = 1;
    }

    for (int i = 0; i < rounds && !predicate(); ++i) {
      pump_once(client);
    }

    return predicate();
  }

  template <class Client>
  void pump_for(Client &client, int total_ms) {
    int rounds = total_ms / 10;
    if (rounds <= 0) {
      rounds = 1;
    }

    for (int i = 0; i < rounds; ++i) {
      pump_once(client);
    }
  }

  bool close() {
    if (closed_) {
      return true;
    }

    if (nullptr != loop_) {
      event_base_free(loop_);
      loop_ = nullptr;
    }

    closed_ = true;
    return true;
  }

 private:
  event_base *loop_;
  bool closed_;
  time_t sec_;
  time_t usec_;
};
#  endif

}  // namespace

CASE_TEST(happ_integration_raw, ping_and_roundtrip) {
  const std::string host = get_env_or_default("HIREDIS_HAPP_TEST_SINGLE_HOST", "127.0.0.1");
  const uint16_t port = get_env_port("HIREDIS_HAPP_TEST_SINGLE_PORT", 6390);

  EventLoopHarness loop;
  hiredis::happ::raw raw;
  bool saw_connect = false;
  bool saw_connected = false;

  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_OK, raw.init(host, port));
  raw.set_on_connect([&loop, &saw_connect](hiredis::happ::raw *, hiredis::happ::connection *conn) {
    CASE_EXPECT_NE(nullptr, conn);
    saw_connect = true;
    if (nullptr != conn) {
      loop.attach(conn->get_context());
    }
  });
  raw.set_on_connected([&saw_connected](hiredis::happ::raw *, hiredis::happ::connection *,
                                        const struct redisAsyncContext *, int status) {
    if (REDIS_OK == status) {
      saw_connected = true;
    }
  });
  raw.set_timeout(5);

  ReplyCapture ping;
  const char *ping_argv[] = {"PING"};
  size_t ping_argv_len[] = {4};
  CASE_EXPECT_NE(nullptr, raw.exec(capture_reply, &ping, 1, ping_argv, ping_argv_len));
  CASE_EXPECT_TRUE(loop.pump_until(raw, [&ping]() { return ping.done; }, 5000));
  CASE_EXPECT_TRUE(saw_connect);
  CASE_EXPECT_TRUE(saw_connected);
  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_OK, ping.error_code);
  CASE_EXPECT_EQ(REDIS_REPLY_STATUS, ping.reply_type);
  CASE_EXPECT_TRUE("PONG" == ping.string_value);

  const std::string key = make_test_key("hiredis-happ:raw");
  ReplyCapture set_reply;
  const char *set_argv[] = {"SET", key.c_str(), "value-1"};
  size_t set_argv_len[] = {3, key.size(), 7};
  CASE_EXPECT_NE(nullptr, raw.exec(capture_reply, &set_reply, 3, set_argv, set_argv_len));
  CASE_EXPECT_TRUE(loop.pump_until(raw, [&set_reply]() { return set_reply.done; }, 5000));
  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_OK, set_reply.error_code);
  CASE_EXPECT_EQ(REDIS_REPLY_STATUS, set_reply.reply_type);
  CASE_EXPECT_TRUE("OK" == set_reply.string_value);

  ReplyCapture get_reply;
  const char *get_argv[] = {"GET", key.c_str()};
  size_t get_argv_len[] = {3, key.size()};
  CASE_EXPECT_NE(nullptr, raw.exec(capture_reply, &get_reply, 2, get_argv, get_argv_len));
  CASE_EXPECT_TRUE(loop.pump_until(raw, [&get_reply]() { return get_reply.done; }, 5000));
  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_OK, get_reply.error_code);
  CASE_EXPECT_EQ(REDIS_REPLY_STRING, get_reply.reply_type);
  CASE_EXPECT_TRUE("value-1" == get_reply.string_value);

  raw.reset();
  loop.pump_for(raw, 100);
  CASE_EXPECT_TRUE(loop.close());
}

CASE_TEST(happ_integration_cluster, set_get_and_hash_tag_roundtrip) {
  const std::string host = get_env_or_default("HIREDIS_HAPP_TEST_CLUSTER_HOST", "127.0.0.1");
  const uint16_t port = get_env_port("HIREDIS_HAPP_TEST_CLUSTER_PORT", 7300);

  EventLoopHarness loop;
  hiredis::happ::cluster clu;
  bool saw_connect = false;
  bool saw_connected = false;

  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_OK, clu.init(host, port));
  clu.set_on_connect([&loop, &saw_connect](hiredis::happ::cluster *, hiredis::happ::connection *conn) {
    CASE_EXPECT_NE(nullptr, conn);
    saw_connect = true;
    if (nullptr != conn) {
      loop.attach(conn->get_context());
    }
  });
  clu.set_on_connected([&saw_connected](hiredis::happ::cluster *, hiredis::happ::connection *,
                                        const struct redisAsyncContext *, int status) {
    if (REDIS_OK == status) {
      saw_connected = true;
    }
  });
  clu.set_timeout(5);
  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_OK, clu.start());

  const std::string tag = "{" + make_test_key("hiredis-happ:cluster") + "}";
  const std::string key_a = tag + ":a";
  const std::string key_b = tag + ":b";

  ReplyCapture mset_reply;
  const char *mset_argv[] = {"MSET", key_a.c_str(), "1", key_b.c_str(), "2"};
  size_t mset_argv_len[] = {4, key_a.size(), 1, key_b.size(), 1};
  CASE_EXPECT_NE(nullptr,
                 clu.exec(key_a.c_str(), key_a.size(), capture_reply, &mset_reply, 5, mset_argv, mset_argv_len));
  CASE_EXPECT_TRUE(loop.pump_until(clu, [&mset_reply]() { return mset_reply.done; }, 8000));
  CASE_EXPECT_TRUE(saw_connect);
  CASE_EXPECT_TRUE(saw_connected);
  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_OK, mset_reply.error_code);
  CASE_EXPECT_EQ(REDIS_REPLY_STATUS, mset_reply.reply_type);
  CASE_EXPECT_TRUE("OK" == mset_reply.string_value);

  ReplyCapture mget_reply;
  const char *mget_argv[] = {"MGET", key_a.c_str(), key_b.c_str()};
  size_t mget_argv_len[] = {4, key_a.size(), key_b.size()};
  CASE_EXPECT_NE(nullptr,
                 clu.exec(key_a.c_str(), key_a.size(), capture_reply, &mget_reply, 3, mget_argv, mget_argv_len));
  CASE_EXPECT_TRUE(loop.pump_until(clu, [&mget_reply]() { return mget_reply.done; }, 8000));
  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_OK, mget_reply.error_code);
  CASE_EXPECT_EQ(REDIS_REPLY_ARRAY, mget_reply.reply_type);
  CASE_EXPECT_EQ(static_cast<size_t>(2), mget_reply.array_values.size());
  CASE_EXPECT_TRUE("1" == mget_reply.array_values[0]);
  CASE_EXPECT_TRUE("2" == mget_reply.array_values[1]);

  ReplyCapture del_reply;
  const char *del_argv[] = {"DEL", key_a.c_str(), key_b.c_str()};
  size_t del_argv_len[] = {3, key_a.size(), key_b.size()};
  CASE_EXPECT_NE(nullptr, clu.exec(key_a.c_str(), key_a.size(), capture_reply, &del_reply, 3, del_argv, del_argv_len));
  CASE_EXPECT_TRUE(loop.pump_until(clu, [&del_reply]() { return del_reply.done; }, 8000));
  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_OK, del_reply.error_code);

  clu.reset();
  loop.pump_for(clu, 100);
  CASE_EXPECT_TRUE(loop.close());
}
#endif
