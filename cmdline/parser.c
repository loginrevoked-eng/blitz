#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include <ctype.h>
#include <errno.h>
#include "parser.h"
#include "panic.h"
#include "str.h"
#include "subcommand.h"


static char *MoolahCliStrLower(char *str)
{
    char *p = str;
    while (*p)
    {
        *p = tolower((unsigned char)*p);
        p++;
    }
    return str;
}




#include "builtins.h"


/* MoolahCli flags */

static void FreeCtx(MoolahCliContext *ctx)
{
    if(ctx->current_arg)
    {
        free(ctx->current_arg->storage);
        free(ctx->current_arg);
    }
    free(ctx);
}

static void UpdateCtx(MoolahCliContext *ctx)
{
    ctx->argv_index++;
    if(ctx->current_arg)
    {
        free(ctx->current_arg->storage);
        ctx->current_arg->storage = NULL;
        ctx->current_arg->flag = NULL;
        ctx->current_arg->value = NULL;
    }
}

int MoolahCliIsFlag(const char *str)
{
    if (!str || str[0] != '-') return 0;
    char *end = NULL;
    (void)strtod(str, &end);
    return end == str || *end != '\0';
}

static int ShouldStop(MoolahCliContext *ctx) { return ctx->argv_index >= ctx->argc; }
static int IsPositional(MoolahCliContext *ctx) { return !MoolahCliIsFlag(ctx->argv[ctx->argv_index]); }



static void OptionFree(MoolahCliOption *opt)
{
    if (!opt) return;

    free(opt->name);
    free(opt->flag);
    free(opt->short_flag);
    free(opt->desc);
    free(opt->mutex_group);
    free(opt);
}

static int MoolahCliDestroySubcommand(const char *name,
    const MoolahValue *value, void *context)
{
    (void)name;
    (void)context;
    if (value != NULL && value->kind == MOOLAH_VALUE_POINTER)
        MoolahCliDestroy(value->p);
    return 1;
}

static void OptionAppend(MoolahCliOptionNode **list, MoolahCliOption *opt)
{
    MoolahCliOptionNode *node = malloc(sizeof(MoolahCliOptionNode));
    if(!node) return;

    node->option = opt;
    node->next = NULL;

    if(!*list)
    {
        *list = node;
        return;
    }

    MoolahCliOptionNode *tail = *list;
    while(tail->next)
        tail = tail->next;

    tail->next = node;
}

static char *MoolahCliOptionToString(MoolahCliOption *opt)
{
    char *str = NULL;

    if (!(opt->flags & MOOLAHCLI_OPT_POSITIONAL) && opt->flag)
        MoolahCliAppendf(&str, "%s", opt->flag);

    if (!(opt->flags & MOOLAHCLI_OPT_POSITIONAL) && opt->short_flag)
        MoolahCliAppendf(&str, ", %s", opt->short_flag);

    if (!(opt->flags & MOOLAHCLI_OPT_POSITIONAL))
    {
        switch (opt->kind)
        {
            case MOOLAHCLI_OPT_BOOL:
                break;

            case MOOLAHCLI_OPT_LONG:
                MoolahCliAppendf(&str, " <long>");
                break;

            case MOOLAHCLI_OPT_DOUBLE:
                MoolahCliAppendf(&str, " <double>");
                break;

            case MOOLAHCLI_OPT_STR:
                MoolahCliAppendf(&str, " <string>");
                break;
        }
    }
    else
    {
        MoolahCliAppendf(&str, " <%s>", opt->name);
    }

    if (opt->flags & MOOLAHCLI_OPT_VARIADIC)
        MoolahCliAppendf(&str, "...");

    if (opt->flags & MOOLAHCLI_OPT_REQUIRED)
        MoolahCliAppendf(&str, " [required]");

    if (opt->desc)
        MoolahCliAppendf(&str, "  %s", opt->desc);

    return str;
}


