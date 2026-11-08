#include "config.h"

#include "cmdline/public.h"
#include "cmdline/value.h"
#include "logging/logging.h"

#include <stdlib.h>
#include <string.h>

static int ReadU32(MoolahCli *parser, const char *name, uint32_t fallback,
    uint32_t *output)
{
    Value *value = MoolahCliGet(parser, name);
    if (value == NULL) {
        *output = fallback;
        return 1;
    }
    if (value->kind != VALUE_LONG || value->l < 0 ||
        (unsigned long)value->l > UINT32_MAX)
        return 0;

    *output = (uint32_t)value->l;
    return 1;
}

static int ParseLogLevel(const char *text, LogLevel *level)
{
    static const struct {
        const char *name;
        LogLevel level;
    } levels[] = {
        { "debug", LOG_LEVEL_DEBUG },
        { "info", LOG_LEVEL_INFO },
        { "warn", LOG_LEVEL_WARN },
        { "error", LOG_LEVEL_ERROR },
        { "fatal", LOG_LEVEL_FATAL },
        { "log", LOG_LEVEL_LOG },
        { "quiet", LOG_LEVEL_QUIET }
    };

    for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); ++i) {
        if (strcmp(text, levels[i].name) == 0) {
            *level = levels[i].level;
            return 1;
        }
    }
    return 0;
}

static int ParsePorts(PortList *ports, const char *text)
{
    if (text == NULL)
        return 1;

    char *copy = strdup(text);
    if (copy == NULL)
        return 0;

    char *context = NULL;
    char *token = strtok_s(copy, ",", &context);
    while (token != NULL) {
        char *end = NULL;
        unsigned long port = strtoul(token, &end, 10);
        if (*token == '\0' || *end != '\0' || port == 0 || port > UINT16_MAX) {
            free(copy);
            return 0;
        }

        uint16_t *items = realloc(ports->items, (ports->len + 1) * sizeof(*items));
        if (items == NULL) {
            free(copy);
            return 0;
        }
        ports->items = items;
        ports->items[ports->len++] = (uint16_t)port;
        token = strtok_s(NULL, ",", &context);
    }

    free(copy);
    return ports->len != 0;
}

int ProbeConfig_Parse(ProbeConfig *config, int argc, char **argv)
{
    memset(config, 0, sizeof(*config));
    config->method = 0;
    config->timeout_ms = 1000;
    config->retries = 1;
    config->log_level = LOG_LEVEL_LOG;

    MoolahCli *parser = MoolahCliNew(argv[0], "multithreaded host checker",
        MOOLAHCLI_IGNORE_CASE | MOOLAHCLI_REMOVE_BUILTINS, argv);
    if (parser == NULL)
        return 0;

    MoolahCliAdd(parser, OPT_STR, "cidr", NULL, NULL, "IPv4 CIDR",
        OPT_POSITIONAL | OPT_REQUIRED, NULL, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "icmp", "--icmp", NULL, "ICMP echo check", 0, NULL, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "syn", "--syn", NULL, "TCP connect check", 0, NULL, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "datagram", "--datagram", NULL, "UDP probe", 0, NULL, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "banners", "--banners", NULL, "grab HTTP, FTP, RTSP banners from responsive hosts", 0, NULL, NULL);
    MoolahCliAdd(parser, OPT_STR, "ports", "--ports", NULL, "comma-separated ports", 0, NULL, NULL);
    MoolahCliAdd(parser, OPT_LONG, "threads", "--threads", NULL, "worker count", 0, NULL, NULL);
    MoolahCliAdd(parser, OPT_LONG, "limit", "--limit", NULL, "number of addresses", 0, NULL, NULL);
    MoolahCliAdd(parser, OPT_LONG, "bucket-width", "--bucket-width", NULL, "pool bucket width", 0, NULL, NULL);
    MoolahCliAdd(parser, OPT_LONG, "cooldown", "--cooldown-period", NULL, "pool cooldown seconds", 0, NULL, NULL);
    MoolahCliAdd(parser, OPT_STR, "resume", "--resume", NULL, "resume file", 0, NULL, NULL);
    MoolahCliAdd(parser, OPT_STR, "dump", "--dump", NULL, "pool dump file", 0, NULL, NULL);
    MoolahCliAdd(parser, OPT_LONG, "timeout", "--timeout", NULL, "timeout in ms", 0, NULL, NULL);
    MoolahCliAdd(parser, OPT_LONG, "retries", "--retries", NULL, "retry count", 0, NULL, NULL);
    MoolahCliAdd(parser, OPT_STR, "set-log-level", "--set-log-level", NULL, "logging level: debug, info, warn, error, fatal, log, quiet", 0, NULL, NULL);
    MoolahCliParse(parser, argc, argv);

    config->cidr = strdup(MoolahCliGetString(parser, "cidr"));
    Value *value = MoolahCliGet(parser, "syn");
    if (value != NULL && value->b)
        config->method = (HostUpCheckMethod)(config->method | HOST_CHECK_METHOD_TCP_SYN);
    value = MoolahCliGet(parser, "datagram");
    if (value != NULL && value->b)
        config->method = (HostUpCheckMethod)(config->method | HOST_CHECK_METHOD_UDP);
    value = MoolahCliGet(parser, "icmp");
    if (value != NULL && value->b)
        config->method = (HostUpCheckMethod)(config->method | HOST_CHECK_METHOD_ICMP_ECHO);
    if (config->method == 0)
        config->method = HOST_CHECK_METHOD_ICMP_ECHO;

    value = MoolahCliGet(parser, "banners");
    config->banners = value != NULL && value->b;

    int valid_log_level = 1;
    const char *log_level = MoolahCliGetString(parser, "set-log-level");
    if (log_level != NULL)
        valid_log_level = ParseLogLevel(log_level, &config->log_level);

    int valid = valid_log_level && config->cidr != NULL &&
        ReadU32(parser, "threads", 32, &config->concurrency) &&
        ReadU32(parser, "limit", 100, &config->limit) &&
        ReadU32(parser, "bucket-width", 64, &config->bucket_width) &&
        ReadU32(parser, "cooldown", 100, &config->cooldown) &&
        ReadU32(parser, "timeout", 1000, &config->timeout_ms) &&
        ReadU32(parser, "retries", 1, &config->retries) &&
        config->concurrency != 0 && config->limit != 0 && config->bucket_width != 0 &&
        ParsePorts(&config->ports, MoolahCliGetString(parser, "ports"));

    value = MoolahCliGet(parser, "resume");
    if (value != NULL && value->s)
        config->resume = strdup(value->s);
    value = MoolahCliGet(parser, "dump");
    if (value != NULL && value->s)
        config->dump = strdup(value->s);

    MoolahCliDestroy(parser);
    if (!valid) {
        ProbeConfig_Free(config);
        return 0;
    }
    return 1;
}

void ProbeConfig_Free(ProbeConfig *config)
{
    free(config->cidr);
    free(config->resume);
    free(config->dump);
    free(config->ports.items);
    memset(config, 0, sizeof(*config));
}
