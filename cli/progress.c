#include "progress.h"
#include "logging/logging.h"
#include "str/str.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct StringCycle {
    const char **strs;
    size_t *lens;
    size_t count;
    size_t index;
};

typedef struct {
    size_t step;
    size_t percent;
    int eta;
} ProgressBarState;

struct ProgressBar {
    StringCycle *frames;
    char *verb;
    size_t steps;
    ULONGLONG started;
    ProgressBarState state;
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

/* ------------------------------------------------------------------ */
/* StringCycle                                                         */
/* ------------------------------------------------------------------ */

StringCycle *StringCycleCreate(const char **src, size_t count)
{
    if (!src || count == 0) return NULL;

    StringCycle *c = malloc(sizeof *c);
    if (!c) return NULL;

    c->strs = malloc(count * sizeof *c->strs);
    c->lens = malloc(count * sizeof *c->lens);
    if (!c->strs || !c->lens) {
        free(c->strs);
        free(c->lens);
        free(c);
        return NULL;
    }

    for (size_t i = 0; i < count; i++) {
        c->strs[i] = src[i] ? src[i] : "";
        c->lens[i] = strlen(c->strs[i]);
    }
    c->count = count;
    c->index = 0;
    return c;
}

/* Variadic constructor from a va_list. */
StringCycle *StringCycleCreateVA(size_t count, va_list ap)
{
    if (count == 0) return NULL;

    const char **tmp = malloc(count * sizeof *tmp);
    if (!tmp) return NULL;

    for (size_t i = 0; i < count; i++)
        tmp[i] = va_arg(ap, const char *);

    /* StringCycleCreate copies the pointers into its own arrays,
       so the temporary array can be freed right away. */
    StringCycle *c = StringCycleCreate(tmp, count);
    free(tmp);
    return c;
}

/* Variadic constructor: StringCycleCreateV(4, "|", "/", "-", "\\")
   The strings are not copied, so they must outlive the cycle
   (string literals are fine). Every variadic argument must be a
   const char * (or char *); pass NULL for an empty frame. */
StringCycle *StringCycleCreateV(size_t count, ...)
{
    va_list ap;
    va_start(ap, count);
    StringCycle *c = StringCycleCreateVA(count, ap);
    va_end(ap);
    return c;
}

const char *StringCycleGet(const StringCycle *c, size_t i, size_t *len)
{
    if (!c || i >= c->count) return NULL;
    if (len) *len = c->lens[i];
    return c->strs[i];
}

size_t StringCycleLength(const StringCycle *c)
{
    return c ? c->count : 0;
}

int StringCycleSet(StringCycle *c, size_t i, const char *s)
{
    if (!c || i >= c->count || !s) return -1;
    c->strs[i] = s;
    c->lens[i] = strlen(s);
    return 0;
}

const char *StringCycleCurrent(const StringCycle *c, size_t *len)
{
    if (!c || c->count == 0) return NULL;
    return StringCycleGet(c, c->index, len);
}

const char *StringCycleNext(StringCycle *c, size_t *len)
{
    if (!c || c->count == 0) return NULL;
    const char *s = c->strs[c->index];
    if (len) *len = c->lens[c->index];
    c->index = (c->index + 1) % c->count;
    return s;
}

void StringCycleReset(StringCycle *c)
{
    if (c) c->index = 0;
}

void StringCycleDelete(StringCycle *c)
{
    if (!c) return;
    free(c->strs);
    free(c->lens);
    free(c);
}

/* ------------------------------------------------------------------ */
/* ProgressBar                                                         */
/* ------------------------------------------------------------------ */

ProgressBar *ProgressBarCreate(StringCycle *frames, const char *verb, size_t steps)
{
    ProgressBar *bar = calloc(1, sizeof *bar);
    if (!bar) return NULL;

    bar->verb = DupStr(verb);
    if (verb && !bar->verb) {
        free(bar);
        return NULL;
    }
    bar->frames = frames;
    bar->steps = steps;
    bar->started = GetTickCount64();
    bar->state.eta = -1;
    return bar;
}

size_t ProgressBarGetStep(const ProgressBar *bar)
{
    return bar ? bar->state.step : 0;
}

size_t ProgressBarGetSteps(const ProgressBar *bar)
{
    return bar ? bar->steps : 0;
}

size_t ProgressBarGetPercent(const ProgressBar *bar)
{
    return bar ? bar->state.percent : 0;
}

int ProgressBarGetEta(const ProgressBar *bar)
{
    return bar ? bar->state.eta : -1;
}

size_t ProgressBarGetVerb(const ProgressBar *bar, char *dst, size_t cap)
{
    if (!bar) return 0;
    return CopyOut(bar->verb, dst, cap);
}

static void ProgressBarRefresh(ProgressBar *bar)
{
    if (bar->state.step > bar->steps) bar->state.step = bar->steps;
    bar->state.percent = bar->steps ? bar->state.step * 100 / bar->steps : 0;

    if (bar->state.step == 0) {
        bar->state.eta = -1;
        return;
    }
    ULONGLONG elapsed = GetTickCount64() - bar->started;
    ULONGLONG remaining = elapsed * (bar->steps - bar->state.step) / bar->state.step;
    bar->state.eta = (int)(remaining / 1000);
}

int ProgressBarSetStep(ProgressBar *bar, size_t step)
{
    if (!bar) return -1;
    bar->state.step = step;
    ProgressBarRefresh(bar);
    return 0;
}

void ProgressBarUpdateSteps(ProgressBar *bar, int delta)
{
    if (!bar) return;
    if (delta < 0) {
        size_t d = (size_t)(-(long long)delta);
        bar->state.step = d > bar->state.step ? 0 : bar->state.step - d;
    } else {
        bar->state.step += (size_t)delta;
    }
    ProgressBarRefresh(bar);
}

void ProgressBarStep(ProgressBar *bar)
{
    ProgressBarUpdateSteps(bar, 1);
}

int ProgressBarSetSteps(ProgressBar *bar, size_t steps)
{
    if (!bar) return -1;
    bar->steps = steps;
    ProgressBarRefresh(bar);
    return 0;
}

int ProgressBarSetVerb(ProgressBar *bar, const char *verb)
{
    if (!bar) return -1;
    char *copy = DupStr(verb);
    if (verb && !copy) return -1;
    free(bar->verb);
    bar->verb = copy;
    return 0;
}

int ProgressBarClearVerb(ProgressBar *bar)
{
    return ProgressBarSetVerb(bar, NULL);
}

void ProgressBarSetFrames(ProgressBar *bar, StringCycle *frames)
{
    if (bar) bar->frames = frames;
}

void ProgressBarReset(ProgressBar *bar)
{
    if (!bar) return;
    bar->state.step = 0;
    bar->state.percent = 0;
    bar->state.eta = -1;
    bar->started = GetTickCount64();
}

char *ProgressBarDraw(ProgressBar *bar)
{
    if (!bar) return NULL;

    ProgressBarRefresh(bar);

    const char *frame = StringCycleNext(bar->frames, NULL);
    if (!frame) frame = "";

    char *line = strf("\r  %s %s [", frame, bar->verb ? bar->verb : "");
    if (!line) return NULL;

    size_t done = bar->state.step;
    if (bar->steps == SIZE_MAX) {
        free(line);
        return NULL;
    }
    char *track = malloc(bar->steps + 1);
    if (track == NULL) {
        free(line);
        return NULL;
    }
    for (size_t i = 0; i < bar->steps; i++)
        track[i] = i < done ? '=' : (i == done ? '>' : '-');
    track[bar->steps] = '\0';

    if (appendf(&line, "%s", track) == NULL ||
        (bar->state.eta >= 0
            ? appendf(&line, "] %zu%% ETA %ds", bar->state.percent, bar->state.eta)
            : appendf(&line, "] %zu%%", bar->state.percent)) == NULL) {
        free(track);
        free(line);
        return NULL;
    }
    free(track);
    return line;
}

int ProgressBarWrite(ProgressBar *bar, HANDLE hndl)
{
    if (!bar || !ValidHandle(hndl)) return -1;

    char *line = ProgressBarDraw(bar);
    if (!line) return -1;

    DWORD written = 0;
    LoggingOutputLock();
    BOOL ok = WriteFile(hndl, line, (DWORD)strlen(line), &written, NULL);
    LoggingOutputUnlock();
    free(line);
    return ok ? 0 : -1;
}

int ProgressBarStepAndWrite(ProgressBar *bar, HANDLE hndl)
{
    if (!bar || !ValidHandle(hndl)) return -1;
    ProgressBarStep(bar);
    return ProgressBarWrite(bar, hndl);
}

/* Write to stderr by default. */
int ProgressBarWriteStderr(ProgressBar *bar)
{
    return ProgressBarWrite(bar, GetStdHandle(STD_ERROR_HANDLE));
}

int ProgressBarStepAndWriteStderr(ProgressBar *bar)
{
    return ProgressBarStepAndWrite(bar, GetStdHandle(STD_ERROR_HANDLE));
}

void ProgressBarDelete(ProgressBar *bar)
{
    if (!bar) return;
    free(bar->verb);
    free(bar);
}

