#ifndef BANNERS_H
#define BANNERS_H

#include "probe/probe.h"
#include "registry/registry.h"

int GrabBanners(HostUpCheck *check, RegistryFindingHandler on_finding,
    void *context);

#endif
