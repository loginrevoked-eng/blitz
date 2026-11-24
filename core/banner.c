#include "banner.h"

#include "../logging/logging.h"
#include "../ev/tcp/public.h"

#include <stdatomic.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BANNER_TIMEOUT_MS 5000u
#define BANNER_BUFFER_SIZE 4096u
#define BANNER_HOST_BUCKETS 4096u

typedef struct BannerHost BannerHost;

struct BannerHost {
    Ipv4 ip;
    uint32_t remaining;
    uint32_t pending_receives;
    uint32_t responsive;
    int chatty;
    int submissions_done;
    int track_reachability;
    ProbeStats *stats;
    BannerHost *next;
};

typedef struct {
    Ipv4 ip;
    uint16_t port;
    char address[16];
    ByteArray *response;
    BannerHost *host;
} BannerRequest;

static SRWLOCK requests_lock = SRWLOCK_INIT;
static CONDITION_VARIABLE requests_finished = CONDITION_VARIABLE_INIT;
static unsigned int outstanding_requests;
static atomic_ullong next_task_id;
static uint32_t connect_timeout_ms = BANNER_TIMEOUT_MS;
static uint32_t connect_retries;
static SRWLOCK hosts_lock = SRWLOCK_INIT;
static BannerHost *hosts[BANNER_HOST_BUCKETS];

static size_t BannerHostBucket(Ipv4 ip)
{
    return (size_t)((uint32_t)ip % BANNER_HOST_BUCKETS);
}

static BannerHost *BannerFindHostLocked(Ipv4 ip)
{
    for (BannerHost *host = hosts[BannerHostBucket(ip)]; host != NULL;
        host = host->next) {
        if (host->ip == ip)
            return host;
    }
    return NULL;
}

static void BannerFinishHostLocked(BannerHost **link)
{
    BannerHost *host = *link;
    if (host == NULL || !host->submissions_done || host->remaining != 0 ||
        host->pending_receives != 0)
        return;

    if (host->track_reachability)
        atomic_fetch_add(host->responsive ? &host->stats->up : &host->stats->down, 1);
    *link = host->next;
    free(host);
}

static void BannerHostConnectComplete(BannerHost *host, int responsive,
    int will_receive)
{
    if (host == NULL)
        return;

    AcquireSRWLockExclusive(&hosts_lock);
    if (host->remaining != 0) {
        host->responsive |= responsive != 0;
        --host->remaining;
        if (will_receive)
            ++host->pending_receives;
    }
    BannerHost **link = &hosts[BannerHostBucket(host->ip)];
    while (*link != NULL && *link != host)
        link = &(*link)->next;
    BannerFinishHostLocked(link);
    ReleaseSRWLockExclusive(&hosts_lock);
}

int BannerBeginHost(Ipv4 ip, uint32_t port_count, ProbeStats *stats,
    int track_reachability)
{
    if (stats == NULL)
        return 1;
    if (port_count == 0) {
        if (track_reachability)
            atomic_fetch_add(&stats->down, 1);
        return 1;
    }

    BannerHost *host = calloc(1, sizeof(*host));
    if (host == NULL)
        return 0;
    host->ip = ip;
    host->remaining = port_count;
    host->stats = stats;
    host->track_reachability = track_reachability;

    AcquireSRWLockExclusive(&hosts_lock);
    if (BannerFindHostLocked(ip) != NULL) {
        ReleaseSRWLockExclusive(&hosts_lock);
        free(host);
        return 0;
    }
    size_t bucket = BannerHostBucket(ip);
    host->next = hosts[bucket];
    hosts[bucket] = host;
    ReleaseSRWLockExclusive(&hosts_lock);
    return 1;
}

static void BannerHostReceiveComplete(BannerHost *host, int received_data)
{
    if (host == NULL)
        return;

    AcquireSRWLockExclusive(&hosts_lock);
    if (host->pending_receives != 0) {
        if (received_data && !host->chatty) {
            host->chatty = 1;
            atomic_fetch_add(&host->stats->chatty_hosts, 1);
        }
        --host->pending_receives;
    }
    BannerHost **link = &hosts[BannerHostBucket(host->ip)];
    while (*link != NULL && *link != host)
        link = &(*link)->next;
    BannerFinishHostLocked(link);
    ReleaseSRWLockExclusive(&hosts_lock);
}

static BannerHost *BannerFindHost(Ipv4 ip)
{
    AcquireSRWLockShared(&hosts_lock);
    BannerHost *host = BannerFindHostLocked(ip);
    ReleaseSRWLockShared(&hosts_lock);
    return host;
}

void BannerEndHost(Ipv4 ip)
{
    AcquireSRWLockExclusive(&hosts_lock);
    size_t bucket = BannerHostBucket(ip);
    BannerHost **link = &hosts[bucket];
    while (*link != NULL && (*link)->ip != ip)
        link = &(*link)->next;
    if (*link != NULL) {
        (*link)->submissions_done = 1;
        BannerFinishHostLocked(link);
    }
    ReleaseSRWLockExclusive(&hosts_lock);
}

static ByteArray *BannerBufferNew(uint32_t capacity)
{
    ByteArray *array = calloc(1, sizeof(*array));
    if (array == NULL)
        return NULL;
    array->buffer = malloc(capacity);
    if (array->buffer == NULL) {
        free(array);
        return NULL;
    }
    array->cap = capacity;
    return array;
}

static void BannerBufferDestroy(ByteArray *array)
{
    if (array == NULL)
        return;
    free(array->buffer);
    free(array);
}

