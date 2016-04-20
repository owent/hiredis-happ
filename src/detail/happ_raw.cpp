#ifdef _MSC_VER
#include <winsock2.h>
#else
#include <sys/time.h>
#endif

#include <algorithm>
#include <cstdio>
#include <random>
#include <sstream>
#include <ctime>
#include <detail/happ_cmd.h>
#include <iomanip>

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
        }

        int raw::init(const std::string& ip, uint16_t port) {
            connection::set_key(conf.init_connection, ip, port);
            
            return error_code::REDIS_HAPP_OK;
        }

        int raw::start() {
            // just do nothing
            return error_code::REDIS_HAPP_OK;
        }

        int raw::reset() {
            // close connection if it's available
            if (conn_ && NULL != conn_->get_context()) {
                redisAsyncDisconnect(conn_->get_context());
            }

            // 释放timer pending list
            while(!timer_actions.timer_pending.empty()) {
                cmd_t* cmd = timer_actions.timer_pending.front().cmd;
                timer_actions.timer_pending.pop_front();

                call_cmd(cmd, error_code::REDIS_HAPP_TIMEOUT, NULL, NULL);
                destroy_cmd(cmd);
            }

            timer_actions.last_update_sec = 0;
            timer_actions.last_update_usec = 0;

            // reset timeout
            timer_actions.timer_conn.sequence = 0;
            timer_actions.timer_conn.timeout = 0;

            // reset connection, should be empty here
            conn_.reset();

            // log buffer
            if (NULL != conf.log_buffer) {
                free(conf.log_buffer);
                conf.log_buffer = NULL;
            }

            return 0;
        }

        raw::cmd_t* raw::exec(cmd_t::callback_fn_t cbk, void* priv_data, int argc, const char** argv, const size_t* argvlen) {
            cmd_t* cmd = create_cmd(cbk, priv_data);
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

        raw::cmd_t* raw::exec(cmd_t::callback_fn_t cbk, void* priv_data, const char* fmt, ...) {
            cmd_t* cmd = create_cmd(cbk, priv_data);
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

        raw::cmd_t* raw::exec(cmd_t::callback_fn_t cbk, void* priv_data, const char* fmt, va_list ap) {
            cmd_t* cmd = create_cmd(cbk, priv_data);
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

        raw::cmd_t* raw::exec(cmd_t* cmd) {
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

            // 转发到建立连接
            connection_t* conn_inst = get_connection();
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

        raw::cmd_t* raw::exec(connection_t* conn, cmd_t* cmd) {
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

            // 主循环逻辑回包处理
            int res = conn->redis_cmd(cmd, on_reply_wrapper);

            if (REDIS_OK != res) {
                // hiredis的代码，仅在网络关闭和命令错误会返回出错
                // 其他情况都应该直接出错回调
                if (conn->get_context()->c.flags & (REDIS_DISCONNECTING | REDIS_FREEING)) {
                    // fix hiredis 的BUG，可能会漏调用onDisconnect
                    // 只要不在hiredis的回调函数内，一旦标记了REDIS_DISCONNECTING或REDIS_FREEING则是已经释放完毕了
                    // 如果是回调函数，则出回调以后会调用disconnect，从而触发disconnect回调，这里就不需要释放了
                    if (conn_.get() == conn && !(conn->get_context()->c.flags & REDIS_IN_CALLBACK)) {
                        release_connection(false, error_code::REDIS_HAPP_CONNECTION);
                    }

                    // conn = NULL;
                    // 连接丢失需要重连，先随机重新找可用连接
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

        raw::cmd_t* raw::retry(cmd_t* cmd, connection_t* conn) {
            // 重试次数较少则直接重试
            if(NULL == cmd) {
                return NULL;
            }

            if (false == is_timer_active() || cmd->ttl > HIREDIS_HAPP_TTL / 2) {
                if (NULL == conn) {
                    return exec(cmd);
                } else {
                    return exec(conn, cmd);
                }
            }

            // 重试次数较多则等一会重试
            // 延迟重试的命令不记录连接信息，因为可能到时候连接已经丢失
            add_timer_cmd(cmd);
            return cmd;
        }

        const raw::connection_t* raw::get_connection() const {
            return conn_.get();
        }

        raw::connection_t* raw::get_connection() {
            return conn_.get();
        }

        raw::connection_t* raw::make_connection() {
            holder_t h;
            if (conn_) {
                log_debug("connection %s already exists", conf.init_connection.name.c_str());
                return NULL;
            }

            redisAsyncContext* c = redisAsyncConnect(conf.init_connection.ip.c_str(), static_cast<int>(conf.init_connection.port));
            if (NULL == c || c->err) {
                log_info("redis connect to %s failed, msg: %s", conf.init_connection.name.c_str(), NULL == c? detail::NONE_MSG: c->errstr);
                return NULL;
            }

            h.r = this;
            redisAsyncSetConnectCallback(c, on_connected_wrapper);
            redisAsyncSetDisconnectCallback(c, on_disconnected_wrapper);
            redisEnableKeepAlive(&c->c);
            if (conf.timer_timeout_sec > 0) {
                struct timeval tv;
                tv.tv_sec =conf.timer_timeout_sec;
                tv.tv_usec = 0;
                redisSetTimeout(&c->c, tv);
            }

            connection_ptr_t ret_ptr(new connection_t());
            connection_t& ret = *ret_ptr;
            ::hiredis::happ::unique_ptr<connection_t>::swap(conn_, ret_ptr);
            ret.init(h, conf.init_connection);
            ret.set_connecting(c);

            c->data = &ret;

            // timeout timer
            if(conf.timer_timeout_sec > 0 && is_timer_active()) {
                timer_actions.timer_conn.sequence = ret.get_sequence();
                timer_actions.timer_conn.timeout = timer_actions.last_update_sec + conf.timer_timeout_sec;
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
            switch(from_status) {
                // 递归调用，直接退出
                case connection_t::status::DISCONNECTED:
                    return true;

                // 正在连接，响应connected事件
                case connection_t::status::CONNECTING:
                    if(callbacks.on_connected) {
                        callbacks.on_connected(this, conn_.get(), conn_->get_context(),
                            error_code::REDIS_HAPP_OK == status? error_code::REDIS_HAPP_CONNECTION: status
                        );
                    }
                    break;

                // 已连接，响应disconnected事件
                case connection_t::status::CONNECTED:
                    if(callbacks.on_disconnected) {
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

        void raw::set_cmd_buffer_size(size_t s) {
            conf.cmd_buffer_size = s;
        }

        size_t raw::get_cmd_buffer_size() const {
            return conf.cmd_buffer_size;
        }

        bool raw::is_timer_active() const {
            return (timer_actions.last_update_sec != 0 || timer_actions.last_update_usec != 0) &&
                (conf.timer_interval_sec > 0 || conf.timer_interval_usec > 0);
        }

        void raw::set_timer_interval(time_t sec, time_t usec) {
            conf.timer_interval_sec = sec;
            conf.timer_interval_usec = usec;
        }

        void raw::set_timeout(time_t sec) {
            conf.timer_timeout_sec = sec;
        }

        void raw::add_timer_cmd(cmd_t* cmd) {
            if (NULL == cmd) {
                return;
            }

            if (is_timer_active()) {
                timer_actions.timer_pending.push_back(timer_t::delay_t());
                timer_t::delay_t& d = timer_actions.timer_pending.back();
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
                timer_t::delay_t& rd = timer_actions.timer_pending.front();
                if (rd.sec > sec || (rd.sec == sec && rd.usec > usec)) {
                    break;
                }


                timer_t::delay_t d = rd;
                timer_actions.timer_pending.pop_front();

                exec(d.cmd);

                ++ret;
            }

            // connection timeout
            while(0 != timer_actions.timer_conn.timeout && sec >= timer_actions.timer_conn.timeout) {
                if (conn_ && conn_->get_sequence() == timer_actions.timer_conn.sequence) {
                    release_connection(true, error_code::REDIS_HAPP_TIMEOUT);
                }
            }

            return ret;
        }

        raw::cmd_t* raw::create_cmd(cmd_t::callback_fn_t cbk, void* pridata) {
            holder_t h;
            h.r = this;
            cmd_t* ret = cmd_t::create(h, cbk, pridata, conf.cmd_buffer_size);
            return ret;
        }

        void raw::destroy_cmd(cmd_t* c) {
            if (NULL == c) {
                log_debug("can not destroy null cmd");
                return;
            }

            // 丢失连接
            if (NULL != c->callback) {
                call_cmd(c, error_code::REDIS_HAPP_UNKNOWD, NULL, NULL);
            }

            cmd_t::destroy(c);
        }

        int raw::call_cmd(cmd_t* c, int err, redisAsyncContext* context, void* reply) {
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

        void raw::on_reply_wrapper(redisAsyncContext* c, void* r, void* privdata) {
            connection_t* conn = reinterpret_cast<connection_t*>(c->data);
            cmd_t* cmd = reinterpret_cast<cmd_t*>(privdata);
            raw* self = cmd->holder.r;

            // 正在释放的连接重试也只会死循环，所以直接失败退出
            if (c->c.flags & REDIS_DISCONNECTING) {
                self->log_debug("redis cmd %p reply when disconnecting context err %d,msg %s", cmd, c->err, NULL == c->errstr? detail::NONE_MSG: c->errstr);
                cmd->err = error_code::REDIS_HAPP_CONNECTION;
                conn->call_reply(cmd, r);
                return;
            }

            if (REDIS_ERR_IO == c->err && REDIS_ERR_EOF == c->err) {
                self->log_debug("redis cmd %p reply context err %d and will retry, %s", cmd, c->err, c->errstr);
                // 网络错误则重试
                conn->pop_reply(cmd);
                self->retry(cmd);
                return;
            }

            if (REDIS_OK != c->err || NULL == r) {
                self->log_debug("redis cmd %p reply context err %d and abort, %s", cmd, c->err, NULL == c->errstr? detail::NONE_MSG: c->errstr);
                // 其他错误则向上传递
                conn->call_reply(cmd, r);
                return;
            }

            redisReply* reply = reinterpret_cast<redisReply*>(r);

            // 错误处理
            if (REDIS_REPLY_ERROR == reply->type) {
                self->log_debug("redis cmd %p reply error and abort, msg: %s", cmd, NULL == reply->str? detail::NONE_MSG: reply->str);
                // 其他错误则向上传递
                conn->call_reply(cmd, r);
                return;
            }

            // 正常回调
            self->log_debug("redis cmd %p got reply success.(ttl=%3d)", cmd, NULL == cmd ? -1: static_cast<int>(cmd->ttl));
            conn->call_reply(cmd, r);
        }

        void raw::on_connected_wrapper(const struct redisAsyncContext* c, int status) {
            connection_t* conn = reinterpret_cast<connection_t*>(c->data);
            raw* self = conn->get_holder().r;
            
            // event callback
            if (self->callbacks.on_connected) {
                self->callbacks.on_connected(self, conn, c, status);
            }

            // 失败则释放资源
            if (REDIS_OK != status) {
                self->log_debug("connect to %s failed, status: %d, msg: %s", conn->get_key().name.c_str(), status, c->errstr);
                self->release_connection(false, status);

            } else {
                conn->set_connected();

                self->log_debug("connect to %s success", conn->get_key().name.c_str());
            }
        }

        void raw::on_disconnected_wrapper(const struct redisAsyncContext* c, int status) {
            connection_t* conn = reinterpret_cast<connection_t*>(c->data);
            raw* self = conn->get_holder().r;

            // 释放资源
            self->release_connection(false, status);
        }

        void raw::dump(std::ostream& out, redisReply* reply, int ident) {
            if (NULL == reply) {
                return;
            }

            // dump reply
            switch(reply->type) {
            case REDIS_REPLY_NIL: {
                out << "[NIL]"<< std::endl;
                break;
            }
            case REDIS_REPLY_STATUS: {
                out << "[STATUS]: "<< reply->str << std::endl;
                break;
            }
            case REDIS_REPLY_ERROR: {
                out << "[ERROR]: " << reply->str << std::endl;
                break;
            }
            case REDIS_REPLY_INTEGER: {
                out << reply->integer << std::endl;
                break;
            }
            case REDIS_REPLY_STRING: {
                out << reply->str << std::endl;
                break;
            }
            case REDIS_REPLY_ARRAY: {
                std::string ident_str;
                ident_str.assign(static_cast<size_t>(ident), ' ');

                out << "[ARRAY]: " << std::endl;
                for (size_t i = 0; i < reply->elements; ++ i) {
                    out << ident_str << std::setw(7) << (i + 1) << ": ";
                    dump(out, reply->element[i], ident + 2);
                }

                break;
            }
            default: {
                log_debug("[UNKNOWN]");
                break;
            }
            }

        }

        void raw::log_debug(const char* fmt, ...) {
            if (NULL == conf.log_fn_debug || 0 == conf.log_max_size ) {
                return;
            }

            if (NULL == conf.log_buffer) {
                conf.log_buffer = reinterpret_cast<char*>(malloc(conf.log_max_size));
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

        void raw::log_info(const char* fmt, ...) {
            if (NULL == conf.log_fn_info || 0 == conf.log_max_size) {
                return;
            }

            if (NULL == conf.log_buffer) {
                conf.log_buffer = reinterpret_cast<char*>(malloc(conf.log_max_size));
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
