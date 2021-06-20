// Copyright 2021 owent
// Created by owent on 2015/7/5.
//

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#endif

#ifdef _MSC_VER
#  include <winsock2.h>
#else
#  include <sys/time.h>
#endif

#include <assert.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <list>
#include <mutex>
#include <sstream>
#include <string>

#if defined(HIREDIS_HAPP_ENABLE_LIBUV)
#  include "hiredis/adapters/libuv.h"
#elif defined(HIREDIS_HAPP_ENABLE_LIBEVENT)
#  include "hiredis/adapters/libevent.h"
#endif

#include "hiredis_happ.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#  include <Windows.h>
#  include <process.h>
#  include <sys/locking.h>
#  include <winsock2.h>
#  define THREAD_SPIN_COUNT 2000

typedef HANDLE sample_thread_t;
#  define THREAD_FUNC unsigned __stdcall
#  define THREAD_CREATE(threadvar, fn, arg)                                 \
    do {                                                                    \
      uintptr_t threadhandle = _beginthreadex(NULL, 0, fn, (arg), 0, NULL); \
      (threadvar) = (sample_thread_t)threadhandle;                          \
    } while (0)
#  define THREAD_JOIN(th) WaitForSingleObject(th, INFINITE)
#  define THREAD_RETURN return (0)

#  define THREAD_SLEEP_MS(TM) Sleep(TM)
#elif defined(__GNUC__) || defined(__clang__)
#  include <errno.h>
#  include <pthread.h>
#  include <unistd.h>

typedef pthread_t sample_thread_t;
#  define THREAD_FUNC void *
#  define THREAD_CREATE(threadvar, fn, arg) pthread_create(&(threadvar), NULL, fn, arg)
#  define THREAD_JOIN(th) pthread_join(th, NULL)
#  define THREAD_RETURN \
    pthread_exit(NULL); \
    return NULL

#  define THREAD_SLEEP_MS(TM)                     \
    ((TM > 1000) ? sleep(TM / 1000) : usleep(0)); \
    usleep((TM % 1000) * 1000)
#endif

#ifdef __cplusplus
}
#endif

static hiredis::happ::cluster g_clu;

#if defined(HIREDIS_HAPP_ENABLE_LIBUV)
static uv_loop_t *main_loop;
#elif defined(HIREDIS_HAPP_ENABLE_LIBEVENT)
static event_base *main_loop;
#endif

static std::mutex g_mutex;
static std::list<std::string> g_cmds;

static void on_connect_cbk(hiredis::happ::cluster *, hiredis::happ::connection *conn) {
#if defined(HIREDIS_HAPP_ENABLE_LIBUV)
  redisLibuvAttach(conn->get_context(), main_loop);
#elif defined(HIREDIS_HAPP_ENABLE_LIBEVENT)
  redisLibeventAttach(conn->get_context(), main_loop);
#endif

  if (NULL != conn) {
    printf("start connect to %s\n", conn->get_key().name.c_str());
  } else {
    printf("error: connection not found when connect\n");
  }
}

static void on_connected_cbk(hiredis::happ::cluster *, hiredis::happ::connection *conn,
                             const struct redisAsyncContext *c, int status) {
  if (NULL == conn) {
    printf("error: connection not found when connected\n");
    return;
  }

  if (REDIS_OK == status) {
    printf("%s connected success\n", conn->get_key().name.c_str());
  } else {
    char no_msg[] = "none";
    printf("%s connected failed, status %d, err: %d, msg %s\n", conn->get_key().name.c_str(), status, c ? c->err : 0,
           (c && c->errstr) ? c->errstr : no_msg);
  }
}

static void on_disconnected_cbk(hiredis::happ::cluster *, hiredis::happ::connection *conn,
                                const struct redisAsyncContext *c, int status) {
  if (NULL == conn) {
    printf("error: connection not found when connected\n");
    return;
  }

  if (REDIS_OK == status) {
    printf("%s disconnected\n", conn->get_key().name.c_str());
  } else {
    char no_msg[] = "none";
    printf("%s disconnected status %d, err: %d, msg %s\n", conn->get_key().name.c_str(), status, c ? c->err : 0,
           (c && c->errstr) ? c->errstr : no_msg);
  }
}

