#ifndef BOT_CLI_MARQUEE_PROGRESS_H
#define BOT_CLI_MARQUEE_PROGRESS_H

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

typedef struct MarqueeProgress MarqueeProgress;

MarqueeProgress *MarqueeProgressCreate(const char *verb, size_t width,
                                       char character, HANDLE output,
                                       DWORD sweep_interval_ms);
MarqueeProgress *MarqueeProgressCreateStderr(const char *verb, size_t width,
                                             char character,
                                             DWORD sweep_interval_ms);
int MarqueeProgressStart(MarqueeProgress *progress);
void MarqueeProgressStop(MarqueeProgress *progress);
int MarqueeProgressUpdate(MarqueeProgress *progress, const char *verb,
                          uint64_t current, uint64_t total);
void MarqueeProgressDelete(MarqueeProgress *progress);

#endif
