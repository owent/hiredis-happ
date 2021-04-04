#include <cstdio>
#include <cstring>
#include <ctime>
#include <detail/happ_cmd.h>
#include <iostream>
#include <set>

#include "frame/test_macros.h"
#include "hiredis_happ.h"

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
