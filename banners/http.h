#ifndef BANNERS_HTTP_H
#define BANNERS_HTTP_H

#include "probe/probe.h"

char *HttpGrab(HostUpCheck *check, uint16_t port);
char *HttpResponseFmt(const char *response);

#endif
