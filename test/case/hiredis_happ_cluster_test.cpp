#include <detail/crc16.h>
#include <detail/happ_cmd.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <set>

#include "frame/test_macros.h"
#include "hiredis_happ.h"
#include "test_redis_reply_helper.h"

namespace hiredis {
namespace happ {
struct cluster_unit_test_access {
  static bool is_slot_ok(const cluster &clu) { return cluster::slot_status::OK == clu.slot_flag_; }

  static size_t slot_host_count(const cluster &clu, int index) { return clu.slots_[index].hosts.size(); }

  static const connection::key_t &slot_host(const cluster &clu, int index, size_t host_index) {
    return clu.slots_[index].hosts[host_index];
  }

  static void on_reply_update_slot(cmd_exec *cmd, redisAsyncContext *ctx, void *reply) {
    cluster::on_reply_update_slot(cmd, ctx, reply, nullptr);
  }
};
}  // namespace happ
}  // namespace hiredis

static int expected_cluster_slot(const char *key, size_t key_len) {
  return static_cast<int>(hiredis::happ::crc16(key, key_len) % HIREDIS_HAPP_SLOT_NUMBER);
}

static int happ_cluster_f = 0;
static void on_connect_cbk_1(hiredis::happ::cluster *clu, hiredis::happ::connection *conn) {
  CASE_EXPECT_NE(nullptr, conn);
  CASE_EXPECT_NE(nullptr, clu);

  CASE_EXPECT_EQ(1, happ_cluster_f);
  ++happ_cluster_f;
}

static void on_connected_cbk_1(hiredis::happ::cluster *clu, hiredis::happ::connection *conn,
                               const struct redisAsyncContext *c, int status) {
  CASE_EXPECT_NE(nullptr, conn);
  CASE_EXPECT_NE(nullptr, clu);

  CASE_EXPECT_EQ(2, happ_cluster_f);
  ++happ_cluster_f;
}

static void on_disconnected_cbk_1(hiredis::happ::cluster *clu, hiredis::happ::connection *conn,
                                  const struct redisAsyncContext *c, int status) {
  CASE_EXPECT_NE(nullptr, conn);
  CASE_EXPECT_NE(nullptr, clu);

  CASE_EXPECT_EQ(2, happ_cluster_f);
  ++happ_cluster_f;
}

CASE_TEST(happ_cluster, connection_callback) {
  hiredis::happ::cluster clu;

  clu.init("127.0.0.1", 6370);

  happ_cluster_f = 1;

  clu.set_on_connect(on_connect_cbk_1);
  clu.set_on_connected(on_connected_cbk_1);
  clu.set_on_disconnected(on_disconnected_cbk_1);

  clu.proc(1, 0);
  CASE_EXPECT_EQ(1, happ_cluster_f);
  clu.set_timeout(57);
  clu.start();
  CASE_EXPECT_EQ(static_cast<size_t>(1), clu.get_timer_actions().timer_conns.size());
  CASE_EXPECT_TRUE("127.0.0.1:6370" == clu.get_timer_actions().timer_conns.front().name);
  CASE_EXPECT_EQ(static_cast<time_t>(58), clu.get_timer_actions().timer_conns.front().timeout);
  CASE_EXPECT_EQ(2, happ_cluster_f);

  clu.proc(57, 0);
  CASE_EXPECT_EQ(2, happ_cluster_f);

  clu.proc(58, 0);
  CASE_EXPECT_EQ(3, happ_cluster_f);

  CASE_EXPECT_EQ(static_cast<size_t>(0), clu.get_timer_actions().timer_conns.size());
  CASE_EXPECT_EQ(static_cast<size_t>(0), clu.get_connection_size());

  clu.reset();
}

