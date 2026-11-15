#include "parser.h"

#include <stdlib.h>
#include <string.h>

#include "panic.h"
#include "ctors.h"

MoolahHashMap *MoolahHashMap_New(size_t capacity)
{
    if (capacity == 0)
        return NULL;

    MoolahHashMap *map = malloc(sizeof(*map));
    if (map == NULL)
        return NULL;

    map->buckets = calloc(capacity, sizeof(*map->buckets));
    map->capacity = capacity;
    if (map->buckets == NULL) {
        free(map);
        return NULL;
    }
    return map;
}

MoolahValue *MoolahValueNew(MoolahValueKind kind)
{
    MoolahValue *value = malloc(sizeof(*value));
    if (value == NULL)
        return NULL;
    value->kind = kind;
    value->free = NULL;
    if (kind == MOOLAH_VALUE_LIST)
        value->list = NULL;
    return value;
}

MoolahValue *MoolahValueNewEx(MoolahValueKind kind,
    MoolahValueFreeFunc free_func)
{
    MoolahValue *value = MoolahValueNew(kind);
    if (value == NULL)
        return NULL;
    value->free = free_func != NULL ? free_func : MoolahValueDeepFree;
    return value;
}

MoolahCli *MoolahCliNew(char *program, char *description, int flags,
    char **argv)
{
    if (program == NULL && (argv == NULL || argv[0] == NULL))
        return NULL;

    MoolahCli *parser = calloc(1, sizeof(*parser));
    if (parser == NULL)
        return NULL;
    parser->config = calloc(1, sizeof(*parser->config));
    if (parser->config == NULL) {
        free(parser);
        return NULL;
    }
    parser->config->program = strdup(program != NULL ? program : argv[0]);
    parser->config->description = description != NULL ? strdup(description) : NULL;
    parser->config->flags = flags;
    parser->lookup = MoolahHashMap_New(16);
    parser->args = MoolahHashMap_New(16);
    parser->subcommands = MoolahHashMap_New(8);
    if (parser->config->program == NULL ||
        (description != NULL && parser->config->description == NULL) ||
        parser->lookup == NULL || parser->args == NULL ||
        parser->subcommands == NULL) {
        MoolahCliDestroy(parser);
        return NULL;
    }
    MoolahCliSetPrefix(parser->config->program);
    return parser;
}

MoolahCli *MoolahCliAddSubcommand(MoolahCli *root, char *name,
    char *program, char *description, int flags, char **argv)
{
    if (root == NULL || name == NULL || name[0] == '\0' ||
        root->subcommands == NULL || MoolahHashMap_Get(root->subcommands, name))
        return NULL;

    MoolahCli *subcommand = MoolahCliNew(program, description, flags, argv);
    if (subcommand == NULL)
        return NULL;

    MoolahValue value = { .kind = MOOLAH_VALUE_POINTER, .p = subcommand };
    if (!MoolahHashMap_Set(root->subcommands, name, value)) {
        MoolahCliDestroy(subcommand);
        return NULL;
    }
    subcommand->parent = root;
    return subcommand;
}

MoolahCliOption *MoolahCliOptionNew(MoolahCliOptionKind kind, char *name,
    char *long_flag, char *short_flag, char *description, int flags,
    MoolahCliOptionCallback callback, char *mutex_group)
{
    MoolahCliOption *option = calloc(1, sizeof(*option));
    if (option == NULL)
        return NULL;
    option->kind = kind;
    option->name = strdup(name != NULL ? name : "");
    option->flag = long_flag != NULL ? strdup(long_flag) : NULL;
    option->short_flag = short_flag != NULL ? strdup(short_flag) : NULL;
    option->desc = description != NULL ? strdup(description) : NULL;
    option->mutex_group = mutex_group != NULL ? strdup(mutex_group) : NULL;
    option->callback = callback;
    option->flags = flags;
    if (option->name == NULL ||
        (long_flag != NULL && option->flag == NULL) ||
        (short_flag != NULL && option->short_flag == NULL) ||
        (description != NULL && option->desc == NULL) ||
        (mutex_group != NULL && option->mutex_group == NULL)) {
        free(option->name);
        free(option->flag);
        free(option->short_flag);
        free(option->desc);
        free(option->mutex_group);
        free(option);
        return NULL;
    }
    return option;
}

MoolahCliMutexGroupNode *MoolahMutexGroupNodeNew(char *group_name,
    int required)
{
    if (group_name == NULL || group_name[0] == '\0')
        return NULL;
    MoolahCliMutexGroupNode *node = calloc(1, sizeof(*node));
    if (node == NULL)
        return NULL;
    node->args.group_name = strdup(group_name);
    if (node->args.group_name == NULL) {
        free(node);
        return NULL;
    }
    node->args.required = required != 0;
    return node;
}
