#include "spinner.h"
#include "logging/logging.h"
#include "str/str.h"

#include <stdlib.h>
#include <string.h>

struct Spinner {
    StringCycle *frames;
    char *verb;
    HANDLE out;
    HANDLE thread;
    HANDLE stop;
    DWORD interval;
    CRITICAL_SECTION lock;
    int running;
    int ownsFrames;
};

static char *DupStr(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static size_t CopyOut(const char *src, char *dst, size_t cap)
{
    if (!dst || cap == 0) return 0;
    size_t n = src ? strlen(src) : 0;
    if (n >= cap) n = cap - 1;
    if (n) memcpy(dst, src, n);
    dst[n] = '\0';
    return n;
}

static int ValidHandle(HANDLE h)
{
    return h && h != INVALID_HANDLE_VALUE;
}

static int AppendSpaces(char **text, size_t count)
{
    if (count == 0)
        return 1;
    char *spaces = malloc(count + 1);
    if (spaces == NULL)
        return 0;
    memset(spaces, ' ', count);
    spaces[count] = '\0';
    int ok = appendf(text, "%s", spaces) != NULL;
    free(spaces);
    return ok;
}

static DWORD WINAPI SpinnerThread(LPVOID arg)
{
    Spinner *sp = arg;
    char *buf = NULL;
    DWORD written;
    DWORD interval;
    size_t last = 0;

    for (;;) {
        EnterCriticalSection(&sp->lock);
        const char *frame = StringCycleNext(sp->frames, NULL);
        buf = strf("\r%s %s", frame ? frame : "", sp->verb ? sp->verb : "");
        interval = sp->interval;
        LeaveCriticalSection(&sp->lock);

        if (buf == NULL)
            break;
        size_t len = strlen(buf);
        if (len < last + 1 && !AppendSpaces(&buf, last + 1 - len)) {
            free(buf);
            break;
        }

        LoggingOutputLock();
        WriteFile(sp->out, buf, (DWORD)strlen(buf), &written, NULL);
        LoggingOutputUnlock();
        free(buf);
        buf = NULL;
        last = len > 0 ? len - 1 : 0;

        if (WaitForSingleObject(sp->stop, interval) != WAIT_TIMEOUT) break;
    }

    buf = strf("\r");
    if (buf != NULL && AppendSpaces(&buf, last) && appendf(&buf, "\r") != NULL) {
        LoggingOutputLock();
        WriteFile(sp->out, buf, (DWORD)strlen(buf), &written, NULL);
        LoggingOutputUnlock();
    }
    free(buf);
    return 0;
}

Spinner *SpinnerCreate(StringCycle *frames, const char *verb, HANDLE out, DWORD interval)
{
    if (!frames || StringCycleLength(frames) == 0 || !ValidHandle(out)) return NULL;

    Spinner *sp = calloc(1, sizeof *sp);
    if (!sp) return NULL;

    sp->verb = DupStr(verb);
    if (verb && !sp->verb) {
        free(sp);
        return NULL;
    }
    sp->frames = frames;
    sp->out = out;
    sp->interval = interval;
    InitializeCriticalSection(&sp->lock);
    return sp;
}

/* Same as SpinnerCreate, but writes to stderr. */
Spinner *SpinnerCreateStderr(StringCycle *frames, const char *verb, DWORD interval)
{
    return SpinnerCreate(frames, verb, GetStdHandle(STD_ERROR_HANDLE), interval);
}

/* Variadic constructor from a va_list. Builds its own frames cycle from
   `count` const char * arguments; SpinnerDelete frees that cycle. */
Spinner *SpinnerCreateVA(const char *verb, HANDLE out, DWORD interval, size_t count, va_list ap)
{
    if (count == 0 || !ValidHandle(out)) return NULL;

    StringCycle *frames = StringCycleCreateVA(count, ap);
    if (!frames) return NULL;

    Spinner *sp = SpinnerCreate(frames, verb, out, interval);
    if (!sp) {
        StringCycleDelete(frames);
        return NULL;
    }
    sp->ownsFrames = 1;
    return sp;
}

/* Spinner that creates and owns its frames:
   SpinnerCreateV("Loading", out, 100, 4, "|", "/", "-", "\\")
   Frame strings are not copied, so they must outlive the spinner
   (string literals are fine). */
Spinner *SpinnerCreateV(const char *verb, HANDLE out, DWORD interval, size_t count, ...)
{
    va_list ap;
    va_start(ap, count);
    Spinner *sp = SpinnerCreateVA(verb, out, interval, count, ap);
    va_end(ap);
    return sp;
}

/* Same as SpinnerCreateV, but writes to stderr. */
Spinner *SpinnerCreateVStderr(const char *verb, DWORD interval, size_t count, ...)
{
    va_list ap;
    va_start(ap, count);
    Spinner *sp = SpinnerCreateVA(verb, GetStdHandle(STD_ERROR_HANDLE), interval, count, ap);
    va_end(ap);
    return sp;
}

int SpinnerStart(Spinner *sp)
{
    if (!sp) return -1;
    if (sp->running) return 0;

    sp->stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!sp->stop) return -1;

    sp->thread = CreateThread(NULL, 0, SpinnerThread, sp, 0, NULL);
    if (!sp->thread) {
        CloseHandle(sp->stop);
        sp->stop = NULL;
        return -1;
    }
    sp->running = 1;
    return 0;
}

