#include "repr.h"

#include "str/str.h"

#include <stdlib.h>

static const char *HostStatusName(HostUpCheckStatus status)
{
    switch (status) {
    case HOST_CHECK_STATUS_SUCCEEDED: return "succeeded";
    case HOST_CHECK_STATUS_PENDING: return "pending";
    case HOST_CHECK_STATUS_ERROR: return "error";
    case HOST_CHECK_STATUS_RETRIES_EXHAUSTED: return "retries-exhausted";
    }
    return "unknown";
}

static const char *IcmpResultName(IcmpResult result)
{
    switch (result) {
    case ICMP_RESULT_ECHO_REPLY: return "echo-reply";
    case ICMP_RESULT_TIMEOUT: return "timeout";
    case ICMP_RESULT_UNREACHABLE: return "unreachable";
    case ICMP_RESULT_PROHIBITED: return "prohibited";
    case ICMP_RESULT_ERROR: return "error";
    }
    return "unknown";
}

static const char *TcpResultName(TcpResult result)
{
    switch (result) {
    case TCP_RESULT_SYN_ACK_RECEIVED: return "syn-ack";
    case TCP_RESULT_TIMEOUT: return "timeout";
    case TCP_RESULT_RESET_RECEIVED: return "reset";
    case TCP_RESULT_PROHIBITED: return "prohibited";
    case TCP_RESULT_ERROR: return "error";
    }
    return "unknown";
}

static const char *UdpResultName(UdpResult result)
{
    switch (result) {
    case UDP_RESULT_RESPONSE_RECEIVED: return "response";
    case UDP_RESULT_TIMEOUT: return "timeout";
    case UDP_RESULT_PORT_UNREACHABLE: return "port-unreachable";
    case UDP_RESULT_PROHIBITED: return "prohibited";
    case UDP_RESULT_ERROR: return "error";
    }
    return "unknown";
}

static int Append(char **repr, const char *format, const char *value)
{
    return appendf(repr, format, value) != NULL;
}

char *HostupCheckResultRepr(HostUpCheck *check)
{
    if (check == NULL)
        return NULL;

    Ipv4 address;
    if (check->ioptions != NULL)
        address = check->ioptions->probe.addr;
    else if (check->toptions != NULL)
        address = check->toptions->probe.addr;
    else if (check->uoptions != NULL)
        address = check->uoptions->probe.addr;
    else
        return NULL;

    char *ip = Ipv4Repr(address);
    if (ip == NULL)
        return NULL;

    char *repr = strf("HostUpCheck(ip=%s, status=%s", ip,
        HostStatusName(check->result.status));
    free(ip);
    if (repr == NULL)
        return NULL;

    if (check->result.iresult != NULL &&
        !Append(&repr, ", icmp=%s", IcmpResultName(*check->result.iresult)))
        goto error;

    if (check->result.tresult != NULL) {
        for (size_t i = 0; i < check->result.tresult->len; ++i) {
            TcpResultWrapper *result = &check->result.tresult->results[i];
            if (appendf(&repr, ", tcp/%u=%s", (unsigned)result->port,
                    TcpResultName(result->result)) == NULL)
                goto error;
        }
    }

    if (check->result.uresult != NULL) {
        for (size_t i = 0; i < check->result.uresult->len; ++i) {
            UdpResultWrapper *result = &check->result.uresult->results[i];
            if (appendf(&repr, ", udp/%u=%s", (unsigned)result->port,
                    UdpResultName(result->result)) == NULL)
                goto error;
        }
    }

    if (appendf(&repr, ")") == NULL)
        goto error;
    return repr;

error:
    free(repr);
    return NULL;
}
