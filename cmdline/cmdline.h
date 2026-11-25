#ifndef MOOLAHCLI_CMDLINE_H
#define MOOLAHCLI_CMDLINE_H

#include "value.h"

typedef struct MoolahCli MoolahCli;
typedef struct MoolahCliContext MoolahCliContext;
typedef void (*MoolahCliOptionCallback)(MoolahCliContext *);

typedef enum {
    MOOLAHCLI_OPT_BOOL, MOOLAHCLI_OPT_LONG, MOOLAHCLI_OPT_DOUBLE, MOOLAHCLI_OPT_STR
} MoolahCliOptionKind;

enum {
    MOOLAHCLI_IGNORE_CASE = 1 << 0,
    MOOLAHCLI_IGNORE_UNKNOWN = 1 << 1,
    MOOLAHCLI_REMOVE_BUILTINS = 1 << 2,
    MOOLAHCLI_INCLUDE_BUILTINS_USAGE = 1 << 3
};

enum {
    MOOLAHCLI_OPT_POSITIONAL = 1 << 0,
    MOOLAHCLI_OPT_VARIADIC = 1 << 1,
    MOOLAHCLI_OPT_DONE = 1 << 2,
    MOOLAHCLI_OPT_REQUIRED = 1 << 3,
    MOOLAHCLI_OPT_LIST = 1 << 4
};

MoolahCli *MoolahCliNew(char *program, char *description, int flags, char **argv);
void MoolahCliAdd(MoolahCli *parser, MoolahCliOptionKind kind, char *name,
               char *long_flag, char *short_flag, char *description,
               int flags, MoolahCliOptionCallback callback, MoolahValue *default_value,
               char *mutex_group);
void MoolahCliAddMutexGroup(MoolahCli *parser, char *group_name, int required);
MoolahCli *MoolahCliAddSubcommand(MoolahCli *root, char *name,
               char *program, char *description, int flags, char **argv);
void MoolahCliParse(MoolahCli *parser, int argc, char **argv);
MoolahValue *MoolahCliGet(MoolahCli *parser, const char *name);
char *MoolahCliGetString(MoolahCli *parser, const char *name);
void MoolahCliDestroy(MoolahCli *parser);

#endif
