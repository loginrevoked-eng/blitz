#include "random.h"
#include "pool/time.h"

#include <windows.h>
#include <stdlib.h>

static uint64_t RandomValue(void)
{
    static __declspec(thread) int seeded = 0;
    if (!seeded) {
        srand((unsigned)(TimeNowNs() ^ ((uint64_t)GetCurrentThreadId() << 16)));
        seeded = 1;
    }

    uint64_t value = 0;
    for (int i = 0; i < 5; i++)
        value = (value << 15) ^ (uint64_t)rand();
    return value;
}

uint32_t FiestelSeed(void)
{
    return (uint32_t)RandomValue();
}

uint64_t RandomU64(uint64_t least, uint64_t highest)
{
    return least + RandomValue() % (highest - least + 1);
}