static std::vector<std::string> split_word(const std::string &cmd_line) {
  std::vector<std::string> ret;

  std::string seg;
  for (size_t i = 0; i < cmd_line.size(); ++i) {
    if (cmd_line[i] == '\r' || cmd_line[i] == '\n' || cmd_line[i] == '\t' || cmd_line[i] == ' ') {
      if (seg.empty()) {
        continue;
      }

      ret.push_back(seg);
      seg.clear();
    } else {
      char c = cmd_line[i];
      if ('\'' == c || '\"' == c) {
        for (++i; i < cmd_line.size() && c != cmd_line[i]; ++i) {
          if (c == '\"' && '\\' == cmd_line[i]) {
            ++i;
            if (i < cmd_line.size()) {
              seg.push_back(cmd_line[i]);
            }
          } else {
            seg.push_back(cmd_line[i]);
          }
        }

        ++i;
      } else {
        seg.push_back(c);
      }
    }
  }

  if (!seg.empty()) {
    ret.push_back(seg);
  }

  return ret;
}

static void dump_callback(hiredis::happ::cmd_exec *cmd, struct redisAsyncContext *, void *r, void *p) {
  assert(p == reinterpret_cast<void *>(dump_callback));

  if (cmd->result() != hiredis::happ::error_code::REDIS_HAPP_OK) {
    printf("cmd_exec result: %d\n", cmd->result());
  }
  hiredis::happ::cmd_exec::dump(std::cout, reinterpret_cast<redisReply *>(r), 0);
}

static void subscribe_callback(struct redisAsyncContext *, void *r, void *p) {
  assert(p == reinterpret_cast<void *>(subscribe_callback));

  printf(" ===== subscribe message received =====\n");
  hiredis::happ::cmd_exec::dump(std::cout, reinterpret_cast<redisReply *>(r), 0);
}

static void monitor_callback(struct redisAsyncContext *, void *r, void *p) {
  assert(p == reinterpret_cast<void *>(monitor_callback));

  printf(" ----- monitor message received ----- \n");
  hiredis::happ::cmd_exec::dump(std::cout, reinterpret_cast<redisReply *>(r), 0);
}

static void on_timer_proc(
#if defined(HIREDIS_HAPP_ENABLE_LIBUV)
    uv_timer_t *handle
#elif defined(HIREDIS_HAPP_ENABLE_LIBEVENT)
    evutil_socket_t fd, short event, void *arg
#endif
) {
  static time_t sec = time(NULL);
  static time_t usec = 0;

  usec += 100000;
  if (usec > 10000000) {
    sec += usec / 10000000;
    usec %= 10000000;
  }

  // get command for cli
  g_mutex.lock();
  std::list<std::string> pending_cmds;
  if (!g_cmds.empty()) {
    pending_cmds.swap(g_cmds);
  }
  g_mutex.unlock();

  // run command
  while (!pending_cmds.empty()) {
    std::string cmd_line = pending_cmds.front();
    pending_cmds.pop_front();

    std::vector<std::string> cmds = split_word(cmd_line);
    std::string cmd = cmds.empty() ? "" : cmds.front();
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    hiredis::happ::cmd_exec::callback_fn_t cbk = dump_callback;
    redisCallbackFn *raw_cbk;
    bool is_raw = false;
    int k = 1;
    if ("INFO" == cmd || "MULTI" == cmd || "EXEC" == cmd || "SLAVEOF" == cmd || "CONFIG" == cmd || "SHUTDOWN" == cmd ||
        "CLUSTER" == cmd) {
      k = -1;
    } else if ("SCRIPT" == cmd) {
      k = -1;
    } else if ("EVALSHA" == cmd || "EVAL" == cmd) {
      k = 3;
    } else if ("SUBSCRIBE" == cmd || "UNSUBSCRIBE" == cmd || "PSUBSCRIBE" == cmd || "PUNSUBSCRIBE" == cmd) {
      raw_cbk = subscribe_callback;
      is_raw = true;
    } else if ("MONITOR" == cmd) {
      raw_cbk = monitor_callback;
      is_raw = true;
    }

    // get parameters
    std::vector<const char *> pc;
    std::vector<size_t> ps;
    for (size_t i = 0; i < cmds.size(); ++i) {
      pc.push_back(cmds[i].c_str());
      ps.push_back(cmds[i].size());
    }

    if (is_raw) {  // run special command

      const hiredis::happ::cluster::slot_t *slot_info = NULL;
      if (cmds.size() > 1) {
        slot_info = g_clu.get_slot_by_key(cmds[1].c_str(), cmds[1].size());
        assert(slot_info);
      }

      const hiredis::happ::connection::key_t *conn_key =
          g_clu.get_slot_master(NULL == slot_info ? -1 : slot_info->index);
      if (NULL == conn_key) {
        printf("connection not found.\n");
        continue;
      }

      hiredis::happ::connection *conn = g_clu.get_connection(conn_key->name);
      if (NULL == conn) {
        conn = g_clu.make_connection(*conn_key);
      }

      if (NULL == conn) {
        printf("connect to %s failed.\n", conn_key->name.c_str());
        continue;
      }
      conn->redis_raw_cmd(raw_cbk, reinterpret_cast<void *>(raw_cbk), static_cast<int>(cmds.size()), &pc[0], &ps[0]);

    } else {  // run Request-Response command
      if (k >= 0 && k < static_cast<int>(cmds.size())) {
        cmd = cmds[k];
        g_clu.exec(cmd.c_str(), cmd.size(), cbk, reinterpret_cast<void *>(cbk), static_cast<int>(cmds.size()), &pc[0],
                   &ps[0]);
      } else {
        g_clu.exec(NULL, 0, cbk, reinterpret_cast<void *>(cbk), static_cast<int>(cmds.size()), &pc[0], &ps[0]);
      }
    }
  }

  g_clu.proc(sec, usec);
}

