#ifdef _MSC_VER
#  include <winsock2.h>
#else
#  include <sys/time.h>
#endif

#include <algorithm>
#include <assert.h>
#include <cstdio>
#include <ctime>
#include <detail/happ_cmd.h>
#include <random>
#include <sstream>

#include "detail/crc16.h"
#include "detail/happ_cluster.h"

namespace hiredis {
namespace happ {
namespace detail {
static int random() {
#if defined(__cplusplus) && __cplusplus >= 201103L
  static std::mt19937 g;
  return static_cast<int>(g());
#else
  static bool inited = false;
  if (!inited) {
    inited = true;
    srand(time(NULL));
  }

  return rand();
#endif
}

static char NONE_MSG[] = "none";
}  // namespace detail

HIREDIS_HAPP_API cluster::cluster() : slot_flag_(slot_status::INVALID) {
  conf_.log_fn_debug = conf_.log_fn_info = NULL;
  conf_.log_buffer = NULL;
  conf_.log_max_size = 0;
  conf_.timer_interval_sec = HIREDIS_HAPP_TIMER_INTERVAL_SEC;
  conf_.timer_interval_usec = HIREDIS_HAPP_TIMER_INTERVAL_USEC;
  conf_.timer_timeout_sec = HIREDIS_HAPP_TIMER_TIMEOUT_SEC;
  conf_.cmd_buffer_size = 0;

  for (int i = 0; i < HIREDIS_HAPP_SLOT_NUMBER; ++i) {
    slots_[i].index = i;
  }

  callbacks_.on_connect = NULL;
  callbacks_.on_connected = NULL;
  callbacks_.on_disconnected = NULL;

  timer_actions_.last_update_sec = 0;
  timer_actions_.last_update_usec = 0;
}

HIREDIS_HAPP_API cluster::~cluster() {
  reset();

  // log buffer
  if (NULL != conf_.log_buffer) {
    free(conf_.log_buffer);
    conf_.log_buffer = NULL;
  }
}

HIREDIS_HAPP_API int cluster::init(const std::string &ip, uint16_t port) {
  connection::set_key(conf_.init_connection, ip, port);

  return error_code::REDIS_HAPP_OK;
}

HIREDIS_HAPP_API const std::string &cluster::get_auth_password() { return auth_.password; }

HIREDIS_HAPP_API void cluster::set_auth_password(const std::string &passwd) {
  auth_.password = passwd;
}

HIREDIS_HAPP_API const connection::auth_fn_t &cluster::get_auth_fn() { return auth_.auth_fn; }

HIREDIS_HAPP_API void cluster::set_auth_fn(connection::auth_fn_t fn) { auth_.auth_fn = fn; }

HIREDIS_HAPP_API int cluster::start() {
  reload_slots();
  return error_code::REDIS_HAPP_OK;
}

HIREDIS_HAPP_API int cluster::reset() {
  std::vector<redisAsyncContext *> all_contexts;
  all_contexts.reserve(connections_.size());

  // Store connections_ first, the iterator may be invalid when connections_ changed
  {
    connection_map_t::const_iterator it_b = connections_.begin();
    connection_map_t::const_iterator it_e = connections_.end();
    for (; it_b != it_e; ++it_b) {
      if (NULL != it_b->second->get_context()) {
        all_contexts.push_back(it_b->second->get_context());
      }
    }
  }

  // disable slot update
  slot_flag_ = slot_status::UPDATING;

  // disconnect all connections_.
  // the connected/disconnected callback_ will be triggered if not in callback_
  for (size_t i = 0; i < all_contexts.size(); ++i) {
    redisAsyncDisconnect(all_contexts[i]);
  }

  // release slot pending list
  while (!slot_pending_.empty()) {
    cmd_t *cmd = slot_pending_.front();
    slot_pending_.pop_front();

    call_cmd(cmd, error_code::REDIS_HAPP_SLOT_UNAVAILABLE, NULL, NULL);
    destroy_cmd(cmd);
  }

  for (int i = 0; i < HIREDIS_HAPP_SLOT_NUMBER; ++i) {
    slots_[i].hosts.clear();
  }

  // release timer pending list
  while (!timer_actions_.timer_pending.empty()) {
    cmd_t *cmd = timer_actions_.timer_pending.front().cmd;
    timer_actions_.timer_pending.pop_front();

    call_cmd(cmd, error_code::REDIS_HAPP_TIMEOUT, NULL, NULL);
    destroy_cmd(cmd);
  }

  // connection timeout
  // while(!timer_actions_.timer_conns.empty()) {
  //    timer_t::conn_timetout_t& conn_expire = timer_actions_.timer_conns.front();

  //    connection_t* conn = get_connection(conn_expire.name);
  //    if (NULL != conn && conn->get_sequence() == conn_expire.sequence) {
  //        // if connection is in callback_ mode, the cmds in it will not finish
  //        // so the connection can not be released right now
  //        // this will be released after callback_ in disconnect event
  //        if (!(conn->get_context()->c.flags & REDIS_IN_CALLBACK)) {
  //            release_connection(conn->get_key(), true, error_code::REDIS_HAPP_TIMEOUT);
  //        }
  //    }

  //    timer_actions_.timer_conns.pop_front();
  //}

  // all connections_ are marked disconnection or disconnected, so timeout timers are useless
  timer_actions_.timer_conns.clear();
  timer_actions_.last_update_sec = 0;
  timer_actions_.last_update_usec = 0;

  // If in a callback_, cmds in this connection will not finished, so it can not be freed.
  // In this case, it will call disconnect callback_ after callback_ is finished and then release
  // the connection. If not in a callback_, this connection is already freed at the begining
  // "redisAsyncDisconnect(all_contexts[i]);" connections_.clear();  // can not clear connections_
  // here

  // reset slot status
  slot_flag_ = slot_status::INVALID;

  return 0;
}

HIREDIS_HAPP_API cluster::cmd_t *cluster::exec(const char *key, size_t ks, cmd_t::callback_fn_t cbk,
                                               void *priv_data, int argc, const char **argv,
                                               const size_t *argvlen) {
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

  return exec(key, ks, cmd);
}

HIREDIS_HAPP_API cluster::cmd_t *cluster::exec(const char *key, size_t ks, cmd_t::callback_fn_t cbk,
                                               void *priv_data, const char *fmt, ...) {
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

  return exec(key, ks, cmd);
}

HIREDIS_HAPP_API cluster::cmd_t *cluster::exec(const char *key, size_t ks, cmd_t::callback_fn_t cbk,
                                               void *priv_data, const char *fmt, va_list ap) {
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

  return exec(key, ks, cmd);
}

HIREDIS_HAPP_API cluster::cmd_t *cluster::exec(const char *key, size_t ks, cmd_t *cmd) {
  if (NULL == cmd) {
    return NULL;
  }

  // calculate the slot index
  if (NULL != key && 0 != ks) {
    cmd->engine_.slot = static_cast<int>(crc16(key, ks) % HIREDIS_HAPP_SLOT_NUMBER);
  }

  // ttl_ pre-judge
  if (0 == cmd->ttl_) {
    log_debug("cmd %p at slot %d ttl_ expired", cmd, cmd->engine_.slot);
    call_cmd(cmd, error_code::REDIS_HAPP_TTL, NULL, NULL);
    destroy_cmd(cmd);
    return NULL;
  }

  // update slot
  if (slot_status::INVALID == slot_flag_ || slot_status::UPDATING == slot_flag_) {
    log_debug("transfer cmd at slot %d to slot update pending list", cmd->engine_.slot);
    slot_pending_.push_back(cmd);

    reload_slots();
    return cmd;
  }

  // get a connection in the specified slot
  const connection::key_t *conn_key = get_slot_master(cmd->engine_.slot);

  if (NULL == conn_key) {
    log_info("get connect of slot %d failed", cmd->engine_.slot);
    call_cmd(cmd, error_code::REDIS_HAPP_CONNECTION, NULL, NULL);
    destroy_cmd(cmd);
    return NULL;
  }

  // move cmd into connection
  connection_t *conn_inst = get_connection(conn_key->name);
  if (NULL == conn_inst) {
    conn_inst = make_connection(*conn_key);
  }

  if (NULL == conn_inst) {
    log_info("connect to %s failed", conn_key->name.c_str());

    call_cmd(cmd, error_code::REDIS_HAPP_CONNECTION, NULL, NULL);
    destroy_cmd(cmd);

    return NULL;
  }

  return exec(conn_inst, cmd);
}

HIREDIS_HAPP_API cluster::cmd_t *cluster::exec(connection_t *conn, cmd_t *cmd) {
  if (NULL == cmd) {
    return NULL;
  }

  // ttl_
  if (0 == cmd->ttl_) {
    log_debug("cmd %p at slot %d ttl_ expired", cmd, cmd->engine_.slot);
    call_cmd(cmd, error_code::REDIS_HAPP_TTL, NULL, NULL);
    destroy_cmd(cmd);
    return NULL;
  }

  // ttl_
  --cmd->ttl_;

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
      // remove the invalid connection, so this connection will not be selected next time.
      remove_connection_key(conn->get_key().name);

      // Patch: hiredis will miss onDisconnect in some older version
      // If not in hiredis's callback_, REDIS_DISCONNECTING or REDIS_FREEING means resource is freed
      // If in hiredis's callback_, disconnect will be called after callback_ finished, so do
      // nothing here
      if (!(conn->get_context()->c.flags & REDIS_IN_CALLBACK)) {
        release_connection(conn->get_key(), false, error_code::REDIS_HAPP_CONNECTION);
      }

      // conn = NULL;
      // retry and reload slot information if the connection lost
      cmd->engine_.slot = -1;
      return retry(cmd, NULL);
    } else {
      call_cmd(cmd, error_code::REDIS_HAPP_HIREDIS, conn->get_context(), NULL);
      destroy_cmd(cmd);
    }
    return NULL;
  }

