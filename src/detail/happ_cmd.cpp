
#include <algorithm>
#include <assert.h>
#include <cstdlib>
#include <cstring>
#include <iomanip>

#include "detail/happ_cmd.h"

#if (defined(__cplusplus) && __cplusplus >= 201103L) || (defined(_MSC_VER) && _MSC_VER >= 1600)
#  include <type_traits>

// static_assert(std::is_pod<hiredis::happ::cmd_exec>::value, "hiredis::happ::cmd_exec should be a
// pod type");

#endif

namespace hiredis {
namespace happ {

HIREDIS_HAPP_API cmd_exec::cmd_exec()
    : ttl_(HIREDIS_HAPP_TTL), callback_(nullptr), error_code_(0), private_data_(nullptr) {
  holder_.r = nullptr;
  raw_cmd_content_.raw_len = 0;
  raw_cmd_content_.content.raw = nullptr;
  engine_.slot = -1;
}

HIREDIS_HAPP_API cmd_exec::~cmd_exec() {}

HIREDIS_HAPP_API cmd_exec *cmd_exec::create(holder_t holder_, callback_fn_t cbk, void *pridata,
                                            size_t buffer_len) {
  size_t sum_len = sizeof(cmd_exec) + buffer_len;
  // padding to sizeof(void*)
  sum_len = (sum_len + sizeof(void *) - 1) & (~(sizeof(void *) - 1));

  cmd_exec *ret = reinterpret_cast<cmd_exec *>(malloc(sum_len));

  if (NULL == ret) {
    return NULL;
  }

  memset(ret, 0, sizeof(cmd_exec));

  ret->holder_ = holder_;
  ret->callback_ = cbk;
  ret->private_data_ = pridata;
  ret->ttl_ = HIREDIS_HAPP_TTL;

  ret->engine_.slot = -1;
  return ret;
}

static void free_cmd_content(cmd_content *c) {
  if (NULL == c) {
    return;
  }

  if (0 == c->raw_len) {
    if (NULL != c->content.redis_sds) {
      redisFreeSdsCommand(c->content.redis_sds);
      c->content.redis_sds = NULL;
    }
  } else {
    if (NULL != c->content.raw) {
      redisFreeCommand(c->content.raw);
      c->content.raw = NULL;
    }
    c->raw_len = 0;
  }
}

HIREDIS_HAPP_API void cmd_exec::destroy(cmd_exec *c) {
  if (NULL == c) {
    return;
  }

  free_cmd_content(&c->raw_cmd_content_);

  free(c);
}

HIREDIS_HAPP_API int cmd_exec::vformat(int argc, const char **argv, const size_t *argvlen) {
  free_cmd_content(&raw_cmd_content_);

  raw_cmd_content_.raw_len = 0;
  return redisFormatSdsCommandArgv(&raw_cmd_content_.content.redis_sds, argc, argv, argvlen);
}

HIREDIS_HAPP_API int cmd_exec::format(const char *fmt, ...) {
  va_list ap;

  free_cmd_content(&raw_cmd_content_);
  va_start(ap, fmt);
  int ret = redisvFormatCommand(&raw_cmd_content_.content.raw, fmt, ap);
  va_end(ap);

  raw_cmd_content_.raw_len = ret;
  return ret;
}

HIREDIS_HAPP_API int cmd_exec::vformat(const char *fmt, va_list ap) {
  free_cmd_content(&raw_cmd_content_);

  va_list ap_c;
  va_copy(ap_c, ap);

  int ret = redisvFormatCommand(&raw_cmd_content_.content.raw, fmt, ap_c);
  raw_cmd_content_.raw_len = ret;
  return ret;
}

HIREDIS_HAPP_API int cmd_exec::vformat(const sds *src) {
  free_cmd_content(&raw_cmd_content_);

  if (NULL == src) {
    return 0;
  }

  raw_cmd_content_.content.redis_sds = sdsdup(*src);
  raw_cmd_content_.raw_len = 0;

  return static_cast<int>(sdslen(raw_cmd_content_.content.redis_sds));
}

HIREDIS_HAPP_API int cmd_exec::call_reply(int rcode, redisAsyncContext *context, void *reply) {
  if (NULL == callback_) {
    return error_code::REDIS_HAPP_OK;
  }

  error_code_ = rcode;
  callback_fn_t tc = callback_;
  callback_ = NULL;
  tc(this, context, reply, private_data_);

  return error_code::REDIS_HAPP_OK;
}

HIREDIS_HAPP_API void *cmd_exec::buffer() { return reinterpret_cast<void *>(this + 1); }

HIREDIS_HAPP_API const void *cmd_exec::buffer() const {
  return reinterpret_cast<const void *>(this + 1);
}

HIREDIS_HAPP_API void *cmd_exec::private_data() const { return private_data_; }

HIREDIS_HAPP_API void cmd_exec::private_data(void *pd) { private_data_ = pd; }

HIREDIS_HAPP_API const char *cmd_exec::pick_argument(const char *start, const char **str,
                                                     size_t *len) {
  if (NULL == start) {
    if (0 == raw_cmd_content_.raw_len) {
      // because sds is typedefed to be a char*, so we can only use it directly here.
      start = raw_cmd_content_.content.redis_sds;
    } else {
      start = raw_cmd_content_.content.raw;
    }
  }

  if (NULL == start) {
    return NULL;
  }

  // @see http://redis.io/topics/protocol
  // Clients send commands to a Redis server as a RESP Array of Bulk Strings
  if (start[0] != '$') {
    start = strchr(start, '$');
    if (NULL == start) return NULL;
  }

  if (NULL == len || NULL == str) {
    return start;
  }

  // redis bulk strings can not be greater than 512MB
  *len = static_cast<size_t>(strtol(start + 1, NULL, 10));
  start = strchr(start, '\r');
  assert(start);

  // bulk string format: $[LENGTH]\r\n[CONTENT]\r\n
  *str = start + 2;
  return start + 2 + (*len) + 2;
}

HIREDIS_HAPP_API const char *cmd_exec::pick_cmd(const char **str, size_t *len) {
  return pick_argument(NULL, str, len);
}

HIREDIS_HAPP_API void cmd_exec::dump(std::ostream &out, redisReply *reply, int ident) {
  if (NULL == reply) {
    return;
  }

  // dump reply
  switch (reply->type) {
    case REDIS_REPLY_NIL: {
      out << "[NIL]" << std::endl;
      break;
    }
    case REDIS_REPLY_STATUS: {
      out << "[STATUS]: " << reply->str << std::endl;
      break;
    }
    case REDIS_REPLY_ERROR: {
      out << "[ERROR]: " << reply->str << std::endl;
      break;
    }
    case REDIS_REPLY_INTEGER: {
      out << reply->integer << std::endl;
      break;
    }
    case REDIS_REPLY_STRING: {
      out << reply->str << std::endl;
      break;
    }
    case REDIS_REPLY_ARRAY: {
      std::string ident_str;
      ident_str.assign(static_cast<size_t>(ident), ' ');

      out << "[ARRAY]: " << std::endl;
      for (size_t i = 0; i < reply->elements; ++i) {
        out << ident_str << std::setw(7) << (i + 1) << ": ";
        dump(out, reply->element[i], ident + 2);
      }

      break;
    }
    default: {
      out << "[UNKNOWN]" << std::endl;
      break;
    }
  }
}

HIREDIS_HAPP_API holder_t cmd_exec::get_holder() const { return holder_; }

HIREDIS_HAPP_API cmd_exec::callback_fn_t cmd_exec::get_callback_fn() const { return callback_; }

HIREDIS_HAPP_API cmd_content cmd_exec::get_cmd_raw_content() const { return raw_cmd_content_; }

HIREDIS_HAPP_API int cmd_exec::get_error_code() const { return error_code_; }
}  // namespace happ
}  // namespace hiredis
