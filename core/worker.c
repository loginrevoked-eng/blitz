#include <winsock2.h>

#include "worker.h"
#include "probe.h"
#include "cleanup.h"
#include "telnet.h"

#include <stdatomic.h>
#include <stdlib.h>

DWORD WINAPI ProbeWorker(LPVOID argument)
{
    WorkerContext *ctx = argument;
    if (ctx == NULL)
        return 0;

    ProbeStats *stats = ctx->stats;
    ProbeConfig *config = ctx->config;
    if (stats != NULL)
        atomic_fetch_add(&stats->started, 1);
    if (ctx->start_event != NULL)
        WaitForSingleObject(ctx->start_event, INFINITE);

    Ipv4 ip;
    while (IpPicker_Pick(ctx->picker, &ip, NULL) &&
        atomic_load(&stats->checked) < config->limit) {
        if (stats != NULL) {
            atomic_fetch_add(&stats->claimed, 1);
            atomic_fetch_add(&stats->checked, 1);
        }

        const PortList *ports = config->ports;
        for (size_t i = 0; i < ports->len; ++i) {
            uint16_t port = ports->items[i];
            BOOL exhausted = FALSE;
            while(!exhausted)
            {
		char *ipr = Ipv4Repr(ip);
                Log("creating telnet session for %s", ipr);
		free(ipr);
                TelnetCreateSessionHardcodedII(ip, port, &exhausted);
            }
        }
    }

    ProbeCleanupWorkerContext(ctx);
    return 0;
}
