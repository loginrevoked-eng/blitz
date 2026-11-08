#include <winsock2.h>

#include "banners.h"
#include "ftp.h"
#include "http.h"
#include "lookup.h"
#include "rtsp.h"
#include "logging/logging.h"
#include "pool/cidr.h"

#include <stdlib.h>
#include <string.h>

static int LogFtpBanner(HostUpCheck *check, uint16_t port)
{
    char *response = FtpGrab(check, port);
    if (response == NULL)
        return 0;

    char *formatted = FtpResponseFmt(response);
    free(response);
    if (formatted == NULL)
        return 0;

    char *ip = Ipv4Repr(check->toptions->probe.addr);
    int logged = 0;
    if (ip != NULL) {
        Info("FTP %s:%u\n%s", ip, (unsigned)port, formatted);
        free(ip);
        logged = 1;
    }
    free(formatted);
    return logged;
}

static int LogHttpBanner(HostUpCheck *check, uint16_t port)
{
    char *response = HttpGrab(check, port);
    if (response == NULL)
        return 0;
    char *formatted = HttpResponseFmt(response);
    free(response);
    if (formatted == NULL)
        return 0;

    char *ip = Ipv4Repr(check->toptions->probe.addr);
    int logged = 0;
    if (ip != NULL) {
        Info("HTTP %s:%u\n%s", ip, (unsigned)port, formatted);
        free(ip);
        logged = 1;
    }
    free(formatted);
    return logged;
}

static int LogRtspBanner(HostUpCheck *check, uint16_t port)
{
    char *response = RtspGrab(check, port);
    if (response == NULL)
        return 0;

    char *formatted = RtspResponseFmt(response);
    free(response);
    if (formatted == NULL)
        return 0;

    char *ip = Ipv4Repr(check->toptions->probe.addr);
    int logged = 0;
    if (ip != NULL) {
        Info("RTSP %s:%u\n%s", ip, (unsigned)port, formatted);
        free(ip);
        logged = 1;
    }
    free(formatted);
    return logged;
}

int GrabBanners(HostUpCheck *check)
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
            grabbed += LogFtpBanner(check, result->port);
        if (services & SERVICE_RTSP)
            grabbed += LogRtspBanner(check, result->port);
        if (services & SERVICE_HTTP)
            grabbed += LogHttpBanner(check, result->port);
    }
    return grabbed;
}
