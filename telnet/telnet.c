#include "credit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "connector.h"
#include "libtelnet.h"
#include "telnet.h"
#include "cidr.h"

#define TMO  3000

typedef enum { S_LOGIN, S_PASS, S_AFTER, S_COLLECT } State;

typedef struct {
    telnet_t *tn;
    State st;
    LONG finishing;
    int send_pending;
    int seq;
    char sock_id[48];
    char active_task_id[64];
    char *username;
    char *password;
    char tail[256];
    size_t tail_len;
    BOOL login_rejected;
    uint8_t rxbuf[4096], txbuf[1024];
    ByteArray rx, tx;
} Session;

static BOOL OnRecv(TcpConnectorTask *task);

static INIT_ONCE sessions_once = INIT_ONCE_STATIC_INIT;
static SRWLOCK sessions_lock = SRWLOCK_INIT;
static HANDLE sessions_idle;
static LONG active_sessions;

static BOOL CALLBACK initialize_sessions(PINIT_ONCE once, PVOID parameter,
    PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;
    sessions_idle = CreateEventA(NULL, TRUE, TRUE, NULL);
    return sessions_idle != NULL;
}

static BOOL sessions_init(void)
{
    return InitOnceExecuteOnce(&sessions_once, initialize_sessions, NULL, NULL);
}

static BOOL session_started(void)
{
    if (!sessions_init())
        return FALSE;
    AcquireSRWLockExclusive(&sessions_lock);
    ResetEvent(sessions_idle);
    ++active_sessions;
    ReleaseSRWLockExclusive(&sessions_lock);
    return TRUE;
}

static void session_stopped(void)
{
    AcquireSRWLockExclusive(&sessions_lock);
    if (--active_sessions == 0)
        SetEvent(sessions_idle);
    ReleaseSRWLockExclusive(&sessions_lock);
}

BOOL TelnetWaitForSessions(DWORD timeout_ms)
{
    return sessions_init() &&
        WaitForSingleObject(sessions_idle, timeout_ms) == WAIT_OBJECT_0;
}

static char *copy_string(const char *value)
{
    size_t size = strlen(value) + 1;
    char *copy = malloc(size);
    if (copy != NULL)
        memcpy(copy, value, size);
    return copy;
}

static void free_session(Session *s)
{
    telnet_free(s->tn);
    free(s->username);
    free(s->password);
    free(s);
    session_stopped();
}

static DWORD WINAPI reap_session(LPVOID argument)
{
    Session *s = argument;
    if (s->active_task_id[0])
        TcpConnectorAwaitTask(s->active_task_id);
    TcpConnectorCloseSocketById(s->sock_id);
    free_session(s);
    return 0;
}

static void finish(Session *s, DWORD result)
{
    if (InterlockedCompareExchange(&s->finishing, 1, 0) != 0)
        return;

    if (result != TCP_CONNECTOR_SUCCESS)
        fprintf(stderr, "telnet session ended: error=%lu\n", result);

    HANDLE reaper = CreateThread(NULL, 0, reap_session, s, 0, NULL);
    if (reaper != NULL) {
        CloseHandle(reaper);
        return;
    }

    fprintf(stderr, "could not start Telnet cleanup thread: %lu\n", GetLastError());
    reap_session(s);
}

static int task_failed(const TcpConnectorTask *task)
{
    return task->status != TCP_CONNECTOR_TASK_COMPLETED || task->WSA_error != 0;
}

static BOOL submit(Session *s, TcpConnectorTaskOptionKind kind, ByteArray *buf)
{
    TcpConnectorTask task;
    snprintf(s->active_task_id, sizeof s->active_task_id,
        "%s-%d", s->sock_id, s->seq++);
    TcpConnectorTaskInit(&task);
    if (!TcpConnectorTaskSetKind(&task, kind) ||
        !TcpConnectorTaskSetId(&task, s->active_task_id) ||
        !TcpConnectorTaskSetTimeout(&task, TMO) ||
        !TcpConnectorTaskSetSocketId(&task, s->sock_id) ||
        !TcpConnectorTaskSetPayload(&task, buf))
        return FALSE;
    TcpConnectorTaskSetCallback(&task, OnRecv, s);
    return TcpConnectorCreateTask(task.options, task.id,
        task.on_completed, task.userdata) == TCP_CONNECTOR_SUCCESS;
}

static void arm_recv(Session *s)
{
    s->rx.len = 0;
    s->rx.cap = sizeof s->rxbuf;
    if (!submit(s, TCP_CONNECTOR_TASK_RECV, &s->rx))
        finish(s, TCP_CONNECTOR_ERROR_IO);
}

