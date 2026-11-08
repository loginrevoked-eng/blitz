#ifndef BANNERS_LOOKUP_H
#define BANNERS_LOOKUP_H

#include <stdint.h>

#define MAX_LOOKUP_INDEX UINT32_C(65535)

typedef enum {
    SERVICE_NONE = 0,
    SERVICE_HTTP = 1u << 0,
    SERVICE_HTTPS = 1u << 1,
    SERVICE_FTP = 1u << 2,
    SERVICE_RTSP = 1u << 3
} ServiceFlags;

extern const uint32_t SERVICES[MAX_LOOKUP_INDEX + 1];

int IsOOB(uint32_t max_index, uint32_t index);

#endif