void MoolahCliDestroy(MoolahCli *parser)
{
    if(!parser) return;
    MoolahHashMap_RemoveIf(parser->subcommands,
        MoolahCliDestroySubcommand, NULL);
    if(parser->lookup) MoolahHashMap_Free(parser->lookup);
    if(parser->args) MoolahHashMap_Free(parser->args);
    if(parser->subcommands) MoolahHashMap_Free(parser->subcommands);
    MoolahCliMutexGroupNode *group = parser->mutex_groups;
    while (group != NULL) {
        MoolahCliMutexGroupNode *next = group->next;
        free(group->args.group_name);
        free(group);
        group = next;
    }
    MoolahCliOptionNode *node = parser->options;
    while (node) {
        MoolahCliOptionNode *next = node->next;
        OptionFree(node->option);
        free(node);
        node = next;
    }
    if(parser->config) {
        free(parser->config->usage);
        free(parser->config->program);
        free(parser->config->description);
        free(parser->config);
    }
    free(parser);
}


void MoolahCliAdd(MoolahCli *parser, MoolahCliOptionKind kind, char *name,
    char *long_flag, char *short_flag, char *desc, int flags,
    MoolahCliOptionCallback callback, MoolahValue *def, char *mutex_group)
{
    if (mutex_group != NULL && !MoolahCliHasMutexGroup(parser, mutex_group))
        MoolahCliPanic("Unknown mutex group %s\n", mutex_group);
    MoolahCliOption *opt = MoolahCliOptionNew(kind, name, long_flag, short_flag, desc,
        flags, callback, mutex_group);

    if (!opt)
        MoolahCliPanic("Unable to allocate memory for new option\n");

    MoolahValue *optval = MoolahValueNew(MOOLAH_VALUE_POINTER);

    if (!optval)
    {
        OptionFree(opt);
        MoolahCliPanic("Unable to allocate memory for new option\n");
    }

    optval->p = opt;
    OptionAppend(&parser->options, opt);
    if(def)
    {
        MoolahCliPanicIf(!MoolahHashMap_Set(parser->args, opt->name, *def), "Unable to store default for %s\n", opt->name);
    }
    if (parser->config->flags & MOOLAHCLI_IGNORE_CASE)
    {
        if (opt->flag)
            MoolahCliPanicIf(!MoolahHashMap_Set(parser->lookup, MoolahCliStrLower(opt->flag), *optval), "Unable to register %s\n", opt->name);

        if (opt->short_flag)
            MoolahCliPanicIf(!MoolahHashMap_Set(parser->lookup, MoolahCliStrLower(opt->short_flag), *optval), "Unable to register %s\n", opt->name);
    }
    else
    {
        if (opt->flag) MoolahCliPanicIf(!MoolahHashMap_Set(parser->lookup, opt->flag, *optval), "Unable to register %s\n", opt->name);

        if (opt->short_flag)
            MoolahCliPanicIf(!MoolahHashMap_Set(parser->lookup, opt->short_flag, *optval), "Unable to register %s\n", opt->name);
    }

    free(optval);
}



static MoolahValue *ParseValue(char *str, MoolahCliOptionKind kind)
{
    MoolahValueKind vkind = MOOLAH_VALUE_UNKNOWN;

    switch(kind)
    {
        case MOOLAHCLI_OPT_BOOL:
            vkind = MOOLAH_VALUE_BOOL;
            break;
        case MOOLAHCLI_OPT_DOUBLE:
            vkind = MOOLAH_VALUE_DOUBLE;
            break;
        case MOOLAHCLI_OPT_LONG:
            vkind = MOOLAH_VALUE_LONG;
            break;
        case MOOLAHCLI_OPT_STR:
            vkind = MOOLAH_VALUE_STRING;
            break;
        default:
            MoolahCliPanic("Invalid MoolahCliOption type\n");
    }

    MoolahValue *val = MoolahValueNew(vkind);
    if(!val) MoolahCliPanic("Failed to allocate memory for argument value\n");

    switch(kind)
    {
        case MOOLAHCLI_OPT_BOOL:
            val->b = MOOLAH_TRUE;
            break;
        case MOOLAHCLI_OPT_DOUBLE: {
            char *end = NULL;
            errno = 0;
            val->d = strtod(str, &end);
            if (errno == ERANGE || end == str || *end != '\0')
                MoolahCliPanic("Invalid number: %s\n", str);
            break;
        }
        case MOOLAHCLI_OPT_LONG: {
            char *end = NULL;
            errno = 0;
            val->l = strtol(str, &end, 10);
            if (errno == ERANGE || end == str || *end != '\0')
                MoolahCliPanic("Invalid integer: %s\n", str);
            break;
        }
        case MOOLAHCLI_OPT_STR:
            val->s = str;
            break;
    }

    return val;
}


