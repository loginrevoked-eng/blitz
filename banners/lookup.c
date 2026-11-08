#include "lookup.h"

const uint32_t SERVICES[MAX_LOOKUP_INDEX + 1] = {
    [21] = SERVICE_FTP,
    [554] = SERVICE_RTSP,
    [80] = SERVICE_HTTP,
    [443] = SERVICE_HTTPS,
    [3000] = SERVICE_HTTP,
    [5000] = SERVICE_HTTP,
    [8000] = SERVICE_HTTP,
    [8008] = SERVICE_HTTP,
    [8080] = SERVICE_HTTP,
    [8081] = SERVICE_HTTP,
    [8443] = SERVICE_HTTPS,
    [8554] = SERVICE_RTSP,
    [8888] = SERVICE_HTTP,
    [9000] = SERVICE_HTTP,
    [9443] = SERVICE_HTTPS
};

int IsOOB(uint32_t max_index, uint32_t index)
{
    return index > max_index;
}
