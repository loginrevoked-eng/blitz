#ifndef PROBE_CONFIG_H
#define PROBE_CONFIG_H
#include "probe.h"
#include "logging/logging.h"
typedef struct ProbeConfig {
    char *cidr, *resume, *dump;
    uint32_t bucket_width, cooldown, concurrency, limit;
    HostUpCheckMethod method;
    LogLevel log_level;
    uint32_t timeout_ms;
    unsigned retries;
    int banners;
    PortList ports;
} ProbeConfig;
int ProbeConfig_Parse(ProbeConfig *cfg, int argc, char **argv);
void ProbeConfig_Free(ProbeConfig *cfg);
#endif