static void Callback(MoolahCliContext *ctx)
{
    if(ctx->current_option && ctx->current_option->callback)
    {
        ctx->current_option->callback(ctx);
    }
}



static char *FlagRepresentation(MoolahCli *parser)
{
    char *repr = MoolahCliStrf("");
    int flags = parser->config->flags;

    if (flags & MOOLAHCLI_IGNORE_CASE)
        MoolahCliAppendf(&repr, "The parser ignores case when matching flags. ");

    if (flags & MOOLAHCLI_IGNORE_UNKNOWN)
        MoolahCliAppendf(&repr, "Unknown flags are ignored. ");

    if (flags & MOOLAHCLI_REMOVE_BUILTINS)
        MoolahCliAppendf(&repr, "Built-in options are disabled. ");

    if (flags & MOOLAHCLI_INCLUDE_BUILTINS_USAGE)
        MoolahCliAppendf(&repr, "Built-in options are included in the usage information. ");

    return repr;
}



static void GenerateUsage(MoolahCli *parser)
{
    char *usage = NULL;

    MoolahCliAppendf(&usage, "Usage: %s [options]\n\n", parser->config->program);

    if (parser->config->description)
        MoolahCliAppendf(&usage, "%s\n\n", parser->config->description);

    MoolahCliAppendf(&usage, "Options:\n");

    for (MoolahCliOptionNode *node = parser->options; node; node = node->next)
    {
        MoolahCliAppendf(&usage, "  %s\n", MoolahCliOptionToString(node->option));
    }
    MoolahCliAppendf(&usage, "\n%s\n", FlagRepresentation(parser));

    parser->config->usage = usage;
}


static void AddEasterEggs(MoolahCli *parser)
{
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "fuckyou", "--fuckyou", "-fy", "fucked and cursed easteregg", 0, fuckyoutoo, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "yomomma", "--yomomma", "-ym", "yo momma 😭", 0, yomomma, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "yousuck", "--yousuck", "-ys", "You suck", 0, yousuck, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "skillissue", "--skillissue", "-si", "Diagnose the obvious", 0, skillissue, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "getgood", "--getgood", "-gg", "Professional advice", 0, getgood, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "what", "--what", "-w", "What the fuck did you expect?", 0, what, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "bonk", "--bonk", "-b", "Bonk", 0, bonk, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "perish", "--perish", "-p", "Perish", 0, perish, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "bruh", "--bruh", "-br", "Bruh", 0, bruh, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "touchgrass", "--touchgrass", "-tg", "Attempt to locate grass", 0, touchgrass, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "cope", "--cope", "-c", "Cope harder", 0, cope, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "seethe", "--seethe", "-s", "Seethe", 0, seethe, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "mald", "--mald", "-m", "Mald", 0, mald, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "imagine", "--imagine", "-i", "Imagine needing help", 0, imagine, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "developer", "--developer", "-dev", "Works on my machine", 0, developer, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "worksforme", "--worksforme", "-wfm", "Works for me", 0, worksforme, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "amogus", "--amogus", "-a", "ඞ", 0, amogus, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "ohio", "--ohio", "-oh", "Enter an unstable geopolitical state", 0, ohio, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "nuclear", "--nuclear", "-n", "Launch absolutely nothing", 0, nuclear, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "fuckit", "--fuckit", "-fi", "Fuck it, we ball", 0, fuckit, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "ball", "--ball", "-ba", "We ball", 0, ball, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "rickroll", "--rickroll", "-rr", "Definitely not a rickroll", 0, rickroll, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "adlib", "--adlib", "-ad", "Lil Wayne is the GOAT!", 0, adlib, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "whatsgood", "--whatsgood", "-wg", "What's good?", 0, whatsgood, NULL, NULL);
}