void SpinnerStop(Spinner *sp)
{
    if (!sp || !sp->running) return;

    SetEvent(sp->stop);
    WaitForSingleObject(sp->thread, INFINITE);
    CloseHandle(sp->thread);
    CloseHandle(sp->stop);
    sp->thread = NULL;
    sp->stop = NULL;
    sp->running = 0;
}

int SpinnerIsRunning(const Spinner *sp)
{
    return sp ? sp->running : 0;
}

DWORD SpinnerGetInterval(Spinner *sp)
{
    if (!sp) return 0;
    EnterCriticalSection(&sp->lock);
    DWORD v = sp->interval;
    LeaveCriticalSection(&sp->lock);
    return v;
}

size_t SpinnerGetVerb(Spinner *sp, char *dst, size_t cap)
{
    if (!sp) return 0;
    EnterCriticalSection(&sp->lock);
    size_t n = CopyOut(sp->verb, dst, cap);
    LeaveCriticalSection(&sp->lock);
    return n;
}

int SpinnerSetInterval(Spinner *sp, DWORD interval)
{
    if (!sp) return -1;
    EnterCriticalSection(&sp->lock);
    sp->interval = interval;
    LeaveCriticalSection(&sp->lock);
    return 0;
}

int SpinnerSetVerb(Spinner *sp, const char *verb)
{
    if (!sp) return -1;
    char *copy = DupStr(verb);
    if (verb && !copy) return -1;
    EnterCriticalSection(&sp->lock);
    free(sp->verb);
    sp->verb = copy;
    LeaveCriticalSection(&sp->lock);
    return 0;
}

int SpinnerClearVerb(Spinner *sp)
{
    return SpinnerSetVerb(sp, NULL);
}

int SpinnerSetFrames(Spinner *sp, StringCycle *frames)
{
    if (!sp || !frames || StringCycleLength(frames) == 0) return -1;
    EnterCriticalSection(&sp->lock);
    if (sp->ownsFrames && sp->frames != frames) {
        StringCycleDelete(sp->frames);
        sp->ownsFrames = 0; /* the new cycle belongs to the caller */
    }
    sp->frames = frames;
    LeaveCriticalSection(&sp->lock);
    return 0;
}

void SpinnerDelete(Spinner *sp)
{
    if (!sp) return;
    SpinnerStop(sp);
    DeleteCriticalSection(&sp->lock);
    if (sp->ownsFrames) StringCycleDelete(sp->frames);
    free(sp->verb);
    free(sp);
}

