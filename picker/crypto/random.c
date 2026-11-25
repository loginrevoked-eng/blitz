#include "random.h"

#include <windows.h>
#include <bcrypt.h>
#include <stdlib.h>

static uint64_t RandomValue(void)
{
    uint64_t value;
    if (BCryptGenRandom(NULL, (PUCHAR)&value, sizeof value,
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        abort();
    return value;
}

uint64_t RandomU64(uint64_t lower, uint64_t upper)
{
    if (lower > upper)
        abort();

    uint64_t range = upper - lower + 1;
    if (range == 0)
        return RandomValue();

    uint64_t threshold = -range % range;
    uint64_t value;
    do {
        value = RandomValue();
    } while (value < threshold);

    return lower + value % range;
}

uint32_t FeistelSeed(void)
{
    return RandomU64(0, UINT32_MAX);
}
