#ifndef BOT_CLI_MARQUEE_H
#define BOT_CLI_MARQUEE_H

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

typedef struct Marquee Marquee;

typedef enum {
    MARQUEE_FLAGS_NONE = 0,
    OVERWRITE_ON_STOP = 1u << 0
} MarqueeFlags;

Marquee *MarqueeCreate(const char *verb, size_t width, char character,
                       HANDLE out, DWORD interval);
Marquee *MarqueeCreateWithFlags(const char *verb, size_t width, char character,
                                HANDLE out, DWORD interval, MarqueeFlags flags);
Marquee *MarqueeCreateStderr(const char *verb, size_t width, char character,
                             DWORD interval);
Marquee *MarqueeCreateStderrWithFlags(const char *verb, size_t width,
                                      char character, DWORD interval,
                                      MarqueeFlags flags);
int MarqueeStart(Marquee *m);
void MarqueeStop(Marquee *m);
int MarqueeIsRunning(const Marquee *m);
size_t MarqueeGetWidth(Marquee *m);
DWORD MarqueeGetInterval(Marquee *m);
size_t MarqueeGetVerb(Marquee *m, char *dst, size_t cap);
int MarqueeSetWidth(Marquee *m, size_t width);
int MarqueeSetInterval(Marquee *m, DWORD interval);
int MarqueeSetVerb(Marquee *m, const char *verb);
int MarqueeUpdateVerb(Marquee *m, const char *verb);
int MarqueeSetStatus(Marquee *m, const char *verb,
                     uint64_t current, uint64_t total);
int MarqueeClearVerb(Marquee *m);
void MarqueeDelete(Marquee *m);

#endif