CASE_TEST(happ_cluster, hash_tag_slot) {
  hiredis::happ::cluster clu;

  const char *tagged_key_1 = "{user1000}.following";
  const char *tagged_key_2 = "{user1000}.followers";
  const char *empty_tag_key = "foo{}{bar}";
  const char *nested_tag_key = "foo{{bar}}zap";
  const char *multi_tag_key = "foo{bar}{zap}";

  const hiredis::happ::cluster::slot_t *tagged_slot_1 = clu.get_slot_by_key(tagged_key_1, strlen(tagged_key_1));
  const hiredis::happ::cluster::slot_t *tagged_slot_2 = clu.get_slot_by_key(tagged_key_2, strlen(tagged_key_2));
  const hiredis::happ::cluster::slot_t *empty_tag_slot = clu.get_slot_by_key(empty_tag_key, strlen(empty_tag_key));
  const hiredis::happ::cluster::slot_t *nested_tag_slot = clu.get_slot_by_key(nested_tag_key, strlen(nested_tag_key));
  const hiredis::happ::cluster::slot_t *multi_tag_slot = clu.get_slot_by_key(multi_tag_key, strlen(multi_tag_key));

  CASE_EXPECT_NE(nullptr, tagged_slot_1);
  CASE_EXPECT_NE(nullptr, tagged_slot_2);
  CASE_EXPECT_NE(nullptr, empty_tag_slot);
  CASE_EXPECT_NE(nullptr, nested_tag_slot);
  CASE_EXPECT_NE(nullptr, multi_tag_slot);

  CASE_EXPECT_EQ(expected_cluster_slot("user1000", strlen("user1000")), tagged_slot_1->index);
  CASE_EXPECT_EQ(tagged_slot_1->index, tagged_slot_2->index);
  CASE_EXPECT_EQ(expected_cluster_slot(empty_tag_key, strlen(empty_tag_key)), empty_tag_slot->index);
  CASE_EXPECT_EQ(expected_cluster_slot("{bar", strlen("{bar")), nested_tag_slot->index);
  CASE_EXPECT_EQ(expected_cluster_slot("bar", strlen("bar")), multi_tag_slot->index);
  CASE_EXPECT_EQ(nullptr, clu.get_slot_by_key(nullptr, 0));
}

CASE_TEST(happ_cluster, slot_reply_nil_endpoint_fallback) {
  hiredis::happ::cluster clu;
  clu.init("10.0.0.1", 7000);

  hiredis::happ::holder_t h;
  h.clu = &clu;
  hiredis::happ::cmd_exec *cmd = hiredis::happ::cmd_exec::create(h, nullptr, nullptr, 0);
  CASE_EXPECT_NE(nullptr, cmd);

  redisAsyncContext vir_context;
  memset(&vir_context, 0, sizeof(vir_context));
  vir_context.c.connection_type = REDIS_CONN_TCP;
  vir_context.c.tcp.host = const_cast<char *>("127.0.0.1");

  hiredis_happ_test::redis_reply_ptr reply =
      hiredis_happ_test::adopt_reply(hiredis_happ_test::make_array_reply({hiredis_happ_test::make_array_reply(
          {hiredis_happ_test::make_integer_reply(0), hiredis_happ_test::make_integer_reply(1),
           hiredis_happ_test::make_array_reply(
               {hiredis_happ_test::make_nil_reply(), hiredis_happ_test::make_integer_reply(7000)}),
           hiredis_happ_test::make_array_reply(
               {hiredis_happ_test::make_string_reply("127.0.0.2"), hiredis_happ_test::make_integer_reply(7001)})})}));
  CASE_EXPECT_NE(nullptr, reply.get());

  hiredis::happ::cluster_unit_test_access::on_reply_update_slot(cmd, &vir_context, reply.get());
  CASE_EXPECT_TRUE(hiredis::happ::cluster_unit_test_access::is_slot_ok(clu));
  CASE_EXPECT_EQ(static_cast<size_t>(2), hiredis::happ::cluster_unit_test_access::slot_host_count(clu, 0));
  CASE_EXPECT_TRUE("127.0.0.1" == hiredis::happ::cluster_unit_test_access::slot_host(clu, 0, 0).ip);
  CASE_EXPECT_EQ(static_cast<uint16_t>(7000), hiredis::happ::cluster_unit_test_access::slot_host(clu, 0, 0).port);
  CASE_EXPECT_TRUE("127.0.0.2" == hiredis::happ::cluster_unit_test_access::slot_host(clu, 1, 1).ip);
  CASE_EXPECT_EQ(static_cast<uint16_t>(7001), hiredis::happ::cluster_unit_test_access::slot_host(clu, 1, 1).port);

  hiredis::happ::cmd_exec::destroy(cmd);
}

