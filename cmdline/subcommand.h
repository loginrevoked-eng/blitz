#ifndef MOOLAHCLI_SUBCOMMAND_H
#define MOOLAHCLI_SUBCOMMAND_H

#include "parser.h"

int MoolahCliIsSubcommand(MoolahCliContext *ctx);
int MoolahCliSwapParser(MoolahCliContext *ctx);

#endif