static BOOL contains_text_n(const char *text, size_t text_len, const char *needle);

static void on_telnet(telnet_t *tn, telnet_event_t *event, void *userdata)
{
    Session *s = userdata;
    (void)tn;
    if (event->type == TELNET_EV_SEND) {
        if (s->tx.len + event->data.size <= s->tx.cap) {
            memcpy(s->tx.buffer + s->tx.len, event->data.buffer, event->data.size);
            s->tx.len += (uint32_t)event->data.size;
        }
    } else if (event->type == TELNET_EV_DATA) {
        if (s->st == S_COLLECT) {
            if (contains_text_n(event->data.buffer, event->data.size, "incorrect") ||
                contains_text_n(event->data.buffer, event->data.size,
                    "authentication failed"))
                s->login_rejected = TRUE;
            else
                fwrite(event->data.buffer, 1, event->data.size, stdout);
        } else {
            size_t n = event->data.size;
            size_t room = sizeof s->tail - 1 - s->tail_len;
            if (n > room)
                n = room;
            memcpy(s->tail + s->tail_len, event->data.buffer, n);
            s->tail_len += n;
            s->tail[s->tail_len] = 0;
        }
    }
}

static void say(Session *s, const char *line)
{
    telnet_send(s->tn, line, strlen(line));
    telnet_send(s->tn, "\r\n", 2);
    s->tail_len = 0;
    s->tail[0] = 0;
}

static BOOL contains_text_n(const char *text, size_t text_len, const char *needle)
{
    size_t needle_len = strlen(needle);
    if (needle_len > text_len)
        return FALSE;
    for (size_t start = 0; start <= text_len - needle_len; ++start) {
        size_t i = 0;
        while (i < needle_len &&
            tolower((unsigned char)text[start + i]) == tolower((unsigned char)needle[i]))
            ++i;
        if (i == needle_len)
            return TRUE;
    }
    return FALSE;
}

static BOOL contains_text(const char *text, const char *needle)
{
    return contains_text_n(text, strlen(text), needle);
}

static BOOL is_blank_tail(const Session *s)
{
    for (size_t i = 0; i < s->tail_len; ++i)
        if (!isspace((unsigned char)s->tail[i]))
            return FALSE;
    return TRUE;
}

static BOOL is_password_echo(const Session *s)
{
    size_t first = 0;
    size_t last = s->tail_len;
    size_t password_len = strlen(s->password);
    while (first < last && (s->tail[first] == '\r' || s->tail[first] == '\n'))
        ++first;
    while (last > first && (s->tail[last - 1] == '\r' || s->tail[last - 1] == '\n'))
        --last;
    return last - first == password_len &&
        memcmp(s->tail + first, s->password, password_len) == 0;
}

static BOOL OnRecv(TcpConnectorTask *task)
{
    Session *s = task->userdata;

    if (task_failed(task)) {
        if (s->st == S_COLLECT && task->status == TCP_CONNECTOR_TASK_CANCELED &&
            task->WSA_error == WSAETIMEDOUT) {
            finish(s, TCP_CONNECTOR_SUCCESS);
        } else {
            fprintf(stderr, "task failed: status=%d wsa=%lu state=%d\n",
                task->status, task->WSA_error, s->st);
            finish(s, task->WSA_error ? task->WSA_error : TCP_CONNECTOR_ERROR_IO);
        }
        return TRUE;
    }

    if (s->send_pending) {
        s->send_pending = 0;
        s->tx.len = 0;
        arm_recv(s);
        return TRUE;
    }

    if (s->rx.len == 0) {
        finish(s, s->st == S_COLLECT ? TCP_CONNECTOR_SUCCESS : TCP_CONNECTOR_ERROR_IO);
        return TRUE;
    }

    telnet_recv(s->tn, (const char *)s->rx.buffer, s->rx.len);
    if (s->login_rejected) {
        fprintf(stderr, "login rejected by server\n");
        finish(s, TCP_CONNECTOR_INVALID_DATA);
        return TRUE;
    }

    switch (s->st) {
    case S_LOGIN:
        if (strstr(s->tail, "ogin") || strstr(s->tail, "ser")) {
            say(s, s->username);
            s->st = S_PASS;
        }
        break;
    case S_PASS:
        if (strstr(s->tail, "ass")) {
            say(s, s->password);
            s->st = S_AFTER;
        }
        break;
    case S_AFTER:
        if (contains_text(s->tail, "incorrect") ||
            contains_text(s->tail, "authentication failed")) {
            fprintf(stderr, "login rejected by server\n");
            finish(s, TCP_CONNECTOR_INVALID_DATA);
            return TRUE;
        }
        if (is_password_echo(s)) {
            s->tail_len = 0;
            s->tail[0] = 0;
            break;
        }
        if (is_blank_tail(s)) {
            s->tail_len = 0;
            s->tail[0] = 0;
            break;
        }
        s->st = S_COLLECT;
        break;
    case S_COLLECT:
        break;
    }

    if (s->tx.len) {
        s->send_pending = 1;
        if (!submit(s, TCP_CONNECTOR_TASK_SEND, &s->tx))
            finish(s, TCP_CONNECTOR_ERROR_IO);
    } else {
        arm_recv(s);
    }
    return TRUE;
}

