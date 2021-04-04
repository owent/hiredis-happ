#include <cstdio>
#include <cstring>
#include <ctime>
#include <detail/happ_cmd.h>
#include <iostream>
#include <set>

#include "frame/test_macros.h"
#include "hiredis_happ.h"

static void happ_cmd_basic_1(hiredis::happ::cmd_exec *cmd, struct redisAsyncContext *c, void *r,
                             void *pridata) {
  CASE_EXPECT_EQ(cmd, pridata);
  CASE_EXPECT_EQ(c, r);

  CASE_EXPECT_NE(r, nullptr);
  CASE_EXPECT_NE(pridata, nullptr);

  CASE_EXPECT_EQ(131313131, *reinterpret_cast<int *>(cmd->buffer()));
}

CASE_TEST(happ_cmd, basic) {
  hiredis::happ::holder_t h;
  hiredis::happ::cluster clu;
  redisAsyncContext vir_ontext;
  h.clu = &clu;

  hiredis::happ::cmd_exec *cmd =
      hiredis::happ::cmd_exec::create(h, happ_cmd_basic_1, &clu, sizeof(int));

  CASE_EXPECT_EQ(cmd->get_holder().clu, &clu);
  CASE_EXPECT_EQ(cmd->private_data(), &clu);
  CASE_EXPECT_EQ(cmd->get_callback_fn(), happ_cmd_basic_1);

  cmd->private_data(cmd);
  *reinterpret_cast<int *>(cmd->buffer()) = 131313131;

  int len = cmd->format("GET %s", "HERO");
  CASE_EXPECT_EQ(static_cast<size_t>(len), cmd->get_cmd_raw_content().raw_len);

  std::string cmd_content_1, cmd_content_2;
  cmd_content_1.assign(cmd->get_cmd_raw_content().content.raw, cmd->get_cmd_raw_content().raw_len);

  const char *argv[] = {"GET", "HERO"};
  size_t argvlen[] = {strlen(argv[0]), strlen(argv[1])};
  len = cmd->vformat(2, argv, argvlen);
  CASE_EXPECT_EQ(static_cast<size_t>(0), cmd->get_cmd_raw_content().raw_len);

  cmd_content_2.assign(cmd->get_cmd_raw_content().content.redis_sds, static_cast<size_t>(len));
  CASE_EXPECT_EQ(static_cast<size_t>(len), sdslen(cmd->get_cmd_raw_content().content.redis_sds));
  CASE_EXPECT_EQ(cmd_content_1, cmd_content_2);

  int res = cmd->call_reply(hiredis::happ::error_code::REDIS_HAPP_TTL, &vir_ontext, &vir_ontext);
  CASE_EXPECT_EQ(res, hiredis::happ::error_code::REDIS_HAPP_OK);
  CASE_EXPECT_EQ(cmd->get_error_code(), hiredis::happ::error_code::REDIS_HAPP_TTL);

  CASE_EXPECT_EQ(cmd->get_callback_fn(), nullptr);

  res = cmd->call_reply(hiredis::happ::error_code::REDIS_HAPP_TTL, &vir_ontext, nullptr);
  CASE_EXPECT_EQ(res, hiredis::happ::error_code::REDIS_HAPP_OK);

  hiredis::happ::cmd_exec::destroy(cmd);
}
