#include "init.h"
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

HostUpCheckProbeOptions *HostUpCheckProbeOptions_New(Ipv4 addr,
    DWORD timeout_ms, unsigned int retries)
{
    HostUpCheckProbeOptions *options = malloc(sizeof(*options));
    if (options != NULL) {
        options->addr = addr;
        options->timeout_ms = timeout_ms;
        options->retries = retries;
    }
    return options;
}

PortList *PortList_New(size_t len)
{
    PortList *ports = calloc(1, sizeof(*ports));
    if (ports == NULL)
        return NULL;
    if (len != 0) {
        ports->items = calloc(len, sizeof(*ports->items));
        if (ports->items == NULL) {
            free(ports);
            return NULL;
        }
    }
    ports->len = len;
    return ports;
}

IcmpOptions *IcmpOptions_New(const HostUpCheckProbeOptions *probe)
{
    if (probe == NULL)
        return NULL;
    IcmpOptions *options = malloc(sizeof(*options));
    if (options != NULL)
        options->probe = *probe;
    return options;
}

static int CopyPorts(PortList *destination, const PortList *source)
{
    destination->items = NULL;
    destination->len = 0;
    if (source == NULL || source->len == 0)
        return 1;

    destination->items = malloc(source->len * sizeof(*destination->items));
    if (destination->items == NULL)
        return 0;
    memcpy(destination->items, source->items, source->len * sizeof(*destination->items));
    destination->len = source->len;
    return 1;
}

UdpOptions *UdpOptions_New(const HostUpCheckProbeOptions *probe, const PortList *ports)
{
    if (probe == NULL)
        return NULL;
    UdpOptions *options = calloc(1, sizeof(*options));
    if (options == NULL)
        return NULL;
    options->probe = *probe;
    if (!CopyPorts(&options->ports, ports)) {
        free(options);
        return NULL;
    }
    return options;
}

TcpOptions *TcpOptions_New(const HostUpCheckProbeOptions *probe, const PortList *ports)
{
    if (probe == NULL)
        return NULL;
    TcpOptions *options = calloc(1, sizeof(*options));
    if (options == NULL)
        return NULL;
    options->probe = *probe;
    if (!CopyPorts(&options->ports, ports)) {
        free(options);
        return NULL;
    }
    return options;
}

UdpResultWrapper *UdpResultWrapper_New(uint16_t port, UdpResult result,
    unsigned int retries)
{
    UdpResultWrapper *wrapper = malloc(sizeof(*wrapper));
    if (wrapper != NULL) {
        wrapper->port = port;
        wrapper->result = result;
        wrapper->retries = retries;
    }
    return wrapper;
}

TcpResultWrapper *TcpResultWrapper_New(uint16_t port, TcpResult result,
    unsigned int retries)
{
    TcpResultWrapper *wrapper = malloc(sizeof(*wrapper));
    if (wrapper != NULL) {
        wrapper->port = port;
        wrapper->result = result;
        wrapper->retries = retries;
    }
    return wrapper;
}

UdpResultList *UdpResultList_New(size_t len)
{
    UdpResultList *results = calloc(1, sizeof(*results));
    if (results == NULL)
        return NULL;
    if (len != 0) {
        results->results = calloc(len, sizeof(*results->results));
        if (results->results == NULL) {
            free(results);
            return NULL;
        }
    }
    results->len = len;
    return results;
}

TcpResultList *TcpResultList_New(size_t len)
{
    TcpResultList *results = calloc(1, sizeof(*results));
    if (results == NULL)
        return NULL;
    if (len != 0) {
        results->results = calloc(len, sizeof(*results->results));
        if (results->results == NULL) {
            free(results);
            return NULL;
        }
    }
    results->len = len;
    return results;
}

