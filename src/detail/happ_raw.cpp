#ifdef _MSC_VER
#include <winsock2.h>
#else
#include <sys/time.h>
#endif

#include <algorithm>
#include <assert.h>
#include <cstdio>
#include <ctime>
#include <detail/happ_cmd.h>
#include <random>
#include <sstream>

#include "detail/happ_raw.h"

namespace hiredis {
    namespace happ {
        namespace detail {
            static char NONE_MSG[] = "none";
        }

        raw::raw() {
            conf.log_fn_debug = conf.log_fn_info = NULL;
            conf.log_buffer = NULL;
            conf.log_max_size = 0;
            conf.timer_interval_sec = HIREDIS_HAPP_TIMER_INTERVAL_SEC;
            conf.timer_interval_usec = HIREDIS_HAPP_TIMER_INTERVAL_USEC;
            conf.timer_timeout_sec = HIREDIS_HAPP_TIMER_TIMEOUT_SEC;
            conf.cmd_buffer_size = 0;

            memset(&callbacks, 0, sizeof(callbacks));

            timer_actions.last_update_sec = 0;
            timer_actions.last_update_usec = 0;

            timer_actions.timer_conn.sequence = 0;
            timer_actions.timer_conn.timeout = 0;
        }

        raw::~raw() {
            reset();

            // log buffer
            if (NULL != conf.log_buffer) {
                free(conf.log_buffer);
                conf.log_buffer = NULL;
            }
        }

        int raw::init(const std::string &ip, uint16_t port) {
            connection::set_key(conf.init_connection, ip, port);

            return error_code::REDIS_HAPP_OK;
        }

        const std::string &raw::get_auth_password() { return auth.password; }

        void raw::set_auth_password(const std::string &passwd) { auth.password = passwd; }

        const connection::auth_fn_t &raw::get_auth_fn() { return auth.auth_fn; }

        void raw::set_auth_fn(connection::auth_fn_t fn) { auth.auth_fn = fn; }

        int raw::start() {
            // just do nothing
            return error_code::REDIS_HAPP_OK;
        }

        int raw::reset() {
            // close connection if it's available
            if (conn_ && NULL != conn_->get_context()) {
                redisAsyncDisconnect(conn_->get_context());
            }

            // release timer pending list
            while (!timer_actions.timer_pending.empty()) {
                cmd_t *cmd = timer_actions.timer_pending.front().cmd;
                timer_actions.timer_pending.pop_front();

                call_cmd(cmd, error_code::REDIS_HAPP_TIMEOUT, NULL, NULL);
                destroy_cmd(cmd);
            }

            timer_actions.last_update_sec = 0;
            timer_actions.last_update_usec = 0;

            // reset timeout
            timer_actions.timer_conn.sequence = 0;
            timer_actions.timer_conn.timeout = 0;

            // If in a callback, cmds in this connection will not finished, so it can not be freed.
            // In this case, it will call disconnect callback after callback is finished and then release the connection.
            // If not in a callback, this connection is already freed at the begining "redisAsyncDisconnect(conn_->get_context());"
            // conn_.reset(); // can not reset connection

            return 0;
        }

        raw::cmd_t *raw::exec(cmd_t::callback_fn_t cbk, void *priv_data, int argc, const char **argv, const size_t *argvlen) {
            cmd_t *cmd = create_cmd(cbk, priv_data);
            if (NULL == cmd) {
                return NULL;
            }

            int len = cmd->vformat(argc, argv, argvlen);
            if (len <= 0) {
                log_info("format cmd with argc=%d failed", argc);
                destroy_cmd(cmd);
                return NULL;
            }

            return exec(cmd);
        }

        raw::cmd_t *raw::exec(cmd_t::callback_fn_t cbk, void *priv_data, const char *fmt, ...) {
            cmd_t *cmd = create_cmd(cbk, priv_data);
            if (NULL == cmd) {
                return NULL;
            }

            va_list ap;
            va_start(ap, fmt);
            int len = cmd->vformat(fmt, ap);
            va_end(ap);
            if (len <= 0) {
                log_info("format cmd with format=%s failed", fmt);
                destroy_cmd(cmd);
                return NULL;
            }

            return exec(cmd);
        }

        raw::cmd_t *raw::exec(cmd_t::callback_fn_t cbk, void *priv_data, const char *fmt, va_list ap) {
            cmd_t *cmd = create_cmd(cbk, priv_data);
            if (NULL == cmd) {
                return NULL;
            }

            int len = cmd->vformat(fmt, ap);
            if (len <= 0) {
                log_info("format cmd with format=%s failed", fmt);
                destroy_cmd(cmd);
                return NULL;
            }

            return exec(cmd);
        }

