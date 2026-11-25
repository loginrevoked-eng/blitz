#include "internal.h"

void TcpConnectorSignalSlotAvailable(TcpConnectorLoop *loop)
{
    WakeConditionVariable(&loop->control.slot_available);
}

void TcpConnectorSignalAllSlotAvailable(TcpConnectorLoop *loop)
{
    WakeAllConditionVariable(&loop->control.slot_available);
}

BOOL TcpConnectorWaitSlotAvailableSignal(TcpConnectorLoop *loop)
{
    return SleepConditionVariableSRW(&loop->control.slot_available,
        &loop->control.state_lock, INFINITE, 0);
}

void TcpConnectorSignalSubmittersDone(TcpConnectorLoop *loop)
{
    WakeConditionVariable(&loop->control.submitters_done);
}

void TcpConnectorSignalAllSubmittersDone(TcpConnectorLoop *loop)
{
    WakeAllConditionVariable(&loop->control.submitters_done);
}

BOOL TcpConnectorWaitSubmittersDoneSignal(TcpConnectorLoop *loop)
{
    return SleepConditionVariableSRW(&loop->control.submitters_done,
        &loop->control.state_lock, INFINITE, 0);
}

void TcpConnectorSignalTaskFinished(TcpConnectorLoop *loop)
{
    WakeConditionVariable(&loop->control.task_finished);
}

void TcpConnectorSignalAllTaskFinished(TcpConnectorLoop *loop)
{
    WakeAllConditionVariable(&loop->control.task_finished);
}

BOOL TcpConnectorWaitTaskFinishedSignal(TcpConnectorLoop *loop)
{
    return SleepConditionVariableSRW(&loop->control.task_finished,
        &loop->control.state_lock, INFINITE, 0);
}

void TcpConnectorSignalCallbackAvailable(TcpConnectorLoop *loop)
{
    WakeConditionVariable(&loop->control.callback_available);
}

void TcpConnectorSignalAllCallbackAvailable(TcpConnectorLoop *loop)
{
    WakeAllConditionVariable(&loop->control.callback_available);
}

BOOL TcpConnectorWaitCallbackAvailableSignal(TcpConnectorLoop *loop)
{
    return SleepConditionVariableSRW(&loop->control.callback_available,
        &loop->control.callback_lock, INFINITE, 0);
}

