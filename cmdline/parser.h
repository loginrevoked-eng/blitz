#ifndef MOOLAHCLI_PARSER_H
#define MOOLAHCLI_PARSER_H


#include "cmdline.h"
#include "hashmap.h"


typedef struct {
    MoolahCliOptionKind kind;
    int flags;
    int nargs;
    char *name;
    char *flag;
    char *desc;
    char *short_flag;
    char *mutex_group;
    MoolahCliOptionCallback callback;
} MoolahCliOption;


typedef struct MoolahCliOptionNode {
    MoolahCliOption *option;
    struct MoolahCliOptionNode *next;
} MoolahCliOptionNode;



typedef struct {
    char *program;
    char *description;
    int flags;
    char *usage;
}MoolahCliConfig;

typedef struct {
    int seen;
    int required;
    char *group_name;
    char *first_seen;
}MoolahCliMutexArgs;

typedef struct MoolahCliMutexGroupNode {
    MoolahCliMutexArgs args;
    struct MoolahCliMutexGroupNode *next;
} MoolahCliMutexGroupNode;

struct MoolahCli {
    MoolahCli *parent;
    MoolahCliOptionNode *options;
    MoolahCliConfig *config;
    MoolahHashMap *lookup;
    MoolahHashMap *args;
    MoolahHashMap *subcommands;
    MoolahCliMutexGroupNode *mutex_groups;
    int initialized;
};


typedef struct {
    char *flag;
    char *value;
    char *storage;
}MoolahCliArgument;

struct MoolahCliContext {
    MoolahCli *root;
    int level;
    /* Active parser. */
    MoolahCli *parser;
    int argv_index;
    int argc;
    char **argv;
    MoolahCliOption *current_option;
    MoolahCliArgument *current_arg;
};

int MoolahCliIsSubcommand(MoolahCliContext *ctx);
int MoolahCliSwapParser(MoolahCliContext *ctx);
int MoolahCliIsFlag(const char *argument);
void MoolahCliEnsureRequired(MoolahCli *parser);
int MoolahCliHasMutexGroup(MoolahCli *parser, const char *group_name);
MoolahCliOption *MoolahCliOptionNew(MoolahCliOptionKind kind, char *name,
    char *long_flag, char *short_flag, char *description, int flags,
    MoolahCliOptionCallback callback, char *mutex_group);
MoolahCliMutexGroupNode *MoolahMutexGroupNodeNew(char *group_name,
    int required);
MoolahCli *MoolahCliNew(char *program, char *description, int flags,
    char **argv);
void MoolahCliDestroy(MoolahCli *parser);



#endif
