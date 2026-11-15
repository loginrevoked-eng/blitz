#include "mutex.h"

#include <stdlib.h>
#include <string.h>

#include "ctors.h"
#include "panic.h"

static MoolahCliMutexGroupNode *MoolahMutexGroupFind(MoolahCli *parser,
    const char *group_name)
{
    if (parser == NULL || group_name == NULL)
        return NULL;
    for (MoolahCliMutexGroupNode *group = parser->mutex_groups; group != NULL;
        group = group->next) {
        if (strcmp(group->args.group_name, group_name) == 0)
            return group;
    }
    return NULL;
}

int MoolahCliHasMutexGroup(MoolahCli *parser, const char *group_name)
{
    return MoolahMutexGroupFind(parser, group_name) != NULL;
}

void MoolahCliAddMutexGroup(MoolahCli *parser, char *group_name,
    int required)
{
    if (parser == NULL || group_name == NULL || group_name[0] == '\0')
        MoolahCliPanic("Invalid mutex group\n");
    if (MoolahCliHasMutexGroup(parser, group_name))
        MoolahCliPanic("Duplicate mutex group %s\n", group_name);

    MoolahCliMutexGroupNode *group = MoolahMutexGroupNodeNew(group_name, required);
    if (group == NULL)
        MoolahCliPanic("Unable to allocate mutex group %s\n", group_name);

    MoolahCliMutexGroupNode **tail = &parser->mutex_groups;
    while (*tail != NULL)
        tail = &(*tail)->next;
    *tail = group;
}

void MoolahCliEnsureRequiredMutexGroups(MoolahCli *parser)
{
    if (parser == NULL)
        return;

    for (MoolahCliMutexGroupNode *group = parser->mutex_groups; group != NULL;
        group = group->next) {
        group->args.seen = 0;
        group->args.first_seen = NULL;
    }

    for (MoolahCliOptionNode *node = parser->options; node != NULL; node = node->next) {
        MoolahCliOption *option = node->option;
        if (option->mutex_group == NULL || !(option->flags & MOOLAHCLI_OPT_DONE))
            continue;
        MoolahCliMutexGroupNode *group = MoolahMutexGroupFind(parser,
            option->mutex_group);
        if (group != NULL && !group->args.seen) {
            group->args.seen = 1;
            group->args.first_seen = option->name;
        }
    }

    for (MoolahCliMutexGroupNode *group = parser->mutex_groups; group != NULL;
        group = group->next) {
        if (group->args.required && !group->args.seen)
            MoolahCliPanic("Required mutex group %s was not selected\n",
                group->args.group_name);
    }
}

void MoolahCliEnsureMutexRespected(MoolahCli *parser)
{
    if (parser == NULL)
        return;

    for (MoolahCliMutexGroupNode *group = parser->mutex_groups; group != NULL;
        group = group->next) {
        for (MoolahCliOptionNode *node = parser->options; node != NULL;
            node = node->next) {
            MoolahCliOption *option = node->option;
            if (option->mutex_group != NULL &&
                strcmp(option->mutex_group, group->args.group_name) == 0 &&
                (option->flags & MOOLAHCLI_OPT_DONE) && group->args.first_seen != NULL &&
                strcmp(option->name, group->args.first_seen) != 0) {
                MoolahCliPanic("Options %s and %s are mutually exclusive in group %s\n",
                    group->args.first_seen, option->name,
                    group->args.group_name);
            }
        }
    }
}
