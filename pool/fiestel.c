#include "fiestel.h"

#define FEISTEL_ROUNDS 6

static uint64_t SplitMix64(uint64_t *state)
{
    uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static uint32_t Fmix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

int FeistelPermute(uint32_t seed, uint32_t n, uint32_t width, uint32_t *out)
{
    if (width == 0 || n >= width)
        return 0;

    /* Smallest even bit count whose range covers width (even so it splits in two halves). */
    unsigned bits = 2;
    while (bits < 32 && ((uint64_t)1 << bits) < width)
        bits += 2;

    unsigned half_bits = bits / 2;
    uint32_t mask = (uint32_t)(((uint64_t)1 << half_bits) - 1);

    /* One round key per round, expanded from the seed. */
    uint32_t keys[FEISTEL_ROUNDS];
    uint64_t state = seed;
    for (int i = 0; i < FEISTEL_ROUNDS; i++)
        keys[i] = (uint32_t)SplitMix64(&state);

    /* The network is a bijection on [0, 2^bits). Cycle-walking re-applies it
       until the result lands inside [0, width), which keeps it a bijection there. */
    uint32_t x = n;
    do {
        uint32_t l = x >> half_bits;
        uint32_t r = x & mask;

        for (int i = 0; i < FEISTEL_ROUNDS; i++) {
            uint32_t t = l ^ (Fmix32(r ^ keys[i]) & mask);
            l = r;
            r = t;
        }
        x = (l << half_bits) | r;
    } while (x >= width);

    *out = x;
    return 1;
}
