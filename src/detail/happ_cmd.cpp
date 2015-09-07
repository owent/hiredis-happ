
#include <cstring>
#include <cstdlib>

#include "detail/happ_cmd.h"

#if (defined(__cplusplus) && __cplusplus >= 201103L) || (defined(_MSC_VER) && _MSC_VER >= 1600)
#include <type_traits>

//static_assert(std::is_pod<hiredis::happ::cmd_exec>::value, "hiredis::happ::cmd_exec should be a pod type");

#endif


namespace hiredis {
    namespace happ {
        namespace detail {
            template<size_t s>
            struct power2;

            template<>
            struct power2<0> {
                static const size_t value = 0;
            };

            template<>
            struct power2<1> {
                static const size_t value = 0;
            };

            template<size_t s>
            struct power2 {
                static const size_t value = 1 + power2<(s >> 1)>::value;
            };
        }

        static const size_t cmd_exec_s = sizeof(cmd_exec);

        cmd_exec* cmd_exec::create(holder_t holder, callback_fn_t cbk, void* pridata, size_t buffer_len) {
            size_t sum_len = sizeof(cmd_exec) + buffer_len;
            // 对齐到sizeof(void*)字节，以便执行内存对齐
            sum_len = (((sum_len - 1) >> detail::power2<sizeof(void*)>::value) + 1) << detail::power2<sizeof(void*)>::value;

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
        
        void* cmd_exec::pri_data() const {
            return pri_data;
        }
            
        void cmd_exec::pri_data(void* pd) const {
            pri_data = pd;
        }
    }
}
