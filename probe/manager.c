#include "probe.h"
#include "icmp.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "logging.h"

typedef struct {
    IcmpConfig *config;
    IcmpResult *result;
} IcmpProbe;

typedef struct {
    Ipv4 target;
    uint64_t picked_index;
    ProbeType type;
    IcmpStatus icmp;
    uint64_t roundtrip;
    int sent;
} ProbeResult;

typedef struct {
    Ipv4 target;
    uint64_t picked_index;
    ProbeType type;
    IcmpProbe icmp;
    struct ProbeManager *manager;
} ProbeTask;

typedef struct {
    ProbeManager *manager;
    IpPicker *picker;
    ProbeStatsPrinter stats_printer;
    void *stats_printer_context;
    ProbeTask *task;
    size_t thread_index;
    FILTER_PROBE_RESULT filter_by;
} ProbeThreadCtx;



void SemaphoreInit(Semaphore *s, size_t initial)
{
    InitializeSRWLock(&s->lock);
    InitializeConditionVariable(&s->cv);
    s->slot = initial;
}

void SemaphoreAcquire(Semaphore *s)
{
    AcquireSRWLockExclusive(&s->lock);
    if(s->slot == 0)
        Warn("Waiting for semaphore to be released..\n");
    while (s->slot == 0)
        SleepConditionVariableSRW(&s->cv, &s->lock, INFINITE, 0);

    s->slot--;
    ReleaseSRWLockExclusive(&s->lock);
}

void SemaphoreRelease(Semaphore *s)
{
    AcquireSRWLockExclusive(&s->lock);
    s->slot++;
    ReleaseSRWLockExclusive(&s->lock);
    WakeConditionVariable(&s->cv);
}


static struct in_addr U64ToInAddr(Ipv4 ip);
static int ThreadsAppend(Threads *threads, HANDLE thread,
                         ProbeThreadCtx *ctx, size_t *index)
{
    if (threads->count == threads->capacity) {
        if (threads->capacity > SIZE_MAX / 2 / sizeof *threads->threads)
            return 0;
        size_t capacity = threads->capacity == 0 ? 8 : threads->capacity * 2;
        HANDLE *grown_threads = malloc(capacity * sizeof *grown_threads);
        void **grown_contexts = malloc(capacity * sizeof *grown_contexts);
        if (grown_threads == NULL || grown_contexts == NULL) {
            free(grown_threads);
            free(grown_contexts);
            return 0;
        }
        if (threads->count > 0) {
            memcpy(grown_threads, threads->threads,
                   threads->count * sizeof *grown_threads);
            memcpy(grown_contexts, threads->contexts,
                   threads->count * sizeof *grown_contexts);
        }
        free(threads->threads);
        free(threads->contexts);
        threads->threads = grown_threads;
        threads->contexts = grown_contexts;
        threads->capacity = capacity;
    }

    *index = threads->count;
    ctx->thread_index = *index;
    threads->threads[threads->count] = thread;
    threads->contexts[threads->count] = ctx;
    threads->count++;
    return 1;
}

static void PrintProbeResult(const ProbeResult *result)
{
    char *target = Ipv4Repr(result->target);
    const char *ip = target != NULL ? target : "(invalid IP)";
    switch (result->icmp)
    {
        case ICMP_UP:
            Log("%llu) %s: UP (%llu ms)\n",
                (unsigned long long)result->picked_index, ip,
                (unsigned long long)result->roundtrip);
            break;
        case ICMP_NO_REPLY:
            Log("%llu) %s: no reply (down or filtering ICMP)\n",
                (unsigned long long)result->picked_index, ip);
            break;
        case ICMP_UNREACHABLE:
            Log("%llu) %s: unreachable\n",
                (unsigned long long)result->picked_index, ip);
            break;
        case ICMP_PROHIBITED:
            Log("%llu) %s: rejected\n",
                (unsigned long long)result->picked_index, ip);
            break;
        default:
            Log("%llu) %s: error\n",
                (unsigned long long)result->picked_index, ip);
            break;
    }
    free(target);
}

