#include "dbg.h"
#include <stddef.h>
#include <synchapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>

#include "logging.h"
#include "pcap/pcap.h"
#include "pcap_i.h"
#include "picker/cidr.h"
#include "picker/init.h"
#include "picker/picker.h"
#include "pkt.h"
#include "route.h"
#include "scan.h"
#include "picker/init.h"

#include <stddef.h>
#include <stdint.h>
#include "list.h"
#include "str.h"

#include "hashset/hashset.h"

typedef enum{
    SYN_SCAN_QUIT_RX_DIED,
    SYN_SCAN_QUIT_TX_DIED,
    SYN_SCAN_QUIT_CTRL_C,
    SYN_SCAN_QUIT_POOL_EXHAUSED,
    SYN_SCAN_QUIT_FINISHED,
}SynScanQuitReason;

typedef struct{
    HANDLE event;
    SynScanQuitReason reason;
    char *msg;
}QuitEventWrapper;


typedef struct{
    float pps;
}syn_scan_settings_t;

typedef struct {
    scan_target_list_t     open;
    scan_target_list_t     alive;
    SRWLOCK                lock;
    HANDLE                 tx_stop;
    QuitEventWrapper       quit;
    HANDLE                 rx_stop;
    u16_list_t             ports;
    SynScannerHashSet      *seen;
    IpPicker               *picker;
    HANDLE                 rxth;
    HANDLE                 txth;
    struct route_t         route;
    syn_scan_config_t      cfg;
    syn_scan_settings_t    settings;
} SynScanCtx;



void syn_scan_trigger_quit(SynScanCtx *ctx, SynScanQuitReason reason, const char *fmt, ...) {
    AcquireSRWLockExclusive(&ctx->lock);
    if (WaitForSingleObject(ctx->quit.event, 0) != WAIT_OBJECT_0)
    {
        if (fmt) {
            va_list ap;
            va_start(ap, fmt);
            ctx->quit.msg = strfv(fmt, ap);
            va_end(ap);
        } else {
            ctx->quit.msg = NULL;
        }

        ctx->quit.reason = reason;
        SetEvent(ctx->quit.event);
    }
    ReleaseSRWLockExclusive(&ctx->lock);
}


DWORD WINAPI syn_scan_rx(LPVOID arg)
{
    SynScanCtx *ctx = (SynScanCtx *)arg;

    char err[PCAP_ERRBUF_SIZE];
    pcap_t *rx_hndl = open_handle(&ctx->route, err, PCAP_ERRBUF_SIZE);
    if (!rx_hndl)
    {
        syn_scan_trigger_quit(ctx, SYN_SCAN_QUIT_RX_DIED, "RX thread failed to open pcap handle, %s", err);
        return 1;
    }

    set_filters(rx_hndl, ctx->cfg.src_port);

    HANDLE pkt_event = pcap_getevent(rx_hndl);
    HANDLE wait_handles[2] = { ctx->rx_stop, pkt_event };

    for(;;)
    {
        DWORD w = WaitForMultipleObjects(2, wait_handles, FALSE, 100);
        if (w == WAIT_OBJECT_0)
        {
            break;
        }

        scan_target_t host;
        int status;//-2=pcap error, -1=our error, 1=not ours, 0=alive
        int is_open = 0;
        while (1)
        {
            is_open = 0;
            status = recv_pcap_pkt(&ctx->cfg, rx_hndl, &host, &is_open);
            if (status == -2)
                break;
            if(status == -3)
            {
                error("pcap failed to read..");
                break;
            }
            if(status == -1)
            {
                error("timeout while reading from pcap...");
                break;
            }
            if (status != 0)
                continue;

            uint64_t key = scan_target_u64_cannonical_key(&host);
            AcquireSRWLockExclusive(&ctx->lock);
            if (SynScannerHashSet_GetU64(ctx->seen, key))
            {
                ReleaseSRWLockExclusive(&ctx->lock);
                continue;
            }

            if (!SynScannerHashSet_PutU64(ctx->seen, key))
            {
                ReleaseSRWLockExclusive(&ctx->lock);
                error("failed to record scan response in the seen hashset");
                continue;
            }

            scan_target_list_t *alive_p = &ctx->alive;
            list_push(&alive_p, host);
            if (is_open)
            {
                scan_target_list_t *open_p = &ctx->open;
                list_push(&open_p, host);
            }
            ReleaseSRWLockExclusive(&ctx->lock);

            char ip_str[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, host.ip, ip_str, sizeof(ip_str));
            info("the host at %s is alive and the port %u is %s.", ip_str,
                 host.port, is_open ? "open" : "fucking closed!");
        }
    }

    pcap_close(rx_hndl);
    return 0;
}

