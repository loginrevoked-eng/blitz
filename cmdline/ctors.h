#ifndef MOOLAHCLI_CTORS_H
#define MOOLAHCLI_CTORS_H

#include "parser.h"

MoolahHashMap *MoolahHashMap_New(size_t capacity);
MoolahValue *MoolahValueNew(MoolahValueKind kind);
MoolahValue *MoolahValueNewEx(MoolahValueKind kind,
    MoolahValueFreeFunc free_func);
MoolahCli *MoolahCliNew(char *program, char *description, int flags,
    char **argv);
MoolahCli *MoolahCliAddSubcommand(MoolahCli *root, char *name,
    char *program, char *description, int flags, char **argv);
MoolahCliOption *MoolahCliOptionNew(MoolahCliOptionKind kind, char *name,
    char *long_flag, char *short_flag, char *description, int flags,
    MoolahCliOptionCallback callback, char *mutex_group);
MoolahCliMutexGroupNode *MoolahMutexGroupNodeNew(char *group_name,
    int required);

#endif