CASE_TEST(happ_cluster, slot_reply_empty_endpoint_and_range_clamp) {
  hiredis::happ::cluster clu;
  clu.init("10.0.0.9", 7999);

  hiredis::happ::holder_t h;
  h.clu = &clu;
  hiredis::happ::cmd_exec *cmd = hiredis::happ::cmd_exec::create(h, nullptr, nullptr, 0);
  CASE_EXPECT_NE(nullptr, cmd);

  redisAsyncContext vir_context;
  memset(&vir_context, 0, sizeof(vir_context));
  vir_context.c.connection_type = REDIS_CONN_TCP;
  vir_context.c.tcp.host = const_cast<char *>("127.0.0.9");

  hiredis_happ_test::redis_reply_ptr reply =
      hiredis_happ_test::adopt_reply(hiredis_happ_test::make_array_reply({hiredis_happ_test::make_array_reply(
          {hiredis_happ_test::make_integer_reply(-10),
           hiredis_happ_test::make_integer_reply(HIREDIS_HAPP_SLOT_NUMBER + 16),
           hiredis_happ_test::make_array_reply(
               {hiredis_happ_test::make_string_reply(""), hiredis_happ_test::make_integer_reply(7300)})})}));
  CASE_EXPECT_NE(nullptr, reply.get());

  hiredis::happ::cluster_unit_test_access::on_reply_update_slot(cmd, &vir_context, reply.get());
  CASE_EXPECT_TRUE(hiredis::happ::cluster_unit_test_access::is_slot_ok(clu));
  CASE_EXPECT_EQ(static_cast<size_t>(1), hiredis::happ::cluster_unit_test_access::slot_host_count(clu, 0));
  CASE_EXPECT_EQ(static_cast<size_t>(1),
                 hiredis::happ::cluster_unit_test_access::slot_host_count(clu, HIREDIS_HAPP_SLOT_NUMBER - 1));
  CASE_EXPECT_TRUE("127.0.0.9" == hiredis::happ::cluster_unit_test_access::slot_host(clu, 0, 0).ip);
  CASE_EXPECT_EQ(static_cast<uint16_t>(7300), hiredis::happ::cluster_unit_test_access::slot_host(clu, 0, 0).port);

  hiredis::happ::cmd_exec::destroy(cmd);
}

CASE_TEST(happ_cluster, slot_reply_unknown_endpoint_skips_slot_host) {
  hiredis::happ::cluster clu;
  clu.init("127.0.0.9", 7999);

  hiredis::happ::holder_t h;
  h.clu = &clu;
  hiredis::happ::cmd_exec *cmd = hiredis::happ::cmd_exec::create(h, nullptr, nullptr, 0);
  CASE_EXPECT_NE(nullptr, cmd);

  redisAsyncContext vir_context;
  memset(&vir_context, 0, sizeof(vir_context));
  vir_context.c.connection_type = REDIS_CONN_TCP;
  vir_context.c.tcp.host = const_cast<char *>("127.0.0.9");

  hiredis_happ_test::redis_reply_ptr reply =
      hiredis_happ_test::adopt_reply(hiredis_happ_test::make_array_reply({hiredis_happ_test::make_array_reply(
          {hiredis_happ_test::make_integer_reply(3), hiredis_happ_test::make_integer_reply(3),
           hiredis_happ_test::make_array_reply(
               {hiredis_happ_test::make_string_reply("?"), hiredis_happ_test::make_integer_reply(7303)})})}));
  CASE_EXPECT_NE(nullptr, reply.get());

  hiredis::happ::cluster_unit_test_access::on_reply_update_slot(cmd, &vir_context, reply.get());
  CASE_EXPECT_TRUE(hiredis::happ::cluster_unit_test_access::is_slot_ok(clu));
  CASE_EXPECT_EQ(static_cast<size_t>(0), hiredis::happ::cluster_unit_test_access::slot_host_count(clu, 3));
  CASE_EXPECT_TRUE("127.0.0.9:7999" == clu.get_slot_master(3)->name);

  hiredis::happ::cmd_exec::destroy(cmd);
}
