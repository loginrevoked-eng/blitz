#ifndef FEISTEL_H
#define FEISTEL_H

#include <stdint.h>

int FeistelPermute(uint32_t seed, uint32_t n, uint32_t width, uint32_t *out);

#endif
