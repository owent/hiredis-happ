#pragma once

#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include <hiredis/hiredis.h>

namespace hiredis_happ_test {

inline redisReply *make_reply(int type) {
  redisReply *ret = reinterpret_cast<redisReply *>(calloc(1, sizeof(redisReply)));
  if (nullptr != ret) {
    ret->type = type;
  }
  return ret;
}

inline redisReply *make_string_like_reply(int type, const std::string &value) {
  redisReply *ret = make_reply(type);
  if (nullptr == ret) {
    return nullptr;
  }

  ret->len = value.size();
  ret->str = reinterpret_cast<char *>(malloc(value.size() + 1));
  if (nullptr == ret->str) {
    free(ret);
    return nullptr;
  }

  if (!value.empty()) {
    memcpy(ret->str, value.data(), value.size());
  }
  ret->str[value.size()] = 0;
  return ret;
}

inline redisReply *make_status_reply(const std::string &value) {
  return make_string_like_reply(REDIS_REPLY_STATUS, value);
}

inline redisReply *make_error_reply(const std::string &value) {
  return make_string_like_reply(REDIS_REPLY_ERROR, value);
}

inline redisReply *make_string_reply(const std::string &value) {
  return make_string_like_reply(REDIS_REPLY_STRING, value);
}

inline redisReply *make_integer_reply(long long value) {
  redisReply *ret = make_reply(REDIS_REPLY_INTEGER);
  if (nullptr != ret) {
    ret->integer = value;
  }
  return ret;
}

inline redisReply *make_nil_reply() { return make_reply(REDIS_REPLY_NIL); }

inline redisReply *make_array_reply(const std::vector<redisReply *> &children) {
  redisReply *ret = make_reply(REDIS_REPLY_ARRAY);
  if (nullptr == ret) {
    return nullptr;
  }

  ret->elements = children.size();
  if (!children.empty()) {
    ret->element = reinterpret_cast<redisReply **>(calloc(children.size(), sizeof(redisReply *)));
    if (nullptr == ret->element) {
      free(ret);
      return nullptr;
    }

    for (size_t i = 0; i < children.size(); ++i) {
      ret->element[i] = children[i];
    }
  }

  return ret;
}

inline redisReply *make_array_reply(std::initializer_list<redisReply *> children) {
  return make_array_reply(std::vector<redisReply *>(children));
}

struct redis_reply_deleter {
  void operator()(redisReply *reply) const {
    if (nullptr != reply) {
      freeReplyObject(reply);
    }
  }
};

using redis_reply_ptr = std::unique_ptr<redisReply, redis_reply_deleter>;

inline redis_reply_ptr adopt_reply(redisReply *reply) { return redis_reply_ptr(reply); }

}  // namespace hiredis_happ_test
