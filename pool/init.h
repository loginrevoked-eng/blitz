#ifndef POOL_INIT_H
#define POOL_INIT_H

#include "picker.h"

IpPicker *IpPicker_Initialize(char *cidr_text, uint32_t bucket_width,
                              uint32_t cooldown_period, int exclude_private_ranges,
                              char *resume_file, int resumable);
char *IpPicker_ConfigHighlight(IpPicker *picker);

#endif
