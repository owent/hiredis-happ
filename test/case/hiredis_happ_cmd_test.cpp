#include <detail/happ_cmd.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <set>
#include <sstream>


#include "frame/test_macros.h"
#include "hiredis_happ.h"
#include "test_redis_reply_helper.h"

static void happ_cmd_basic_1(hiredis::happ::cmd_exec *cmd, struct redisAsyncContext *c, void *r, void *pridata) {
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

  hiredis::happ::cmd_exec *cmd = hiredis::happ::cmd_exec::create(h, happ_cmd_basic_1, &clu, sizeof(int));

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

CASE_TEST(happ_cmd, pick_argument_vformat_and_dump) {
  hiredis::happ::holder_t h;
  h.clu = nullptr;

  hiredis::happ::cmd_exec *cmd = hiredis::happ::cmd_exec::create(h, nullptr, nullptr, 0);
  CASE_EXPECT_NE(nullptr, cmd);

  int len = cmd->format("SET %s %s", "player", "42");
  CASE_EXPECT_GT(len, 0);

  const char *arg = nullptr;
  size_t arg_len = 0;
  const char *next = cmd->pick_cmd(&arg, &arg_len);
  CASE_EXPECT_TRUE(std::string(arg, arg_len) == "SET");

  next = cmd->pick_argument(next, &arg, &arg_len);
  CASE_EXPECT_TRUE(std::string(arg, arg_len) == "player");

  next = cmd->pick_argument(next, &arg, &arg_len);
  CASE_EXPECT_TRUE(std::string(arg, arg_len) == "42");

  const char *raw_sds_cmd = "*2\r\n$4\r\nLLEN\r\n$6\r\nfoobar\r\n";
  sds input = sdsnewlen(raw_sds_cmd, strlen(raw_sds_cmd));
  CASE_EXPECT_NE(nullptr, input);
  len = cmd->vformat(&input);
  CASE_EXPECT_EQ(static_cast<int>(sdslen(input)), len);

  next = cmd->pick_cmd(&arg, &arg_len);
  CASE_EXPECT_TRUE(std::string(arg, arg_len) == "LLEN");
  next = cmd->pick_argument(next, &arg, &arg_len);
  CASE_EXPECT_TRUE(std::string(arg, arg_len) == "foobar");

  sdsfree(input);

  CASE_EXPECT_EQ(0, cmd->vformat(static_cast<const sds *>(nullptr)));

  hiredis_happ_test::redis_reply_ptr reply = hiredis_happ_test::adopt_reply(hiredis_happ_test::make_array_reply(
      {hiredis_happ_test::make_status_reply("OK"), hiredis_happ_test::make_integer_reply(7),
       hiredis_happ_test::make_string_reply("hello"), hiredis_happ_test::make_nil_reply(),
       hiredis_happ_test::make_error_reply("ERR boom")}));
  CASE_EXPECT_NE(nullptr, reply.get());

  std::ostringstream oss;
  hiredis::happ::cmd_exec::dump(oss, reply.get(), 0);
  const std::string dump_output = oss.str();
  CASE_EXPECT_NE(std::string::npos, dump_output.find("[ARRAY]"));
  CASE_EXPECT_NE(std::string::npos, dump_output.find("[STATUS]: OK"));
  CASE_EXPECT_NE(std::string::npos, dump_output.find("hello"));
  CASE_EXPECT_NE(std::string::npos, dump_output.find("[NIL]"));
  CASE_EXPECT_NE(std::string::npos, dump_output.find("[ERROR]: ERR boom"));

  hiredis::happ::cmd_exec::destroy(cmd);
}
