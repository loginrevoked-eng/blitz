#ifndef PROBE_CONFIG_H
#define PROBE_CONFIG_H

#include "models.h"
#include "ip_pool.h"
#include "logging.h"

typedef struct {
    char *cidr;
    char *resume;
    char *dump;
    LogLevel loglevel;
    uint32_t bucket_width, cooldown, workers, limit, timeout_ms, retries;
    int keep_private_ranges;
    PortList *ports;
} ProbeConfig;

int ProbeConfig_Parse(ProbeConfig *cfg, int argc, char **argv);
void ProbeConfig_Free(ProbeConfig *cfg);
#endif
