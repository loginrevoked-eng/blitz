#ifndef VALUE_REPR_H
#define VALUE_REPR_H

#include "value.h"

/* Returns a heap-allocated representation. The caller must free it. */
char *ValueRepr(const Value *value);

#endif
