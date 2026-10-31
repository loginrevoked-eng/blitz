#include "icmp.h"

#include <iphlpapi.h>
#include <icmpapi.h>
#include <string.h>
#include <stdlib.h>

IcmpConfig IcmpConfig_New(void)
{
    IcmpConfig config;
    config.timeout = ICMP_DEFAULT_TIMEOUT;
    config.payload = ICMP_DEFAULT_PAYLOAD;
    config.reply_size = sizeof(ICMP_ECHO_REPLY) + strlen(ICMP_DEFAULT_PAYLOAD) + 8 + 64;
    return config;
}

IcmpResult IcmpDo(const IcmpConfig *icmp, struct in_addr addr,
                  ProbeManager *manager)
{
    IcmpResult result = { .status = ICMP_ERROR, .roundtrip = 0, .sent = 0 };

    size_t payload_len = strlen(icmp->payload);
    size_t min_size = sizeof(ICMP_ECHO_REPLY) + payload_len + 8 + 64;
    size_t reply_size = icmp->reply_size < min_size ? min_size : icmp->reply_size;
    char *reply = malloc(reply_size);
    if (reply == NULL)
        return result;

    HANDLE handle = IcmpCreateFile();
    if (handle == INVALID_HANDLE_VALUE) {
        free(reply);
        return result;
    }

    AcquireSRWLockExclusive(&manager->lock);
    if (manager->stats.initial_probe_time == 0)
        manager->stats.initial_probe_time = GetTickCount64();
    ReleaseSRWLockExclusive(&manager->lock);
    result.sent = 1;

    DWORD count = IcmpSendEcho(handle, addr.S_un.S_addr, icmp->payload,
                               (WORD)payload_len, NULL, reply,
                               (DWORD)reply_size, icmp->timeout);
    DWORD status;
    if (count > 0) {
        ICMP_ECHO_REPLY *echo = (ICMP_ECHO_REPLY *)reply;
        status = echo->Status;
        result.roundtrip = echo->RoundTripTime;
    } else {
        status = GetLastError();
    }

    switch (status) {
    case IP_SUCCESS:
        result.status = count > 0 ? ICMP_UP : ICMP_ERROR;
        break;
    case IP_REQ_TIMED_OUT:
        result.status = ICMP_NO_REPLY;
        break;
    case IP_DEST_HOST_UNREACHABLE:
    case IP_DEST_NET_UNREACHABLE:
        result.status = ICMP_UNREACHABLE;
        break;
    case IP_DEST_PROT_UNREACHABLE:
    case IP_DEST_PORT_UNREACHABLE:
        result.status = ICMP_PROHIBITED;
        break;
    default:
        result.status = count > 0 ? ICMP_NO_REPLY : ICMP_ERROR;
        break;
    }

    IcmpCloseHandle(handle);
    free(reply);
    return result;
}
