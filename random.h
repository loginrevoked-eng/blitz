#ifndef RANDOM_H
#define RANDOM_H

#include <stdint.h>

uint32_t FiestelSeed(void);
uint64_t RandomU64(uint64_t least, uint64_t highest);

#endif
