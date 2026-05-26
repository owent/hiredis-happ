#include <detail/crc16.h>
#include <detail/happ_cmd.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <set>

#include "frame/test_macros.h"
#include "hiredis_happ.h"

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
