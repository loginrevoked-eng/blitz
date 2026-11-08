#ifndef BANNERS_SSH_H
#define BANNERS_SSH_H

#include "probe/probe.h"

char *SshGrab(HostUpCheck *check, uint16_t port);
char *SshResponseFmt(const char *response);

#endif
