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
            struct status {
                enum type { DISCONNECTED = 0, CONNECTING, CONNECTED };
            };

            struct key_t {
                std::string name;
                uint16_t port;
                std::string ip;
            };

            typedef std::function<const std::string& (connection*, const std::string&)> auth_fn_t;
            struct auth_info_t {
                auth_fn_t auth_fn;
                std::string password;
            };

            connection();
            ~connection();

            inline const uint64_t get_sequence() const { return sequence; }

            void init(holder_t h, const std::string &ip, uint16_t port);

            void init(holder_t h, const key_t &k);

            status::type set_connecting(redisAsyncContext *c);

            status::type set_disconnected(bool close_fd);

            status::type set_connected();

            /**
             * @brief send message wrapped with cmd_exec to redis server
             * @param c cmd data
             * @param fn callback
             * @return 0 or error code
             */
            int redis_cmd(cmd_exec *c, redisCallbackFn fn);

            /**
             * @brief send raw message redis server
             * @param fn callback
             * @param priv_data private data passed to callback
             * @param fmt format string
             * @param ... format data
             * @return 0 or error code
             */
            int redis_raw_cmd(redisCallbackFn *fn, void *priv_data, const char *fmt, ...);

            /**
             * @brief send raw message redis server
             * @param fn callback
             * @param priv_data private data passed to callback
             * @param fmt format string
             * @param ap format data
             * @return 0 or error code
             */
            int redis_raw_cmd(redisCallbackFn *fn, void *priv_data, const char *fmt, va_list ap);

            /**
             * @brief send raw message redis server
             * @param fn callback
             * @param priv_data private data passed to callback
             * @param src formated sds object
             * @return 0 or error code
             */
            int redis_raw_cmd(redisCallbackFn *fn, void *priv_data, const sds *src);

            /**
             * @brief send raw message redis server
             * @param fn callback
             * @param priv_data private data passed to callback
             * @param argc argument count
             * @param argv pointer of every argument
             * @param argvlen size of every argument
             * @return 0 or error code
             */
            int redis_raw_cmd(redisCallbackFn *fn, void *priv_data, int argc, const char **argv, const size_t *argvlen);

            /**
             * @brief call reply callback of c with reply
             * @note if c!=NULL, it will always call callback and be freed
             */
            int call_reply(cmd_exec *c, void *reply);

            /**
             * @brief pop specify cmd from pending list, do nothing if c!=NULL and is not in pending list
             * @note if c!=NULL, all cmds before c will trigger timeout
             * @return first cmd or c
             */
            cmd_exec *pop_reply(cmd_exec *c);

            redisAsyncContext *get_context() const;

            void release(bool close_fd);

            inline const key_t &get_key() const { return key; }

            inline holder_t get_holder() const { return holder; }

            inline status::type get_status() const { return conn_status; }

        private:
            connection(const connection &);
            connection &operator=(const connection &);

            void make_sequence();

        public:
            static std::string make_name(const std::string &ip, uint16_t port);
            static void set_key(connection::key_t &k, const std::string &ip, uint16_t port);
            static bool pick_name(const std::string &name, std::string &ip, uint16_t &port);

            HIREDIS_HAPP_PRIVATE : key_t key;
            uint64_t sequence;

            holder_t holder;
            redisAsyncContext *context;

            // cmds inner this connection
            std::list<cmd_exec *> reply_list;
            status::type conn_status;
        };
    }
}

#endif // HIREDIS_HAPP_HIREDIS_HAPP_CONNECTION_H
