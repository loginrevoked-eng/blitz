#ifndef CONFIG_H
#define CONFIG_H

#include "cmdline/public.h"
#include "logging.h"
#include "models.h"
#include <stdint.h>

typedef struct Config {
    char *cidr;
    IntList *ports;
    uint32_t bucket_width;
    uint32_t cooldown_period;
    uint32_t probe_semaphore;
    uint32_t limit;
    int exclude_private_ranges;
    char *out_path;
    LogLevel log_level;
    FILTER_PROBE_RESULT filter_by;
} Config;

MoolahCli *ParseArgv(int argc, char **argv);
Config *ConfigNew(MoolahCli *moolah);
Config *ConfigOrDie(int argc, char **argv, MoolahCli **cli);
void FreeConfig(Config *config);

#endif