static MoolahCliContext *Init(MoolahCli *parser, int argc, char **argv)
{
    MoolahCliContext *ctx = malloc(sizeof(MoolahCliContext));
    if(!ctx)
    {
        MoolahCliPanic("Unable to allocate memory for parser context\n");
    }
    ctx->parser = parser;
    ctx->root = parser;
    ctx->level = 0;
    ctx->argc = argc;
    ctx->argv = argv;
    ctx->argv_index = 1;
    ctx->current_option = NULL;
    ctx->current_arg = calloc(1, sizeof(MoolahCliArgument));
    if(!ctx->current_arg)
    {
        free(ctx);
        MoolahCliPanic("Unable to allocate memory for current argument\n");
    }

    HANDLE out = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode;

    if (GetConsoleMode(out, &mode))
    {
        SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    if (!parser->initialized) {
        MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "help", "--help", "-h", "Show this help message", 0, help, NULL, NULL);
        GenerateUsage(parser);
        if (!(parser->config->flags & MOOLAHCLI_REMOVE_BUILTINS)) {
            AddEasterEggs(parser);
            if (parser->config->flags & MOOLAHCLI_INCLUDE_BUILTINS_USAGE)
                GenerateUsage(parser);
        }
        parser->initialized = 1;
    }
    return ctx;
}



static MoolahValue *ParseList(int argc, char **argv, MoolahCliOption *opt, char *flag, int *index, char *first_value)
{
    if(opt->flags & MOOLAHCLI_OPT_LIST)
    {
        MoolahValue *list = MoolahValueNew(MOOLAH_VALUE_LIST);

        if(!list)
            MoolahCliPanic("failed to allocate list\n");

        if (first_value) {
            MoolahValue *first = ParseValue(first_value, opt->kind);
            list = MoolahValueAppend(list, first);
            free(first);
        }

        for (*index = *index + 1; *index < argc && !MoolahCliIsFlag(argv[*index]); (*index)++)
        {
            MoolahValue *parsed = ParseValue(argv[*index], opt->kind);

            if (!parsed)
            {
                MoolahValueDeepFree(list);
                return NULL;
            }

            list = MoolahValueAppend(list, parsed);

            if(!list)
            {
                free(parsed);
                MoolahCliPanic("failed to append value\n");
            }

            free(parsed);

            if (!(opt->flags & MOOLAHCLI_OPT_VARIADIC) && (size_t)opt->nargs == MoolahValueLen(list))
            {
                opt->flags |= MOOLAHCLI_OPT_DONE;
                if (*index < argc && MoolahCliIsFlag(argv[*index])) --(*index);
                return list;
            }
        }

        if (!(opt->flags & MOOLAHCLI_OPT_VARIADIC) && !(opt->flags & MOOLAHCLI_OPT_DONE))
            MoolahCliPanic("not enough positional values for %s\n", flag);

        if (*index < argc && MoolahCliIsFlag(argv[*index])) (*index)--;
        return list;
    }

    return NULL;
}


static void ParsePositional(MoolahCli *parser, int argc, char **argv, int *index)
{
    for (MoolahCliOptionNode *node = parser->options; node; node = node->next)
    {
        MoolahCliOption *opt = node->option;

        if (!(opt->flags & MOOLAHCLI_OPT_POSITIONAL) || (opt->flags & MOOLAHCLI_OPT_DONE))
            continue;

        if (opt->flags & MOOLAHCLI_OPT_LIST)
        {
            MoolahValue *list = ParseList(argc, argv, opt, opt->flag, index, argv[*index]);
            MoolahCliPanicIf(!list, "Unable to parse values for %s\n", opt->flag);

            MoolahCliPanicIf(!MoolahHashMap_Set(parser->args, opt->name, *list), "Unable to store %s\n", opt->name);
            opt->flags |= MOOLAHCLI_OPT_DONE;
            free(list);
            return;
        }

        MoolahValue *parsed = ParseValue(argv[*index], opt->kind);
        MoolahCliPanicIf(!parsed, "Unable to parse value for %s\n", opt->flag);

        MoolahCliPanicIf(!MoolahHashMap_Set(parser->args, opt->name, *parsed), "Unable to store %s\n", opt->name);
        opt->flags |= MOOLAHCLI_OPT_DONE;
        return;
    }
}


