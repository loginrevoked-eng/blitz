#include "probe.h"
#include "banners.h"
#include "config.h"
#include "init.h"
#include "io/signal.h"
#include "repr.h"
#include "icmp.h"
#include "picker.h"
#include "tcp.h"
#include "udp.h"
#include "hashset/hashset.h"
#include "logging/logging.h"
#include "str/str.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HostUpCheck *CheckInit(ProbeContext *state, Ipv4 address)
{
    return HostUpCheck_New(state->config->method, address, state->config->timeout_ms, state->config->retries, &state->config->ports);
}

static void CheckHost(HostUpCheck *check, HostUpCheckCallback callback)
{
    if (check == NULL)
        return;
    int responsive = 0;

    if (check->ioptions != NULL && check->result.iresult != NULL)
        *check->result.iresult = ProbeIcmp(&check->ioptions->probe);
    if (check->result.iresult != NULL)
        responsive |= *check->result.iresult == ICMP_RESULT_ECHO_REPLY;

    if (check->toptions != NULL && check->result.tresult != NULL) {
        for (size_t i = 0; i < check->toptions->ports.len; ++i) {
            uint16_t port = check->toptions->ports.items[i];
            TcpResult result = ProbeTcp(&check->toptions->probe, port);
            check->result.tresult->results[i] = (TcpResultWrapper){ port, result, 0 };
            responsive |= result == TCP_RESULT_SYN_ACK_RECEIVED;
        }
    }

    if (check->uoptions != NULL && check->result.uresult != NULL) {
        for (size_t i = 0; i < check->uoptions->ports.len; ++i) {
            uint16_t port = check->uoptions->ports.items[i];
            UdpResult result = ProbeUdp(&check->uoptions->probe, port);
            check->result.uresult->results[i] = (UdpResultWrapper){ port, result, 0 };
            responsive |= result == UDP_RESULT_RESPONSE_RECEIVED;
        }
    }

    check->result.status = responsive ? HOST_CHECK_STATUS_SUCCEEDED : HOST_CHECK_STATUS_RETRIES_EXHAUSTED;
    if (check->result.status == HOST_CHECK_STATUS_SUCCEEDED && callback != NULL)
        callback(check);
}

static void CheckSummarize(ProbeContext *context, HostUpCheck *check)
{
    if (check == NULL || check->result.status != HOST_CHECK_STATUS_SUCCEEDED)
        return;

    char *repr = HostupCheckResultRepr(check);
    if (repr == NULL)
        return;

    atomic_fetch_add(&context->summary.responsive, 1);
    Log("UP %s\n", repr);
    free(repr);
}

static void CheckFinish(ProbeContext *state, HostUpCheck *check)
{
    if (check == NULL)
        return;
    atomic_fetch_add(&state->summary.checked, 1);
    CheckSummarize(state, check);
    HostUpCheck_Free(check);
}

static DWORD WINAPI ProbeWorker(LPVOID argument)
{
    ProbeContext *state = argument;
    Ipv4 address;
    while (atomic_fetch_add(&state->summary.claimed, 1) < state->config->limit && IpPicker_Pick(state->picker, &address, NULL))
    {
        HostUpCheck *check = CheckInit(state, address);
        CheckHost(check, (state->flags & PROBE_GRAB_BANNERS) != 0
            ? GrabBanners : NULL);
        CheckFinish(state, check);
    }
    return 0;
}




HANDLE ProbeSpawnWorker(HostupCheckHub *hub, ProbeContext *context)
{
    if (hub == NULL || hub->threads == NULL || context == NULL)
        return NULL;

    HANDLE handle = CreateThread(NULL, 0, ProbeWorker, context, 0, NULL);
    if (handle == NULL)
        return NULL;
    if (!HashSet_PutU64(hub->threads, (uint64_t)(uintptr_t)handle)) {
        WaitForSingleObject(handle, INFINITE);
        CloseHandle(handle);
        return NULL;
    }
    return handle;
}

uint32_t ProbeSpawnWorkers(HostupCheckHub *hub, ProbeContext *context)
{
    if (hub == NULL || context == NULL || context->config == NULL)
        return 0;

    uint32_t target = context->config->concurrency;
    while (atomic_load(&context->summary.started) < target) {
        if (ProbeSpawnWorker(hub, context) == NULL)
            break;
        atomic_fetch_add(&context->summary.started, 1);
    }
    return atomic_load(&context->summary.started);
}

