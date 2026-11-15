#include "parser.h"

#include <stdlib.h>

#include "mutex.h"
#include "panic.h"
#include "str.h"

void MoolahCliEnsureRequired(MoolahCli *parser)
{
    if (parser == NULL)
        return;

    MoolahCliEnsureRequiredMutexGroups(parser);
    MoolahCliEnsureMutexRespected(parser);

    char *error = NULL;
    for (MoolahCliOptionNode *node = parser->options; node != NULL; node = node->next) {
        MoolahCliOption *option = node->option;
        if ((option->flags & MOOLAHCLI_OPT_REQUIRED) &&
            !MoolahHashMap_Get(parser->args, option->name)) {
            const char *label = option->flag != NULL
                ? option->flag : option->name;
            MoolahCliAppendf(&error, "Missing required %s %s\n",
                (option->flags & MOOLAHCLI_OPT_POSITIONAL) ? "argument" : "option",
                label != NULL ? label : "(unnamed)");
        }
    }
    if (error != NULL)
        MoolahCliPanic("%s", error);
    free(error);
}
