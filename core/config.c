#include "config.h"
#include "cleanup.h"

#include "../cmdline/public.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int ReadU32(MoolahCli *parser, const char *name, uint32_t fallback,
    uint32_t *output)
{
    MoolahValue *value = MoolahCliGet(parser, name);
    if (value == NULL) {
        *output = fallback;
        return 1;
    }
    if (value->kind != MOOLAH_VALUE_LONG || value->l < 0 ||
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

static int ParsePorts(PortList **output, const MoolahValue *value)
{
    *output = NULL;
    if (value == NULL)
        return 1;
    if (value->kind != MOOLAH_VALUE_LIST)
        return 0;

    PortList *ports = calloc(1, sizeof(*ports));
    if (ports == NULL)
        return 0;

    for (const MoolahValueNode *node = value->list; node != NULL;
         node = node->next) {
        const MoolahValue *item = &node->value;
        if (item->kind != MOOLAH_VALUE_LONG || item->l <= 0 ||
            item->l > UINT16_MAX) {
            ProbeCleanupPortList(ports);
            return 0;
        }

        uint16_t *items = realloc(ports->items,
            (ports->len + 1) * sizeof(*items));
        if (items == NULL) {
            ProbeCleanupPortList(ports);
            return 0;
        }
        ports->items = items;
        ports->items[ports->len++] = (uint16_t)item->l;
    }

    if (ports->len == 0) {
        ProbeCleanupPortList(ports);
        return 0;
    }
    *output = ports;
    return 1;
}

static int CopyPorts(PortList *destination, const PortList *source)
{
    if (source == NULL || source->len == 0)
        return 1;

    destination->items = malloc(source->len * sizeof(*source->items));
    if (destination->items == NULL)
        return 0;
    memcpy(destination->items, source->items,
        source->len * sizeof(*source->items));
    destination->len = source->len;
    return 1;
}

static int InitOptions(ProbeConfig *config, const PortList *ports)
{
    HostUpCheckProbeOptions probe = { 0 };
    probe.timeout_ms = config->timeout_ms;
    probe.retries = config->retries;
    config->options.method = config->method;

    if (config->method & HOST_CHECK_METHOD_ICMP) {
        config->options.icmp_options.probe = probe;
    }
    if (config->method & HOST_CHECK_METHOD_UDP) {
        config->options.udp_options.probe = probe;
        if (!CopyPorts(&config->options.udp_options.ports, ports))
            return 0;
    }
    if (config->method & HOST_CHECK_METHOD_TCP) {
        config->options.tcp_options.probe = probe;
        if (!CopyPorts(&config->options.tcp_options.ports, ports))
            return 0;
    }
    return 1;
}

int ProbeConfig_Parse(ProbeConfig *config, int argc, char **argv)
{
    if (config == NULL || argc < 1 || argv == NULL || argv[0] == NULL)
        return 0;

    memset(config, 0, sizeof(*config));
    config->loglevel = LOG_LEVEL_LOG;

    MoolahCli *parser = MoolahCliNew(argv[0], "multithreaded host checker",
        MOOLAHCLI_IGNORE_CASE | MOOLAHCLI_REMOVE_BUILTINS, argv);
    if (parser == NULL)
        return 0;

#define ADD(kind, name, flag, description, flags) \
    MoolahCliAdd(parser, kind, name, flag, NULL, description, flags, \
        NULL, NULL, NULL)


    ADD(MOOLAHCLI_OPT_STR, "cidr", NULL, "IPv4 CIDR", MOOLAHCLI_OPT_POSITIONAL | MOOLAHCLI_OPT_REQUIRED);
    MoolahCliAddMutexGroup(parser, "icmp-ports", 0);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "icmp", "--icmp", NULL,
        "ICMP echo check", 0, NULL, NULL, "icmp-ports");
    ADD(MOOLAHCLI_OPT_BOOL, "syn", "--syn", "TCP probe", 0);
    ADD(MOOLAHCLI_OPT_BOOL, "datagram", "--datagram", "UDP probe", 0);
    ADD(MOOLAHCLI_OPT_BOOL, "keep-private-ranges", "--keep-private-ranges",
        "include private IP ranges", 0);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_LONG, "ports", "--ports", NULL,
        "one or more TCP/UDP ports",
        MOOLAHCLI_OPT_LIST | MOOLAHCLI_OPT_VARIADIC, NULL, NULL,
        "icmp-ports");
    ADD(MOOLAHCLI_OPT_LONG, "threads", "--threads", "worker count", 0);
    ADD(MOOLAHCLI_OPT_LONG, "limit", "--limit", "number of addresses", 0);
    ADD(MOOLAHCLI_OPT_LONG, "bucket-width", "--bucket-width", "pool bucket width", MOOLAHCLI_OPT_REQUIRED);
    ADD(MOOLAHCLI_OPT_LONG, "cooldown", "--cooldown-period", "pool cooldown seconds", MOOLAHCLI_OPT_REQUIRED);
    ADD(MOOLAHCLI_OPT_LONG, "timeout", "--timeout", "probe timeout in ms", MOOLAHCLI_OPT_REQUIRED);
    ADD(MOOLAHCLI_OPT_LONG, "retries", "--retries", "retry count", MOOLAHCLI_OPT_REQUIRED);
    ADD(MOOLAHCLI_OPT_STR, "resume", "--resume", "resume file", 0);
    ADD(MOOLAHCLI_OPT_STR, "dump", "--dump", "pool dump file", 0);
    ADD(MOOLAHCLI_OPT_STR, "script", "--script", "Lua script file", 0);
    ADD(MOOLAHCLI_OPT_STR, "set-log-level", "--set-log-level", "logging level: debug, info, warn, error, fatal, log, quiet", 0);
#undef ADD

    MoolahCliParse(parser, argc, argv);

    MoolahValue *value = MoolahCliGet(parser, "icmp");
    if (value != NULL && value->b)
        config->method = (HostUpCheckMethod)(config->method | HOST_CHECK_METHOD_ICMP);
    value = MoolahCliGet(parser, "syn");
    if (value != NULL && value->b)
        config->method = (HostUpCheckMethod)(config->method | HOST_CHECK_METHOD_TCP);
    value = MoolahCliGet(parser, "datagram");
    if (value != NULL && value->b)
        config->method = (HostUpCheckMethod)(config->method | HOST_CHECK_METHOD_UDP);

    value = MoolahCliGet(parser, "keep-private-ranges");
    config->keep_private_ranges = value != NULL && value->b;

    const char *cidr_text = MoolahCliGetString(parser, "cidr");
    if (cidr_text != NULL) {
        CidrRange *parsed_cidr = CidrRangeNew(cidr_text);
        if (parsed_cidr != NULL)
            ProbeCleanupCidrRange(parsed_cidr);
        else
            cidr_text = NULL;
    }
    if (cidr_text != NULL)
        config->cidr = _strdup(cidr_text);
    const char *resume = MoolahCliGetString(parser, "resume");
    if (resume != NULL)
        config->resume = _strdup(resume);
    const char *dump = MoolahCliGetString(parser, "dump");
    if (dump != NULL)
        config->dump = _strdup(dump);
    const char *script = MoolahCliGetString(parser, "script");
    if (script != NULL)
        config->script = _strdup(script);

    const char *log_level = MoolahCliGetString(parser, "set-log-level");
    int needs_ports = (config->method &
        (HOST_CHECK_METHOD_TCP | HOST_CHECK_METHOD_UDP)) != 0;
    int valid = config->method != 0 && cidr_text != NULL && config->cidr != NULL &&
        (resume == NULL || config->resume != NULL) &&
        (dump == NULL || config->dump != NULL) &&
        (script == NULL || config->script != NULL) &&
        (log_level == NULL || ParseLogLevel(log_level, &config->loglevel)) &&
        ReadU32(parser, "threads", 32, &config->workers) &&
        ReadU32(parser, "limit", 100, &config->limit) &&
        ReadU32(parser, "bucket-width", 64, &config->bucket_width) &&
        ReadU32(parser, "cooldown", 100, &config->cooldown) &&
        ReadU32(parser, "timeout", 1000, &config->timeout_ms) &&
        ReadU32(parser, "retries", 1, &config->retries);

    valid = valid &&
        config->workers != 0 && config->limit != 0 &&
        config->bucket_width != 0 &&
        ParsePorts(&config->ports, MoolahCliGet(parser, "ports")) &&
        (!needs_ports || config->ports != NULL) &&
        InitOptions(config, config->ports);

    MoolahCliDestroy(parser);
    if (!valid) {
        ProbeConfig_Free(config);
        return 0;
    }
    return 1;
}

