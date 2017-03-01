//
// Created by 欧文韬 on 2015/08/18.
//

#ifndef HIREDIS_HAPP_HIREDIS_HAPP_CLUSTER_H
#define HIREDIS_HAPP_HIREDIS_HAPP_CLUSTER_H

#pragma once

#include <vector>
#include <list>
#include <ostream>

#include "config.h"

#include "happ_connection.h"

namespace hiredis {
    namespace happ {
        class cluster {
        public:
            typedef cmd_exec cmd_t;

            struct slot_t {
                int index;
                std::vector<connection::key_t> hosts;
            };
            typedef connection connection_t;
            typedef HIREDIS_HAPP_MAP(std::string, ::hiredis::happ::unique_ptr<connection_t>::type) connection_map_t;

            typedef std::function<void(cluster *, connection_t *)> onconnect_fn_t;
            typedef std::function<void(cluster *, connection_t *, const struct redisAsyncContext *, int status)> onconnected_fn_t;
            typedef std::function<void(cluster *, connection_t *, const struct redisAsyncContext *, int)> ondisconnected_fn_t;
            typedef std::function<void(const char *)> log_fn_t;

        private:
            cluster(const cluster &);
            cluster &operator=(const cluster &);

        public:
            cluster();
            ~cluster();

            int init(const std::string &ip, uint16_t port);

            const std::string& get_auth_password();
            void set_auth_password(const std::string& passwd);

            const connection::auth_fn_t& get_auth_fn();
            void set_auth_fn(connection::auth_fn_t fn);

            int start();

            int reset();

            /**
             * @breif send a request to redis server
             * @param key the key used to calculate slot id
             * @param ks  key size
             * @param cbk callback
             * @param priv_data private data passed to callback
             * @param argc argument count
             * @param argv pointer of every argument
             * @param argvlen size of every argument
             *
             * @note it can not be used to send subscribe, unsubscribe or monitor command.(because they are not request-response message)
             *       hiredis deal with these command without notify event,
             *       so you can only use connection::redis_raw_cmd to do these when connection finished or disconnected
             *
             * @see connection::redis_raw_cmd
             * @see connection::redis_cmd
             * @return command wrapper of this message, NULL if failed
             */
            cmd_t *exec(const char *key, size_t ks, cmd_t::callback_fn_t cbk, void *priv_data, int argc, const char **argv, const size_t *argvlen);

            /**
             * @breif send a request to redis server
             * @param key the key used to calculate slot id
             * @param ks  key size
             * @param cbk callback
             * @param priv_data private data passed to callback
             * @param fmt format string
             * @param ... format data
             *
             * @note it can not be used to send subscribe, unsubscribe or monitor command.(because they are not request-response message)
             *       hiredis deal with these command without notify event,
             *       so you can only use connection::redis_raw_cmd to do these when connection finished or disconnected
             *
             * @see connection::redis_raw_cmd
             * @see connection::redis_cmd
             * @return command wrapper of this message, NULL if failed
             */
            cmd_t *exec(const char *key, size_t ks, cmd_t::callback_fn_t cbk, void *priv_data, const char *fmt, ...);

            /**
             * @breif send a request to redis server
             * @param key the key used to calculate slot id
             * @param ks  key size
             * @param cbk callback
             * @param priv_data private data passed to callback
             * @param fmt format string
             * @param ap format data
             *
             * @note it can not be used to send subscribe, unsubscribe or monitor command.(because they are not request-response message)
             *       hiredis deal with these command without notify event,
             *       so you can only use connection::redis_raw_cmd to do these when connection finished or disconnected
             *
             * @see connection::redis_raw_cmd
             * @see connection::redis_cmd
             * @return command wrapper of this message, NULL if failed
             */
            cmd_t *exec(const char *key, size_t ks, cmd_t::callback_fn_t cbk, void *priv_data, const char *fmt, va_list ap);

            /**
             * @breif send a request to redis server
             * @param key the key used to calculate slot id
             * @param ks  key size
             * @param cmd cmd wrapper
             *
             * @note it can not be used to send subscribe, unsubscribe or monitor command.(because they are not request-response message)
             *       hiredis deal with these command without notify event,
             *       so you can only use connection::redis_raw_cmd to do these when connection finished or disconnected
             *
             * @see connection::redis_raw_cmd
             * @see connection::redis_cmd
             * @return command wrapper of this message, NULL if failed
             */
            cmd_t *exec(const char *key, size_t ks, cmd_t *cmd);

            /**
             * @breif send a request to specifed redis server
             * @param conn which connect to sent to
             * @param cmd cmd wrapper
             *
             * @note it can not be used to send subscribe, unsubscribe or monitor command.(because they are not request-response message)
             *       hiredis deal with these command without notify event,
             *       so you can only use connection::redis_raw_cmd to do these when connection finished or disconnected
             *
             * @see connection::redis_raw_cmd
             * @see connection::redis_cmd
             * @return command wrapper of this message, NULL if failed
             */
            cmd_t *exec(connection_t *conn, cmd_t *cmd);

