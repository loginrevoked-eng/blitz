#include "internal.h"

static SRWLOCK loop_instance_lock = SRWLOCK_INIT;

void TcpConnectorLockStateLock(TcpConnectorLoop *loop)
{
    AcquireSRWLockExclusive(&loop->control.state_lock);
}

void TcpConnectorUnlockStateLock(TcpConnectorLoop *loop)
{
    ReleaseSRWLockExclusive(&loop->control.state_lock);
}

void TcpConnectorLockCallbackLock(TcpConnectorLoop *loop)
{
    AcquireSRWLockExclusive(&loop->control.callback_lock);
}

void TcpConnectorUnlockCallbackLock(TcpConnectorLoop *loop)
{
    ReleaseSRWLockExclusive(&loop->control.callback_lock);
}

void TcpConnectorLockLoopInstanceShared(void)
{
    AcquireSRWLockShared(&loop_instance_lock);
}

void TcpConnectorUnlockLoopInstanceShared(void)
{
    ReleaseSRWLockShared(&loop_instance_lock);
}

void TcpConnectorLockLoopInstanceExclusive(void)
{
    AcquireSRWLockExclusive(&loop_instance_lock);
}

void TcpConnectorUnlockLoopInstanceExclusive(void)
{
    ReleaseSRWLockExclusive(&loop_instance_lock);
}

