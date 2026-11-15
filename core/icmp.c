#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#include "icmp.h"

IcmpResult ProbeIcmp(const HostUpCheckProbeOptions *options)
{
    HANDLE handle = IcmpCreateFile();
    if (handle == INVALID_HANDLE_VALUE)
        return ICMP_RESULT_ERROR;

    char payload = 'x';
    char reply[sizeof(ICMP_ECHO_REPLY) + 16];
    DWORD reply_count = IcmpSendEcho(handle, (IPAddr)options->addr,
        &payload, sizeof(payload), NULL, reply, sizeof(reply), options->timeout_ms);

    IcmpResult result = ICMP_RESULT_TIMEOUT;
    if (reply_count != 0) {
        ICMP_ECHO_REPLY *echo = (ICMP_ECHO_REPLY *)reply;
        if (echo->Status == IP_SUCCESS)
            result = ICMP_RESULT_ECHO_REPLY;
        else if (echo->Status == IP_DEST_HOST_UNREACHABLE)
            result = ICMP_RESULT_UNREACHABLE;
        else
            result = ICMP_RESULT_ERROR;
    }

    IcmpCloseHandle(handle);
    return result;
}
