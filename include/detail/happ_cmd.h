//
// Created by OWenT on 2015/08/19.
//

#ifndef HIREDIS_HAPP_HIREDIS_HAPP_CMD_H
#define HIREDIS_HAPP_HIREDIS_HAPP_CMD_H

#pragma once

#include <ostream>
#include "config.h"

namespace hiredis {
    namespace happ {
        class cluster;
        class raw;
        class connection;

        union holder_t {
            cluster* clu;
            raw* r;
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

            void* buffer();

            const void* buffer() const;
            
            void* private_data() const;
            
            void private_data(void* pd);

            const char* pick_argument(const char* start, const char** str, size_t* len);
            
            const char* pick_cmd(const char** str, size_t* len);
            
            static void dump(std::ostream& out, redisReply* reply, int ident = 0);
        HIREDIS_HAPP_PRIVATE:
            /**
             * @brief create cmd object
             * @param holder owner of this
             * @param cbk callback when cmd finished
             * @param pridata private data
             * @param buff_len alloacte some memory inner cmd(this can be used to store some more data for later usage)
             * @return address of cmd object if success
             */
            static cmd_exec* create(holder_t holder, callback_fn_t cbk, void* pridata, size_t buffer_len);
            static void destroy(cmd_exec* c);

            friend class cluster;
            friend class raw;
            friend class connection;
        HIREDIS_HAPP_PRIVATE:
            holder_t holder;            // holder
            cmd_content cmd;
            size_t ttl;                 // left retry times(just like network ttl)
            callback_fn_t callback;     // user callback function

            // ========= exec data =========
            int err;                    // error code, just like redisAsyncContext::err
            union {
                int slot;               // slot index if in cluster, -1 means random
            } engine;

            void* pri_data;             // user pri data
        };
    }
}

#endif //HIREDIS_HAPP_HIREDIS_HAPP_CMD_H
