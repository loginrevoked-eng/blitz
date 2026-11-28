#ifndef POOL_INIT_H
#define POOL_INIT_H

#include "picker.h"
#include "hook.h"

/* resume_json is optional caller-owned JSON, parsed during initialization. */
IpPicker *IpPicker_Initialize(char *cidr_text, uint32_t bucket_width,
                              uint32_t cooldown_period, const char *resume_json,
                              IpPickerFlags flags,
                              InitCallback callback, void *callback_arg);
char *IpPicker_ConfigHighlight(IpPicker *picker);
IpPicker *IpPicker_InitSimple(char *cidr, uint32_t max_hosts, uint32_t rate);
#endif
