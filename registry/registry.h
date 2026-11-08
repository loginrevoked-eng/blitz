#ifndef REGISTRY_REGISTRY_H
#define REGISTRY_REGISTRY_H

#include <stddef.h>
#include <stdint.h>

#include "pool/cidr.h"

typedef enum {
    REGISTRY_SERVICE_HTTP,
    REGISTRY_SERVICE_FTP,
    REGISTRY_SERVICE_RTSP,
    REGISTRY_SERVICE_SSH
} RegistryService;

typedef struct {
    const char *headers;
} RegistryHttpFinding;

typedef struct {
    const char *greeting;
} RegistryFtpFinding;

typedef struct {
    const char *headers;
} RegistryRtspFinding;

typedef struct {
    const char *identification;
} RegistrySshFinding;

typedef struct {
    RegistryService service;
    Ipv4 ip;
    uint16_t port;
    /* Detail strings are owned by the banner grabber; copy them to retain. */
    union {
        RegistryHttpFinding http;
        RegistryFtpFinding ftp;
        RegistryRtspFinding rtsp;
        RegistrySshFinding ssh;
    } detail;
} RegistryFinding;

typedef struct Registry Registry;

/* Return nonzero to print the finding's banner; return zero to skip it. */
typedef int (*RegistryCallback)(Ipv4 ip, uint16_t port,
    const RegistryFinding *finding, void *context);
typedef int (*RegistryFindingHandler)(const RegistryFinding *finding,
    void *context);

Registry *RegistryNew(size_t capacity);
void RegistryFree(Registry *registry);
int RegistryCreateCallback(Registry *registry, RegistryService service,
    RegistryCallback callback, void *context);
int RegistrySetCallback(Registry *registry, RegistryService service,
    RegistryCallback callback, void *context);
int RegistryUpdateCallback(Registry *registry, RegistryService service,
    RegistryCallback callback, void *context);
RegistryCallback RegistryFindCallback(Registry *registry,
    const RegistryFinding *finding, void **context);
int RegistryRemoveCallback(Registry *registry, RegistryService service);
void RegistryClear(Registry *registry);
size_t RegistryCount(Registry *registry);

#endif
