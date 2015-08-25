
#include <assert.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>

#include "detail/happ_connection.h"



namespace hiredis {
    namespace happ {
        connection::connection(): context(NULL), conn_status(status::DISCONNECTED) {
            // ID主要用于判定超时的时候，防止地址或名称重用，超时时间内64位整数不可能会重复
#if defined(HIREDIS_HAPP_ATOMIC_STD)
            static std::atomic<uint64_t> seq_alloc(0);
            sequence = seq_alloc.fetch_add(1);

#elif defined(HIREDIS_HAPP_ATOMIC_MSVC)
            static LONGLONG volatile seq_alloc = 0;
            sequence = static_cast<uint64_t>(InterlockedAdd64(&seq_alloc, 1));

#elif defined(HIREDIS_HAPP_ATOMIC_GCC_ATOMIC)
            static volatile uint64_t seq_alloc = 0;
            sequence = __atomic_add_fetch(&seq_alloc, 1, __ATOMIC_SEQ_CST);
#elif defined(HIREDIS_HAPP_ATOMIC_GCC)
            static volatile uint64_t seq_alloc = 0;
            sequence = __sync_fetch_and_add (&seq_alloc, 1);
#else
            static volatile uint64_t seq_alloc = 0;
            sequence = ++ seq_alloc;
#endif
            holder.clu = NULL;


        }

        connection::~connection() {
            release(NULL, true);
        }

        void connection::init(holder_t h, const std::string& ip, uint16_t port) {
            set_key(key, ip, port);
            init(h, key);
        }

        void connection::init(holder_t h, const key_t& k) {
            if(&key != &k) {
                key = k;
            }

            holder = h;
        }

        status::type connection::set_connecting(redisAsyncContext* c) {
            status::type ret = conn_status;
            if (status::CONNECTING == conn_status) {
                return ret;
            }

            if (c == context) {
                return ret;
            }

            context = c;
            conn_status = status::CONNECTING;
            c->data = this;
            return ret;
        }

        status::type connection::set_disconnected(std::list<cmd_exec*>* pending, bool close_fd) {
            status::type ret = conn_status;
            if (status::DISCONNECTED == conn_status) {
                return ret;
            }

            release(pending, close_fd);
            return ret;
        }

        status::type connection::set_connected(std::list<cmd_exec*>& pending) {
            status::type ret = conn_status;
            if (status::CONNECTING != conn_status || NULL == context) {
                return ret;
            }

            conn_status = status::CONNECTED;

            // 导出等待列表
            pending.clear();
            pending.swap(pending_list);
            return ret;
        }

        int connection::redis_cmd(cmd_exec* c, redisCallbackFn fn) {
            if (NULL == c) {
                return error_code::REDIS_HAPP_PARAM;
            }

            if (NULL == context) {
                return error_code::REDIS_HAPP_CREATE;
            }

            switch (conn_status) {
            case status::CONNECTING: { // 正在连接则进入发送等待队列
                pending_list.push_back(c);
                return error_code::REDIS_HAPP_OK;
            }
            case status::DISCONNECTED: { // 未连接则直接失败
                return error_code::REDIS_HAPP_CONNECTION;
            }
            case status::CONNECTED: {
                int res = 0;
                if (0 == c->cmd.raw_len) {
                    res = redisAsyncFormattedCommand(context, fn, c, c->cmd.content.redis_sds, sdslen(c->cmd.content.redis_sds));
                } else {
                    res = redisAsyncFormattedCommand(context, fn, c, c->cmd.content.raw, c->cmd.raw_len);
                }

                if (REDIS_OK == res) {
                    reply_list.push_back(c);
                }

                return res;
            }
            default: {
                assert(0);
                break;
            }
            }

            // 未知异常，回收cmd
            c->call_reply(error_code::REDIS_HAPP_UNKNOWD, context, NULL);
            cmd_exec::destroy(c);

            return error_code::REDIS_HAPP_OK;
        }

