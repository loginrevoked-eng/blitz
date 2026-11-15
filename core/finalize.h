#ifndef CORE_UPCHECK_FINALIZE_H
#define CORE_UPCHECK_FINALIZE_H

#include "probe.h"

/* Returns nonzero when all workers and owned resources are finalized. */
int ProbeFinalize(ProbeContext *context);

#endif
