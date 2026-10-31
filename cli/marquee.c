#include "marquee.h"
#include "logging/logging.h"
#include "str/str.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

#define MARQUEE_BAND 5
#define MARQUEE_TRACK 238

struct Marquee {
    char *verb;
    char character;
    HANDLE out;
    HANDLE thread;
    HANDLE stop;
    DWORD interval;
    size_t width;
    CRITICAL_SECTION lock;
    int running;
    uint64_t progress_current;
    uint64_t progress_total;
};

static const int MarqueeShades[MARQUEE_BAND] = { 255, 251, 247, 244, 241 };

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

static void EnableVT(HANDLE out)
{
    DWORD mode;
    if (GetConsoleMode(out, &mode))
        SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

static int ValidHandle(HANDLE h)
{
    return h && h != INVALID_HANDLE_VALUE;
}

static DWORD WINAPI MarqueeThread(LPVOID arg)
{
    Marquee *m = arg;
    static const char hide_cursor[] = "\x1b[?25l";
    static const char show_cursor[] = "\x1b[?25h";
    size_t pos = 0;
    size_t last = 0;
    DWORD written;
    DWORD interval;

    LoggingOutputLock();
    WriteFile(m->out, hide_cursor, (DWORD)(sizeof hide_cursor - 1), &written, NULL);
    LoggingOutputUnlock();

    for (;;) {
        EnterCriticalSection(&m->lock);
        size_t width = m->width;
        size_t vlen = m->verb ? strlen(m->verb) : 0;
        interval = m->interval;

        size_t completed = 0;
        if (m->progress_total != 0) {
            if (m->progress_current >= m->progress_total)
                completed = width;
            else
                completed = (size_t)(((long double)m->progress_current /
                                      (long double)m->progress_total) * width);
        }
        size_t remaining = width - completed;
        size_t period = remaining + MARQUEE_BAND;
        pos %= period;

        char *buf = strf("\r%s [", m->verb ? m->verb : "");
        for (size_t i = 0; i < width; i++) {
            int shade;
            if (i < completed) {
                shade = 46;
            } else {
                size_t sweep_index = i - completed;
                shade = MARQUEE_TRACK;
                if (pos >= sweep_index && pos - sweep_index < MARQUEE_BAND)
                    shade = MarqueeShades[pos - sweep_index];
            }
            if (buf == NULL || appendf(&buf, "\x1b[38;5;%dm%c",
                                       shade, m->character) == NULL) {
                free(buf);
                buf = NULL;
                break;
            }
        }
        if (buf != NULL && appendf(&buf, "\x1b[0m]") == NULL) {
            free(buf);
            buf = NULL;
        }
        LeaveCriticalSection(&m->lock);

        if (buf == NULL)
            break;

        size_t visible = vlen + width + 3;
        size_t pad = last > visible ? last - visible : 0;
        if (pad != 0) {
            char *spaces = malloc(pad + 1);
            if (spaces == NULL) {
                free(buf);
                break;
            }
            memset(spaces, ' ', pad);
            spaces[pad] = '\0';
            if (appendf(&buf, "%s", spaces) == NULL) {
                free(spaces);
                free(buf);
                break;
            }
            free(spaces);
        }

        LoggingOutputLock();
        WriteFile(m->out, buf, (DWORD)strlen(buf), &written, NULL);
        LoggingOutputUnlock();
        free(buf);
        last = visible;
        pos = (pos + 1) % period;

        if (WaitForSingleObject(m->stop, interval) != WAIT_TIMEOUT) break;
    }

    if (last != 0) {
        char *buf = malloc(last + 2);
        if (buf != NULL) {
            buf[0] = '\r';
            memset(buf + 1, ' ', last);
            buf[last + 1] = '\r';
            LoggingOutputLock();
            WriteFile(m->out, buf, (DWORD)(last + 2), &written, NULL);
            LoggingOutputUnlock();
            free(buf);
        }
    }

    LoggingOutputLock();
    WriteFile(m->out, show_cursor, (DWORD)(sizeof show_cursor - 1), &written, NULL);
    LoggingOutputUnlock();
    return 0;
}

Marquee *MarqueeCreate(const char *verb, size_t width, char character,
                       HANDLE out, DWORD interval)
{
    if (width == 0 || !ValidHandle(out)) return NULL;

    Marquee *m = calloc(1, sizeof *m);
    if (!m) return NULL;

    m->verb = DupStr(verb);
    if (verb && !m->verb) {
        free(m);
        return NULL;
    }
    m->width = width;
    m->character = character;
    m->out = out;
    m->interval = interval;
    InitializeCriticalSection(&m->lock);
    return m;
}

/* Same as MarqueeCreate, but writes to stderr. */
Marquee *MarqueeCreateStderr(const char *verb, size_t width, char character,
                             DWORD interval)
{
    return MarqueeCreate(verb, width, character,
                         GetStdHandle(STD_ERROR_HANDLE), interval);
}

int MarqueeStart(Marquee *m)
{
    if (!m) return -1;
    if (m->running) return 0;

    EnableVT(m->out);

    m->stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!m->stop) return -1;

    m->thread = CreateThread(NULL, 0, MarqueeThread, m, 0, NULL);
    if (!m->thread) {
        CloseHandle(m->stop);
        m->stop = NULL;
        return -1;
    }
    m->running = 1;
    return 0;
}