        raw::cmd_t *raw::exec(cmd_t *cmd) {
            if (NULL == cmd) {
                return NULL;
            }

            // ttl pre judge
            if (0 == cmd->ttl) {
                log_debug("cmd %p ttl expired", cmd);
                call_cmd(cmd, error_code::REDIS_HAPP_TTL, NULL, NULL);
                destroy_cmd(cmd);
                return NULL;
            }

            // move cmd into connection
            connection_t *conn_inst = get_connection();
            if (NULL == conn_inst) {
                conn_inst = make_connection();
            }

            if (NULL == conn_inst) {
                log_info("connect to %s failed", conf.init_connection.name.c_str());

                call_cmd(cmd, error_code::REDIS_HAPP_CONNECTION, NULL, NULL);
                destroy_cmd(cmd);

                return NULL;
            }

            return exec(conn_inst, cmd);
        }

        raw::cmd_t *raw::exec(connection_t *conn, cmd_t *cmd) {
            if (NULL == cmd) {
                return NULL;
            }

            // ttl judge
            if (0 == cmd->ttl) {
                log_debug("cmd %p at connection %s ttl expired", cmd, conf.init_connection.name.c_str());
                call_cmd(cmd, error_code::REDIS_HAPP_TTL, NULL, NULL);
                destroy_cmd(cmd);
                return NULL;
            }

            // ttl
            --cmd->ttl;

            if (NULL == conn) {
                call_cmd(cmd, error_code::REDIS_HAPP_CONNECTION, NULL, NULL);
                destroy_cmd(cmd);
                return NULL;
            }

            // main loop
            int res = conn->redis_cmd(cmd, on_reply_wrapper);

            if (REDIS_OK != res) {
                // some version of hiredis will miss onDisconnect, patch it
                // other situation should trigger error
                if (conn->get_context()->c.flags & (REDIS_DISCONNECTING | REDIS_FREEING)) {
                    // Patch: hiredis will miss onDisconnect in some older version
                    // If not in hiredis's callback, REDIS_DISCONNECTING or REDIS_FREEING means resource is freed
                    // If in hiredis's callback, disconnect will be called after callback finished, so do nothing here
                    if (conn_.get() == conn && !(conn->get_context()->c.flags & REDIS_IN_CALLBACK)) {
                        release_connection(false, error_code::REDIS_HAPP_CONNECTION);
                    }

                    // conn = NULL;
                    // retry if the connection lost
                    return retry(cmd, NULL);
                } else {
                    call_cmd(cmd, error_code::REDIS_HAPP_HIREDIS, conn->get_context(), NULL);
                    destroy_cmd(cmd);
                }
                return NULL;
            }

            log_debug("exec cmd %p, connection %s", cmd, conn->get_key().name.c_str());
            return cmd;
        }

        raw::cmd_t *raw::retry(cmd_t *cmd, connection_t *conn) {
            if (NULL == cmd) {
                return NULL;
            }

            // First, retry immediately for several times.
            if (false == is_timer_active() || cmd->ttl > HIREDIS_HAPP_TTL / 2) {
                if (NULL == conn) {
                    return exec(cmd);
                } else {
                    return exec(conn, cmd);
                }
            }

            // If it's still failed, maybe it will take some more time to recover the connection,
            // so wait for a while and retry again.
            add_timer_cmd(cmd);
            return cmd;
        }

        const raw::connection_t *raw::get_connection() const { return conn_.get(); }

        raw::connection_t *raw::get_connection() { return conn_.get(); }