  log_debug("exec cmd %p at slot %d, connection %s", cmd, cmd->engine_.slot,
            conn->get_key().name.c_str());
  return cmd;
}

HIREDIS_HAPP_API cluster::cmd_t *cluster::retry(cmd_t *cmd, connection_t *conn) {
  if (NULL == cmd) {
    return NULL;
  }

  // First, retry immediately for several times.
  if (false == is_timer_active() || cmd->ttl_ > HIREDIS_HAPP_TTL / 2) {
    if (NULL == conn) {
      return exec(NULL, 0, cmd);
    } else {
      return exec(conn, cmd);
    }
  }

  // If it's still failed, maybe it will take some more time to recover the connection,
  // so wait for a while and retry again.
  add_timer_cmd(cmd);
  return cmd;
}

HIREDIS_HAPP_API bool cluster::reload_slots() {
  if (slot_status::UPDATING == slot_flag_) {
    return false;
  }

  const connection::key_t *conn_key = get_slot_master(-1);
  if (NULL == conn_key) {
    return false;
  }

  connection_t *conn = get_connection(conn_key->name);
  if (NULL == conn) {
    conn = make_connection(*conn_key);
  }

  if (NULL == conn) {
    return false;
  }

  // CLUSTER SLOTS cmd
  cmd_t *cmd = create_cmd(on_reply_update_slot, NULL);
  if (NULL == cmd) {
    log_info("create cmd CLUSTER SLOTS failed");
    return false;
  }

  int len = cmd->format("CLUSTER SLOTS");
  if (len <= 0) {
    log_info("format cmd CLUSTER SLOTS failed");
    destroy_cmd(cmd);
    return false;
  }

  if (NULL != exec(conn, cmd)) {
    slot_flag_ = slot_status::UPDATING;
  }

  return true;
}

