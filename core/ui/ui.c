#include "ui.h"

#include "../../logging/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct UIHandle {
    HANDLE thread;
    ProbeContext *context;
    volatile LONG stop;
};

static unsigned long long ElapsedSeconds(const ProbeStats *stats)
{
    time_t now = time(NULL);
    if (now <= stats->started_epoch)
        return 0;
    return (unsigned long long)(now - stats->started_epoch);
}

static size_t WriteProgressLine(const char *line, size_t previous_length)
{
    size_t length = strlen(line);
    fputc('\r', stderr);
    fputs(line, stderr);
    for (size_t i = length; i < previous_length; ++i)
        fputc(' ', stderr);
    fflush(stderr);
    return length > previous_length ? length : previous_length;
}

static DWORD WINAPI UIThread(LPVOID argument)
{
    UIHandle *handle = argument;
    ProbeStats *stats = &handle->context->stats;
    size_t previous_line_length = 0;

    while (InterlockedCompareExchange(&handle->stop, 0, 0) == 0) {
        unsigned claimed = atomic_load(&stats->claimed);
        unsigned checked = atomic_load(&stats->checked);
        unsigned up = atomic_load(&stats->up);
        unsigned down = atomic_load(&stats->down);
        unsigned confirmed = atomic_load(&stats->script_confirmed_hosts);
        unsigned workers = atomic_load(&stats->spawned_threads);
        unsigned long long elapsed = ElapsedSeconds(stats);
        char line[256];
        snprintf(line, sizeof(line),
            "elapsed %02llu:%02llu:%02llu | checked %u | claimed %u | up %u | down %u | confirmed %u | workers %u",
            elapsed / 3600, (elapsed / 60) % 60, elapsed % 60,
            checked, claimed, up, down, confirmed, workers);

        LoggingOutputLock();
        previous_line_length = WriteProgressLine(line, previous_line_length);
        LoggingOutputUnlock();
        Sleep(250);
    }

    unsigned checked = atomic_load(&stats->checked);
    unsigned up = atomic_load(&stats->up);
    unsigned down = atomic_load(&stats->down);
    unsigned confirmed = atomic_load(&stats->script_confirmed_hosts);
    unsigned workers = atomic_load(&stats->spawned_threads);
    unsigned long long elapsed = ElapsedSeconds(stats);
    char line[256];
    snprintf(line, sizeof(line),
        "elapsed %02llu:%02llu:%02llu | checked %u | up %u | down %u | confirmed %u | workers %u",
        elapsed / 3600, (elapsed / 60) % 60, elapsed % 60,
        checked, up, down, confirmed, workers);
    LoggingOutputLock();
    previous_line_length = WriteProgressLine(line, previous_line_length);
    fputc('\n', stderr);
    fflush(stderr);
    LoggingOutputUnlock();
    return 0;
}

UIHandle *UILaunch(ProbeContext *context)
{
    if (context == NULL)
        return NULL;

    UIHandle *handle = calloc(1, sizeof(*handle));
    if (handle == NULL)
        return NULL;

    handle->context = context;
    handle->thread = CreateThread(NULL, 0, UIThread, handle, 0, NULL);
    if (handle->thread == NULL) {
        free(handle);
        return NULL;
    }
    return handle;
}

void UIClose(UIHandle *handle)
{
    if (handle == NULL)
        return;

    InterlockedExchange(&handle->stop, 1);
    WaitForSingleObject(handle->thread, INFINITE);
    CloseHandle(handle->thread);
    free(handle);
}