        raw::connection_t *raw::make_connection() {
            holder_t h;
            if (conn_) {
                log_debug("connection %s already exists", conf.init_connection.name.c_str());
                return NULL;
            }

            redisAsyncContext *c = redisAsyncConnect(conf.init_connection.ip.c_str(), static_cast<int>(conf.init_connection.port));
            if (NULL == c || c->err) {
                log_info("redis connect to %s failed, msg: %s", conf.init_connection.name.c_str(), NULL == c ? detail::NONE_MSG : c->errstr);
                return NULL;
            }

            h.r = this;
            redisAsyncSetConnectCallback(c, on_connected_wrapper);
            redisAsyncSetDisconnectCallback(c, on_disconnected_wrapper);
            redisEnableKeepAlive(&c->c);
            if (conf.timer_timeout_sec > 0) {
                struct timeval tv;
                tv.tv_sec = conf.timer_timeout_sec;
                tv.tv_usec = 0;
                redisSetTimeout(&c->c, tv);
            }

            connection_ptr_t ret_ptr(new connection_t());
            connection_t &ret = *ret_ptr;
            ::hiredis::happ::unique_ptr<connection_t>::swap(conn_, ret_ptr);
            ret.init(h, conf.init_connection);
            ret.set_connecting(c);

            c->data = &ret;

            // timeout timer
            if (conf.timer_timeout_sec > 0 && is_timer_active()) {
                timer_actions.timer_conn.sequence = ret.get_sequence();
                timer_actions.timer_conn.timeout = timer_actions.last_update_sec + conf.timer_timeout_sec;
            }

            // auth command
            if (auth.auth_fn || !auth.password.empty()) {
                // AUTH cmd
                cmd_t *cmd = create_cmd(on_reply_auth, NULL);
                if (NULL != cmd) {
                    int len = 0;
                    if (auth.auth_fn) {
                        const std::string &passwd = auth.auth_fn(&ret, auth.password);
                        len = cmd->format("AUTH %b", passwd.c_str(), passwd.size());
                    } else if (!auth.password.empty()) {
                        len = cmd->format("AUTH %b", auth.password.c_str(), auth.password.size());
                    }

                    if (len <= 0) {
                        log_info("format cmd AUTH failed");
                        destroy_cmd(cmd);
                        return NULL;
                    }

                    exec(&ret, cmd);
                }
            }

            // event callback
            if (callbacks.on_connect) {
                callbacks.on_connect(this, &ret);
            }

            log_debug("redis make connection to %s ", conf.init_connection.name.c_str());
            return &ret;
        }

        bool raw::release_connection(bool close_fd, int status) {
            if (!conn_) {
                log_debug("connection %s not found", conf.init_connection.name.c_str());
                return false;
            }

            connection_t::status::type from_status = conn_->set_disconnected(close_fd);
            switch (from_status) {
            // recursion, exit
            case connection_t::status::DISCONNECTED:
                return true;

            // connecting, call on_connected event
            case connection_t::status::CONNECTING:
                if (callbacks.on_connected) {
                    callbacks.on_connected(this, conn_.get(), conn_->get_context(),
                                           error_code::REDIS_HAPP_OK == status ? error_code::REDIS_HAPP_CONNECTION : status);
                }
                break;

            // connecting, call on_disconnected event
            case connection_t::status::CONNECTED:
                if (callbacks.on_disconnected) {
                    callbacks.on_disconnected(this, conn_.get(), conn_->get_context(), status);
                }
                break;

            default:
                log_info("unknown connection status %d", static_cast<int>(from_status));
                break;
            }

            log_debug("release connection %s", conf.init_connection.name.c_str());

            // can not use conf.init_connection any more
            conn_.reset();
            timer_actions.timer_conn.sequence = 0;
            timer_actions.timer_conn.timeout = 0;

            return true;
        }

        raw::onconnect_fn_t raw::set_on_connect(onconnect_fn_t cbk) {
            using std::swap;
            swap(cbk, callbacks.on_connect);
            return cbk;
        }

        raw::onconnected_fn_t raw::set_on_connected(onconnected_fn_t cbk) {
            using std::swap;
            swap(cbk, callbacks.on_connected);
            return cbk;
        }

        raw::ondisconnected_fn_t raw::set_on_disconnected(ondisconnected_fn_t cbk) {
            using std::swap;
            swap(cbk, callbacks.on_disconnected);
            return cbk;
        }

        void raw::set_cmd_buffer_size(size_t s) { conf.cmd_buffer_size = s; }

        size_t raw::get_cmd_buffer_size() const { return conf.cmd_buffer_size; }

        bool raw::is_timer_active() const {
            return (timer_actions.last_update_sec != 0 || timer_actions.last_update_usec != 0) && (conf.timer_interval_sec > 0 || conf.timer_interval_usec > 0);
        }

        void raw::set_timer_interval(time_t sec, time_t usec) {
            conf.timer_interval_sec = sec;
            conf.timer_interval_usec = usec;
        }

        void raw::set_timeout(time_t sec) { conf.timer_timeout_sec = sec; }

        void raw::add_timer_cmd(cmd_t *cmd) {
            if (NULL == cmd) {
                return;
            }

            if (is_timer_active()) {
                timer_actions.timer_pending.push_back(timer_t::delay_t());
                timer_t::delay_t &d = timer_actions.timer_pending.back();
                d.sec = timer_actions.last_update_sec + conf.timer_interval_sec;
                d.usec = timer_actions.last_update_usec + conf.timer_interval_usec;
                d.cmd = cmd;
            } else {
                exec(cmd);
            }
        }

