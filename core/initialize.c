#include <winsock2.h>

#include "config.h"
#include "models.h"
#include "probe.h"
#include "cleanup.h"
#include "util.h"
#include "../logging/logging.h"
#include "../utils/fs.h"
#include <processthreadsapi.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <synchapi.h>
#include "../lua/include/lauxlib.h"
#include <time.h>
#include "worker.h"


static void ProbeInitCheckArgumentValidity(ProbeContext *ctx, int argc, char**argv)
{
    if(!argv)
    {
        Panic("provided invalid argument vector char** which is NULL");
    }
    if(argc < 1)
    {
        Panic("Provided Invalid argument count");
    }

    if(!ctx)
    {
        Panic("provided invalid context pointer to initialize into...");
    }
}

static void EnsureScript(ProbeConfig *config)
{
    if (config == NULL)
    {
        Panic("Cannot ensure a script for a null probe config");
    }

    const char *path = config->script != NULL ? config->script : "main.lua";
    char *contents = Fs_ReadFile(path, NULL);
    if (contents == NULL)
    {
        Panic("Couldn't read Lua script '%s' (filesystem error %d)",
            path, (int)Fs_LastError());
    }
    ProbeCleanupString(contents);
}





static int WorkerContext_New(ProbeConfig *config, ProbeStats *stats, IpPicker *picker, WorkerContext **out, SRWLOCK *lock)
{
    if (!out || !config || !stats|| !lock)
    {
        return 0;
    }

    WorkerContext *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
    {
        return 0;
    }

    ctx->check.options = config->options;
    ctx->check.result.status = HOST_CHECK_STATUS_PENDING;
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
    ctx->stats.started_epoch = time(NULL);
    atomic_init(&ctx->stats.started, 0);
    atomic_init(&ctx->stats.claimed, 0);
    atomic_init(&ctx->stats.checked, 0);
    atomic_init(&ctx->stats.up, 0);
    atomic_init(&ctx->stats.down, 0);
    atomic_init(&ctx->stats.script_confirmed_hosts, 0);
    atomic_init(&ctx->stats.spawned_threads, 0);
    atomic_init(&ctx->stats.state.ui_running, 0);
    atomic_init(&ctx->stats.state.signal_number, 0);
}

void ProbeInitializeOrPanic(ProbeContext *ctx, int argc, char **argv)
{
    ProbeInitCheckArgumentValidity(ctx, argc, argv);


    if (!ProbeConfig_Parse(&ctx->config, argc, argv))
    {
        Panic("Couldn't initialize configuration for the probe");
    }
    SetLogLevel(ctx->config.loglevel);
    EnsureScript(&ctx->config);

    WSADATA winsock_data;
    if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0)
    {
        Panic("Couldn't initialize Winsock");
    }
    ctx->winsock_started = 1;


    IpPickerFlags picker_flags = IP_PICKER_FLAGS_NONE;
    if (!ctx->config.keep_private_ranges)
    {
        picker_flags |= IP_PICKER_EXCLUDE_PRIVATE_RANGES;
    }
    if (ctx->config.resume != NULL || ctx->config.dump != NULL)
    {
        picker_flags |= IP_PICKER_RESUMABLE;
    }

    ProbeConfig config = ctx->config;
    ctx->picker = IpPicker_Initialize(config.cidr, config.bucket_width, config.cooldown, config.resume, config.dump, picker_flags, NULL, NULL);
    if (!ctx->picker)
    {
        Panic("Couldn't initialize the ip picker pool");
    }

    ctx->workers = HandleListNew();
    if (!ctx->workers)
    {
        Panic("Couldn't initialize worker handle list");
    }


    ctx->lock = malloc(sizeof(*ctx->lock));
    if (!ctx->lock)
    {
        Panic("Couldn't initialize worker lock");
    }

    InitializeSRWLock(ctx->lock);
    ProbeInitializeStats(ctx);


    for (uint32_t i = 0; i < ctx->config.workers; ++i)
    {
        WorkerContext *worker_ctx = NULL;
        if (!WorkerContext_New(&ctx->config, &ctx->stats, ctx->picker, &worker_ctx, ctx->lock))
        {
            Panic("Couldn't initialize worker context for worker number %d", i);
        }

        worker_ctx->lua = luaL_newstate();

        HANDLE worker = CreateThread(NULL, 0, ProbeWorker, worker_ctx, 0, NULL);
        if (!worker)
        {
            ProbeCleanupWorkerContext(worker_ctx);
            break;
        }
        if (!HandleListPush(ctx->workers, worker))
        {
            WaitForSingleObject(worker, INFINITE);
            CloseHandle(worker);
            Panic("Couldn't store worker handle for worker number %d", i);
        }
        atomic_fetch_add(&ctx->stats.spawned_threads, 1);
    }
}
