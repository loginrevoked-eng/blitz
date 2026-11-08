#ifndef PROBE_H
#define PROBE_H
#include "hashset/hashset.h"
#include "pool/cidr.h"
#include "pool/ip_pool.h"
#include "tui/marquee.h"
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <time.h>
#include <synchapi.h>
#include <windows.h>

typedef enum {
    HOST_STATUS_NO_RESPONSE = 0,
    HOST_STATUS_RESPONSIVE = 2,
    HOST_STATUS_RESPONSIVE_WITH_OPEN_PORT = 3,
} HostStatus;

typedef enum {
    HOST_CHECK_METHOD_UDP = 1u << 0,
    HOST_CHECK_METHOD_TCP_SYN = 1u << 1,
    HOST_CHECK_METHOD_ICMP_ECHO = 1u << 2,
} HostUpCheckMethod;

typedef enum {
    PROBE_FLAGS_NONE = 0,
    PROBE_GRAB_BANNERS = 1u << 0
} ProbeFlags;


typedef struct {
    Ipv4 addr;
    DWORD timeout_ms;
    unsigned int retries;
} HostUpCheckProbeOptions;

/* Owns items; its containing options object releases the buffer. */
typedef struct {
    uint16_t *items;
    size_t len;
} PortList;

typedef struct {
    HostUpCheckProbeOptions probe;
} IcmpOptions;


typedef struct{
    HostUpCheckProbeOptions probe;
    PortList ports;
}UdpOptions;

typedef struct{
    HostUpCheckProbeOptions probe;
    PortList ports;
}TcpOptions;


typedef enum {
    ICMP_RESULT_ECHO_REPLY,
    ICMP_RESULT_TIMEOUT,
    ICMP_RESULT_UNREACHABLE,
    ICMP_RESULT_PROHIBITED,
    ICMP_RESULT_ERROR
} IcmpResult;

typedef enum {
    UDP_RESULT_RESPONSE_RECEIVED,
    UDP_RESULT_TIMEOUT,
    UDP_RESULT_PORT_UNREACHABLE,
    UDP_RESULT_PROHIBITED,
    UDP_RESULT_ERROR
} UdpResult;

typedef enum {
    TCP_RESULT_SYN_ACK_RECEIVED,
    TCP_RESULT_TIMEOUT,
    TCP_RESULT_RESET_RECEIVED,
    TCP_RESULT_PROHIBITED,
    TCP_RESULT_ERROR
} TcpResult;

typedef enum {
    HOST_CHECK_STATUS_SUCCEEDED,
    HOST_CHECK_STATUS_PENDING,
    HOST_CHECK_STATUS_ERROR,
    HOST_CHECK_STATUS_RETRIES_EXHAUSTED,
} HostUpCheckStatus;


typedef struct {
    uint16_t port;
    UdpResult result;
    unsigned int retries;
}UdpResultWrapper;

typedef struct {
    uint16_t port;
    TcpResult result;
    unsigned int retries;
}TcpResultWrapper;

/* Owns results; its containing check releases the buffer. */
typedef struct {
    UdpResultWrapper *results;
    size_t len;
} UdpResultList;

/* Owns results; its containing check releases the buffer. */
typedef struct {
    TcpResultWrapper *results;
    size_t len;
} TcpResultList;



typedef struct{
    HostUpCheckStatus status;
    IcmpResult *iresult;
    UdpResultList *uresult;
    TcpResultList *tresult;
}HostUpCheckResult;

typedef struct {
    HostUpCheckMethod kind;
    IcmpOptions *ioptions;
    UdpOptions *uoptions;
    TcpOptions *toptions;
    HostUpCheckResult result;
} HostUpCheck;

typedef int (*HostUpCheckCallback)(HostUpCheck *check);


typedef struct {
    HANDLE handle;
}HostUpCheckThreadHandle;


typedef struct {
    HostUpCheck  *check; /* Owned by this context. */
    HashSet      *threads;
    SRWLOCK      *lock;
    char         *thread_key;
}HostUpCheckThreadContext;


struct ProbeConfig;
typedef struct ProbeContext ProbeContext;
struct Registry;
typedef struct Registry Registry;

typedef struct {
    HashSet *threads;
}HostupCheckHub;

typedef struct {
    time_t started_epoch;
    atomic_uint started;
    atomic_uint claimed;
    atomic_uint checked;
    atomic_uint responsive;
    atomic_bool ui_running;
    atomic_int signal_number;
} ProbeSummary;

struct ProbeContext {
    HostupCheckHub *hub;
    IpPicker *picker;
    const struct ProbeConfig *config;
    Registry *registry;
    ProbeFlags flags;
    ProbeSummary summary;
    Marquee *marquee;
};

int ProbeRun(HostupCheckHub *hub, IpPicker *picker, const struct ProbeConfig *config,
    ProbeFlags flags, Registry *registry);
HANDLE ProbeSpawnWorker(HostupCheckHub *hub, ProbeContext *context);
uint32_t ProbeSpawnWorkers(HostupCheckHub *hub, ProbeContext *context);
void ProbeWaitWorkers(HostupCheckHub *hub);
char *ProbeOnelinerStats(const ProbeContext *context);
DWORD WINAPI ProbeUiThread(LPVOID argument);

#endif

