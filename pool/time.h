#ifndef POOL_TIME_H
#define POOL_TIME_H

#include <windows.h>
#include <stdint.h>

uint64_t TimeNowNs(void);
DWORD TimeRemainingMs(uint64_t expires_at, uint64_t now);

#endif
