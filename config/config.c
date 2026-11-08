#include "config.h"
#include "logging.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static int ReadU32Option(MoolahCli *moolah, const char *name, uint32_t *out)
{
    Value *value = MoolahCliGet(moolah, name);
    if (value == NULL || value->kind != VALUE_LONG || value->l < 0 ||
        (uint64_t)value->l > UINT32_MAX)
        return 0;
    *out = (uint32_t)value->l;
    return 1;
}

static int ParseLogLevel(const char *text, LogLevel *out)
{
    if (text == NULL || out == NULL)
        return 0;

    size_t len = strlen(text);
    char *normalized = malloc(len + 1);
    if (normalized == NULL)
        return 0;

    for (size_t i = 0; i <= len; i++)
        normalized[i] = (char)tolower((unsigned char)text[i]);

    int valid = 1;
    if (strcmp(normalized, "debug") == 0)
        *out = LOG_LEVEL_DEBUG;
    else if (strcmp(normalized, "info") == 0)
        *out = LOG_LEVEL_INFO;
    else if (strcmp(normalized, "warn") == 0 || strcmp(normalized, "warning") == 0)
        *out = LOG_LEVEL_WARN;
    else if (strcmp(normalized, "error") == 0)
        *out = LOG_LEVEL_ERROR;
    else if (strcmp(normalized, "fatal") == 0)
        *out = LOG_LEVEL_FATAL;
    else if (strcmp(normalized, "log") == 0)
        *out = LOG_LEVEL_LOG;
    else if (strcmp(normalized, "quiet") == 0)
        *out = LOG_LEVEL_QUIET;
    else
        valid = 0;

    free(normalized);
    return valid;
}

static int ParseFilterBy(const char *text, FILTER_PROBE_RESULT *out)
{
    if (text == NULL || out == NULL)
        return 0;

    if (tolower((unsigned char)text[0]) == 'u' &&
        tolower((unsigned char)text[1]) == 'p' && text[2] == '\0') {
        *out = FILTER_BY_UP;
        return 1;
    }
    return 0;
}

MoolahCli *ParseArgv(int argc, char **argv)
{
    if (argc < 1 || argv == NULL)
        return NULL;

    MoolahCli *moolah = MoolahCliNew(argv[0], "experimental networking toy", MOOLAHCLI_REMOVE_BUILTINS | MOOLAHCLI_IGNORE_CASE, argv);
    if (moolah == NULL)
        return NULL;

    MoolahCliAdd(moolah, OPT_STR, "CIDR-TEXT", NULL, NULL, "cidr text to parse into ip ranges", OPT_POSITIONAL | OPT_REQUIRED, NULL, NULL);
    MoolahCliAdd(moolah, OPT_LONG, "ports", "--ports", "-p", "ports to scan", OPT_LIST | OPT_VARIADIC, NULL, NULL);
    MoolahCliAdd(moolah, OPT_LONG, "bucket-width", "--bucket-width", NULL, "number of IPv4 addresses in each picker bucket", OPT_REQUIRED, NULL, NULL);
    MoolahCliAdd(moolah, OPT_LONG, "cooldown-period", "--cooldown-period", NULL, "bucket cooldown period in seconds", OPT_REQUIRED, NULL, NULL);
    MoolahCliAdd(moolah, OPT_LONG, "probe-semaphore", "--probe-semaphore", NULL, "maximum number of concurrent probes", OPT_REQUIRED, NULL, NULL);
    MoolahCliAdd(moolah, OPT_LONG, "limit", "--limit", NULL, "number of IPs to pick", OPT_REQUIRED, NULL, NULL);
    MoolahCliAdd(moolah, OPT_STR, "filter-by", "--filter-by", NULL, "filter probe results by status (up)", 0, NULL, NULL);
    MoolahCliAdd(moolah, OPT_BOOL, "quiet", "--quiet", NULL, "suppress all non-fatal output", 0, NULL, NULL);
    MoolahCliAdd(moolah, OPT_BOOL, "exclude-private-ranges", "--exclude-private-ranges", NULL, "exclude private and reserved IPv4 ranges", 0, NULL, NULL);
    MoolahCliAdd(moolah, OPT_STR, "out", "--out", NULL, "write up hosts to this text file", OPT_REQUIRED, NULL, NULL);
    MoolahCliAdd(moolah, OPT_BOOL, "verbose", "--verbose", "-v", "enable debug logging", 0, NULL, NULL);
    MoolahCliAdd(moolah, OPT_STR, "log-level", "--log-level", NULL, "logging level: debug, info, warn, error, fatal, or log", 0, NULL, NULL);
    MoolahCliParse(moolah, argc, argv);
    return moolah;
}

