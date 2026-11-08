#ifndef CRYPTO_RANDOM_H
#define CRYPTO_RANDOM_H

#include <stdint.h>

uint64_t RandomU64(uint64_t lower, uint64_t upper);
uint32_t FeistelSeed(void);

#endif
