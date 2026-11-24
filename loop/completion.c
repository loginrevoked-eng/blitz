#include "models.h"

DWORD WINAPI TcpConnectorIoCompletionWorker(LPVOID argument)
{
    (void)argument;
    TcpConnectorLoop *loop = TcpConnectorGetLoop();
    if (loop)
    {
        while(!atomic_load(&loop->control.io_completion_worker_stop))
        {
            DWORD error = TcpConnectorHandleCompletion(loop, INFINITE);
            if (error != TCP_CONNECTOR_SUCCESS)
                SetConnectorError(loop, error);
        }
        return 0;
    }
    return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;
}