            /**
             * @breif retry to send a request to redis server
             * @param cmd cmd wrapper
             * @param conn which connect to sent to(pass NULL to try to get one using the key in cmd)
             *
             * @note it can not be used to send subscribe, unsubscribe or monitor command.(because they are not request-response message)
             *       hiredis deal with these command without notify event,
             *       so you can only use connection::redis_raw_cmd to do these when connection finished or disconnected
             *
             * @see connection::redis_raw_cmd
             * @see connection::redis_cmd
             * @return command wrapper of this message, NULL if failed
             */
            cmd_t *retry(cmd_t *cmd, connection_t *conn = NULL);

            bool reload_slots();

            const connection::key_t *get_slot_master(int index);

            /**
             * @breif get slot info of a key
             * @param key the key used to calculate slot id
             * @param ks  key size
             * @return slot info of this key
             */
            const slot_t *get_slot_by_key(const char *key, size_t ks) const;

            const connection_t *get_connection(const std::string &key) const;
            connection_t *get_connection(const std::string &key);

            const connection_t *get_connection(const std::string &ip, uint16_t port) const;
            connection_t *get_connection(const std::string &ip, uint16_t port);

            connection_t *make_connection(const connection::key_t &key);
            bool release_connection(const connection::key_t &key, bool close_fd, int status);

            onconnect_fn_t set_on_connect(onconnect_fn_t cbk);
            onconnected_fn_t set_on_connected(onconnected_fn_t cbk);
            ondisconnected_fn_t set_on_disconnected(ondisconnected_fn_t cbk);

            void set_cmd_buffer_size(size_t s);

            size_t get_cmd_buffer_size() const;

            bool is_timer_active() const;

            void set_timer_interval(time_t sec, time_t usec);

            void set_timeout(time_t sec);

            void add_timer_cmd(cmd_t *cmd);

            int proc(time_t sec, time_t usec);

            void set_log_writer(log_fn_t info_fn, log_fn_t debug_fn, size_t max_size = 65536);

            HIREDIS_HAPP_PRIVATE : cmd_t *create_cmd(cmd_t::callback_fn_t cbk, void *pridata);
            void destroy_cmd(cmd_t *c);
            int call_cmd(cmd_t *c, int err, redisAsyncContext *context, void *reply);

            static void on_reply_wrapper(redisAsyncContext *c, void *r, void *privdata);
            static void on_reply_update_slot(cmd_exec *cmd, redisAsyncContext *c, void *r, void *privdata);
            static void on_reply_asking(redisAsyncContext *c, void *r, void *privdata);
            static void on_connected_wrapper(const struct redisAsyncContext *, int status);
            static void on_disconnected_wrapper(const struct redisAsyncContext *, int status);

            static void on_reply_auth(cmd_exec *cmd, redisAsyncContext *c, void *r, void *privdata);

            void remove_connection_key(const std::string &name);

        private:
            void log_debug(const char *fmt, ...);

            void log_info(const char *fmt, ...);

            HIREDIS_HAPP_PRIVATE : struct config_t {
                connection::key_t init_connection;
                log_fn_t log_fn_info;
                log_fn_t log_fn_debug;
                char *log_buffer;
                size_t log_max_size;

                time_t timer_interval_sec;
                time_t timer_interval_usec;
                time_t timer_timeout_sec;

                size_t cmd_buffer_size;
            };
            config_t conf;

            // authorization information
            connection::auth_info_t auth;

            // slot information
            struct slot_status {
                enum type { INVALID = 0, UPDATING, OK };
            };
            slot_t slots[HIREDIS_HAPP_SLOT_NUMBER];
            slot_status::type slot_flag;
            // retry cmd queue after slots reloaded
            std::list<cmd_t *> slot_pending;

            // connection pool
            connection_map_t connections;


            // timer
            struct timer_t {
                time_t last_update_sec;
                time_t last_update_usec;

                struct delay_t {
                    time_t sec;
                    time_t usec;
                    cmd_t *cmd;
                };
                std::list<delay_t> timer_pending;

                struct conn_timetout_t {
                    std::string name;
                    uint64_t sequence;
                    time_t timeout;
                };
                std::list<conn_timetout_t> timer_conns;
            };
            timer_t timer_actions;

            // callbacks 
            struct callback_set_t {
                onconnect_fn_t on_connect;
                onconnected_fn_t on_connected;
                ondisconnected_fn_t on_disconnected;
            };
            callback_set_t callbacks;
        };
    }
}

#endif // HIREDIS_HAPP_HIREDIS_HAPP_CLUSTER_H