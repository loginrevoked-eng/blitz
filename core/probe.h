#ifndef PROBE_H
#define PROBE_H
#include "../lua/include/lua.h"
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <time.h>
#include <synchapi.h>
#include <windows.h>
#include "models.h"
#include "config.h"

typedef enum {
    HOST_STATUS_DOWN = 0,
    HOST_STATUS_UP = 2,
    HOST_STATUS_SERVING = 3,
} HostStatus;

typedef enum {
    PROBE_RUN_UPCHECK = 1 << 0,
    PROBE_RUN_BANNER_GRABBERS = 2 << 1,
    PROBE_RUN_SCRIPTS = 3 << 2,
} ProbePipelineOption;



typedef struct{
    HostUpCheckStatus status;
    IcmpResult *iresult;
    UdpResultList *uresult;
    TcpResultList *tresult;
}HostUpCheckResult;

typedef struct {
    HostupCheckOptions options;
    HostUpCheckResult result;
} HostUpCheck;












typedef struct {
    HANDLE *handles;
    uint32_t cap;
    uint32_t len;
}HandleList;






typedef struct {
    atomic_uint ui_running;
    atomic_uint signal_number;
}ProbeStateFlags;

typedef struct {
    time_t           started_epoch;
    atomic_uint      started;
    atomic_uint      claimed;
    atomic_uint      checked;
    atomic_uint      up;
    atomic_uint      down;
    atomic_uint      script_confirmed_hosts;
    atomic_uint      spawned_threads;
    ProbeStateFlags  state;
} ProbeStats;



typedef struct {
    IpPicker *picker;
    HostUpCheck  check;
    SRWLOCK      *lock;
    ProbeStats   *stats;
    ProbeConfig  *config;
    lua_State    *lua;
}WorkerContext;


typedef struct {
    IpPicker *picker;
    HandleList *workers;
    ProbeConfig config;
    ProbeStats stats;
    SRWLOCK   *lock;
    int winsock_started;
}ProbeContext;



int ProbeInitialize(int argc, char **argv);
int ProbeRun(ProbeContext *ctx);
int ProbeFinalize(ProbeContext *ctx);
//all the other functions are static

#endif