DWORD WINAPI syn_scan_tx(LPVOID arg)
{
    SynScanCtx *ctx = (SynScanCtx *)arg;

    char err[PCAP_ERRBUF_SIZE];
    pcap_t *tx_hndl = open_handle(&ctx->route, err, PCAP_ERRBUF_SIZE);
    int total_pkts = 0;
    if (!tx_hndl)
    {
        syn_scan_trigger_quit(ctx, SYN_SCAN_QUIT_TX_DIED, "TX thread failed to open pcap handle, %s", err);
        return 1;
    }

    while (WaitForSingleObject(ctx->tx_stop, 0) != WAIT_OBJECT_0)
    {
        if (total_pkts % 10 == 0) Sleep(10);

        Ipv4 target_ip = 0;

        int err = IpPicker_Pick(ctx->picker, &target_ip, NULL); //has internal lock...so it doesnt need relocking
        Symbol syms[] = { SYM(target_ip), SYM(err) };
        debug_brk(syms, 2);
        if(!err)
        {
            syn_scan_trigger_quit(ctx, SYN_SCAN_QUIT_POOL_EXHAUSED, "IP pool exhausted or failed to pick somehow... err=%d", err);
            break;
        }

        for (uint32_t i = 0; i < ctx->ports.len; i++)
        {
            if (WaitForSingleObject(ctx->tx_stop, 0) == WAIT_OBJECT_0)
            {
                break;
            }
            if (send_pkt(tx_hndl, &ctx->cfg, target_ip, ctx->ports.items[i]) != 0)
            {
                syn_scan_trigger_quit(ctx, SYN_SCAN_QUIT_TX_DIED, "TX thread failed to send packet");
                pcap_close(tx_hndl);
                return 1;
            }
            total_pkts++;
        }
    }

    syn_scan_trigger_quit(ctx, SYN_SCAN_QUIT_FINISHED, "%d SYNs sent..", total_pkts);
    pcap_close(tx_hndl);
    return 0;
}


void syn_scan_free_ctx(SynScanCtx *ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->txth)  CloseHandle(ctx->txth);
    if (ctx->rxth)  CloseHandle(ctx->rxth);
    if (ctx->tx_stop)    CloseHandle(ctx->tx_stop);
    if (ctx->rx_stop)    CloseHandle(ctx->rx_stop);
    if (ctx->quit.event) CloseHandle(ctx->quit.event);

    free(ctx->quit.msg);
    list_frees(&ctx->open);
    list_frees(&ctx->alive);
    u16_list_frees(&ctx->ports);
    SynScannerHashSet_Free(ctx->seen);
    memset(ctx, 0, sizeof(*ctx));
}



