#include "uibridge.h"

#include "str/str.h"

#include <stdlib.h>
#include <string.h>

static char *ProbeStatsRepr(const ProbeStats *stats, uint64_t cidr_length)
{
    uint64_t percent_hundredths = cidr_length == 0 ? 0 :
        (uint64_t)(((long double)stats->probed * 10000.0L) / cidr_length);
    if (percent_hundredths > 10000)
        percent_hundredths = 10000;
    uint64_t elapsed_seconds = stats->initial_probe_time == 0 ? 0 :
        (GetTickCount64() - stats->initial_probe_time) / 1000;
    uint64_t hours = elapsed_seconds / 3600;
    uint64_t minutes = (elapsed_seconds % 3600) / 60;
    uint64_t seconds = elapsed_seconds % 60;

    return strf("Probed %llu of %llu IP addresses (%llu.%02llu%%); %llu %s up; elapsed %02llu:%02llu:%02llu",
                (unsigned long long)stats->probed,
                (unsigned long long)cidr_length,
                (unsigned long long)(percent_hundredths / 100),
                (unsigned long long)(percent_hundredths % 100),
                (unsigned long long)stats->up,
                stats->up == 1 ? "is" : "are",
                (unsigned long long)hours,
                (unsigned long long)minutes,
                (unsigned long long)seconds);
}

static void UIBridgeRefresh(UIBridge *ui, IpPicker *picker)
{
    ProbeStats stats;
    AcquireSRWLockShared(&ui->probe_manager->lock);
    stats = ui->probe_manager->stats;
    ReleaseSRWLockShared(&ui->probe_manager->lock);

    uint64_t cidr_length = picker->exclude_private_ranges
        ? picker->eligible_count : CidrRangeLen(picker->universe);
    char *repr = ProbeStatsRepr(&stats, cidr_length);
    if (repr == NULL)
        return;
    MarqueeProgressUpdate(ui->progress, repr, stats.probed, cidr_length);
    free(repr);
}

void UIBridgePrintProbeStats(void *context, IpPicker *picker)
{
    if (context != NULL && picker != NULL)
        UIBridgeRefresh(context, picker);
}

static DWORD WINAPI UIBridgeThread(LPVOID arg)
{
    UIBridge *ui = arg;
    for (;;) {
        UIBridgeRefresh(ui, ui->picker);
        if (WaitForSingleObject(ui->stop, ui->report_interval_ms) != WAIT_TIMEOUT)
            break;
    }
    UIBridgeRefresh(ui, ui->picker);
    return 0;
}

int UIBridgeInitialize(UIBridge *ui, ProbeManager *probe_manager,
                       IpPicker *picker, HANDLE output, size_t marquee_width,
                       char marquee_character, DWORD marquee_interval_ms,
                       DWORD report_interval_ms)
{
    if (ui == NULL || probe_manager == NULL || picker == NULL ||
        output == NULL || output == INVALID_HANDLE_VALUE || marquee_width == 0 ||
        marquee_interval_ms == 0 || report_interval_ms == 0)
        return 0;

    memset(ui, 0, sizeof *ui);
    ui->probe_manager = probe_manager;
    ui->picker = picker;
    ui->output = output;
    ui->marquee_width = marquee_width;
    ui->marquee_character = marquee_character;
    ui->marquee_interval_ms = marquee_interval_ms;
    ui->report_interval_ms = report_interval_ms;
    ui->progress = MarqueeProgressCreate("Scanning targets", marquee_width,
                                        marquee_character, output,
                                        marquee_interval_ms);
    if (ui->progress == NULL || MarqueeProgressStart(ui->progress) != 0) {
        UIBridgeFinalize(ui);
        return 0;
    }

    ui->stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (ui->stop == NULL) {
        UIBridgeFinalize(ui);
        return 0;
    }

    ui->thread = CreateThread(NULL, 0, UIBridgeThread, ui, 0, NULL);
    if (ui->thread == NULL) {
        UIBridgeFinalize(ui);
        return 0;
    }
    return 1;
}

void UIBridgeFinalize(UIBridge *ui)
{
    if (ui == NULL)
        return;
    if (ui->stop != NULL)
        SetEvent(ui->stop);
    if (ui->thread != NULL) {
        WaitForSingleObject(ui->thread, INFINITE);
        CloseHandle(ui->thread);
    }
    if (ui->stop != NULL)
        CloseHandle(ui->stop);
    MarqueeProgressDelete(ui->progress);
    memset(ui, 0, sizeof *ui);
}