        int raw::proc(time_t sec, time_t usec) {
            int ret = 0;

            timer_actions.last_update_sec = sec;
            timer_actions.last_update_usec = usec;

            while (!timer_actions.timer_pending.empty()) {
                timer_t::delay_t &rd = timer_actions.timer_pending.front();
                if (rd.sec > sec || (rd.sec == sec && rd.usec > usec)) {
                    break;
                }


                timer_t::delay_t d = rd;
                timer_actions.timer_pending.pop_front();

                exec(d.cmd);

                ++ret;
            }

            // connection timeout
            // can not be call in any callback
            while (0 != timer_actions.timer_conn.timeout && sec >= timer_actions.timer_conn.timeout) {
                // sequence expired skip
                if (conn_ && conn_->get_sequence() == timer_actions.timer_conn.sequence) {
                    assert(!(conn_->get_context()->c.flags & REDIS_IN_CALLBACK));
                    release_connection(true, error_code::REDIS_HAPP_TIMEOUT);
                }

                timer_actions.timer_conn.timeout = 0;
                timer_actions.timer_conn.sequence = 0;
            }

            return ret;
        }

        raw::cmd_t *raw::create_cmd(cmd_t::callback_fn_t cbk, void *pridata) {
            holder_t h;
            h.r = this;
            cmd_t *ret = cmd_t::create(h, cbk, pridata, conf.cmd_buffer_size);
            return ret;
        }

        void raw::destroy_cmd(cmd_t *c) {
            if (NULL == c) {
                log_debug("can not destroy null cmd");
                return;
            }

            // lost connection
            if (NULL != c->callback) {
                call_cmd(c, error_code::REDIS_HAPP_UNKNOWD, NULL, NULL);
            }

            cmd_t::destroy(c);
        }

        int raw::call_cmd(cmd_t *c, int err, redisAsyncContext *context, void *reply) {
            if (NULL == c) {
                log_debug("can not call cmd without cmd object");
                return error_code::REDIS_HAPP_UNKNOWD;
            }

            return c->call_reply(err, context, reply);
        }

        void raw::set_log_writer(log_fn_t info_fn, log_fn_t debug_fn, size_t max_size) {
            using std::swap;
            conf.log_fn_info = info_fn;
            conf.log_fn_debug = debug_fn;
            conf.log_max_size = max_size;

            if (NULL != conf.log_buffer) {
                free(conf.log_buffer);
                conf.log_buffer = NULL;
            }
        }

        void raw::on_reply_wrapper(redisAsyncContext *c, void *r, void *privdata) {
            connection_t *conn = reinterpret_cast<connection_t *>(c->data);
            cmd_t *cmd = reinterpret_cast<cmd_t *>(privdata);
            raw *self = cmd->holder.r;

            // retry if disconnecting will lead to a infinite loop
            if (c->c.flags & REDIS_DISCONNECTING) {
                self->log_debug("redis cmd %p reply when disconnecting context err %d,msg %s", cmd, c->err, NULL == c->errstr ? detail::NONE_MSG : c->errstr);
                cmd->err = error_code::REDIS_HAPP_CONNECTION;
                conn->call_reply(cmd, r);
                return;
            }

            if (REDIS_ERR_IO == c->err && REDIS_ERR_EOF == c->err) {
                self->log_debug("redis cmd %p reply context err %d and will retry, %s", cmd, c->err, c->errstr);
                // retry if it's a network error
                conn->pop_reply(cmd);
                self->retry(cmd);
                return;
            }

            if (REDIS_OK != c->err || NULL == r) {
                self->log_debug("redis cmd %p reply context err %d and abort, %s", cmd, c->err, NULL == c->errstr ? detail::NONE_MSG : c->errstr);
                // other errors will be passed to caller
                conn->call_reply(cmd, r);
                return;
            }

            redisReply *reply = reinterpret_cast<redisReply *>(r);

            // error handler
            if (REDIS_REPLY_ERROR == reply->type) {
                self->log_debug("redis cmd %p reply error and abort, msg: %s", cmd, NULL == reply->str ? detail::NONE_MSG : reply->str);
                // other errors will be passed to caller
                conn->call_reply(cmd, r);
                return;
            }

            // success and call callback
            self->log_debug("redis cmd %p got reply success.(ttl=%3d)", cmd, NULL == cmd ? -1 : static_cast<int>(cmd->ttl));
            conn->call_reply(cmd, r);
        }