HIREDIS_HAPP_API const connection::key_t *cluster::get_slot_master(int index) {
  if (index >= 0 && index < HIREDIS_HAPP_SLOT_NUMBER && !slots_[index].hosts.empty()) {
    return &slots_[index].hosts.front();
  }

  // random a address
  index = (detail::random() & 0xFFFF) % HIREDIS_HAPP_SLOT_NUMBER;
  if (slots_[index].hosts.empty()) {
    return &conf_.init_connection;
  }

  return &slots_[index].hosts.front();
}

HIREDIS_HAPP_API const cluster::slot_t *cluster::get_slot_by_key(const char *key, size_t ks) const {
  int index = static_cast<int>(crc16(key, ks) % HIREDIS_HAPP_SLOT_NUMBER);
  return &slots_[index];
}

HIREDIS_HAPP_API const cluster::connection_t *cluster::get_connection(
    const std::string &key) const {
  connection_map_t::const_iterator it = connections_.find(key);
  if (it == connections_.end()) {
    return NULL;
  }

  return it->second.get();
}

HIREDIS_HAPP_API cluster::connection_t *cluster::get_connection(const std::string &key) {
  connection_map_t::iterator it = connections_.find(key);
  if (it == connections_.end()) {
    return NULL;
  }

  return it->second.get();
}

HIREDIS_HAPP_API const cluster::connection_t *cluster::get_connection(const std::string &ip,
                                                                      uint16_t port) const {
  return get_connection(connection::make_name(ip, port));
}

HIREDIS_HAPP_API cluster::connection_t *cluster::get_connection(const std::string &ip,
                                                                uint16_t port) {
  return get_connection(connection::make_name(ip, port));
}