static void FinishRequest(BannerRequest *request)
{
    if (request == NULL)
        return;

    BannerBufferDestroy(request->response);
    free(request);

    AcquireSRWLockExclusive(&requests_lock);
    if (outstanding_requests > 0)
        --outstanding_requests;
    if (outstanding_requests == 0)
        WakeAllConditionVariable(&requests_finished);
    ReleaseSRWLockExclusive(&requests_lock);
}

static int MakeTaskId(char *buffer, size_t capacity, const char *kind,
    Ipv4 ip, uint16_t port)
{
    unsigned long long sequence = atomic_fetch_add(&next_task_id, 1);
    int length = snprintf(buffer, capacity, "banner-%s-%08lx-%u-%llu", kind,
        (unsigned long)(uint32_t)ip, (unsigned)port, sequence);
    return length > 0 && (size_t)length < capacity;
}

void BannerConfigureConnect(uint32_t timeout_ms, uint32_t retries)
{
    connect_timeout_ms = timeout_ms;
    connect_retries = retries;
}

BOOL BannerPretty(TcpConnectorTask *task)
{
    if (task == NULL)
        return FALSE;

    BannerRequest *request = task->userdata;
    if (request == NULL)
        return FALSE;

    int received_data = task->status == TCP_CONNECTOR_TASK_COMPLETED &&
        task->WSA_error == ERROR_SUCCESS && request->response != NULL &&
        request->response->len != 0;
    if (received_data) {
        int length = request->response->len > INT_MAX ? INT_MAX :
            (int)request->response->len;
        Info("banner %s:%u: %.*s", request->address,
            (unsigned)request->port, length,
            (const char *)request->response->buffer);
    }

    BannerHostReceiveComplete(request->host, received_data);
    request->host = NULL;
    TcpConnectorCloseSocketById(task->options.sr_opt.socket_id);
    FinishRequest(request);
    task->userdata = NULL;
    return TRUE;
}

BOOL OnConnect(TcpConnectorTask *task)
{
    if (task == NULL)
        return FALSE;

    BannerRequest *request = task->userdata;
    if (request == NULL)
        return FALSE;

    int connected = task->status == TCP_CONNECTOR_TASK_COMPLETED &&
        task->WSA_error == ERROR_SUCCESS;
    BannerHostConnectComplete(request->host,
        connected || task->WSA_error == WSAECONNREFUSED, connected);

    if (!connected) {
        request->host = NULL;
        FinishRequest(request);
        task->userdata = NULL;
        return TRUE;
    }

    char receive_id[96];
    if (!MakeTaskId(receive_id, sizeof(receive_id), "recv", request->ip,
            request->port)) {
        BannerHostReceiveComplete(request->host, 0);
        request->host = NULL;
        TcpConnectorCloseSocketById(task->id);
        FinishRequest(request);
        task->userdata = NULL;
        return FALSE;
    }

    TcpConnectorTaskOption receive = { 0 };
    receive.kind = TCP_CONNECTOR_TASK_RECV;
    receive.sr_opt.kind = TCP_CONNECTOR_TASK_RECV;
    receive.sr_opt.payload = request->response;
    receive.sr_opt.socket_id = task->id;
    receive.sr_opt.timeout_ms = BANNER_TIMEOUT_MS;

    DWORD error = TcpConnectorCreateTask(receive, receive_id, BannerPretty,
        request);
    if (error != ERROR_SUCCESS) {
        BannerHostReceiveComplete(request->host, 0);
        request->host = NULL;
        TcpConnectorCloseSocketById(task->id);
        FinishRequest(request);
        task->userdata = NULL;
        return FALSE;
    }

    task->userdata = NULL;
    return TRUE;
}

void RegisterTask(Ipv4 ip, uint16_t port)
{
    BannerHost *host = BannerFindHost(ip);
    if (port == 0) {
        BannerHostConnectComplete(host, 0, 0);
        return;
    }

    BannerRequest *request = calloc(1, sizeof(*request));
    if (request == NULL) {
        BannerHostConnectComplete(host, 0, 0);
        return;
    }
    request->ip = ip;
    request->port = port;
    request->host = host;
    request->response = BannerBufferNew(BANNER_BUFFER_SIZE);
    char *address = Ipv4Repr(ip);
    if (request->response == NULL || address == NULL) {
        free(address);
        BannerBufferDestroy(request->response);
        free(request);
        BannerHostConnectComplete(host, 0, 0);
        return;
    }
    snprintf(request->address, sizeof(request->address), "%s", address);
    free(address);

    char connect_id[96];
    if (!MakeTaskId(connect_id, sizeof(connect_id), "connect", ip, port)) {
        BannerBufferDestroy(request->response);
        free(request);
        BannerHostConnectComplete(host, 0, 0);
        return;
    }

    AcquireSRWLockExclusive(&requests_lock);
    ++outstanding_requests;
    ReleaseSRWLockExclusive(&requests_lock);

    TcpConnectorTaskOption connect = { 0 };
    connect.kind = TCP_CONNECTOR_TASK_CONNECT;
    connect.conn_opt.addr = (uint32_t)ip;
    connect.conn_opt.port = port;
    connect.conn_opt.timeout_ms = connect_timeout_ms;
    connect.conn_opt.retries = connect_retries;

    DWORD error = TcpConnectorCreateTask(connect, connect_id, OnConnect,
        request);
    if (error != ERROR_SUCCESS) {
        BannerHostConnectComplete(request->host, 0, 0);
        request->host = NULL;
        FinishRequest(request);
    }
}

void BannerWait(void)
{
    AcquireSRWLockExclusive(&requests_lock);
    while (outstanding_requests != 0)
        SleepConditionVariableSRW(&requests_finished, &requests_lock, INFINITE, 0);
    ReleaseSRWLockExclusive(&requests_lock);
}
