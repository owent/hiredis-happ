
#include <cstring>
#include <cstdlib>

#include "detail/happ_cmd.h"

#if (defined(__cplusplus) && __cplusplus >= 201103L) || (defined(_MSC_VER) && _MSC_VER >= 1600)
#include <type_traits>

//static_assert(std::is_pod<hiredis::happ::cmd_exec>::value, "hiredis::happ::cmd_exec should be a pod type");

#endif


namespace hiredis {
    namespace happ {
        static const size_t cmd_exec_s = sizeof(cmd_exec);

        cmd_exec* cmd_exec::create(holder_t holder, callback_fn_t cbk, void* pridata) {
            cmd_exec* ret = reinterpret_cast<cmd_exec*>(malloc(sizeof(cmd_exec)));

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
            }
        }

        void cmd_exec::destroy(cmd_exec* c) {
            if (NULL == c) {
                return;
            }

            free_cmd_content(&c->cmd);

            free(c);
        }


        int cmd_exec::format(int argc, const char** argv, const size_t* argvlen) {
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

        int cmd_exec::format(const char* fmt, va_list ap) {
            free_cmd_content(&cmd);

            va_list ap_c;
            va_copy(ap_c, ap);

            return cmd.raw_len = redisvFormatCommand(&cmd.content.raw, fmt, ap_c);
        }

        int cmd_exec::format(const sds* src) {
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

            err = err;
            callback_fn_t tc = callback;
            callback = NULL;
            tc(this, context, reply, pri_data);

            return error_code::REDIS_HAPP_OK;
        }
    }
}