HIREDIS_HAPP_API cluster::connection_t *cluster::make_connection(const connection::key_t &key) {
  holder_t h;
  connection_map_t::iterator check_it = connections_.find(key.name);
  if (check_it != connections_.end()) {
    log_debug("connection %s already exists", key.name.c_str());
    return NULL;
  }

  redisAsyncContext *c = redisAsyncConnect(key.ip.c_str(), static_cast<int>(key.port));
  if (NULL == c || c->err) {
    log_info("redis connect to %s failed, msg: %s", key.name.c_str(),
             NULL == c ? detail::NONE_MSG : c->errstr);
    return NULL;
  }

  h.clu = this;
  redisAsyncSetConnectCallback(c, on_connected_wrapper);
  redisAsyncSetDisconnectCallback(c, on_disconnected_wrapper);
  redisEnableKeepAlive(&c->c);
  if (conf_.timer_timeout_sec > 0) {
    struct timeval tv;
    tv.tv_sec = static_cast<decltype(tv.tv_sec)>(conf_.timer_timeout_sec);
    tv.tv_usec = 0;
    redisSetTimeout(&c->c, tv);
  }

  std::unique_ptr<connection_t> ret_ptr(new connection_t());
  connection_t &ret = *ret_ptr;
  swap(connections_[key.name], ret_ptr);
  ret.init(h, key);
  ret.set_connecting(c);

  c->data = &ret;

  // timeout timer
  if (conf_.timer_timeout_sec > 0 && is_timer_active()) {
    timer_actions_.timer_conns.push_back(timer_t::conn_timetout_t());
    timer_t::conn_timetout_t &conn_expire = timer_actions_.timer_conns.back();
    conn_expire.name = key.name;
    conn_expire.sequence = ret.get_sequence();
    conn_expire.timeout = timer_actions_.last_update_sec + conf_.timer_timeout_sec;
  }

  // auth_ command
  if (auth_.auth_fn || !auth_.password.empty()) {
    // AUTH cmd
    cmd_t *cmd = create_cmd(on_reply_auth, NULL);
    if (NULL != cmd) {
      int len = 0;
      if (auth_.auth_fn) {
        const std::string &passwd = auth_.auth_fn(&ret, auth_.password);
        len = cmd->format("AUTH %b", passwd.c_str(), passwd.size());
      } else if (!auth_.password.empty()) {
        len = cmd->format("AUTH %b", auth_.password.c_str(), auth_.password.size());
      }

      if (len <= 0) {
        log_info("format cmd AUTH failed");
        destroy_cmd(cmd);
        return NULL;
      }

      exec(&ret, cmd);
    }
  }

  // event callback_ must be call at the last
  if (callbacks_.on_connect) {
    callbacks_.on_connect(this, &ret);
  }

  log_debug("redis make connection to %s ", key.name.c_str());
  return &ret;
}

HIREDIS_HAPP_API bool cluster::release_connection(const connection::key_t &key, bool close_fd,
                                                  int status) {
  connection_map_t::iterator it = connections_.find(key.name);
  if (connections_.end() == it) {
    log_debug("connection %s not found", key.name.c_str());
    return false;
  }

  connection_t::status::type from_status = it->second->set_disconnected(close_fd);
  switch (from_status) {
    // recursion, exit
    case connection_t::status::DISCONNECTED:
      return true;

    // connecting, call on_connected event
    case connection_t::status::CONNECTING:
      if (callbacks_.on_connected) {
        callbacks_.on_connected(
            this, it->second.get(), it->second->get_context(),
            error_code::REDIS_HAPP_OK == status ? error_code::REDIS_HAPP_CONNECTION : status);
      }
      break;

    // connecting, call on_disconnected event
    case connection_t::status::CONNECTED:
      if (callbacks_.on_disconnected) {
        callbacks_.on_disconnected(this, it->second.get(), it->second->get_context(), status);
      }
      break;

    default:
      log_info("unknown connection status %d", static_cast<int>(from_status));
      break;
  }

  log_debug("release connection %s", key.name.c_str());

  // can not use key any more
  connections_.erase(it);

  return true;
}

HIREDIS_HAPP_API size_t cluster::get_connection_size() const { return connections_.size(); }

HIREDIS_HAPP_API cluster::onconnect_fn_t cluster::set_on_connect(onconnect_fn_t cbk) {
  using std::swap;
  swap(cbk, callbacks_.on_connect);
  return cbk;
}

HIREDIS_HAPP_API cluster::onconnected_fn_t cluster::set_on_connected(onconnected_fn_t cbk) {
  using std::swap;
  swap(cbk, callbacks_.on_connected);
  return cbk;
}

HIREDIS_HAPP_API cluster::ondisconnected_fn_t cluster::set_on_disconnected(
    ondisconnected_fn_t cbk) {
  using std::swap;
  swap(cbk, callbacks_.on_disconnected);
  return cbk;
}

HIREDIS_HAPP_API void cluster::set_cmd_buffer_size(size_t s) { conf_.cmd_buffer_size = s; }

HIREDIS_HAPP_API size_t cluster::get_cmd_buffer_size() const { return conf_.cmd_buffer_size; }