static int ThreadListPop(Threads *list, size_t index, HANDLE *out)
{
    if (list == NULL || index >= list->count)
        return 0;

    if (out != NULL)
        *out = list->threads[index];

    ProbeThreadCtx *removed = (ProbeThreadCtx *)list->contexts[index];
    if (removed != NULL)
        removed->thread_index = SIZE_MAX;

    size_t tail = list->count - index - 1;
    if (tail > 0) {
        memmove(&list->threads[index], &list->threads[index + 1],
                tail * sizeof *list->threads);
        memmove(&list->contexts[index], &list->contexts[index + 1],
                tail * sizeof *list->contexts);
    }

    list->count--;
    for (size_t i = index; i < list->count; i++) {
        ProbeThreadCtx *ctx = (ProbeThreadCtx *)list->contexts[i];
        if (ctx != NULL)
            ctx->thread_index = i;
    }
    return 1;
}


static void ProbeThreadDiscard(ProbeThreadCtx *ctx, const ProbeResult *result)
{
    if (ctx == NULL)
        return;

    ProbeManager *manager = ctx->manager;
    HANDLE thread = NULL;
    AcquireSRWLockExclusive(&manager->lock);
    if (ctx->thread_index != SIZE_MAX &&
        ctx->thread_index < manager->active_probes.count) {
        ThreadListPop(&manager->active_probes, ctx->thread_index, &thread);
    }
    ReleaseSRWLockExclusive(&manager->lock);

    if (thread != NULL)
        CloseHandle(thread);

    if (result != NULL && result->sent) {
        AcquireSRWLockExclusive(&manager->lock);
        manager->stats.probed++;
        ReleaseSRWLockExclusive(&manager->lock);

        if (ctx->stats_printer != NULL)
            ctx->stats_printer(ctx->stats_printer_context, ctx->picker);
    }

    free(ctx->task->icmp.result);
    free(ctx->task);
    free(ctx);
}


static DWORD WINAPI Probe(ProbeThreadCtx *ctx)
{
    if (ctx == NULL || ctx->task == NULL)
        return 1;
    ProbeTask *task = ctx->task;
    ProbeManager *manager = task->manager;

    ProbeResult result = {
        .target = task->target,
        .picked_index = task->picked_index,
        .type = task->type,
        .icmp = ICMP_ERROR,
        .roundtrip = 0
    };

    switch (task->type) {
    case PROBE_ICMP:
        if (task->icmp.config != NULL && task->icmp.result != NULL) {
            *task->icmp.result = IcmpDo(task->icmp.config,
                                        U64ToInAddr(task->target), manager);
            result.icmp = task->icmp.result->status;
            result.roundtrip = task->icmp.result->roundtrip;
            result.sent = task->icmp.result->sent;
        }
        break;
    case PROBE_NONE:
    default:
        break;
    }

    AcquireSRWLockExclusive(&manager->lock);
    if (result.icmp == ICMP_UP)
        manager->stats.up++;
    if (result.icmp == ICMP_UP && manager->out_file != INVALID_HANDLE_VALUE) {
        char *ip = Ipv4Repr(result.target);
        DWORD written = 0;
        if (ip == NULL ||
            !WriteFile(manager->out_file, ip, (DWORD)strlen(ip), &written, NULL) ||
            written != strlen(ip)) {
            manager->error = 1;
        } else if (!WriteFile(manager->out_file, "\r\n", 2, &written, NULL) ||
                   written != 2) {
            manager->error = 1;
        }
        free(ip);
    }
    if (ctx->filter_by != FILTER_BY_UP || result.icmp == ICMP_UP)
        PrintProbeResult(&result);
    ReleaseSRWLockExclusive(&manager->lock);

    ProbeThreadDiscard(ctx, &result);
    SemaphoreRelease(&manager->semaphore);

    return 0;
}

static DWORD WINAPI ProbeThreadEntry(LPVOID probe_context)
{
    return Probe((ProbeThreadCtx *)probe_context);
}

static void ProbeManagerJoin(ProbeManager *manager)
{
    if (manager == NULL)
        return;

    for (;;) {
        HANDLE thread = NULL;
        AcquireSRWLockExclusive(&manager->lock);
        if (manager->active_probes.count > 0)
            ThreadListPop(&manager->active_probes, 0, &thread);
        ReleaseSRWLockExclusive(&manager->lock);
        if (thread == NULL)
            break;

        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }
    free(manager->active_probes.threads);
    free(manager->active_probes.contexts);
    manager->active_probes.threads = NULL;
    manager->active_probes.contexts = NULL;
    manager->active_probes.count = 0;
    manager->active_probes.capacity = 0;
}

