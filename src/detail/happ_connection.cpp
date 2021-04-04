
#include <algorithm>
#include <assert.h>
#include <cstdlib>
#include <cstring>

#include "detail/happ_connection.h"

namespace hiredis {
namespace happ {
HIREDIS_HAPP_API connection::connection()
    : sequence_(0), context_(NULL), conn_status_(status::DISCONNECTED) {
  make_sequence();
  holder_.clu = NULL;
}

HIREDIS_HAPP_API connection::~connection() { release(true); }

HIREDIS_HAPP_API uint64_t connection::get_sequence() const { return sequence_; }

HIREDIS_HAPP_API void connection::init(holder_t h, const std::string &ip, uint16_t port) {
  set_key(key_, ip, port);
  init(h, key_);
}

HIREDIS_HAPP_API void connection::init(holder_t h, const key_t &k) {
  if (&key_ != &k) {
    key_ = k;
  }

  holder_ = h;
}

HIREDIS_HAPP_API connection::status::type connection::set_connecting(redisAsyncContext *c) {
  status::type ret = conn_status_;
  if (status::CONNECTING == conn_status_) {
    return ret;
  }

  if (c == context_) {
    return ret;
  }

  context_ = c;
  conn_status_ = status::CONNECTING;
  c->data = this;

  // new operation sequence_
  make_sequence();
  return ret;
}

HIREDIS_HAPP_API connection::status::type connection::set_disconnected(bool close_fd) {
  status::type ret = conn_status_;
  if (status::DISCONNECTED == conn_status_) {
    return ret;
  }

  // 先设置，防止重入
  conn_status_ = status::DISCONNECTED;

  release(close_fd);

  // new operation sequence_
  make_sequence();
  return ret;
}

HIREDIS_HAPP_API connection::status::type connection::set_connected() {
  status::type ret = conn_status_;
  if (status::CONNECTING != conn_status_ || NULL == context_) {
    return ret;
  }

  conn_status_ = status::CONNECTED;

  // new operation sequence_
  make_sequence();

  return ret;
}

HIREDIS_HAPP_API int connection::redis_cmd(cmd_exec *c, redisCallbackFn fn) {
  if (NULL == c) {
    return error_code::REDIS_HAPP_PARAM;
  }

  if (NULL == context_) {
    return error_code::REDIS_HAPP_CREATE;
  }

  switch (conn_status_) {
    case status::DISCONNECTED: {  // failed if diconnected
      return error_code::REDIS_HAPP_CONNECTION;
    }

    // call redisAsyncFormattedCommand if connecting or connected.
    // this request will be added to hiredis's request queue
    // we should send data in order to trigger callback
    case status::CONNECTING:
    case status::CONNECTED: {
      int res = 0;
      const char *cstr = NULL;
      size_t clen = 0;
      if (0 == c->raw_cmd_content_.raw_len) {
        res = redisAsyncFormattedCommand(context_, fn, c, c->raw_cmd_content_.content.redis_sds,
                                         sdslen(c->raw_cmd_content_.content.redis_sds));
      } else {
        res = redisAsyncFormattedCommand(context_, fn, c, c->raw_cmd_content_.content.raw,
                                         c->raw_cmd_content_.raw_len);
      }

      if (REDIS_OK == res) {
        c->pick_cmd(&cstr, &clen);
        if (NULL == cstr) {
          reply_list_.push_back(c);
        } else {
          bool is_pattern = tolower(cstr[0]) == 'p';
          if (is_pattern) {
            ++cstr;
          }

          // according to the hiredis code, we can not use both monitor and subscribe in the same
          // connection
          // @note hiredis use a tricky way to check if a reply is subscribe message or
          // request-response message,
          //       so it 's  recommanded not to use both subscribe message and request-response
          //       message at a connection.
          if (0 == HIREDIS_HAPP_STRNCASE_CMP(cstr, "subscribe\r\n", 11)) {
            // subscribe message has not reply
            cmd_exec::destroy(c);
          } else if (0 == HIREDIS_HAPP_STRNCASE_CMP(cstr, "unsubscribe\r\n", 13)) {
            // unsubscribe message has not reply
            cmd_exec::destroy(c);
          } else if (0 == HIREDIS_HAPP_STRNCASE_CMP(cstr, "monitor\r\n", 9)) {
            // monitor message has not reply
            cmd_exec::destroy(c);
          } else {
            // request-response message
            reply_list_.push_back(c);
          }
        }
      }

      return res;
    }
    default: {
      assert(0);
      break;
    }
  }

  // unknown error, recycle cmd
  c->call_reply(error_code::REDIS_HAPP_UNKNOWD, context_, NULL);
  cmd_exec::destroy(c);

  return error_code::REDIS_HAPP_OK;
}

HIREDIS_HAPP_API int connection::redis_raw_cmd(redisCallbackFn *fn, void *priv_data,
                                               const char *fmt, ...) {
  if (NULL == context_) {
    return error_code::REDIS_HAPP_CREATE;
  }

  va_list ap;
  va_start(ap, fmt);
  int res = redisvAsyncCommand(context_, fn, priv_data, fmt, ap);
  va_end(ap);

  return res;
}

HIREDIS_HAPP_API int connection::redis_raw_cmd(redisCallbackFn *fn, void *priv_data,
                                               const char *fmt, va_list ap) {
  if (NULL == context_) {
    return error_code::REDIS_HAPP_CREATE;
  }

  return redisvAsyncCommand(context_, fn, priv_data, fmt, ap);
}

HIREDIS_HAPP_API int connection::redis_raw_cmd(redisCallbackFn *fn, void *priv_data,
                                               const sds *src) {
  if (NULL == context_) {
    return error_code::REDIS_HAPP_CREATE;
  }

  return redisAsyncFormattedCommand(context_, fn, priv_data, *src, sdslen(*src));
}

HIREDIS_HAPP_API int connection::redis_raw_cmd(redisCallbackFn *fn, void *priv_data, int argc,
                                               const char **argv, const size_t *argvlen) {
  if (NULL == context_) {
    return error_code::REDIS_HAPP_CREATE;
  }

  return redisAsyncCommandArgv(context_, fn, priv_data, argc, argv, argvlen);
}

HIREDIS_HAPP_API int connection::call_reply(cmd_exec *c, void *r) {
  if (NULL == context_) {
    // make sure to destroy cmd
    if (NULL != c) {
      c->error_code_ = error_code::REDIS_HAPP_NOT_FOUND;
      c->call_reply(c->error_code_, context_, r);
      cmd_exec::destroy(c);
    }

    return error_code::REDIS_HAPP_CREATE;
  }

  cmd_exec *sc = pop_reply(c);

  if (NULL == sc) {
    // make sure to destroy cmd
    if (NULL != c) {
      c->error_code_ = error_code::REDIS_HAPP_NOT_FOUND;
      c->call_reply(c->error_code_, context_, r);
      cmd_exec::destroy(c);
    }
    return error_code::REDIS_HAPP_NOT_FOUND;
  }

  // translate error code
  if (REDIS_OK != context_->err) {
    sc->error_code_ = error_code::REDIS_HAPP_HIREDIS;
  } else if (r) {
    redisReply *reply = reinterpret_cast<redisReply *>(r);
    if (REDIS_REPLY_ERROR == reply->type) {
      sc->error_code_ = error_code::REDIS_HAPP_HIREDIS;
    }
  }

  int res = sc->call_reply(sc->error_code_, context_, r);

  cmd_exec::destroy(sc);
  return res;
}

HIREDIS_HAPP_API cmd_exec *connection::pop_reply(cmd_exec *c) {
  if (NULL == c) {
    if (reply_list_.empty()) {
      return NULL;
    }

    c = reply_list_.front();
    reply_list_.pop_front();
    return c;
  }

  std::list<cmd_exec *>::iterator it = std::find(reply_list_.begin(), reply_list_.end(), c);
  if (it == reply_list_.end()) {
    return NULL;
  }

  // first, deal with all expired cmd
  while (!reply_list_.empty() && reply_list_.front() != c) {
    cmd_exec *expired_c = reply_list_.front();
    reply_list_.pop_front();

    expired_c->call_reply(error_code::REDIS_HAPP_TIMEOUT, context_, NULL);
    cmd_exec::destroy(expired_c);
  }

  // now, c == reply_list_.front()
  c = reply_list_.front();
  reply_list_.pop_front();
  return c;
}

HIREDIS_HAPP_API redisAsyncContext *connection::get_context() const { return context_; }

HIREDIS_HAPP_API void connection::release(bool close_fd) {
  if (NULL != context_ && close_fd) {
    redisAsyncDisconnect(context_);
  }

  // reply list
  while (!reply_list_.empty()) {
    cmd_exec *expired_c = reply_list_.front();
    reply_list_.pop_front();

    // context_ may already be closed here
    expired_c->call_reply(error_code::REDIS_HAPP_CONNECTION, NULL, NULL);
    cmd_exec::destroy(expired_c);
  }

  context_ = NULL;
  conn_status_ = status::DISCONNECTED;
}

HIREDIS_HAPP_API const connection::key_t &connection::get_key() const { return key_; }

HIREDIS_HAPP_API holder_t connection::get_holder() const { return holder_; }

HIREDIS_HAPP_API connection::status::type connection::get_status() const { return conn_status_; }

void connection::make_sequence() {
  do {
// sequence_ will be used to make a distinction between connections when address is reused
// it can not be duplicated in a short time
#if defined(HIREDIS_HAPP_ATOMIC_STD)
    static std::atomic<uint64_t> seq_alloc(0);
    sequence_ = seq_alloc.fetch_add(1);

#elif defined(HIREDIS_HAPP_ATOMIC_MSVC)
    static LONGLONG volatile seq_alloc = 0;
    sequence_ = static_cast<uint64_t>(InterlockedAdd64(&seq_alloc, 1));

#elif defined(HIREDIS_HAPP_ATOMIC_GCC_ATOMIC)
    static volatile uint64_t seq_alloc = 0;
    sequence_ = __atomic_add_fetch(&seq_alloc, 1, __ATOMIC_SEQ_CST);
#elif defined(HIREDIS_HAPP_ATOMIC_GCC)
    static volatile uint64_t seq_alloc = 0;
    sequence_ = __sync_fetch_and_add(&seq_alloc, 1);
#else
    static volatile uint64_t seq_alloc = 0;
    sequence_ = ++seq_alloc;
#endif
  } while (0 == sequence_);
}

HIREDIS_HAPP_API std::string connection::make_name(const std::string &ip, uint16_t port) {
  std::string ret;
  ret.clear();
  ret.reserve(ip.size() + 8);

  ret = ip;
  ret += ":";

  char buf[8] = {0};
  int i = 7;  // XXXXXXX0
  while (port) {
    buf[--i] = port % 10 + '0';
    port /= 10;
  }

  ret += &buf[i];

  return ret;
}

HIREDIS_HAPP_API void connection::set_key(connection::key_t &k, const std::string &ip,
                                          uint16_t port) {
  k.name = make_name(ip, port);
  k.ip = ip;
  k.port = port;
}

HIREDIS_HAPP_API bool connection::pick_name(const std::string &name, std::string &ip,
                                            uint16_t &port) {
  size_t it = name.find_first_of(':');
  if (it == std::string::npos) {
    return false;
  }

  if (it == name.size() - 1) {
    return false;
  }

  size_t s = 0;
  while (' ' == name[s] || '\t' == name[s] || '\r' == name[s] || '\n' == name[s]) {
    ++s;
  }

  ip = name.substr(s, it - s);
  int p = 0;

  HIREDIS_HAPP_SSCANF(name.c_str() + it + 1, "%d", &p);
  port = static_cast<uint16_t>(p);

  return true;
}
}  // namespace happ
}  // namespace hiredis
