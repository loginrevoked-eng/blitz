#ifndef PROBE_H
#define PROBE_H

#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <synchapi.h>
#include <windows.h>
#include "models.h"
#include "config.h"

typedef struct {
    HANDLE *handles;
    uint32_t cap;
    uint32_t len;
} HandleList;

typedef struct {
    atomic_uint started;
    atomic_uint claimed;
    atomic_uint checked;
    atomic_uint up;
    atomic_uint down;
    atomic_uint chatty_hosts;
    atomic_uint spawned_threads;
} ProbeStats;

typedef struct {
    IpPicker *picker;
    SRWLOCK *lock;
    ProbeStats *stats;
    ProbeConfig *config;
    void *lua;
    HANDLE start_event;
} WorkerContext;

typedef struct {
    IpPicker *picker;
    HandleList *workers;
    ProbeConfig config;
    ProbeStats stats;
    SRWLOCK *lock;
    HANDLE start_event;
    int winsock_started;
    int tcp_connector_started;
} ProbeContext;

int ProbeInitialize(int argc, char **argv);
int ProbeRun(ProbeContext *ctx);
int ProbeFinalize(ProbeContext *ctx);

#endif
