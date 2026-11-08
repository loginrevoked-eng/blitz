#ifndef HANDLERS_SSH_H
#define HANDLERS_SSH_H

#include "registry/registry.h"

int HandlerSshBanner(Ipv4 ip, uint16_t port,
    const RegistryFinding *finding, void *context);

#endif