void MarqueeStop(Marquee *m)
{
    if (!m || !m->running) return;

    SetEvent(m->stop);
    WaitForSingleObject(m->thread, INFINITE);
    CloseHandle(m->thread);
    CloseHandle(m->stop);
    m->thread = NULL;
    m->stop = NULL;
    m->running = 0;
}

int MarqueeIsRunning(const Marquee *m)
{
    return m ? m->running : 0;
}

size_t MarqueeGetWidth(Marquee *m)
{
    if (!m) return 0;
    EnterCriticalSection(&m->lock);
    size_t v = m->width;
    LeaveCriticalSection(&m->lock);
    return v;
}

DWORD MarqueeGetInterval(Marquee *m)
{
    if (!m) return 0;
    EnterCriticalSection(&m->lock);
    DWORD v = m->interval;
    LeaveCriticalSection(&m->lock);
    return v;
}

size_t MarqueeGetVerb(Marquee *m, char *dst, size_t cap)
{
    if (!m) return 0;
    EnterCriticalSection(&m->lock);
    size_t n = CopyOut(m->verb, dst, cap);
    LeaveCriticalSection(&m->lock);
    return n;
}

int MarqueeSetWidth(Marquee *m, size_t width)
{
    if (!m || width == 0) return -1;
    EnterCriticalSection(&m->lock);
    m->width = width;
    LeaveCriticalSection(&m->lock);
    return 0;
}

int MarqueeSetInterval(Marquee *m, DWORD interval)
{
    if (!m) return -1;
    EnterCriticalSection(&m->lock);
    m->interval = interval;
    LeaveCriticalSection(&m->lock);
    return 0;
}

int MarqueeSetVerb(Marquee *m, const char *verb)
{
    if (!m) return -1;
    char *copy = DupStr(verb);
    if (verb && !copy) return -1;
    EnterCriticalSection(&m->lock);
    free(m->verb);
    m->verb = copy;
    LeaveCriticalSection(&m->lock);
    return 0;
}

int MarqueeSetStatus(Marquee *m, const char *verb,
                     uint64_t current, uint64_t total)
{
    if (m == NULL)
        return -1;
    char *copy = DupStr(verb);
    if (verb != NULL && copy == NULL)
        return -1;
    EnterCriticalSection(&m->lock);
    free(m->verb);
    m->verb = copy;
    m->progress_current = current;
    m->progress_total = total;
    LeaveCriticalSection(&m->lock);
    return 0;
}

int MarqueeClearVerb(Marquee *m)
{
    return MarqueeSetVerb(m, NULL);
}

void MarqueeDelete(Marquee *m)
{
    if (!m) return;
    MarqueeStop(m);
    DeleteCriticalSection(&m->lock);
    free(m->verb);
    free(m);
}
