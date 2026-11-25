#include "internal.h"
#include <stdlib.h>
#include <string.h>

static TcpConnectorLoop *loop_instance;

TcpConnector *TcpConnectorGet(void)
{
    return TcpConnectorGetLoop();
}

TcpConnectorLoop *TcpConnectorGetLoop(void)
{
    TcpConnectorLockLoopInstanceShared();
    TcpConnectorLoop *loop = loop_instance;
    TcpConnectorUnlockLoopInstanceShared();
    return loop;
}

BOOL TcpConnectorSetLoop(TcpConnectorLoop *loop)
{
    TcpConnectorLockLoopInstanceExclusive();
    BOOL available = loop_instance == NULL;
    if (available)
        loop_instance = loop;
    TcpConnectorUnlockLoopInstanceExclusive();
    return available;
}

void TcpConnectorDetachLoop(TcpConnectorLoop *loop)
{
    TcpConnectorLockLoopInstanceExclusive();
    if (loop_instance == loop)
        loop_instance = NULL;
    TcpConnectorUnlockLoopInstanceExclusive();
}




