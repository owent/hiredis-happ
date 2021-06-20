//
// Created by OWenT on 2015/08/19.
//

#ifndef HIREDIS_HAPP_HIREDIS_HAPP_CMD_H
#define HIREDIS_HAPP_HIREDIS_HAPP_CMD_H

#pragma once

#include <ostream>

#include "hiredis_happ_config.h"

namespace hiredis {
namespace happ {
class cluster;
class raw;
class connection;

union HIREDIS_HAPP_API_HEAD_ONLY holder_t {
  cluster *clu;
  raw *r;
};

struct HIREDIS_HAPP_API_HEAD_ONLY cmd_content {
  size_t raw_len;
  union {
    char *raw;
    sds redis_sds;
  } content;
};

class cmd_exec {
 public:
  typedef void (*callback_fn_t)(cmd_exec *, struct redisAsyncContext *, void *, void *);

  HIREDIS_HAPP_API int vformat(int argc, const char **argv, const size_t *argvlen);

  HIREDIS_HAPP_API int format(const char *fmt, ...);

  HIREDIS_HAPP_API int vformat(const char *fmt, va_list ap);

  HIREDIS_HAPP_API int vformat(const sds *src);

  HIREDIS_HAPP_API int call_reply(int rcode, redisAsyncContext *context, void *reply);

  HIREDIS_HAPP_API int result() const;

  HIREDIS_HAPP_API void *buffer();

  HIREDIS_HAPP_API const void *buffer() const;

  HIREDIS_HAPP_API void *private_data() const;

  HIREDIS_HAPP_API void private_data(void *pd);

  HIREDIS_HAPP_API const char *pick_argument(const char *start, const char **str, size_t *len);

  HIREDIS_HAPP_API const char *pick_cmd(const char **str, size_t *len);

  static HIREDIS_HAPP_API void dump(std::ostream &out, redisReply *reply, int ident = 0);

  HIREDIS_HAPP_API holder_t get_holder() const;

  HIREDIS_HAPP_API callback_fn_t get_callback_fn() const;

  HIREDIS_HAPP_API cmd_content get_cmd_raw_content() const;

  HIREDIS_HAPP_API int get_error_code() const;

  /**
   * @brief create raw_cmd_content_ object(This function is public only for unit test, please don't
   * use it directly)
   * @param holder_ owner of this
   * @param cbk callback_ when raw_cmd_content_ finished
   * @param pridata private data
   * @param buff_len alloacte some memory inner raw_cmd_content_(this can be used to store some more
   * data for later usage)
   * @return address of raw_cmd_content_ object if success
   */
  static HIREDIS_HAPP_API cmd_exec *create(holder_t holder_, callback_fn_t cbk, void *pridata, size_t buffer_len);

  /**
   * @brief destroy raw_cmd_content_ object(This function is public only for unit test, please don't
   * use it directly)
   * @param c destroy raw_cmd_content_ object
   */
  static HIREDIS_HAPP_API void destroy(cmd_exec *c);

 private:
  friend class cluster;
  friend class raw;
  friend class connection;

  holder_t holder_;  // holder_
  cmd_content raw_cmd_content_;
  size_t ttl_;              // left retry times(just like network ttl_)
  callback_fn_t callback_;  // user callback_ function

  // ========= exec data =========
  int error_code_;  // error code, just like redisAsyncContext::error_code_
  union {
    int slot;  // slot index if in cluster, -1 means random
  } engine_;

  void *private_data_;  // user pri data
};
}  // namespace happ
}  // namespace hiredis

#endif  // HIREDIS_HAPP_HIREDIS_HAPP_CMD_H
