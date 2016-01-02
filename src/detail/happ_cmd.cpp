
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <assert.h>

#include "detail/happ_cmd.h"

#if (defined(__cplusplus) && __cplusplus >= 201103L) || (defined(_MSC_VER) && _MSC_VER >= 1600)
#include <type_traits>

//static_assert(std::is_pod<hiredis::happ::cmd_exec>::value, "hiredis::happ::cmd_exec should be a pod type");

#endif


namespace hiredis {
    namespace happ {
        cmd_exec* cmd_exec::create(holder_t holder, callback_fn_t cbk, void* pridata, size_t buffer_len) {
            size_t sum_len = sizeof(cmd_exec) + buffer_len;
            // 对齐到sizeof(void*)字节，以便执行内存对齐
            sum_len = (sum_len + sizeof(void*) - 1) & (~(sizeof(void*) - 1));

            cmd_exec* ret = reinterpret_cast<cmd_exec*>(malloc(sum_len));

            if (NULL == ret) {
                return NULL;
            }

            memset(ret, 0, sizeof(cmd_exec));

            ret->holder = holder;
            ret->callback = cbk;
            ret->pri_data = pridata;
            ret->ttl = HIREDIS_HAPP_TTL;

            ret->engine.slot = -1;
            return ret;
        }

        static void free_cmd_content(cmd_content* c) {
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

        void cmd_exec::destroy(cmd_exec* c) {
            if (NULL == c) {
                return;
            }

            free_cmd_content(&c->cmd);

            free(c);
        }


        int cmd_exec::vformat(int argc, const char** argv, const size_t* argvlen) {
            free_cmd_content(&cmd);

            cmd.raw_len = 0;
            return redisFormatSdsCommandArgv(&cmd.content.redis_sds, argc, argv, argvlen);
        }

        int cmd_exec::format(const char* fmt, ...) {
            va_list ap;

            free_cmd_content(&cmd);
            va_start(ap, fmt);
            cmd.raw_len = redisvFormatCommand(&cmd.content.raw, fmt, ap);
            va_end(ap);

            return cmd.raw_len;
        }

        int cmd_exec::vformat(const char* fmt, va_list ap) {
            free_cmd_content(&cmd);

            va_list ap_c;
            va_copy(ap_c, ap);

            return cmd.raw_len = redisvFormatCommand(&cmd.content.raw, fmt, ap_c);
        }

        int cmd_exec::vformat(const sds* src) {
            free_cmd_content(&cmd);

            if (NULL == src) {
                return 0;
            }

            cmd.content.redis_sds = sdsdup(*src);
            cmd.raw_len = 0;

            return static_cast<int>(sdslen(cmd.content.redis_sds));
        }

        int cmd_exec::call_reply(int rcode, redisAsyncContext* context, void* reply) {
            if (NULL == callback) {
                return error_code::REDIS_HAPP_OK;
            }

            err = rcode;
            callback_fn_t tc = callback;
            callback = NULL;
            tc(this, context, reply, pri_data);

            return error_code::REDIS_HAPP_OK;
        }

        void* cmd_exec::buffer() {
            return reinterpret_cast<void*>(this + 1);
        }

        const void* cmd_exec::buffer() const {
            return reinterpret_cast<const void*>(this + 1);
        }
        
        void* cmd_exec::private_data() const {
            return pri_data;
        }
            
        void cmd_exec::private_data(void* pd) {
            pri_data = pd;
        }
        
        const char* cmd_exec::pick_argument(const char* start, const char** str, size_t* len) {
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
                start = strchr(start,'$');
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
        
        const char* cmd_exec::pick_cmd(const char** str, size_t* len) {
            return pick_argument(NULL, str, len);
        }
    }
}