Config *ConfigNew(MoolahCli *moolah)
{
    if (moolah == NULL)
        return NULL;

    char *cidr = MoolahCliGetString(moolah, "CIDR-TEXT");
    char *out_path = MoolahCliGetString(moolah, "out");
    Value *port_values = MoolahCliGet(moolah, "ports");
    uint32_t bucket_width, cooldown_period, probe_semaphore, limit;
    if (!ReadU32Option(moolah, "bucket-width", &bucket_width) ||
        !ReadU32Option(moolah, "cooldown-period", &cooldown_period) ||
        !ReadU32Option(moolah, "probe-semaphore", &probe_semaphore) ||
        !ReadU32Option(moolah, "limit", &limit))
        return NULL;

    FILTER_PROBE_RESULT filter_by = FILTER_BY_NONE;
    char *filter_by_text = MoolahCliGetString(moolah, "filter-by");
    if (filter_by_text != NULL && !ParseFilterBy(filter_by_text, &filter_by))
        return NULL;

    LogLevel log_level = LOG_LEVEL_ERROR;
    Value *quiet_value = MoolahCliGet(moolah, "quiet");
    Value *exclude_private_value = MoolahCliGet(moolah, "exclude-private-ranges");
    Value *verbose_value = MoolahCliGet(moolah, "verbose");
    if (quiet_value != NULL && quiet_value->kind == VALUE_BOOL && quiet_value->b == True) {
        log_level = LOG_LEVEL_QUIET;
    } else if (verbose_value != NULL && verbose_value->kind == VALUE_BOOL && verbose_value->b == True) {
        log_level = LOG_LEVEL_DEBUG;
    } else {
        char *log_level_text = MoolahCliGetString(moolah, "log-level");
        if (log_level_text != NULL && !ParseLogLevel(log_level_text, &log_level))
            return NULL;
    }

    Config *config = calloc(1, sizeof *config);
    IntList *ports = port_values ? IntListFromValue(port_values) : calloc(1, sizeof *ports);
    if (config == NULL || ports == NULL || cidr == NULL) {
        free(config);
        if (ports != NULL) {
            free(ports->list);
            free(ports);
        }
        return NULL;
    }

    config->cidr = strdup(cidr);
    if (config->cidr == NULL) {
        free(config);
        free(ports->list);
        free(ports);
        return NULL;
    }
    config->ports = ports;
    config->bucket_width = bucket_width;
    config->cooldown_period = cooldown_period;
    config->probe_semaphore = probe_semaphore;
    config->limit = limit;
    config->exclude_private_ranges = exclude_private_value != NULL &&
        exclude_private_value->kind == VALUE_BOOL && exclude_private_value->b == True;
    config->out_path = out_path != NULL ? strdup(out_path) : NULL;
    if (out_path != NULL && config->out_path == NULL) {
        FreeConfig(config);
        return NULL;
    }
    config->log_level = log_level;
    config->filter_by = filter_by;
    return config;
}

Config *ConfigOrDie(int argc, char **argv, MoolahCli **cli)
{
    if (cli != NULL)
        *cli = NULL;

    MoolahCli *moolah = ParseArgv(argc, argv);
    if (moolah == NULL)
        Panic("Failed to parse command line\n");

    Config *config = ConfigNew(moolah);
    if (config == NULL) {
        MoolahCliDestroy(moolah);
        Panic("Failed to create configuration\n");
    }

    if (cli != NULL)
        *cli = moolah;
    else
        MoolahCliDestroy(moolah);
    return config;
}

void FreeConfig(Config *config)
{
    if (config == NULL)
        return;
    free(config->cidr);
    free(config->out_path);
    if (config->ports != NULL) {
        free(config->ports->list);
        free(config->ports);
    }
    free(config);
}
