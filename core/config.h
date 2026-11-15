#ifndef PROBE_CONFIG_H
#define PROBE_CONFIG_H

#include "models.h"
#include "../pool/ip_pool.h"
#include "../logging/logging.h"

typedef struct {
    HostUpCheckMethod method;
    char *cidr;
    char *resume;
    char *dump;
    char *script;
    LogLevel loglevel;
    uint32_t bucket_width, cooldown, workers, limit, timeout_ms, retries;
    int keep_private_ranges;
    PortList *ports;
    HostupCheckOptions options;
} ProbeConfig;

int ProbeConfig_Parse(ProbeConfig *cfg, int argc, char **argv);
void ProbeConfig_Free(ProbeConfig *cfg);
#endif