int ProbeManagerInitialize(ProbeManager *manager, size_t semaphore_count)
{
    if (manager == NULL)
        return 0;

    manager->active_probes = (Threads){0};
    manager->stats = (ProbeStats){0};
    manager->error = 0;
    manager->winsock_initialized = 0;
    manager->out_file = INVALID_HANDLE_VALUE;
    InitializeSRWLock(&manager->lock);
    SemaphoreInit(&manager->semaphore, semaphore_count);

    if (WSAStartup(MAKEWORD(2, 2), &manager->winsock) != 0) {
        return 0;
    }
    manager->winsock_initialized = 1;
    return 1;
}

int ProbeManagerOpenOutput(ProbeManager *manager, const char *path)
{
    if (manager == NULL || path == NULL || path[0] == '\0' ||
        manager->out_file != INVALID_HANDLE_VALUE)
        return 0;
    manager->out_file = CreateFileA(path, GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                    NULL);
    return manager->out_file != INVALID_HANDLE_VALUE;
}

int ProbeManagerAddProbe(ProbeManager *manager, IpPicker *picker, Ipv4 target,
                         uint64_t picked_index, ProbeType type, IcmpConfig *icmp_config,
                         ProbeStatsPrinter stats_printer,
                         void *stats_printer_context)
{
    if (manager == NULL || picker == NULL ||
        (type == PROBE_ICMP && icmp_config == NULL))
        return 0;


    SemaphoreAcquire(&manager->semaphore);
    ProbeThreadCtx *ctx = calloc(1, sizeof *ctx);
    ProbeTask *task = calloc(1, sizeof *task);
    if (ctx == NULL || task == NULL) {
        free(ctx);
        free(task);
        SemaphoreRelease(&manager->semaphore);
        return 0;
    }
    ctx->manager = manager;
    ctx->picker = picker;
    ctx->stats_printer = stats_printer;
    ctx->stats_printer_context = stats_printer_context;
    ctx->task = task;
    ctx->filter_by = manager->filter_by;
    task->target = target;
    task->picked_index = picked_index;
    task->type = type;
    task->manager = manager;
    task->icmp.config = icmp_config;
    if (type == PROBE_ICMP) {
        task->icmp.result = calloc(1, sizeof *task->icmp.result);
        if (task->icmp.result == NULL) {
            free(ctx);
            free(task);
            SemaphoreRelease(&manager->semaphore);
            return 0;
        }
        task->icmp.result->status = ICMP_ERROR;
    }

    AcquireSRWLockExclusive(&manager->lock);
    HANDLE thread = CreateThread(NULL, 0, ProbeThreadEntry, ctx, CREATE_SUSPENDED, NULL);
    if (thread == NULL) {
        ReleaseSRWLockExclusive(&manager->lock);
        free(task->icmp.result);
        free(ctx);
        free(task);
        SemaphoreRelease(&manager->semaphore);
        return 0;
    }

    int tracked = ThreadsAppend(&manager->active_probes, thread, ctx,
                                &ctx->thread_index);
    if (!tracked) {
        manager->error = 1;
    } else if (ResumeThread(thread) == (DWORD)-1) {
        ThreadListPop(&manager->active_probes, ctx->thread_index, NULL);
        manager->error = 1;
        tracked = 0;
    }
    ReleaseSRWLockExclusive(&manager->lock);
    if (!tracked) {
        TerminateThread(thread, 1);
        CloseHandle(thread);
        free(task->icmp.result);
        free(task);
        free(ctx);
        SemaphoreRelease(&manager->semaphore);
        return 0;
    }

    return 1;
}

void ProbeManagerFinalize(ProbeManager *manager)
{
    if (manager == NULL)
        return;

    ProbeManagerJoin(manager);
    if (manager->out_file != INVALID_HANDLE_VALUE) {
        CloseHandle(manager->out_file);
        manager->out_file = INVALID_HANDLE_VALUE;
    }
    if (manager->winsock_initialized) {
        WSACleanup();
        manager->winsock_initialized = 0;
    }
}

static struct in_addr U64ToInAddr(Ipv4 ip)
{
    struct in_addr addr;
    addr.S_un.S_addr = htonl((unsigned long)ip);
    return addr;
}

