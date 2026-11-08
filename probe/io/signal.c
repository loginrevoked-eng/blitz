#include "signal.h"

#include <signal.h>
#include <stddef.h>

#define PROBE_SIGNAL_CAPACITY 4

typedef struct {
    int number;
    void (*previous_handler)(int);
} ProbeRegisteredSignal;

static volatile sig_atomic_t pending_signal = 0;
static ProbeSignalCallback callback = NULL;
static void *callback_context = NULL;
static ProbeRegisteredSignal registered_signals[PROBE_SIGNAL_CAPACITY];
static size_t registered_signal_count = 0;

static void RecordSignal(int signal_number)
{
    if (pending_signal == 0)
        pending_signal = signal_number;
}

static int RegisterSignal(int signal_number)
{
    if (registered_signal_count >= PROBE_SIGNAL_CAPACITY)
        return 0;

    void (*previous_handler)(int) = signal(signal_number, RecordSignal);
    if (previous_handler == SIG_ERR)
        return 0;

    registered_signals[registered_signal_count++] = (ProbeRegisteredSignal){
        .number = signal_number,
        .previous_handler = previous_handler
    };
    return 1;
}

int ProbeSignalHook(ProbeSignalCallback handler, void *context)
{
    if (handler == NULL)
        return 0;

    ProbeSignalUnhook();
    pending_signal = 0;
    callback = handler;
    callback_context = context;

    if (!RegisterSignal(SIGINT) || !RegisterSignal(SIGTERM))
        goto error;
#ifdef SIGBREAK
    if (!RegisterSignal(SIGBREAK))
        goto error;
#endif
#ifdef SIGHUP
    if (!RegisterSignal(SIGHUP))
        goto error;
#endif
    return 1;

error:
    ProbeSignalUnhook();
    return 0;
}

int ProbeSignalDispatch(void)
{
    int signal_number = (int)pending_signal;
    if (signal_number == 0)
        return 0;

    pending_signal = 0;
    if (callback != NULL)
        callback(signal_number, callback_context);
    return signal_number;
}

void ProbeSignalUnhook(void)
{
    while (registered_signal_count != 0) {
        ProbeRegisteredSignal registered =
            registered_signals[--registered_signal_count];
        signal(registered.number, registered.previous_handler);
    }
    callback = NULL;
    callback_context = NULL;
    pending_signal = 0;
}
