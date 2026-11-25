# TCP Connector: Protocol Adapter Context

This document gives an AI or developer the integration boundary for building application-layer libraries on this TCP/IOCP connector. It describes how to submit TCP work and receive its results. It does not specify how HTTPS, Telnet, or SSH are implemented.

## What the connector provides

The connector owns TCP sockets and performs asynchronous connect, send, and receive tasks through an IO completion port. A protocol library owns its per-connection state, decides which bytes to send or receive next, and interprets the returned bytes. HTTPS, Telnet, and SSH adapters all use the same connector task API; their protocol-specific behavior stays in their own modules.

Use `public.h` as the interface. Keep protocol state and helper APIs in the protocol library. Do not depend on `models.h` or other connector internals.

## Start and stop the event loop

Initialize the connector once before creating protocol sessions:

```c
DWORD error = TcpConnectorInitEventloop(
    max_pending, TCP_CONNECTOR_WITH_IO_COMPLETION_WORKER, callback_queue_capacity);
if (error != TCP_CONNECTOR_SUCCESS) {
    /* handle initialization failure */
}

TcpConnector *connector = TcpConnectorGet();
```

`max_pending` limits concurrent I/O operations. The IO completion worker flag starts a worker that processes IOCP completions; the callback runner is started by initialization and executes callbacks one at a time. The connector is a process-wide singleton, so initialize one active event loop and clear it after sessions have finished:

```c
error = TcpConnectorClearEventloop();
```

Cleanup cancels outstanding work, drains completions, closes sockets, stops workers, and releases Winsock resources. Do not clear the loop while protocol libraries still use it.

## Submit TCP tasks

Tasks are `CONNECT`, `SEND`, or `RECV`. Initialize a task, set its kind first, then set the fields for that kind. Submit it with `TcpConnectorCreateTask`:

```c
TcpConnectorTask task;
TcpConnectorTaskInit(&task);
TcpConnectorTaskSetKind(&task, TCP_CONNECTOR_TASK_CONNECT);
TcpConnectorTaskSetId(&task, "session-42-connect");
TcpConnectorTaskSetAddr(&task, ipv4_address);
TcpConnectorTaskSetPort(&task, port);
TcpConnectorTaskSetTimeout(&task, timeout_ms);
TcpConnectorTaskSetCallback(&task, on_connect, session_state);

error = TcpConnectorCreateTask(task.options, task.id,
    task.on_completed, task.userdata);
```

The setters return `BOOL`; check them before submission. Task IDs must be unique for the lifetime of the event loop. Use stable, library-specific IDs for each connection and operation. Connect task IDs also identify the resulting socket: use that ID with `TcpConnectorTaskSetSocketId` on later send and receive tasks.

For send and receive tasks, provide a `ByteArray *` with `TcpConnectorTaskSetPayload`. A send requires a non-empty `len`; a receive requires non-zero `cap`. A receive fills the supplied buffer and updates its `len`. Each task represents one connector operation; the protocol library decides when to submit the next one.

## Completion and memory ownership

An optional `TcpConnectorCompletionCallback` receives the completed task. It can inspect `task->id`, `task->status`, `task->WSA_error`, and `task->userdata`, then submit further TCP tasks. Callbacks run serially on the single callback runner. Keep them short and avoid blocking while waiting for additional network work; use them to update session state and schedule the next task.

The connector copies task IDs and socket IDs when it stores a task. It borrows the payload buffer, callback, and `userdata`: keep those objects alive until the operation and its callback have finished. Do not reuse or free a receive buffer while its receive is pending. `TcpConnectorTaskDup` copies a stack task to heap storage; it still borrows payload, callback, and userdata pointers.

## Waiting, cancellation, and socket closure

`TcpConnectorAwaitTask(task_id)` waits for one task and returns its Winsock result. `TcpConnectorTasksWaitAny(connector, ids, count, cache)` waits until one listed task settles and returns its ID. When a cache is supplied, the function uses it across calls and removes the returned ID from it; pass the reduced ID array/count on the next call. Pass `NULL` for `cache` when a temporary set for one call is sufficient. Do not manually remove the returned ID from a cache.

Use `TcpConnectorCancelTask(task_id)` to cancel an operation and `TcpConnectorCloseSocketById(socket_id)` to close a connection when the protocol session is done. Check task creation and wait results; a completed task can still carry a non-success `WSA_error`.

## Boundary for HTTPS, Telnet, and SSH adapters

For each adapter, keep protocol-specific state in that adapter and use connector tasks only for TCP transport. Submit a connect task, then use its callback to arrange the adapter's send and receive tasks. Use the supplied `userdata` pointer to associate completions with the correct session. Report bytes and errors back through the adapter's own API.

The connector does not provide hostname resolution, TLS, protocol parsing, authentication, or session policy. Keep those responsibilities in the application-layer library. The connector's address setter accepts an IPv4 address string; resolve hostnames outside the connector before submitting a connect task.
