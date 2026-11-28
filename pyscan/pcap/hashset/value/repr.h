#ifndef HASHSET_VALUE_REPR_H
#define HASHSET_VALUE_REPR_H

#include "value.h"

/* Returns a heap-allocated representation. The caller must free it. */
char *HashSetValue_Repr(const HashSetValue *value);

#endif
