#ifndef IPPICKER_VALUE_REPR_H
#define IPPICKER_VALUE_REPR_H

#include "value.h"

/* Returns a heap-allocated representation. The caller must free it. */
char *IpPickerValue_Repr(const IpPickerValue *value);

#endif
