//
// Created by OWenT on 2015/08/19.
//

#ifndef HIREDIS_HAPP_HIREDIS_HAPP_CMD_H
#define HIREDIS_HAPP_HIREDIS_HAPP_CMD_H

#pragma once

#include "config.h"

namespace hiredis {
    namespace happ {
        class cluster;
        class connection;

        union holder_t {
            cluster* clu;
        };

        struct cmd_content {
            size_t raw_len;
            union {
                char* raw;
                sds redis_sds;
            } content;
        };

        class cmd_exec {
        public:
            typedef void (*callback_fn_t)(cmd_exec* , struct redisAsyncContext*, void*, void*);

            int vformat(int argc, const char** argv, const size_t* argvlen);

            int format(const char* fmt, ...);

            int vformat(const char* fmt, va_list ap);

            int vformat(const sds* src);

            int call_reply(int rcode, redisAsyncContext* context, void* reply);

            inline int result() const { return err; }

        HIREDIS_HAPP_PRIVATE:
            static cmd_exec* create(holder_t holder, callback_fn_t cbk, void* pridata);
            static void destroy(cmd_exec* c);

            friend class cluster;
            friend class connection;
        HIREDIS_HAPP_PRIVATE:
            holder_t holder;            // holder
            cmd_content cmd;
            void* pri_data;             // user pri data
            size_t ttl;                 // left retry times(just like network ttl)
            callback_fn_t callback;     // user callback function

            // ========= exec data =========
            int err;                    // error code, just like redisAsyncContext::err
            union {
                int slot;               // slot index if in cluster, -1 means random
            } engine;
        };
    }
}

#endif //HIREDIS_HAPP_HIREDIS_HAPP_CMD_H
