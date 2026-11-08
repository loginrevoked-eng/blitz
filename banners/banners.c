#include <winsock2.h>

#include "banners.h"
#include "ftp.h"
#include "http.h"
#include "lookup.h"
#include "rtsp.h"
#include "ssh.h"
#include "logging/logging.h"
#include "pool/cidr.h"

#include <stdlib.h>
#include <string.h>

static int LogFtpBanner(HostUpCheck *check, uint16_t port,
    RegistryFindingHandler on_finding, void *context)
{
    char *response = FtpGrab(check, port);
    if (response == NULL)
        return 0;

    char *formatted = FtpResponseFmt(response);
    free(response);
    if (formatted == NULL)
        return 0;

    RegistryFinding finding = {
        .service = REGISTRY_SERVICE_FTP,
        .ip = check->toptions->probe.addr,
        .port = port,
        .detail.ftp = { .greeting = formatted }
    };
    int should_print = on_finding == NULL || on_finding(&finding, context);
    char *ip = should_print ? Ipv4Repr(finding.ip) : NULL;
    if (ip != NULL) {
        Info("FTP %s:%u\n%s", ip, (unsigned)port, formatted);
        free(ip);
    }
    free(formatted);
    return should_print;
}

static int LogHttpBanner(HostUpCheck *check, uint16_t port,
    RegistryFindingHandler on_finding, void *context)
{
    char *response = HttpGrab(check, port);
    if (response == NULL)
        return 0;
    char *formatted = HttpResponseFmt(response);
    free(response);
    if (formatted == NULL)
        return 0;

    RegistryFinding finding = {
        .service = REGISTRY_SERVICE_HTTP,
        .ip = check->toptions->probe.addr,
        .port = port,
        .detail.http = { .headers = formatted }
    };
    int should_print = on_finding == NULL || on_finding(&finding, context);
    char *ip = should_print ? Ipv4Repr(finding.ip) : NULL;
    if (ip != NULL) {
        Info("HTTP %s:%u\n%s", ip, (unsigned)port, formatted);
        free(ip);
    }
    free(formatted);
    return should_print;
}

static int LogRtspBanner(HostUpCheck *check, uint16_t port,
    RegistryFindingHandler on_finding, void *context)
{
    char *response = RtspGrab(check, port);
    if (response == NULL)
        return 0;

    char *formatted = RtspResponseFmt(response);
    free(response);
    if (formatted == NULL)
        return 0;

    RegistryFinding finding = {
        .service = REGISTRY_SERVICE_RTSP,
        .ip = check->toptions->probe.addr,
        .port = port,
        .detail.rtsp = { .headers = formatted }
    };
    int should_print = on_finding == NULL || on_finding(&finding, context);
    char *ip = should_print ? Ipv4Repr(finding.ip) : NULL;
    if (ip != NULL) {
        Info("RTSP %s:%u\n%s", ip, (unsigned)port, formatted);
        free(ip);
    }
    free(formatted);
    return should_print;
}

static int LogSshBanner(HostUpCheck *check, uint16_t port,
    RegistryFindingHandler on_finding, void *context)
{
    char *response = SshGrab(check, port);
    if (response == NULL)
        return 0;

    char *formatted = SshResponseFmt(response);
    free(response);
    if (formatted == NULL)
        return 0;

    RegistryFinding finding = {
        .service = REGISTRY_SERVICE_SSH,
        .ip = check->toptions->probe.addr,
        .port = port,
        .detail.ssh = { .identification = formatted }
    };
    int should_print = on_finding == NULL || on_finding(&finding, context);
    char *ip = should_print ? Ipv4Repr(finding.ip) : NULL;
    if (ip != NULL) {
        Info("SSH %s:%u\n%s", ip, (unsigned)port, formatted);
        free(ip);
    }
    free(formatted);
    return should_print;
}

int GrabBanners(HostUpCheck *check, RegistryFindingHandler on_finding,
    void *context)
{
    if (check == NULL || check->toptions == NULL || check->result.tresult == NULL)
        return 0;

    int grabbed = 0;
    for (size_t i = 0; i < check->result.tresult->len; ++i) {
        TcpResultWrapper *result = &check->result.tresult->results[i];
        if (result->result != TCP_RESULT_SYN_ACK_RECEIVED)
            continue;
        if (IsOOB(MAX_LOOKUP_INDEX, result->port))
            continue;
        uint32_t services = SERVICES[result->port];
        if (services & SERVICE_FTP)
            grabbed += LogFtpBanner(check, result->port, on_finding, context);
        if (services & SERVICE_RTSP)
            grabbed += LogRtspBanner(check, result->port, on_finding, context);
        if (services & SERVICE_SSH)
            grabbed += LogSshBanner(check, result->port, on_finding, context);
        if (services & SERVICE_HTTP)
            grabbed += LogHttpBanner(check, result->port, on_finding, context);
    }
    return grabbed;
}
