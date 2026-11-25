#include <winsock2.h>

#include "config.h"
#include "probe.h"
#include "cleanup.h"
#include "util.h"
#include "logging.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include "worker.h"
#include "banner.h"
#include "connector.h"

static void ProbeInitCheckArgumentValidity(ProbeContext *ctx, int argc,
    char **argv)
{
    if (argv == NULL || argc < 1 || ctx == NULL)
        Panic("Invalid probe initialization arguments");
}

static int WorkerContext_New(ProbeConfig *config, ProbeStats *stats,
    IpPicker *picker, WorkerContext **out, SRWLOCK *lock)
{
    if (out == NULL || config == NULL || stats == NULL || lock == NULL)
        return 0;

    WorkerContext *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
        return 0;
    ctx->lock = lock;
    ctx->stats = stats;
    ctx->lua = NULL;
    ctx->config = config;
    ctx->picker = picker;
    *out = ctx;
    return 1;
}

static void ProbeInitializeStats(ProbeContext *ctx)
{
    atomic_init(&ctx->stats.started, 0);
    atomic_init(&ctx->stats.claimed, 0);
    atomic_init(&ctx->stats.checked, 0);
    atomic_init(&ctx->stats.up, 0);
    atomic_init(&ctx->stats.down, 0);
    atomic_init(&ctx->stats.chatty_hosts, 0);
    atomic_init(&ctx->stats.spawned_threads, 0);
}

void ProbeInitializeOrPanic(ProbeContext *ctx, int argc, char **argv)
{
    ProbeInitCheckArgumentValidity(ctx, argc, argv);
    if (!ProbeConfig_Parse(&ctx->config, argc, argv))
        Panic("Couldn't initialize configuration for the probe");
    SetLogLevel(ctx->config.loglevel);

    WSADATA winsock_data;
    if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0)
        Panic("Couldn't initialize Winsock");
    ctx->winsock_started = 1;

    uint32_t pending_limit = ctx->config.workers > UINT32_MAX / 2u ?
        UINT32_MAX : ctx->config.workers * 2u;
    DWORD connector_error = TcpConnectorInitEventloop(pending_limit,
        TCP_CONNECTOR_WITH_IO_COMPLETION_WORKER, pending_limit);
    if (connector_error != ERROR_SUCCESS)
        Panic("Couldn't initialize TCP connector (error %lu)",
            (unsigned long)connector_error);
    ctx->tcp_connector_started = 1;
    BannerConfigureConnect(ctx->config.timeout_ms, ctx->config.retries);

    IpPickerFlags picker_flags = IP_PICKER_FLAGS_NONE;
    if (!ctx->config.keep_private_ranges)
        picker_flags |= IP_PICKER_EXCLUDE_PRIVATE_RANGES;
    if (ctx->config.resume != NULL || ctx->config.dump != NULL)
        picker_flags |= IP_PICKER_RESUMABLE;

    ctx->picker = IpPicker_Initialize(ctx->config.cidr,
        ctx->config.bucket_width, ctx->config.cooldown, ctx->config.resume,
        ctx->config.dump, picker_flags, NULL, NULL);
    if (ctx->picker == NULL)
        Panic("Couldn't initialize the IP picker pool");

    ctx->workers = HandleListNew();
    if (ctx->workers == NULL)
        Panic("Couldn't initialize worker handle list");

    ctx->lock = malloc(sizeof(*ctx->lock));
    if (ctx->lock == NULL)
        Panic("Couldn't initialize worker lock");
    InitializeSRWLock(ctx->lock);
    ProbeInitializeStats(ctx);

    ctx->start_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ctx->start_event == NULL)
        Panic("Couldn't create worker start barrier");

    for (uint32_t i = 0; i < ctx->config.workers; ++i) {
        WorkerContext *worker_ctx = NULL;
        if (!WorkerContext_New(&ctx->config, &ctx->stats, ctx->picker,
                &worker_ctx, ctx->lock))
            Panic("Couldn't initialize worker context for worker number %u", i);
        worker_ctx->start_event = ctx->start_event;

        HANDLE worker = CreateThread(NULL, 0, ProbeWorker, worker_ctx, 0, NULL);
        if (worker == NULL) {
            ProbeCleanupWorkerContext(worker_ctx);
            break;
        }
        if (!HandleListPush(ctx->workers, worker)) {
            SetEvent(ctx->start_event);
            WaitForSingleObject(worker, INFINITE);
            CloseHandle(worker);
            Panic("Couldn't store worker handle for worker number %u", i);
        }
        atomic_fetch_add(&ctx->stats.spawned_threads, 1);
    }

    Log("starting %u worker threads\n",
        atomic_load(&ctx->stats.spawned_threads));
    SetEvent(ctx->start_event);
}