        int connection::call_reply(cmd_exec* c, void* r) {
            if (NULL == context) {
                return error_code::REDIS_HAPP_CREATE;
            }

            pop_reply(c);

            if (reply_list.empty()) {
                assert(0);
                return error_code::REDIS_HAPP_OK;
            }

            // now, c == reply_list.front()
            reply_list.pop_front();
            int errcode = error_code::REDIS_HAPP_OK;
            if (REDIS_OK != context->err) {
                errcode = error_code::REDIS_HAPP_HIREDIS;
            } else if (r) {
                redisReply* reply = reinterpret_cast<redisReply*>(r);
                if (REDIS_REPLY_ERROR == reply->type) {
                    errcode = error_code::REDIS_HAPP_HIREDIS;
                }
            }

            int res = c->call_reply(
                errcode,
                context, 
                r
            );

            cmd_exec::destroy(c);
            return res;
        }

        cmd_exec* connection::pop_reply(cmd_exec* c) {
            if (NULL == c) {
                if (reply_list.empty()) {
                    return NULL;
                }

                c = reply_list.front();
                reply_list.pop_front();
                return c;
            }

            std::list<cmd_exec*>::iterator it = std::find(reply_list.begin(), reply_list.end(), c);
            if (it == reply_list.end()) {
                return NULL;
            }

            // 先处理掉所有过期请求
            while (!reply_list.empty() && reply_list.front() != c) {
                cmd_exec* expired_c = reply_list.front();
                reply_list.pop_front();

                expired_c->call_reply(error_code::REDIS_HAPP_TIMEOUT, context, NULL);
                cmd_exec::destroy(expired_c);
            }

            // now, c == reply_list.front()
            reply_list.pop_front();
            return c;
        }

        redisAsyncContext* connection::get_context() const {
            return context;
        }

        void connection::release(std::list<cmd_exec*>* dump_pending, bool close_fd) {
            if (NULL != context && close_fd) {
                redisAsyncDisconnect(context);
            }

            // 回包列表
            while (!reply_list.empty()) {
                cmd_exec* expired_c = reply_list.front();
                reply_list.pop_front();

                expired_c->call_reply(error_code::REDIS_HAPP_CONNECTION, context, NULL);
                cmd_exec::destroy(expired_c);
            }

            // 等待列表
            if (NULL == dump_pending) {
                while (!pending_list.empty()) {
                    cmd_exec* expired_c = pending_list.front();
                    pending_list.pop_front();

                    expired_c->call_reply(error_code::REDIS_HAPP_CONNECTION, context, NULL);
                    cmd_exec::destroy(expired_c);
                }
            } else {
                dump_pending->clear();
                dump_pending->swap(pending_list);
            }

            context = NULL;
            conn_status = status::DISCONNECTED;
        }

        std::string connection::make_name(const std::string& ip, uint16_t port) {
            std::string ret;
            ret.clear();
            ret.reserve(ip.size() + 8);

            ret = ip;
            ret += ":";

            char buf[8] = { 0 };
            int i = 7;  // XXXXXXX0
            while (port) {
                buf[--i] = port % 10 + '0';
                port /= 10;
            }

            ret += &buf[i];

            return ret;
        }

        void connection::set_key(connection::key_t& k, const std::string& ip, uint16_t port) {
            k.name = make_name(ip, port);
            k.ip = ip;
            k.port = port;
        }

        bool connection::pick_name(const std::string& name, std::string& ip, uint16_t& port) {
            size_t it = name.find_first_of(':');
            if (it == std::string::npos) {
                return false;
            }

            if (it == name.size() - 1) {
                return false;
            }

            size_t s = 0;
            while (' ' == name[s] || '\t' == name[s] || '\r' == name[s] || '\n' == name[s]) {
                ++s;
            }

            ip = name.substr(s, it - s);
            int p = 0;
            HIREDIS_HAPP_SSCANF(name.c_str() + it + 1, "%d", &p);
            port = static_cast<uint16_t>(p);

            return true;
        }
    }
}