static void ParseNonPositional(MoolahCliContext *ctx)
{
    MoolahCli *parser = ctx->parser;
    MoolahCliArgument *arg = ctx->current_arg;
    MoolahCliOption *opt = ctx->current_option;

    if (opt->flags & MOOLAHCLI_OPT_LIST)
    {
        MoolahValue *value = ParseList(ctx->argc, ctx->argv, opt, arg->flag, &ctx->argv_index, arg->value);
        MoolahCliPanicIf(!value, "Unable to parse list for flag %s\n", arg->flag);

        MoolahCliPanicIf(!MoolahHashMap_Set(parser->args, opt->name, *value), "Unable to store %s\n", opt->name);
        opt->flags |= MOOLAHCLI_OPT_DONE;
        free(value);
        return;
    }

    MoolahValue *parsed = ParseValue(arg->value, opt->kind);
    MoolahCliPanicIf(!parsed, "Unable to parse value for flag %s\n", arg->flag);

    MoolahHashMap_Set(parser->args, opt->name, *parsed);
    opt->flags |= MOOLAHCLI_OPT_DONE;
    free(parsed);
}
















static int FindOption(MoolahCliContext *ctx)
{
    MoolahCliArgument *arg = ctx->current_arg;
    MoolahCli *parser = ctx->parser;

    arg->flag = NULL;
    arg->value = NULL;

    arg->storage = strdup(ctx->argv[ctx->argv_index]);
    if (!arg->storage) MoolahCliPanic("Unable to allocate memory for argument\n");
    char *eq = strchr(arg->storage, '=');

    if (eq)
    {
        *eq = '\0';
        arg->flag = arg->storage;
        arg->value = eq + 1;
    }
    else
    {
        arg->flag = arg->storage;
    }

    if (parser->config->flags & MOOLAHCLI_IGNORE_CASE)
        MoolahCliStrLower(arg->flag);

    MoolahValue *optval = MoolahHashMap_Get(parser->lookup, arg->flag);

    if (!optval)
    {
        MoolahCliPanicIf(!(parser->config->flags & MOOLAHCLI_IGNORE_UNKNOWN), "Unknown flag %s\n", arg->flag);
        return 0;
    }

    ctx->current_option = optval->p;

    if (ctx->current_option->kind == MOOLAHCLI_OPT_BOOL)
        return 1;

    if (!arg->value)
    {
        if (ctx->argv_index + 1 >= ctx->argc)
            MoolahCliPanic("No value provided for %s\n", arg->flag);

        arg->value = ctx->argv[++ctx->argv_index];
    }

    return 1;
}





static void Parse(MoolahCliContext *ctx)
{
    if (IsPositional(ctx))
    {
        if(MoolahCliIsSubcommand(ctx))
        {
            if(!MoolahCliSwapParser(ctx))
            {
                MoolahCliPanic("could not swap the current parser with the requested subcommand");
            }
            return;
        }
        ParsePositional(ctx->parser, ctx->argc, ctx->argv, &ctx->argv_index);
    }
    else if(FindOption(ctx))
    {
        ParseNonPositional(ctx);
        Callback(ctx);
    }
}




void MoolahCliParse(MoolahCli *parser, int argc, char **argv)
{
    MoolahCliContext *ctx = Init(parser, argc, argv);
    while(!ShouldStop(ctx))
    {
        Parse(ctx);
        UpdateCtx(ctx);
    }

    MoolahCli *active = ctx->parser;
    FreeCtx(ctx);
    for (MoolahCli *current = active; current != NULL;
        current = current->parent)
        MoolahCliEnsureRequired(current);
}


MoolahValue *MoolahCliGet(MoolahCli *parser, const char *name)
{
    return parser && parser->args ? MoolahHashMap_Get(parser->args, name) : NULL;
}

char *MoolahCliGetString(MoolahCli *parser, const char *name)
{
    MoolahValue *value = MoolahCliGet(parser, name);
    return value && value->kind == MOOLAH_VALUE_STRING ? value->s : NULL;
}
