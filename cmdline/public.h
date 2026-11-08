#ifndef PUBLIC_H
#define PUBLIC_H

#include "value.h"

typedef struct MoolahCli MoolahCli;
typedef struct Context Context;
typedef void (*OptionCallback)(Context *);

typedef enum {
    OPT_BOOL, OPT_LONG, OPT_DOUBLE, OPT_STR
} MoolahCliOptionKind;

enum {
    MOOLAHCLI_IGNORE_CASE = 1 << 0,
    MOOLAHCLI_IGNORE_UNKNOWN = 1 << 1,
    MOOLAHCLI_REMOVE_BUILTINS = 1 << 2,
    MOOLAHCLI_INCLUDE_BUILTINS_USAGE = 1 << 3
};

enum {
    OPT_POSITIONAL = 1 << 0,
    OPT_VARIADIC = 1 << 1,
    OPT_DONE = 1 << 2,
    OPT_REQUIRED = 1 << 3,
    OPT_LIST = 1 << 4
};

MoolahCli *MoolahCliNew(char *program, char *description, int flags, char **argv);
void MoolahCliAdd(MoolahCli *parser, MoolahCliOptionKind kind, char *name,
               char *long_flag, char *short_flag, char *description,
               int flags, OptionCallback callback, Value *default_value);
void MoolahCliParse(MoolahCli *parser, int argc, char **argv);
Value *MoolahCliGet(MoolahCli *parser, const char *name);
char *MoolahCliGetString(MoolahCli *parser, const char *name);
void MoolahCliDestroy(MoolahCli *parser);

#endif
