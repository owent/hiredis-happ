//
// Created by OWenT on 2015/08/20.
//

#ifndef HIREDIS_HAPP_HIREDIS_HAPP_CONNECTION_H
#define HIREDIS_HAPP_HIREDIS_HAPP_CONNECTION_H

#pragma once

#include <list>

#include "config.h"

#include "happ_cmd.h"

namespace hiredis {
    namespace happ {
        class cluster;

        class connection {
        public:
            struct status {
                enum type {
                    DISCONNECTED = 0,
                    CONNECTING,
                    CONNECTED
                };
            };

            struct key_t {
                std::string name;
                uint16_t port;
                std::string ip;
            };

            connection();
            ~connection();

            void init(holder_t h, const std::string& ip, uint16_t port);

            void init(holder_t h, const key_t& k);

            bool set_connecting(redisAsyncContext* c);

            void set_disconnected(std::list<cmd_exec*>* pending, bool close_fd);

            bool set_connected(std::list<cmd_exec*>& pending);

            int redis_cmd(cmd_exec* c, redisCallbackFn fn);

            int call_reply(cmd_exec* c, void* reply);
            cmd_exec* pop_reply(cmd_exec* c);

            redisAsyncContext* get_context() const;

            void release(std::list<cmd_exec*>* dump_pending, bool close_fd);

            inline const key_t& get_key() const { return key; }

            inline holder_t get_holder() const { return holder; }
        private:
            connection(const connection&);
            connection& operator=(const connection&);

        public:
            static std::string make_name(const std::string& ip, uint16_t port);
            static void set_key(connection::key_t& k, const std::string& ip, uint16_t port);
            static bool pick_name(const std::string& name, std::string& ip, uint16_t& port);

        private:
            key_t key;

            holder_t holder;
            redisAsyncContext* context;
            // 连接完成重新执行列表
            std::list<cmd_exec*> pending_list;
            // 回包响应列表
            std::list<cmd_exec*> reply_list;
            status::type conn_status;
        };
    }
}

#endif //HIREDIS_HAPP_HIREDIS_HAPP_CONNECTION_H
