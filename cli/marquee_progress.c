#include "marquee_progress.h"

#include "marquee.h"

#include <stdlib.h>

struct MarqueeProgress {
    Marquee *marquee;
};

MarqueeProgress *MarqueeProgressCreate(const char *verb, size_t width,
                                       char character, HANDLE output,
                                       DWORD sweep_interval_ms)
{
    MarqueeProgress *progress = calloc(1, sizeof *progress);
    if (progress == NULL)
        return NULL;
    progress->marquee = MarqueeCreate(verb, width, character, output,
                                      sweep_interval_ms);
    if (progress->marquee == NULL) {
        free(progress);
        return NULL;
    }
    return progress;
}

MarqueeProgress *MarqueeProgressCreateStderr(const char *verb, size_t width,
                                             char character,
                                             DWORD sweep_interval_ms)
{
    MarqueeProgress *progress = calloc(1, sizeof *progress);
    if (progress == NULL)
        return NULL;
    progress->marquee = MarqueeCreateStderr(verb, width, character,
                                            sweep_interval_ms);
    if (progress->marquee == NULL) {
        free(progress);
        return NULL;
    }
    return progress;
}

int MarqueeProgressStart(MarqueeProgress *progress)
{
    return progress == NULL ? -1 : MarqueeStart(progress->marquee);
}

void MarqueeProgressStop(MarqueeProgress *progress)
{
    if (progress != NULL)
        MarqueeStop(progress->marquee);
}

int MarqueeProgressUpdate(MarqueeProgress *progress, const char *verb,
                          uint64_t current, uint64_t total)
{
    return progress == NULL ? -1 :
           MarqueeSetStatus(progress->marquee, verb, current, total);
}

void MarqueeProgressDelete(MarqueeProgress *progress)
{
    if (progress == NULL)
        return;
    MarqueeDelete(progress->marquee);
    free(progress);
}
