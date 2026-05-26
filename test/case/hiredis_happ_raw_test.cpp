#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>

#include "frame/test_macros.h"
#include "hiredis_happ.h"

static void happ_raw_on_connect_1(hiredis::happ::raw *, hiredis::happ::connection *) {}
static void happ_raw_on_connect_2(hiredis::happ::raw *, hiredis::happ::connection *) {}
static void happ_raw_on_connected_1(hiredis::happ::raw *, hiredis::happ::connection *, const struct redisAsyncContext *,
                                    int) {}
static void happ_raw_on_disconnected_1(hiredis::happ::raw *, hiredis::happ::connection *,
                                       const struct redisAsyncContext *, int) {}
static const std::string &happ_raw_auth_passthrough(hiredis::happ::connection *, const std::string &passwd) {
  return passwd;
}

CASE_TEST(happ_raw, basic_config_and_timer) {
  hiredis::happ::raw raw;

  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_OK, raw.init("127.0.0.1", 6380));
  CASE_EXPECT_TRUE(raw.get_auth_password().empty());
  raw.set_auth_password("secret");
  CASE_EXPECT_TRUE("secret" == raw.get_auth_password());

  CASE_EXPECT_FALSE(static_cast<bool>(raw.get_auth_fn()));
  raw.set_auth_fn(happ_raw_auth_passthrough);
  CASE_EXPECT_TRUE(static_cast<bool>(raw.get_auth_fn()));

  CASE_EXPECT_FALSE(static_cast<bool>(raw.set_on_connect(happ_raw_on_connect_1)));
  CASE_EXPECT_TRUE(static_cast<bool>(raw.set_on_connect(happ_raw_on_connect_2)));
  CASE_EXPECT_FALSE(static_cast<bool>(raw.set_on_connected(happ_raw_on_connected_1)));
  CASE_EXPECT_FALSE(static_cast<bool>(raw.set_on_disconnected(happ_raw_on_disconnected_1)));

  raw.set_cmd_buffer_size(96);
  CASE_EXPECT_EQ(static_cast<size_t>(96), raw.get_cmd_buffer_size());

  CASE_EXPECT_TRUE(raw.is_timer_available());
  CASE_EXPECT_FALSE(raw.is_timer_active());
  raw.set_timer_interval(0, 0);
  CASE_EXPECT_FALSE(raw.is_timer_available());
  raw.proc(1, 0);
  CASE_EXPECT_FALSE(raw.is_timer_active());

  raw.set_timer_interval(0, 1000);
  CASE_EXPECT_TRUE(raw.is_timer_available());
  CASE_EXPECT_TRUE(raw.is_timer_active());
  raw.proc(1, 1000);
  CASE_EXPECT_TRUE(raw.is_timer_active());

  raw.set_timeout(3);
  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_OK, raw.start());
  CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_OK, raw.reset());
  CASE_EXPECT_FALSE(raw.is_timer_active());
}
