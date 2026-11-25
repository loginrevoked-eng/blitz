// main.c
#include <stdio.h>
#include <stdlib.h>
#include "connector.h"
#include "telnet.h"

int main(int argc, char **argv) {
    char *ip = argc > 1 ? argv[1] : "37.219.244.208";
    uint16_t port = argc > 2 ? (uint16_t)atoi(argv[2]) : 23;

    if (TcpConnectorInitEventloop(8, TCP_CONNECTOR_WITH_IO_COMPLETION_WORKER, 64) != TCP_CONNECTOR_SUCCESS) {
        fprintf(stderr, "init failed\n");
        return 2;
    }
    BOOL exhausted = FALSE;
    if (!TelnetCreateSessionHardcoded(ip, port, &exhausted)) {
        fprintf(stderr, "could not submit Telnet connection\n");
        TcpConnectorClearEventloop();
        return 1;
    }
    if (exhausted) {
        TcpConnectorClearEventloop();
        return 0;
    }
    TelnetWaitForSessions(INFINITE);
    TcpConnectorClearEventloop();
    return 0;
}