HIREDIS_HAPP_API bool cluster::is_timer_active() const {
  return (timer_actions_.last_update_sec != 0 || timer_actions_.last_update_usec != 0) &&
         (conf_.timer_interval_sec > 0 || conf_.timer_interval_usec > 0);
}

HIREDIS_HAPP_API void cluster::set_timer_interval(time_t sec, time_t usec) {
  conf_.timer_interval_sec = sec;
  conf_.timer_interval_usec = usec;
}

HIREDIS_HAPP_API void cluster::set_timeout(time_t sec) { conf_.timer_timeout_sec = sec; }

HIREDIS_HAPP_API void cluster::add_timer_cmd(cmd_t *cmd) {
  if (NULL == cmd) {
    return;
  }

  if (is_timer_active()) {
    timer_actions_.timer_pending.push_back(timer_t::delay_t());
    timer_t::delay_t &d = timer_actions_.timer_pending.back();
    d.sec = timer_actions_.last_update_sec + conf_.timer_interval_sec;
    d.usec = timer_actions_.last_update_usec + conf_.timer_interval_usec;
    d.cmd = cmd;
  } else {
    exec(NULL, 0, cmd);
  }
}

HIREDIS_HAPP_API int cluster::proc(time_t sec, time_t usec) {
  int ret = 0;

  timer_actions_.last_update_sec = sec;
  timer_actions_.last_update_usec = usec;

  while (!timer_actions_.timer_pending.empty()) {
    timer_t::delay_t &rd = timer_actions_.timer_pending.front();
    if (rd.sec > sec || (rd.sec == sec && rd.usec > usec)) {
      break;
    }

    timer_t::delay_t d = rd;
    timer_actions_.timer_pending.pop_front();

    exec(NULL, 0, d.cmd);

    ++ret;
  }

  // connection timeout
  // this can not be call in callback_
  while (!timer_actions_.timer_conns.empty() && sec >= timer_actions_.timer_conns.front().timeout) {
    timer_t::conn_timetout_t &conn_expire = timer_actions_.timer_conns.front();

    connection_t *conn = get_connection(conn_expire.name);
    if (NULL != conn && conn->get_sequence() == conn_expire.sequence) {
      assert(!(conn->get_context()->c.flags & REDIS_IN_CALLBACK));
      release_connection(conn->get_key(), true, error_code::REDIS_HAPP_TIMEOUT);
    }

    timer_actions_.timer_conns.pop_front();
  }

  return ret;
}

HIREDIS_HAPP_API void cluster::set_log_writer(log_fn_t info_fn, log_fn_t debug_fn,
                                              size_t max_size) {
  using std::swap;
  conf_.log_fn_info = info_fn;
  conf_.log_fn_debug = debug_fn;
  conf_.log_max_size = max_size;

  if (NULL != conf_.log_buffer) {
    free(conf_.log_buffer);
    conf_.log_buffer = NULL;
  }
}

HIREDIS_HAPP_API const cluster::timer_t &cluster::get_timer_actions() const {
  return timer_actions_;
}

HIREDIS_HAPP_API cluster::cmd_t *cluster::create_cmd(cmd_t::callback_fn_t cbk, void *pridata) {
  holder_t h;
  h.clu = this;
  cmd_t *ret = cmd_t::create(h, cbk, pridata, conf_.cmd_buffer_size);
  return ret;
}

HIREDIS_HAPP_API void cluster::destroy_cmd(cmd_t *c) {
  if (NULL == c) {
    log_debug("can not destroy null cmd");
    return;
  }

  // lost connection
  if (NULL != c->callback_) {
    call_cmd(c, error_code::REDIS_HAPP_UNKNOWD, NULL, NULL);
  }

  cmd_t::destroy(c);
}

HIREDIS_HAPP_API int cluster::call_cmd(cmd_t *c, int err, redisAsyncContext *context, void *reply) {
  if (NULL == c) {
    log_debug("can not call cmd without cmd object");
    return error_code::REDIS_HAPP_UNKNOWD;
  }

  return c->call_reply(err, context, reply);
}

