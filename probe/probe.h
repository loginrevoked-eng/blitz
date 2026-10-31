#ifndef PROBE_PROBE_H
#define PROBE_PROBE_H

#include <stddef.h>
#include <stdint.h>

#include <winsock2.h>
#include <windows.h>

#include "cidr.h"
#include "pool/picker.h"

#define ICMP_DEFAULT_TIMEOUT 500
#define ICMP_DEFAULT_PAYLOAD "abcdefghijklmnopqrstuvwabcdefghi"

typedef enum {
    ICMP_UP,
    ICMP_NO_REPLY,
    ICMP_UNREACHABLE,
    ICMP_PROHIBITED,
    ICMP_ERROR
} IcmpStatus;

typedef enum {
    PROBE_NONE,
    PROBE_ICMP
} ProbeType;

typedef enum FILTER_PROBE_RESULT {
    FILTER_BY_NONE,
    FILTER_BY_UP
} FILTER_PROBE_RESULT;

typedef struct {
    uint32_t timeout;
    char *payload;
    size_t reply_size;
} IcmpConfig;

typedef struct {
    IcmpStatus status;
    uint64_t roundtrip;
    int sent;
} IcmpResult;

typedef struct {
    HANDLE *threads;
    void **contexts;
    size_t count;
    size_t capacity;
} Threads;

typedef struct {
    uint64_t probed;
    uint64_t up;
    uint64_t initial_probe_time;
} ProbeStats;

typedef void (*ProbeStatsPrinter)(void *context, IpPicker *picker);

typedef struct {
    SRWLOCK lock;
    CONDITION_VARIABLE cv;
    size_t slot;           /* free slots */
} Semaphore;

typedef struct ProbeManager {
    SRWLOCK lock;
    Threads active_probes;
    ProbeStats stats;
    HANDLE out_file;
    WSADATA winsock;
    int winsock_initialized;
    int error;
    Semaphore semaphore;
    FILTER_PROBE_RESULT filter_by;
} ProbeManager;

IcmpConfig IcmpConfig_New(void);
int ProbeManagerInitialize(ProbeManager *manager, size_t semaphore_count);
int ProbeManagerOpenOutput(ProbeManager *manager, const char *path);
int ProbeManagerAddProbe(ProbeManager *manager, IpPicker *picker, Ipv4 target,
                         uint64_t picked_index, ProbeType type, IcmpConfig *icmp_config,
                         ProbeStatsPrinter stats_printer,
                         void *stats_printer_context);
void ProbeManagerFinalize(ProbeManager *manager);

#endif
