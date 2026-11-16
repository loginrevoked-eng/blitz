#include <minwinbase.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <stdint.h>
#include <windows.h>
#include "core/models.h"
#include "core/probe.h"
#include "pool/cidr.h"




typedef enum{
    COMPLETED = 1,
    PENDING = 2
}IocpProbeTaskStatus;

typedef struct {
    HostUpCheckProbeOptions options;
    Ipv4 addr;
    uint32_t port;
}IocpProbeOption;

typedef struct {
    IocpProbeOption options;
    SOCKET             sock;
    IocpProbeTaskStatus status;
}IocpProbeTask;


typedef struct {
    uint32_t pending;
    uint32_t tasks;
}PoolSatus;

typedef struct{
    IocpProbeTask *tasks;
    uint32_t len;
    uint32_t cap;
}IocpProbeTaskList;

typedef struct {
    OVERLAPPED *idontknowwhatthisisforbutitsarrayofthem;
    uint32_t len;
    uint32_t cap;
}OVERLAPPED_List;

typedef struct {
    IocpProbeTaskList tasks;
    HANDLE completion_port;
    PoolSatus status;
    LPFN_CONNECTEX connect_ex;
    OVERLAPPED_List overlapped;
}Pool;
