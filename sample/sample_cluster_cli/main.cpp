//
// Created by 欧文韬 on 2015/7/5.
//

#include <assert.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <list>
#include <ctime>
#include <algorithm>

#include "hiredis_happ.h"

#include "hiredis/adapters/libuv.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
    #include <winsock2.h>
    #include <process.h>
    #include <sys/locking.h>
    #include <Windows.h>
    #define THREAD_SPIN_COUNT 2000

    typedef HANDLE thread_t;
    #define THREAD_FUNC unsigned __stdcall
    #define THREAD_CREATE(threadvar, fn, arg) do { \
        uintptr_t threadhandle = _beginthreadex(NULL,0,fn,(arg),0,NULL); \
        (threadvar) = (thread_t) threadhandle; \
    } while (0)
    #define THREAD_JOIN(th) WaitForSingleObject(th, INFINITE)
    #define THREAD_RETURN return (0)

    #define THREAD_SLEEP_MS(TM) Sleep(TM)
#elif defined(__GNUC__) || defined(__clang__)
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

typedef pthread_t thread_t;
#define THREAD_FUNC void*
#define THREAD_CREATE(threadvar, fn, arg) \
        pthread_create(&(threadvar), NULL, fn, arg)
#define THREAD_JOIN(th) pthread_join(th, NULL)
#define THREAD_RETURN pthread_exit (NULL); return NULL

#define THREAD_SLEEP_MS(TM) ((TM > 1000)? sleep(TM / 1000): usleep(0)); usleep((TM % 1000) * 1000)
#endif

#ifdef __cplusplus
}
#endif



static hiredis::happ::cluster g_clu;
static uv_loop_t* main_loop;
static pthread_mutex_t g_mutex;
static std::list<std::string> g_cmds;

static void on_connect_cbk(hiredis::happ::cluster*, hiredis::happ::connection* conn) {
    redisLibuvAttach(conn->get_context(), main_loop);
}

static std::string pick_word(const std::string& cmd_line, int i) {
    if (i < 0) {
        return "";
    }

    std::stringstream ss;
    ss.str(cmd_line);

    std::string ret;
    while(ss>> ret && i-- > 0);
    return ret;
}

static std::string dump_callback(hiredis::happ::cmd_exec* cmd, struct redisAsyncContext*, void* r, void* p) {
    assert(p == dump_callback);
    g_clu.dump(std::cout, reinterpret_cast<redisReply*>(r), 0);
}

static void on_timer_proc(uv_timer_t* handle) {
    static time_t sec = time(NULL);
    static time_t usec = 0;

    usec += 100000;
    if (usec > 10000000) {
        sec += usec / 10000000;
        usec %= 10000000;
    }

    puts("timeout");

    // 提取命令
    pthread_mutex_lock(&g_mutex);
    std::list<std::string> pending_cmds;
    if(!g_cmds.empty()) {
        pending_cmds.swap(g_cmds);
    }
    pthread_mutex_unlock(&g_mutex);

    // 执行命令
    while(!pending_cmds.empty()) {
        std::string cmd_line = pending_cmds.front();
        pending_cmds.pop_front();

        std::string cmd = pick_word(cmd_line, 0);
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
        hiredis::happ::cmd_exec::callback_fn_t cbk = dump_callback;
        int k = 1;
        if ("INFO" == cmd || "MULTI" == cmd || "EXEC" == cmd || "SLAVEOF" == cmd || "CONFIG" == cmd || "SHUTDOWN" == cmd ||
            "CLUSTER" == cmd) {
            k = -1;
        } else if ("SCRIPT" == cmd) {
            k = 2;
        } else if ("EVALSHA" == cmd || "EVAL" == cmd) {
            k = 3;
        } else if ("SUBSCRIBE" == cmd || "UNSUBSCRIBE" == cmd) {
            cbk = NULL;
        }
        std::string cmd_key = pick_word(cmd_line, k);
        if (cmd_key.empty()) {
            g_clu.exec(NULL, 0, cbk, cbk, cmd.c_str());
        } else {
            g_clu.exec(cmd_key.c_str(), cmd_key.size(), cbk, cbk, cmd.c_str());
        }
    }

    g_clu.proc(sec, usec);
}

static THREAD_FUNC proc_uv_thd(void *) {
    uv_run(main_loop, UV_RUN_DEFAULT);

    THREAD_RETURN;
}

static void on_log_fn(const char* content) {
    puts(content);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("usage: %s <ip> <port>", argv[0]);
        return 0;
    }


    const char* ip = argv[1];
    long lport = 0;
    lport = strtol(argv[2], NULL, 10);
    uint16_t port = static_cast<uint16_t>(lport);

    g_clu.init(ip, port);

    main_loop = uv_default_loop();
    // 事件分发器
    g_clu.set_on_connect(on_connect_cbk);

    // 设置定时器
    uv_timer_t timer_obj;
    uv_timer_init(main_loop, &timer_obj);
    uv_timer_start(&timer_obj, on_timer_proc, 1000, 100);


    // 设置日志
    g_clu.set_log_writer(on_log_fn, on_log_fn, 65536);

    g_clu.start();

    thread_t uv_thd;
    pthread_mutex_init(&g_mutex, NULL);
    THREAD_CREATE(uv_thd, proc_uv_thd, NULL);

    std::string cmd;
    while(std::getline(std::cin, cmd)) {
        if (cmd.empty()) {
            continue;
        }

        // 追加命令
        pthread_mutex_lock(&g_mutex);
        g_cmds.push_back(cmd);
        pthread_mutex_unlock(&g_mutex);

        cmd.clear();
    }

    uv_stop(main_loop);
    THREAD_JOIN(uv_thd);
    pthread_mutex_destroy(&g_mutex);

    g_clu.reset();

    return 0;
}
