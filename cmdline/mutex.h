#ifndef MOOLAHCLI_MUTEX_H
#define MOOLAHCLI_MUTEX_H

#include "parser.h"

void MoolahCliEnsureRequiredMutexGroups(MoolahCli *parser);
void MoolahCliEnsureMutexRespected(MoolahCli *parser);

#endif
