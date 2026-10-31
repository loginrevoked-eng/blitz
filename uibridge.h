#ifndef BOT_UIBRIDGE_H
#define BOT_UIBRIDGE_H

#include <stddef.h>
#include <stdint.h>

#include "probe/probe.h"
#include "picker.h"
#include "cli/marquee_progress.h"

typedef struct {
    ProbeManager *probe_manager;
    IpPicker *picker;
    MarqueeProgress *progress;
    HANDLE thread;
    HANDLE stop;
    HANDLE output;
    size_t marquee_width;
    char marquee_character;
    DWORD marquee_interval_ms;
    DWORD report_interval_ms;
} UIBridge;

int UIBridgeInitialize(UIBridge *ui, ProbeManager *probe_manager,
                       IpPicker *picker, HANDLE output, size_t marquee_width,
                       char marquee_character, DWORD marquee_interval_ms,
                       DWORD report_interval_ms);
void UIBridgeFinalize(UIBridge *ui);
void UIBridgePrintProbeStats(void *context, IpPicker *picker);

#endif
