//
// Created by OWenT on 2015/08/20.
//

#ifndef HIREDIS_HAPP_HIREDIS_HAPP_CONNECTION_H
#define HIREDIS_HAPP_HIREDIS_HAPP_CONNECTION_H

#pragma once

#include <list>

#include "hiredis_happ_config.h"

#include "happ_cmd.h"

namespace hiredis {
    namespace happ {
        class cluster;

        class connection {
        public:
            struct HIREDIS_HAPP_API_HEAD_ONLY status {
                enum type { DISCONNECTED = 0, CONNECTING, CONNECTED };
            };

            struct HIREDIS_HAPP_API_HEAD_ONLY key_t {
                std::string name;
                uint16_t    port;
                std::string ip;
            };

            typedef std::function<const std::string &(connection *, const std::string &)> auth_fn_t;
            struct HIREDIS_HAPP_API_HEAD_ONLY                                             auth_info_t {
                auth_fn_t   auth_fn;
                std::string password;
            };

            HIREDIS_HAPP_API connection();
            HIREDIS_HAPP_API ~connection();

            HIREDIS_HAPP_API uint64_t get_sequence() const;

            HIREDIS_HAPP_API void init(holder_t h, const std::string &ip, uint16_t port);

            HIREDIS_HAPP_API void init(holder_t h, const key_t &k);

            HIREDIS_HAPP_API status::type set_connecting(redisAsyncContext *c);

            HIREDIS_HAPP_API status::type set_disconnected(bool close_fd);

            HIREDIS_HAPP_API status::type set_connected();

            /**
             * @brief send message wrapped with cmd_exec to redis server
             * @param c cmd data
             * @param fn callback
             * @return 0 or error code
             */
            HIREDIS_HAPP_API int redis_cmd(cmd_exec *c, redisCallbackFn fn);

            /**
             * @brief send raw message redis server
             * @param fn callback
             * @param priv_data private data passed to callback
             * @param fmt format string
             * @param ... format data
             * @return 0 or error code
             */
            HIREDIS_HAPP_API int redis_raw_cmd(redisCallbackFn *fn, void *priv_data, const char *fmt, ...);

            /**
             * @brief send raw message redis server
             * @param fn callback
             * @param priv_data private data passed to callback
             * @param fmt format string
             * @param ap format data
             * @return 0 or error code
             */
            HIREDIS_HAPP_API int redis_raw_cmd(redisCallbackFn *fn, void *priv_data, const char *fmt, va_list ap);

            /**
             * @brief send raw message redis server
             * @param fn callback
             * @param priv_data private data passed to callback
             * @param src formated sds object
             * @return 0 or error code
             */
            HIREDIS_HAPP_API int redis_raw_cmd(redisCallbackFn *fn, void *priv_data, const sds *src);

            /**
             * @brief send raw message redis server
             * @param fn callback
             * @param priv_data private data passed to callback
             * @param argc argument count
             * @param argv pointer of every argument
             * @param argvlen size of every argument
             * @return 0 or error code
             */
            HIREDIS_HAPP_API int redis_raw_cmd(redisCallbackFn *fn, void *priv_data, int argc, const char **argv, const size_t *argvlen);

            /**
             * @brief call reply callback of c with reply
             * @note if c!=NULL, it will always call callback and be freed
             */
            HIREDIS_HAPP_API int call_reply(cmd_exec *c, void *reply);

            /**
             * @brief pop specify cmd from pending list, do nothing if c!=NULL and is not in pending list
             * @note if c!=NULL, all cmds before c will trigger timeout
             * @return first cmd or c
             */
            HIREDIS_HAPP_API cmd_exec *pop_reply(cmd_exec *c);

            HIREDIS_HAPP_API redisAsyncContext *get_context() const;

            HIREDIS_HAPP_API void release(bool close_fd);

            HIREDIS_HAPP_API const key_t &get_key() const;

            HIREDIS_HAPP_API holder_t get_holder() const;

            HIREDIS_HAPP_API status::type get_status() const;

        private:
            connection(const connection &);
            connection &operator=(const connection &);

            void make_sequence();

        public:
            static HIREDIS_HAPP_API std::string make_name(const std::string &ip, uint16_t port);
            static HIREDIS_HAPP_API void        set_key(connection::key_t &k, const std::string &ip, uint16_t port);
            static HIREDIS_HAPP_API bool        pick_name(const std::string &name, std::string &ip, uint16_t &port);

            HIREDIS_HAPP_PRIVATE : key_t key;
            uint64_t                     sequence;

            holder_t           holder;
            redisAsyncContext *context;

            // cmds inner this connection
            std::list<cmd_exec *> reply_list;
            status::type          conn_status;
        };
    } // namespace happ
} // namespace hiredis

#endif // HIREDIS_HAPP_HIREDIS_HAPP_CONNECTION_H