int syn_scan_init_ctx(SynScanCtx *ctx, IpPicker *picker, struct route_t *route, const syn_scan_config_t *cfg, u16_list_t ports)
{
    if (!ctx || !picker || !route || !cfg || !ports.items || ports.len == 0) {
        return 1;
    }

    memset(ctx, 0, sizeof(*ctx));

    ctx->picker = picker;
    ctx->route  = *route;
    ctx->cfg    = *cfg;
    ctx->ports  = ports;
    ctx->seen = SynScannerHashSet_New(65535);
    InitializeSRWLock(&ctx->lock);

    if (list_inits(&ctx->open, 64) != 0)
    {
        goto fail;
    }

    if (list_inits(&ctx->alive, 64) != 0) {
        goto fail;
    }

    ctx->tx_stop    = CreateEvent(NULL, TRUE, FALSE, NULL);
    ctx->rx_stop    = CreateEvent(NULL, TRUE, FALSE, NULL);
    ctx->quit.event = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (!ctx->tx_stop || !ctx->rx_stop || !ctx->quit.event)
    {
        goto fail;
    }

    ctx->rxth = CreateThread(NULL, 0, syn_scan_rx, ctx, 0, NULL);
    if (!ctx->rxth)
    {
        goto fail;
    }

    ctx->txth = CreateThread(NULL, 0, syn_scan_tx, ctx, 0, NULL);
    if (!ctx->txth)
    {
        SetEvent(ctx->rx_stop);
        WaitForSingleObject(ctx->rxth, INFINITE);
        goto fail;
    }

    return 0;

fail:
    syn_scan_free_ctx(ctx);
    return 1;
}



static SynScanCtx *g_ctx = NULL;

static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT)
    {
        if (g_ctx)
        {
            syn_scan_trigger_quit(g_ctx, SYN_SCAN_QUIT_CTRL_C, "Ctrl+C pressed by this fuckass user :(");
        }
        return TRUE;
    }
    return FALSE;
}

int main(void)
{
    require_wpcap();

    struct route_t route;
    if (!route_init(&route)) {
        panic(-1, "No active Ethernet route with a reachable gateway found.");
    }

    char out[1024];
    route_sprintf(&route, out, sizeof(out));
    info("Selected route:\n%s", out);

    const uint8_t key[] = "change-me-secret-key";

    syn_scan_config_t cfg;
    syn_cfg_init(&cfg, &route, 8080, key, sizeof(key) - 1);

    // 1. Build port list
    u16_list_t ports;
    u16_list_t *ports_p = &ports;
    u16_list_inits(&ports, 4);
    for(uint16_t port = 1;port < 65535;port++)
    {
        u16_list_push(&ports_p, port);
    }


    IpPicker *picker = IpPicker_InitSimple("10.4.81.1/32", 16, 5);

    SynScanCtx ctx;
    if (syn_scan_init_ctx(&ctx, picker, &route, &cfg, ports) != 0)
    {
        error("Failed to initialize scan context.");
        u16_list_frees(&ports);
        return 1;
    }

    g_ctx = &ctx;
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    info("Scanner running. Press Ctrl+C to abort...");


    WaitForSingleObject(ctx.quit.event, INFINITE);
    info("Quit triggered (reason=%d): %s", ctx.quit.reason, ctx.quit.msg ? ctx.quit.msg : "");

    SetEvent(ctx.tx_stop);
    WaitForSingleObject(ctx.txth, INFINITE);

    if (ctx.quit.reason == SYN_SCAN_QUIT_POOL_EXHAUSED || ctx.quit.reason == SYN_SCAN_QUIT_FINISHED) {
        info("Waiting 1500ms for in-flight replies...");
        Sleep(1500);
    }

    SetEvent(ctx.rx_stop);
    WaitForSingleObject(ctx.rxth, INFINITE);

    g_ctx = NULL;

    info("Scan complete: %u open ports, %u alive responses.", ctx.open.len, ctx.alive.len);
    scan_target_t res;
    while (list_pop(&ctx.open, &res) == 0)
    {
        char ip_str[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, res.ip, ip_str, sizeof(ip_str));
        info("OPEN -> %s:%u", ip_str, res.port);
    }

    syn_scan_free_ctx(&ctx);
    IpPicker_Finalize(picker);

    debug("Scanner exited cleanly.");
    return 0;
}
