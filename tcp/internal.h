#ifndef TCP_CONNECTOR_INTERNAL_H
#define TCP_CONNECTOR_INTERNAL_H

#include "models.h"

Value TcpConnectorPointerValue(void *pointer);
void TcpConnectorSetError(TcpConnectorLoop *connector, DWORD error);
TcpConnectorTask *TcpConnectorFindTask(TcpConnectorLoop *connector, const char *task_id);
TcpConnectorPendingIo *TcpConnectorFindPendingIo(TcpConnectorLoop *connector, const char *task_id);
TcpConnectorSocket *TcpConnectorFindSocket(TcpConnectorLoop *connector, const char *socket_id);
void TcpConnectorCloseSocket(TcpConnectorSocket *socket);
void TcpConnectorRemovePendingIo(TcpConnectorLoop *connector, TcpConnectorPendingIo *io);
void TcpConnectorTaskReadySubmitCallback(TcpConnectorLoop *connector, TcpConnectorTask *task);
BOOL TcpConnectorTaskCallbackQueueInit(TaskCallbackQueue *queue, uint32_t initial_capacity);
BOOL TcpConnectorTaskCallbackQueuePush(TaskCallbackQueue *queue, TaskCallback callback);
BOOL TcpConnectorTaskCallbackQueuePop(TaskCallbackQueue *queue, TaskCallback *callback);
void TcpConnectorTaskCallbackQueueDestroy(TaskCallbackQueue *queue);
BOOL TcpConnectorTakeCallback(TcpConnectorLoop *connector, TaskCallback *callback);
int TcpConnectorStartTask(TcpConnectorLoop *connector, TcpConnectorTask *task);
void TcpConnectorSetTaskFinished(TcpConnectorLoop *connector, TcpConnectorTask *task,
    DWORD error, TcpConnectorStatus status);
ByteArray *TcpConnectorByteArrayNew(uint32_t capacity);
void TcpConnectorByteArrayDestroy(ByteArray *array);
TcpConnectorTask *TcpConnectorTaskNew(const char *task_id, TcpConnectorTaskOption options);
DWORD TcpConnectorHandleCompletion(TcpConnectorLoop *connector, DWORD wait_ms);
DWORD WINAPI TcpConnectorIoCompletionWorker(LPVOID argument);
DWORD WINAPI TcpConnectorTaskCallbackRunner(LPVOID argument);
TcpConnectorLoop *TcpConnectorGetLoop(void);
BOOL TcpConnectorSetLoop(TcpConnectorLoop *loop);
void TcpConnectorDetachLoop(TcpConnectorLoop *loop);
void TcpConnectorLockStateLock(TcpConnectorLoop *loop);
void TcpConnectorUnlockStateLock(TcpConnectorLoop *loop);
void TcpConnectorLockCallbackLock(TcpConnectorLoop *loop);
void TcpConnectorUnlockCallbackLock(TcpConnectorLoop *loop);
void TcpConnectorLockLoopInstanceShared(void);
void TcpConnectorUnlockLoopInstanceShared(void);
void TcpConnectorLockLoopInstanceExclusive(void);
void TcpConnectorUnlockLoopInstanceExclusive(void);
void TcpConnectorSignalSlotAvailable(TcpConnectorLoop *loop);
void TcpConnectorSignalAllSlotAvailable(TcpConnectorLoop *loop);
BOOL TcpConnectorWaitSlotAvailableSignal(TcpConnectorLoop *loop);
void TcpConnectorSignalSubmittersDone(TcpConnectorLoop *loop);
void TcpConnectorSignalAllSubmittersDone(TcpConnectorLoop *loop);
BOOL TcpConnectorWaitSubmittersDoneSignal(TcpConnectorLoop *loop);
void TcpConnectorSignalTaskFinished(TcpConnectorLoop *loop);
void TcpConnectorSignalAllTaskFinished(TcpConnectorLoop *loop);
BOOL TcpConnectorWaitTaskFinishedSignal(TcpConnectorLoop *loop);
void TcpConnectorSignalCallbackAvailable(TcpConnectorLoop *loop);
void TcpConnectorSignalAllCallbackAvailable(TcpConnectorLoop *loop);
BOOL TcpConnectorWaitCallbackAvailableSignal(TcpConnectorLoop *loop);

#endif
