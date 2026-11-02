#include "time.h"

uint64_t TimeNowNs(void)
{
    static LARGE_INTEGER freq;
    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    uint64_t frequency = (uint64_t)freq.QuadPart;
    uint64_t value = (uint64_t)counter.QuadPart;
    return (value / frequency) * 1000000000ULL +
        (value % frequency) * 1000000000ULL / frequency;
}

DWORD TimeRemainingMs(uint64_t expires_at, uint64_t now)
{
    if (expires_at <= now)
        return 0;

    uint64_t ms = (expires_at - now + 999999ULL) / 1000000ULL;
    if (ms >= INFINITE)
        ms = INFINITE - 1;
    return (DWORD)ms;
}