static void WaitAndCloseWorker(uint64_t handle_value, void *context)
{
    (void)context;
    HANDLE handle = (HANDLE)(uintptr_t)handle_value;
    WaitForSingleObject(handle, INFINITE);
    CloseHandle(handle);
}

void ProbeWaitWorkers(HostupCheckHub *hub)
{
    if (hub != NULL && hub->threads != NULL) HashSet_ForEachU64(hub->threads, WaitAndCloseWorker, NULL);
}

char *ProbeOnelinerStats(const ProbeContext *context)
{
    if (context == NULL)
        return NULL;

    time_t elapsed = time(NULL) - context->summary.started_epoch;
    if (elapsed < 0)
        elapsed = 0;
    unsigned long long hours = (unsigned long long)elapsed / 3600;
    unsigned long long minutes = ((unsigned long long)elapsed / 60) % 60;
    unsigned long long seconds = (unsigned long long)elapsed % 60;

    return strf("%02llu:%02llu:%02llu | Workers %u | checked %u | responsive %u",
        hours, minutes, seconds,
        atomic_load(&context->summary.started),
        atomic_load(&context->summary.checked),
        atomic_load(&context->summary.responsive));
}

DWORD WINAPI ProbeUiThread(LPVOID argument)
{
    ProbeContext *context = argument;
    while (atomic_load(&context->summary.ui_running)) {
        ProbeSignalDispatch();
        char *summary = ProbeOnelinerStats(context);
        if (summary != NULL) {
            MarqueeUpdateVerb(context->marquee, summary);
            free(summary);
        }
        Sleep(200);
    }
    return 0;
}

static void ProbeSummarize(const ProbeContext *state)
{
    int signal_number = atomic_load(&state->summary.signal_number);
    if (signal_number != 0) {
        Log("Stopped by signal %d; started %u workers; checked %u hosts; %u responsive.\n",
            signal_number,
            atomic_load(&state->summary.started),
            atomic_load(&state->summary.checked),
            atomic_load(&state->summary.responsive));
    } else {
        Log("Started %u workers; checked %u hosts; %u responsive.\n",
            atomic_load(&state->summary.started),
            atomic_load(&state->summary.checked),
            atomic_load(&state->summary.responsive));
    }
}

static void ProbeSummarySignalWrapper(int signal_number, void *argument)
{
    ProbeContext *context = argument;
    atomic_store(&context->summary.signal_number, signal_number);
    IpPicker_RequestStop(context->picker);
}

int ProbeRun(HostupCheckHub *hub, IpPicker *picker, const ProbeConfig *config,
    ProbeFlags flags)
{
    ProbeContext state;
    if(!ProbeInit(&state, hub, picker, config)) return 0;
    state.flags = flags;

    state.marquee = MarqueeCreateStderr("Preparing checks", 64, '#', 100);
    if (state.marquee == NULL || MarqueeStart(state.marquee) != 0) {
        MarqueeDelete(state.marquee);
        return 2;
    }
    if (!ProbeSignalHook(ProbeSummarySignalWrapper, &state)) {
        MarqueeStop(state.marquee);
        MarqueeDelete(state.marquee);
        return 2;
    }
    atomic_store(&state.summary.ui_running, 1);
    HANDLE ui_thread = CreateThread(NULL, 0, ProbeUiThread, &state, 0, NULL);
    if (ui_thread == NULL) {
        atomic_store(&state.summary.ui_running, 0);
        ProbeSignalUnhook();
        MarqueeStop(state.marquee);
        MarqueeDelete(state.marquee);
        return 2;
    }

    ProbeSpawnWorkers(hub, &state);
    ProbeWaitWorkers(hub);
    ProbeSignalDispatch();
    atomic_store(&state.summary.ui_running, 0);
    WaitForSingleObject(ui_thread, INFINITE);
    CloseHandle(ui_thread);
    ProbeSignalUnhook();
    MarqueeStop(state.marquee);
    ProbeSummarize(&state);
    MarqueeDelete(state.marquee);
    return 0;
}