void cluster::on_reply_wrapper(redisAsyncContext *c, void *r, void *privdata) {
  connection_t *conn = reinterpret_cast<connection_t *>(c->data);
  cmd_t *cmd = reinterpret_cast<cmd_t *>(privdata);
  cluster *self = cmd->holder_.clu;

  // retry if disconnecting will lead to a infinite loop
  if (c->c.flags & REDIS_DISCONNECTING) {
    self->log_debug("redis cmd %p reply when disconnecting context err %d,msg %s", cmd, c->err,
                    NULL == c->errstr ? detail::NONE_MSG : c->errstr);
    cmd->error_code_ = error_code::REDIS_HAPP_CONNECTION;
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
    self->log_debug("redis cmd %p reply context err %d and abort, %s", cmd, c->err,
                    NULL == c->errstr ? detail::NONE_MSG : c->errstr);
    // other errors will be passed to caller
    conn->call_reply(cmd, r);
    return;
  }

  redisReply *reply = reinterpret_cast<redisReply *>(r);

  // error handler
  if (REDIS_REPLY_ERROR == reply->type) {
    int slot_index = 0;
    char addr[260] = {0};

    // detect MOVED,ASK and CLUSTERDOWN
    if (0 == HIREDIS_HAPP_STRNCASE_CMP("ASK", reply->str, 3)) {
      self->log_debug("redis cmd %p %s", cmd, reply->str);
      // send ASK to another connection
#if defined(_MSC_VER)
      HIREDIS_HAPP_SSCANF(reply->str + 4, "%d %s", &slot_index, addr,
                          static_cast<unsigned int>(sizeof(addr)));
#else
      HIREDIS_HAPP_SSCANF(reply->str + 4, "%d %s", &slot_index, addr);
#endif
      std::string ip;
      uint16_t port;
      if (connection::pick_name(addr, ip, port)) {
        connection::key_t conn_key;
        connection::set_key(conn_key, ip, port);

        // ASKING request
        connection_t *ask_conn = self->get_connection(conn_key.name);
        if (NULL == ask_conn) {
          ask_conn = self->make_connection(conn_key);
        }

        // pop from old connection, and run it
        conn->pop_reply(cmd);

        if (NULL != ask_conn) {
          if (REDIS_OK ==
              redisAsyncCommand(ask_conn->get_context(), on_reply_asking, cmd, "ASKING")) {
            return;
          }
        }

        // retry if ASK failed
        self->retry(cmd);
        return;
      }
    } else if (0 == HIREDIS_HAPP_STRNCASE_CMP("MOVED", reply->str, 5)) {
      self->log_debug("redis cmd %p %s", cmd, reply->str);

#if defined(_MSC_VER)
      HIREDIS_HAPP_SSCANF(reply->str + 6, "%d %s", &slot_index, addr,
                          static_cast<unsigned int>(sizeof(addr)));
#else
      HIREDIS_HAPP_SSCANF(reply->str + 6, "%d %s", &slot_index, addr);
#endif

      if (cmd->engine_.slot >= 0 && cmd->engine_.slot != slot_index) {
        self->log_info("cluster cmd key error, expect slot: %d, real slot: %d", cmd->engine_.slot,
                       slot_index);
        cmd->engine_.slot = slot_index;
      }

      std::string ip;
      uint16_t port;
      if (connection::pick_name(addr, ip, port)) {
        // update slot
        self->slots_[slot_index].hosts.clear();
        self->slots_[slot_index].hosts.push_back(connection::key_t());
        connection::set_key(self->slots_[slot_index].hosts.back(), ip, port);

        // retry
        conn->pop_reply(cmd);
        self->retry(cmd);

        // reload all slots_
        // FIXME: Is it necessary to reload all slots_ here?
        //        If we don't reload all slots_, many slot may be expired and will make many cmd has
        //        a long delay later. But if we reload all slots_, there may be too often to do this
        //        if network is not stable for a short time Reload slots_ will use much more CPU
        //        resource than a cmd (clear and copy 16384 lists)
        self->reload_slots();
        return;
      } else {
        self->slot_flag_ = slot_status::INVALID;
      }
    } else if (0 == HIREDIS_HAPP_STRNCASE_CMP("CLUSTERDOWN", reply->str, 11)) {
      self->log_info("cluster down reset all connection, cmd and replys");
      conn->call_reply(cmd, r);
      self->reset();
      return;
    }

    self->log_debug("redis cmd %p reply error and abort, msg: %s", cmd,
                    NULL == reply->str ? detail::NONE_MSG : reply->str);
    // other errors will be passed to caller
    conn->call_reply(cmd, r);
    return;
  }

  // success and call callback_
  self->log_debug("redis cmd %p got reply success.(ttl_=%3d)", cmd,
                  NULL == cmd ? -1 : static_cast<int>(cmd->ttl_));
  conn->call_reply(cmd, r);
}