HostUpCheck *HostUpCheck_New(HostUpCheckMethod kind, Ipv4 addr,
    DWORD timeout_ms, unsigned int retries, const PortList *ports)
{
    if (kind == 0)
        return NULL;

    HostUpCheckProbeOptions *probe = HostUpCheckProbeOptions_New(addr,
        timeout_ms, retries);
    if (probe == NULL)
        return NULL;

    HostUpCheck *check = calloc(1, sizeof(*check));
    if (check == NULL) {
        free(probe);
        return NULL;
    }

    check->kind = kind;
    check->result.status = HOST_CHECK_STATUS_PENDING;
    if (kind & HOST_CHECK_METHOD_ICMP_ECHO) {
        check->ioptions = IcmpOptions_New(probe);
        check->result.iresult = calloc(1, sizeof(*check->result.iresult));
    }
    if (kind & HOST_CHECK_METHOD_UDP) {
        check->uoptions = UdpOptions_New(probe, ports);
        check->result.uresult = UdpResultList_New(ports != NULL ? ports->len : 0);
    }
    if (kind & HOST_CHECK_METHOD_TCP_SYN) {
        check->toptions = TcpOptions_New(probe, ports);
        check->result.tresult = TcpResultList_New(ports != NULL ? ports->len : 0);
    }
    if (((kind & HOST_CHECK_METHOD_ICMP_ECHO) && (!check->ioptions || !check->result.iresult)) ||
        ((kind & HOST_CHECK_METHOD_UDP) && (!check->uoptions || !check->result.uresult)) ||
        ((kind & HOST_CHECK_METHOD_TCP_SYN) && (!check->toptions || !check->result.tresult))) {
        HostUpCheck_Free(check);
        free(probe);
        return NULL;
    }
    free(probe);
    return check;
}

void HostUpCheck_Free(HostUpCheck *check)
{
    if (check == NULL)
        return;
    if (check->uoptions != NULL) {
        free(check->uoptions->ports.items);
        free(check->uoptions);
    }
    if (check->toptions != NULL) {
        free(check->toptions->ports.items);
        free(check->toptions);
    }
    free(check->ioptions);
    free(check->result.iresult);
    if (check->result.uresult != NULL) {
        free(check->result.uresult->results);
        free(check->result.uresult);
    }
    if (check->result.tresult != NULL) {
        free(check->result.tresult->results);
        free(check->result.tresult);
    }
    free(check);
}

HostUpCheckResult *HostUpCheckResult_New(void)
{
    HostUpCheckResult *result = calloc(1, sizeof(*result));
    if (result != NULL)
        result->status = HOST_CHECK_STATUS_PENDING;
    return result;
}

HostUpCheckThreadHandle *HostUpCheckThreadHandle_New(HANDLE handle)
{
    HostUpCheckThreadHandle *thread = malloc(sizeof(*thread));
    if (thread != NULL)
        thread->handle = handle;
    return thread;
}

HostUpCheckThreadContext *HostUpCheckThreadContext_New(void)
{
    return calloc(1, sizeof(HostUpCheckThreadContext));
}

HostupCheckHub *HostupCheckHub_New(unsigned int workers)
{
    if (workers == 0)
        return NULL;
    HostupCheckHub *hub = calloc(1, sizeof(*hub));
    if (hub == NULL)
        return NULL;
    hub->threads = HashSet_New(workers * 2);
    if (hub->threads == NULL) {
        HashSet_Free(hub->threads);
        free(hub);
        return NULL;
    }
    return hub;
}

int ProbeInit(ProbeContext *state, HostupCheckHub *hub, IpPicker *picker,
    const ProbeConfig *config)
{
    if (hub == NULL)
        return 0;
    if (hub->threads == NULL)
        return 0;
    if (state == NULL || picker == NULL || config == NULL)
        return 0;

    state->hub = hub;
    state->picker = picker;
    state->config = config;
    state->summary.started_epoch = time(NULL);
    atomic_init(&state->summary.started, 0);
    atomic_init(&state->summary.claimed, 0);
    atomic_init(&state->summary.checked, 0);
    atomic_init(&state->summary.responsive, 0);
    atomic_init(&state->summary.ui_running, 0);
    atomic_init(&state->summary.signal_number, 0);
    state->marquee = NULL;
    return 1;
}

void HostupCheckHub_Free(HostupCheckHub *hub)
{
    if (hub == NULL)
        return;
    HashSet_Free(hub->threads);
    free(hub);
}