        void raw::on_connected_wrapper(const struct redisAsyncContext *c, int status) {
            connection_t *conn = reinterpret_cast<connection_t *>(c->data);
            raw *self = conn->get_holder().r;

            // hiredis bug, sometimes 0 == status but c is already closed
            if (REDIS_OK == status && hiredis::happ::connection::status::DISCONNECTED == conn->get_status()) {
                status = REDIS_ERR_OTHER;
            }

            // event callback
            if (self->callbacks.on_connected) {
                self->callbacks.on_connected(self, conn, c, status);
            }

            // failed, release resource
            if (REDIS_OK != status) {
                self->log_debug("connect to %s failed, status: %d, msg: %s", conn->get_key().name.c_str(), status, c->errstr);
                self->release_connection(false, status);

            } else {
                conn->set_connected();

                self->log_debug("connect to %s success", conn->get_key().name.c_str());
            }
        }

        void raw::on_disconnected_wrapper(const struct redisAsyncContext *c, int status) {
            connection_t *conn = reinterpret_cast<connection_t *>(c->data);
            raw *self = conn->get_holder().r;

            // release rreource
            self->release_connection(false, status);
        }

        void raw::on_reply_auth(cmd_exec *cmd, redisAsyncContext *rctx, void *r, void *privdata) {
            redisReply *reply = reinterpret_cast<redisReply *>(r);
            raw *self = cmd->holder.r;
            assert(rctx);

            // error and log
            if (NULL == reply || 0 != HIREDIS_HAPP_STRNCASE_CMP("OK", reply->str, 2)) {
                const char *error_text = "";
                if (NULL != reply && NULL != reply->str) {
                    error_text = reply->str;
                }
                if (REDIS_CONN_TCP == rctx->c.connection_type) {
                    self->log_info("tcp:%s:%d AUTH failed. %s",
                                   rctx->c.tcp.host ? rctx->c.tcp.host : (rctx->c.tcp.source_addr ? rctx->c.tcp.source_addr : "UNKNOWN"), rctx->c.tcp.port,
                                   error_text);
                } else if (REDIS_CONN_UNIX == rctx->c.connection_type) {
                    self->log_info("unix:%s AUTH failed. %s", rctx->c.unix_sock.path ? rctx->c.unix_sock.path : "NULL", error_text);
                } else {
                    self->log_info("AUTH failed. %s", error_text);
                }
            } else {
                if (REDIS_CONN_TCP == rctx->c.connection_type) {
                    self->log_info("tcp:%s:%d AUTH success.",
                                   rctx->c.tcp.host ? rctx->c.tcp.host : (rctx->c.tcp.source_addr ? rctx->c.tcp.source_addr : "UNKNOWN"), rctx->c.tcp.port);
                } else if (REDIS_CONN_UNIX == rctx->c.connection_type) {
                    self->log_info("unix:%s AUTH success.", rctx->c.unix_sock.path ? rctx->c.unix_sock.path : "NULL");
                } else {
                    self->log_info("AUTH success.");
                }
            }
        }

        void raw::log_debug(const char *fmt, ...) {
            if (NULL == conf.log_fn_debug || 0 == conf.log_max_size) {
                return;
            }

            if (NULL == conf.log_buffer) {
                conf.log_buffer = reinterpret_cast<char *>(malloc(conf.log_max_size));
            }

            va_list ap;
            va_start(ap, fmt);
            int len = vsnprintf(conf.log_buffer, conf.log_max_size, fmt, ap);
            va_end(ap);

            conf.log_buffer[conf.log_max_size - 1] = 0;
            if (len > 0) {
                conf.log_buffer[len] = 0;
            }

            conf.log_fn_debug(conf.log_buffer);
        }

        void raw::log_info(const char *fmt, ...) {
            if (NULL == conf.log_fn_info || 0 == conf.log_max_size) {
                return;
            }

            if (NULL == conf.log_buffer) {
                conf.log_buffer = reinterpret_cast<char *>(malloc(conf.log_max_size));
            }

            va_list ap;
            va_start(ap, fmt);
            int len = vsnprintf(conf.log_buffer, conf.log_max_size, fmt, ap);
            va_end(ap);

            conf.log_buffer[conf.log_max_size - 1] = 0;
            if (len > 0) {
                conf.log_buffer[len] = 0;
            }

            conf.log_fn_info(conf.log_buffer);
        }
    }
}
