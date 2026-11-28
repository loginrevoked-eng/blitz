#ifndef POOL_RESUME_H
#define POOL_RESUME_H

#include "picker.h"

/* Parses caller-owned JSON without modifying or retaining it. */
int IpPicker_LoadBuckets(IpPicker *picker, const char *json);

/* Returns a heap-allocated JSON string for resumable pickers; caller frees it. */
char *IpPicker_DumpBuckets(IpPicker *picker);

#endif