static THREAD_FUNC proc_uv_thd(void *) {
#if defined(HIREDIS_HAPP_ENABLE_LIBUV)
  uv_run(main_loop, UV_RUN_DEFAULT);
#elif defined(HIREDIS_HAPP_ENABLE_LIBEVENT)
  event_base_dispatch(main_loop);
#endif

  THREAD_RETURN;
}

static void on_log_fn(const char *content) { puts(content); }

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("usage: %s <ip> <port> [passowrd]\n", argv[0]);
    return 0;
  }

#ifdef _MSC_VER
  {
    WORD wVersionRequested;
    WSADATA wsaData;

    wVersionRequested = MAKEWORD(2, 2);

    (void)WSAStartup(wVersionRequested, &wsaData);
  }
#endif

  const char *ip = argv[1];
  long lport = 0;
  lport = strtol(argv[2], NULL, 10);
  uint16_t port = static_cast<uint16_t>(lport);

  g_clu.init(ip, port);

  if (argc > 3) {
    g_clu.set_auth_password(argv[3]);
  }

#if defined(HIREDIS_HAPP_ENABLE_LIBUV)
  main_loop = uv_default_loop();
#elif defined(HIREDIS_HAPP_ENABLE_LIBEVENT)
  main_loop = event_init();
#endif

  // set callbacks
  g_clu.set_on_connect(on_connect_cbk);
  g_clu.set_on_connected(on_connected_cbk);
  g_clu.set_on_disconnected(on_disconnected_cbk);
  g_clu.set_timeout(5);  // set timeout

#if defined(HIREDIS_HAPP_ENABLE_LIBUV)
  // setup timer using libuv
  uv_timer_t timer_obj;
  uv_timer_init(main_loop, &timer_obj);
  uv_timer_start(&timer_obj, on_timer_proc, 1000, 100);

#elif defined(HIREDIS_HAPP_ENABLE_LIBEVENT)
  // setup timer using libevent
  struct timeval tv;
  struct event ev;
  event_assign(&ev, main_loop, -1, EV_PERSIST, on_timer_proc, NULL);
  tv.tv_sec = 0;
  tv.tv_usec = 100000;
  evtimer_add(&ev, &tv);
#endif

  // set log writer to write everything to console
  g_clu.set_log_writer(on_log_fn, on_log_fn, 65536);

  g_clu.start();

  sample_thread_t uv_thd;
  THREAD_CREATE(uv_thd, proc_uv_thd, NULL);

  std::string cmd;
  while (std::getline(std::cin, cmd)) {
    if (cmd.empty()) {
      continue;
    }

    // append command
    g_mutex.lock();
    g_cmds.push_back(cmd);
    g_mutex.unlock();

    cmd.clear();
  }

#if defined(HIREDIS_HAPP_ENABLE_LIBUV)
  uv_stop(main_loop);
#elif defined(HIREDIS_HAPP_ENABLE_LIBEVENT)
  event_base_loopbreak(main_loop);
#endif

  THREAD_JOIN(uv_thd);

  g_clu.reset();

  return 0;
}
