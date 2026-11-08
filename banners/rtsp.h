#ifndef BANNERS_RTSP_H
#define BANNERS_RTSP_H

#include "probe/probe.h"

char *RtspGrab(HostUpCheck *check, uint16_t port);
char *RtspResponseFmt(const char *response);

#endif
