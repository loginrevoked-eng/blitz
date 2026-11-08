#ifndef POOL_INIT_H
#define POOL_INIT_H

#include "picker.h"
#include "hook.h"

IpPicker *IpPicker_Initialize(char *cidr_text, uint32_t bucket_width,
                              uint32_t cooldown_period, char *resume_file,
                              char *dump_file, IpPickerFlags flags,
                              InitCallback callback, void *callback_arg);
char *IpPicker_ConfigHighlight(IpPicker *picker);

#endif