void cluster::on_reply_update_slot(cmd_exec *cmd, redisAsyncContext *, void *r,
                                   void * /*privdata*/) {
  redisReply *reply = reinterpret_cast<redisReply *>(r);
  cluster *self = cmd->holder_.clu;

  // failed and retry
  if (NULL == reply || reply->elements <= 0 || REDIS_REPLY_ARRAY != reply->element[0]->type) {
    self->slot_flag_ = slot_status::INVALID;

    if (!self->slot_pending_.empty()) {
      self->log_info("update slots_ failed and try to retry again.");

      // Wait for a while if it's network problem
      // this message will also retry for TTL times
      // update slots_ always random a connection
      cmd->engine_.slot = -1;
      self->add_timer_cmd(cmd);
    } else {
      self->log_info("update slots_ failed and will retry later.");
    }

    return;
  }

  // clear and reset slots_ ...
  for (size_t i = 0; i < HIREDIS_HAPP_SLOT_NUMBER; ++i) {
    self->slots_[i].hosts.clear();
  }

  for (size_t i = 0; i < reply->elements; ++i) {
    redisReply *slot_node = reply->element[i];
    if (slot_node->elements >= 3) {
      long long si = slot_node->element[0]->integer;
      long long ei = slot_node->element[1]->integer;

      std::vector<connection::key_t> hosts;
      for (size_t j = 2; j < slot_node->elements; ++j) {
        redisReply *addr = slot_node->element[j];
        // redis cluster may response a empty list when some error happened
        if (addr->elements >= 2 && REDIS_REPLY_STRING == addr->element[0]->type &&
            addr->element[0]->str[0] && REDIS_REPLY_INTEGER == addr->element[1]->type) {
          hosts.push_back(connection::key_t());
          connection::set_key(hosts.back(), addr->element[0]->str,
                              static_cast<uint16_t>(addr->element[1]->integer));
        }
      }

      // log
      if (NULL != self->conf_.log_fn_debug && self->conf_.log_max_size > 0) {
        self->log_debug("slot update: [%lld-%lld]", si, ei);
        for (size_t j = 0; j < hosts.size(); ++j) {
          self->log_debug(" -- %s", hosts[j].name.c_str());
        }
      }
      // copy for 16384 times
      for (; si <= ei; ++si) {
        self->slots_[si].hosts = hosts;
      }
    }
  }

  // set status first and then retry, or there will be a infinite loop
  self->slot_flag_ = slot_status::OK;

  self->log_info("update %d slots_ done", static_cast<int>(reply->elements));

  // run pending list
  while (!self->slot_pending_.empty()) {
    cmd_t *first_cmd = self->slot_pending_.front();
    self->slot_pending_.pop_front();
    self->retry(first_cmd);
  }
}

void cluster::on_reply_asking(redisAsyncContext *c, void *r, void *privdata) {
  cmd_t *cmd = reinterpret_cast<cmd_t *>(privdata);
  redisReply *reply = reinterpret_cast<redisReply *>(r);
  connection_t *conn = reinterpret_cast<connection_t *>(c->data);
  cluster *self = conn->get_holder().clu;

  // cmd in ask command is not in any connection
  // so there is no need to pop it, directly retry will be OK

  if (REDIS_ERR_IO == c->err && REDIS_ERR_EOF == c->err) {
    self->log_debug("redis asking err %d and will retry, %s", c->err, c->errstr);
    // retry if network error
    self->retry(cmd);
    return;
  }

  if (REDIS_OK != c->err || NULL == r) {
    self->log_debug("redis asking err %d and abort, %s", c->err,
                    NULL == c->errstr ? detail::NONE_MSG : c->errstr);

    if (c->c.flags & REDIS_DISCONNECTING) {
      cmd->error_code_ = error_code::REDIS_HAPP_CONNECTION;
    }

    // other errors should be passed to callback_
    self->call_cmd(cmd, error_code::REDIS_HAPP_CONNECTION, conn->get_context(), r);
    self->destroy_cmd(cmd);
    return;
  }

  if (NULL != reply->str && 0 == HIREDIS_HAPP_STRNCASE_CMP("OK", reply->str, 2)) {
    self->retry(cmd, conn);
    return;
  }

  self->log_debug("redis reply asking err %d and abort, %s", reply->type,
                  NULL == reply->str ? detail::NONE_MSG : reply->str);
  // other errors should be passed to callback_
  self->call_cmd(cmd, error_code::REDIS_HAPP_CONNECTION, conn->get_context(), r);
  self->destroy_cmd(cmd);
}

void cluster::on_connected_wrapper(const struct redisAsyncContext *c, int status) {
  connection_t *conn = reinterpret_cast<connection_t *>(c->data);
  cluster *self = conn->get_holder().clu;

  // hiredis bug, sometimes 0 == status but c is already closed
  if (REDIS_OK == status && hiredis::happ::connection::status::DISCONNECTED == conn->get_status()) {
    status = REDIS_ERR_OTHER;
  }

  // event callback_
  if (self->callbacks_.on_connected) {
    self->callbacks_.on_connected(self, conn, c, status);
  }

  // failed, release resource
  if (REDIS_OK != status) {
    self->log_debug("connect to %s failed, status: %d, msg: %s", conn->get_key().name.c_str(),
                    status, c->errstr);
    self->release_connection(conn->get_key(), false, status);

    // update slots_ if connect failed
    self->reload_slots();
  } else {
    conn->set_connected();

    self->log_debug("connect to %s success", conn->get_key().name.c_str());

    // reload slots_
    if (slot_status::INVALID == self->slot_flag_) {
      self->reload_slots();
    }
  }
}

