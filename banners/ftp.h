#ifndef BANNERS_FTP_H
#define BANNERS_FTP_H

#include "probe/probe.h"

char *FtpGrab(HostUpCheck *check, uint16_t port);
char *FtpResponseFmt(const char *response);

#endif
