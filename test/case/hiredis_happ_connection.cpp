#include <iostream>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <list>

#include "hiredis_happ.h"
#include "frame/test_macros.h"


template<typename T>
struct AutoPtr {
    typedef T value_type;
    T* v;

    AutoPtr(T* v_): v(v_){}
    ~AutoPtr() {
        if (NULL != v) {
            delete v;
        }
    }

    AutoPtr(AutoPtr& other) {
        v = other.v;
        other.v = NULL;
    }

    AutoPtr& operator=(AutoPtr& other) {
        v = other.v;
        other.v = NULL;
        return (*this);
    }

    T* get(){ return v; }
    const T* get() const { return v; }

    T* operator->(){ return v; }
    const T* operator->() const { return v; }

    T& operator*(){ return *v; }
    const T& operator*() const { return *v; }
};

static int happ_connection_basic_f = 0;
static void happ_connection_basic_1(hiredis::happ::cmd_exec* cmd, struct redisAsyncContext*c , void* r, void* pridata) {
    CASE_EXPECT_NE(pridata, NULL);
    CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_CONNECTION, cmd->err);

    hiredis::happ::connection* conn = reinterpret_cast<hiredis::happ::connection*>(pridata);

    CASE_EXPECT_EQ(c, conn->get_context());

    CASE_EXPECT_EQ(r, NULL);

    ++ happ_connection_basic_f;
}

CASE_TEST(happ_connection, basic)
{
    AutoPtr<hiredis::happ::connection> conn1(new hiredis::happ::connection());
    AutoPtr<hiredis::happ::connection> conn2(new hiredis::happ::connection());

    CASE_EXPECT_EQ(conn1->get_sequence() + 1, conn2->get_sequence());

    hiredis::happ::cluster clu;
    hiredis::happ::holder_t h;
    redisAsyncContext vir_context;

    conn1->init(h, "127.0.0.2", 1234);
    conn2->init(h, "127.0.0.3", 1234);

    CASE_EXPECT_TRUE("127.0.0.2" == conn1->get_key().ip);
    CASE_EXPECT_TRUE(1234 == conn1->get_key().port);
    CASE_EXPECT_TRUE("127.0.0.2:1234" == conn1->get_key().name);

    CASE_EXPECT_EQ(hiredis::happ::connection::status::DISCONNECTED, conn1->conn_status);
    hiredis::happ::cmd_exec* cmd = hiredis::happ::cmd_exec::create(h, happ_connection_basic_1, &vir_context);

    int res = conn1->redis_cmd(cmd, NULL);
    CASE_EXPECT_EQ(hiredis::happ::error_code::REDIS_HAPP_CREATE, res);

    conn1->set_connecting(&vir_context);

    CASE_EXPECT_EQ(conn1->get_context(), &vir_context);
    CASE_EXPECT_EQ(hiredis::happ::connection::status::CONNECTING, conn1->conn_status);
    conn1->set_connected();
    CASE_EXPECT_EQ(hiredis::happ::connection::status::CONNECTED, conn1->conn_status);

    CASE_EXPECT_EQ(conn1->get_context(), &vir_context);

    cmd->pri_data = conn2.get();
    conn2->set_connecting(&vir_context);

    happ_connection_basic_f = 1;
    conn2->redis_cmd(cmd, NULL);
    CASE_EXPECT_EQ(1, happ_connection_basic_f);

    conn1->release(false);
    conn2->release(false);

    CASE_EXPECT_EQ(2, happ_connection_basic_f);

    CASE_EXPECT_EQ(conn1->get_context(), NULL);
    CASE_EXPECT_EQ(conn2->get_context(), NULL);
}