void cluster::on_disconnected_wrapper(const struct redisAsyncContext *c, int status) {
  connection_t *conn = reinterpret_cast<connection_t *>(c->data);
  cluster *self = conn->get_holder().clu;

  // We should update slots_ on next cmd if there is any connection disconnected
  if (REDIS_OK != status) {
    self->remove_connection_key(conn->get_key().name);
  }

  // release resource
  self->release_connection(conn->get_key(), false, status);
}

void cluster::on_reply_auth(cmd_exec *cmd, redisAsyncContext *rctx, void *r, void * /*privdata*/) {
  redisReply *reply = reinterpret_cast<redisReply *>(r);
  cluster *self = cmd->holder_.clu;
  assert(rctx);

  // error and log
  if (NULL == reply || 0 != HIREDIS_HAPP_STRNCASE_CMP("OK", reply->str, 2)) {
    const char *error_text = "";
    if (NULL != reply && NULL != reply->str) {
      error_text = reply->str;
    }
    if (REDIS_CONN_TCP == rctx->c.connection_type) {
      self->log_info("tcp:%s:%d AUTH failed. %s",
                     rctx->c.tcp.host
                         ? rctx->c.tcp.host
                         : (rctx->c.tcp.source_addr ? rctx->c.tcp.source_addr : "UNKNOWN"),
                     rctx->c.tcp.port, error_text);
    } else if (REDIS_CONN_UNIX == rctx->c.connection_type) {
      self->log_info("unix:%s AUTH failed. %s",
                     rctx->c.unix_sock.path ? rctx->c.unix_sock.path : "NULL", error_text);
    } else {
      self->log_info("AUTH failed. %s", error_text);
    }
  } else {
    if (REDIS_CONN_TCP == rctx->c.connection_type) {
      self->log_info("tcp:%s:%d AUTH success.",
                     rctx->c.tcp.host
                         ? rctx->c.tcp.host
                         : (rctx->c.tcp.source_addr ? rctx->c.tcp.source_addr : "UNKNOWN"),
                     rctx->c.tcp.port);
    } else if (REDIS_CONN_UNIX == rctx->c.connection_type) {
      self->log_info("unix:%s AUTH success.",
                     rctx->c.unix_sock.path ? rctx->c.unix_sock.path : "NULL");
    } else {
      self->log_info("AUTH success.");
    }
  }
}

void cluster::remove_connection_key(const std::string &name) {
  slot_flag_ = slot_status::INVALID;

  for (int i = 0; i < HIREDIS_HAPP_SLOT_NUMBER; ++i) {
    std::vector<connection::key_t> &hosts = slots_[i].hosts;
    if (!hosts.empty() && hosts[0].name == name) {
      if (hosts.size() > 1) {
        using std::swap;
        swap(hosts[0], hosts[hosts.size() - 1]);
      }

      hosts.pop_back();
    }
  }
}

void cluster::log_debug(const char *fmt, ...) {
  if (NULL == conf_.log_fn_debug || 0 == conf_.log_max_size) {
    return;
  }

  if (NULL == conf_.log_buffer) {
    conf_.log_buffer = reinterpret_cast<char *>(malloc(conf_.log_max_size));
  }

  va_list ap;
  va_start(ap, fmt);
  int len = vsnprintf(conf_.log_buffer, conf_.log_max_size, fmt, ap);
  va_end(ap);

  conf_.log_buffer[conf_.log_max_size - 1] = 0;
  if (len > 0) {
    conf_.log_buffer[len] = 0;
  }

  conf_.log_fn_debug(conf_.log_buffer);
}

void cluster::log_info(const char *fmt, ...) {
  if (NULL == conf_.log_fn_info || 0 == conf_.log_max_size) {
    return;
  }

  if (NULL == conf_.log_buffer) {
    conf_.log_buffer = reinterpret_cast<char *>(malloc(conf_.log_max_size));
  }

  va_list ap;
  va_start(ap, fmt);
  int len = vsnprintf(conf_.log_buffer, conf_.log_max_size, fmt, ap);
  va_end(ap);

  conf_.log_buffer[conf_.log_max_size - 1] = 0;
  if (len > 0) {
    conf_.log_buffer[len] = 0;
  }

  conf_.log_fn_info(conf_.log_buffer);
}
}  // namespace happ
}  // namespace hiredis
