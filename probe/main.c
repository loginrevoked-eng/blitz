#include <winsock2.h>

#include "config.h"
#include "init.h"
#include "pool/init.h"
#include "probe.h"
#include "logging/logging.h"

#include <stdio.h>

static void PrintUsage(const char *program)
{
    Log(
        "Usage: %s CIDR [--icmp|--syn|--datagram] [--ports 80,443] "
        "[--threads N] [--limit N] [--bucket-width N] "
        "[--cooldown-period SECONDS] [--timeout MS] [--retries N] "
        "[--resume FILE] [--dump FILE] [--banners] [--set-log-level LEVEL]\n",
        program != NULL ? program : "bot");
}

int main(int argc, char **argv)
{
    ProbeConfig config;
    if (!ProbeConfig_Parse(&config, argc, argv)) {
        PrintUsage(argc > 0 ? argv[0] : NULL);
        return 2;
    }
    SetLogLevel(config.log_level);
    SetLoggerFlags(LOGGER_NO_TAGS | LOGGER_NO_TIMESTAMP);

    WSADATA winsock;
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
        Log("Failed to initialize Winsock.\n");
        ProbeConfig_Free(&config);
        return 2;
    }

    IpPickerFlags picker_flags = ((config.resume != NULL || config.dump != NULL) ? IP_PICKER_RESUMABLE : IP_PICKER_FLAGS_NONE) | IP_PICKER_EXCLUDE_PRIVATE_RANGES;
    IpPicker *picker = IpPicker_Initialize(config.cidr, config.bucket_width, config.cooldown, config.resume, config.dump, picker_flags, NULL, NULL);
    if (picker == NULL) {
        Log("Failed to initialize the IP pool.\n");
        WSACleanup();
        ProbeConfig_Free(&config);
        return 2;
    }

    HostupCheckHub *hub = HostupCheckHub_New(config.concurrency);
    if (hub == NULL) {
        Log("Failed to initialize the check hub.\n");
        IpPicker_Finalize(picker);
        WSACleanup();
        ProbeConfig_Free(&config);
        return 2;
    }

    ProbeFlags probe_flags = PROBE_FLAGS_NONE;
    if (config.banners)
        probe_flags = (ProbeFlags)(probe_flags | PROBE_GRAB_BANNERS);
    int status = ProbeRun(hub, picker, &config, probe_flags);

    HostupCheckHub_Free(hub);
    IpPicker_Finalize(picker);
    WSACleanup();
    ProbeConfig_Free(&config);
    return status;
}
