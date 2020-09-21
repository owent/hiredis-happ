
#include <algorithm>
#include <assert.h>
#include <cstdlib>
#include <cstring>
#include <iomanip>

#include "detail/happ_cmd.h"

#if (defined(__cplusplus) && __cplusplus >= 201103L) || (defined(_MSC_VER) && _MSC_VER >= 1600)
#include <type_traits>

// static_assert(std::is_pod<hiredis::happ::cmd_exec>::value, "hiredis::happ::cmd_exec should be a pod type");

#endif


namespace hiredis {
    namespace happ {
        HIREDIS_HAPP_API cmd_exec *cmd_exec::create(holder_t holder, callback_fn_t cbk, void *pridata, size_t buffer_len) {
            size_t sum_len = sizeof(cmd_exec) + buffer_len;
            // padding to sizeof(void*)
            sum_len = (sum_len + sizeof(void *) - 1) & (~(sizeof(void *) - 1));

            cmd_exec *ret = reinterpret_cast<cmd_exec *>(malloc(sum_len));

            if (NULL == ret) {
                return NULL;
            }

            memset(ret, 0, sizeof(cmd_exec));

            ret->holder   = holder;
            ret->callback = cbk;
            ret->pri_data = pridata;
            ret->ttl      = HIREDIS_HAPP_TTL;

            ret->engine.slot = -1;
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

            free_cmd_content(&c->cmd);

            free(c);
        }


        HIREDIS_HAPP_API int cmd_exec::vformat(int argc, const char **argv, const size_t *argvlen) {
            free_cmd_content(&cmd);

            cmd.raw_len = 0;
            return redisFormatSdsCommandArgv(&cmd.content.redis_sds, argc, argv, argvlen);
        }

        HIREDIS_HAPP_API int cmd_exec::format(const char *fmt, ...) {
            va_list ap;

            free_cmd_content(&cmd);
            va_start(ap, fmt);
            cmd.raw_len = redisvFormatCommand(&cmd.content.raw, fmt, ap);
            va_end(ap);

            return cmd.raw_len;
        }

        HIREDIS_HAPP_API int cmd_exec::vformat(const char *fmt, va_list ap) {
            free_cmd_content(&cmd);

            va_list ap_c;
            va_copy(ap_c, ap);

            return cmd.raw_len = redisvFormatCommand(&cmd.content.raw, fmt, ap_c);
        }

        HIREDIS_HAPP_API int cmd_exec::vformat(const sds *src) {
            free_cmd_content(&cmd);

            if (NULL == src) {
                return 0;
            }

            cmd.content.redis_sds = sdsdup(*src);
            cmd.raw_len           = 0;

            return static_cast<int>(sdslen(cmd.content.redis_sds));
        }

        HIREDIS_HAPP_API int cmd_exec::call_reply(int rcode, redisAsyncContext *context, void *reply) {
            if (NULL == callback) {
                return error_code::REDIS_HAPP_OK;
            }

            err              = rcode;
            callback_fn_t tc = callback;
            callback         = NULL;
            tc(this, context, reply, pri_data);

            return error_code::REDIS_HAPP_OK;
        }

        HIREDIS_HAPP_API void *cmd_exec::buffer() { return reinterpret_cast<void *>(this + 1); }

        HIREDIS_HAPP_API const void *cmd_exec::buffer() const { return reinterpret_cast<const void *>(this + 1); }

        HIREDIS_HAPP_API void *cmd_exec::private_data() const { return pri_data; }

        HIREDIS_HAPP_API void cmd_exec::private_data(void *pd) { pri_data = pd; }

        HIREDIS_HAPP_API const char *cmd_exec::pick_argument(const char *start, const char **str, size_t *len) {
            if (NULL == start) {
                if (0 == cmd.raw_len) {
                    // because sds is typedefed to be a char*, so we can only use it directly here.
                    start = cmd.content.redis_sds;
                } else {
                    start = cmd.content.raw;
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
            *len  = static_cast<size_t>(strtol(start + 1, NULL, 10));
            start = strchr(start, '\r');
            assert(start);

            // bulk string format: $[LENGTH]\r\n[CONTENT]\r\n
            *str = start + 2;
            return start + 2 + (*len) + 2;
        }

        HIREDIS_HAPP_API const char *cmd_exec::pick_cmd(const char **str, size_t *len) { return pick_argument(NULL, str, len); }

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
    } // namespace happ
} // namespace hiredis
