#ifndef CORE_UPCHECK_MODELS_H
#define CORE_UPCHECK_MODELS_H

#include <windows.h>
#include <stdint.h>
#include "../pool/cidr.h"



typedef enum {
    ICMP_RESULT_ECHO_REPLY,
    ICMP_RESULT_TIMEOUT,
    ICMP_RESULT_UNREACHABLE,
    ICMP_RESULT_PROHIBITED,
    ICMP_RESULT_ERROR
} IcmpResult;


typedef enum {
    UDP_RESULT_RESPONSE_RECEIVED,
    UDP_RESULT_TIMEOUT,
    UDP_RESULT_PORT_UNREACHABLE,
    UDP_RESULT_PROHIBITED,
    UDP_RESULT_ERROR
} UdpResult;

typedef enum {
    TCP_RESULT_SYN_ACK_RECEIVED,
    TCP_RESULT_TIMEOUT,
    TCP_RESULT_RESET_RECEIVED,
    TCP_RESULT_PROHIBITED,
    TCP_RESULT_ERROR
} TcpResult;

typedef enum {
    HOST_CHECK_STATUS_SUCCEEDED,
    HOST_CHECK_STATUS_PENDING,
    HOST_CHECK_STATUS_ERROR,
    HOST_CHECK_STATUS_RETRIES_EXHAUSTED,
} HostUpCheckStatus;




typedef struct {
    Ipv4 addr;
    DWORD timeout_ms;
    unsigned int retries;
} HostUpCheckProbeOptions;


typedef struct {
    uint16_t *items;
    size_t len;
} PortList;

typedef struct {
    HostUpCheckProbeOptions probe;
} IcmpOptions;


typedef struct{
    HostUpCheckProbeOptions probe;
    PortList ports;
}NonIcmpOptions;


typedef struct {
    uint16_t port;
    UdpResult result;
    unsigned int retries;
}UdpResultWrapper;

typedef struct {
    uint16_t port;
    TcpResult result;
    unsigned int retries;
}TcpResultWrapper;

typedef struct {
    UdpResultWrapper *results;
    size_t len;
} UdpResultList;

typedef struct {
    TcpResultWrapper *results;
    size_t len;
} TcpResultList;

typedef enum {
    HOST_CHECK_METHOD_UDP = 1u << 0,
    HOST_CHECK_METHOD_TCP = 1u << 1,
    HOST_CHECK_METHOD_ICMP = 1u << 2,
} HostUpCheckMethod;

typedef struct {
    HostUpCheckMethod method;
    IcmpOptions icmp_options;
    NonIcmpOptions udp_options;
    NonIcmpOptions tcp_options;
} HostupCheckOptions;

#endif