static BOOL OnConnect(TcpConnectorTask *task)
{
    Session *s = task->userdata;
    if (task_failed(task)) {
        fprintf(stderr, "connect failed: status=%d wsa=%lu\n",
            task->status, task->WSA_error);
        finish(s, task->WSA_error ? task->WSA_error : TCP_CONNECTOR_ERROR_IO);
    } else {
        arm_recv(s);
    }
    return TRUE;
}

BOOL TelnetCreateSession(char *ip, uint16_t port, char *username, char *password)
{
    static volatile LONG counter;
    static const telnet_telopt_t options[] = { { -1, 0, 0 } };

    if (ip == NULL || username == NULL || password == NULL || port == 0 ||
        !TcpConnectorGet())
        return FALSE;

    Session *s = calloc(1, sizeof *s);
    if (s == NULL)
        return FALSE;

    s->username = copy_string(username);
    s->password = copy_string(password);
    if (s->username == NULL || s->password == NULL) {
        free(s->username);
        free(s->password);
        free(s);
        return FALSE;
    }

    s->rx.buffer = s->rxbuf;
    s->rx.cap = sizeof s->rxbuf;
    s->tx.buffer = s->txbuf;
    s->tx.cap = sizeof s->txbuf;
    snprintf(s->sock_id, sizeof s->sock_id, "telnet-%ld",
        InterlockedIncrement(&counter));
    s->tn = telnet_init(options, on_telnet, 0, s);
    if (s->tn == NULL || !session_started()) {
        if (s->tn != NULL)
            telnet_free(s->tn);
        free(s->username);
        free(s->password);
        free(s);
        return FALSE;
    }

    TcpConnectorTask task;
    TcpConnectorTaskInit(&task);
    if (!TcpConnectorTaskSetKind(&task, TCP_CONNECTOR_TASK_CONNECT) ||
        !TcpConnectorTaskSetId(&task, s->sock_id) ||
        !TcpConnectorTaskSetAddr(&task, ip) ||
        !TcpConnectorTaskSetPort(&task, port) ||
        !TcpConnectorTaskSetTimeout(&task, TMO)) {
        telnet_free(s->tn);
        free(s->username);
        free(s->password);
        free(s);
        session_stopped();
        return FALSE;
    }

    snprintf(s->active_task_id, sizeof s->active_task_id, "%s", s->sock_id);
    TcpConnectorTaskSetCallback(&task, OnConnect, s);
    if (TcpConnectorCreateTask(task.options, task.id,
            task.on_completed, task.userdata) != TCP_CONNECTOR_SUCCESS) {
        telnet_free(s->tn);
        free(s->username);
        free(s->password);
        free(s);
        session_stopped();
        return FALSE;
    }

    return TRUE;
}

BOOL TelnetCreateSessionHardcoded(char *ip, uint16_t port, BOOL *exhaused)
{
    Cred c = {0};
    if (exhaused == NULL)
        return FALSE;
    *exhaused = FALSE;
    if (!GetNextCred(&c)) {
        *exhaused = TRUE;
        return TRUE;
    }
    return TelnetCreateSession(ip, port, c.username, c.password);
}

BOOL TelnetCreateSessionHardcodedII(uint64_t ip, uint16_t port, BOOL *exhaused)
{
    if (exhaused == NULL)
        return FALSE;
    char *address = Ipv4Repr(ip);
    if (address == NULL) {
        *exhaused = TRUE;
        return FALSE;
    }
    BOOL exhausted = FALSE;
    BOOL queued = TelnetCreateSessionHardcoded(address, port, &exhausted);
    *exhaused = exhausted || !queued;
    free(address);
    return queued;
}

