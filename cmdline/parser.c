#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include <ctype.h>
#include <errno.h>
#include "public.h"
#include "map.h"
#include "panic.h"
#include "../str/str.h"


char *strlwr(char *str)
{
    char *p = str;
    while (*p)
    {
        *p = tolower((unsigned char)*p);
        p++;
    }
    return str;
}


typedef struct {
    MoolahCliOptionKind kind;
    int flags;
    int nargs;
    char *name;
    char *flag;
    char *desc;
    char *short_flag;
    OptionCallback callback;
} Option;


typedef struct OptionNode {
    Option *option;
    struct OptionNode *next;
} OptionNode;



typedef struct {
    char *program;
    char *description;
    int flags;
    char *usage;
}MoolahCliConfig;

struct MoolahCli {
    OptionNode *options;
    MoolahCliConfig *config;
    HashMap *lookup;
    HashMap *args;
    int initialized;
};


typedef struct {
    char *flag;
    char *value;
    char *storage;
}Argument;



struct Context {
    MoolahCli *parser;
    int argv_index;
    int argc;
    char **argv;
    Option *current_option;
    Argument *current_arg;
};

#include "builtins.h"


/* MoolahCli flags */

void FreeCtx(Context *ctx)
{
    if(ctx->current_arg)
    {
        free(ctx->current_arg->storage);
        free(ctx->current_arg);
    }
    free(ctx);
}

void UpdateCtx(Context *ctx)
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

int is_flag(const char *str)
{
    if (!str || str[0] != '-') return 0;
    char *end = NULL;
    (void)strtod(str, &end);
    return end == str || *end != '\0';
}

int ShouldStop(Context *ctx) { return ctx->argv_index >= ctx->argc; }
int IsPositional(Context *ctx) { return !is_flag(ctx->argv[ctx->argv_index]); }



Option *OptionNew(MoolahCliOptionKind kind, char *name, char *long_flag, char *short_flag, char *desc, int flags, OptionCallback callback)
{
    Option *opt = malloc(sizeof(Option));
    if(!opt) return NULL;

    opt->kind = kind;
    opt->name = strdup(name ? name : "");
    opt->flag = long_flag ? strdup(long_flag) : NULL;
    opt->short_flag = short_flag ? strdup(short_flag) : NULL;
    opt->desc = desc ? strdup(desc) : NULL;
    opt->callback = callback;
    opt->flags = flags;
    opt->nargs = 0;
    return opt;
}


void OptionFree(Option *opt)
{
    if (!opt) return;

    free(opt->name);
    free(opt->flag);
    free(opt->short_flag);
    free(opt->desc);
    free(opt);
}

void OptionAppend(OptionNode **list, Option *opt)
{
    OptionNode *node = malloc(sizeof(OptionNode));
    if(!node) return;

    node->option = opt;
    node->next = NULL;

    if(!*list)
    {
        *list = node;
        return;
    }

    OptionNode *tail = *list;
    while(tail->next)
        tail = tail->next;

    tail->next = node;
}

char *OptionToStr(Option *opt)
{
    char *str = NULL;

    if (!(opt->flags & OPT_POSITIONAL) && opt->flag)
        appendf(&str, "%s", opt->flag);

    if (!(opt->flags & OPT_POSITIONAL) && opt->short_flag)
        appendf(&str, ", %s", opt->short_flag);

    if (!(opt->flags & OPT_POSITIONAL))
    {
        switch (opt->kind)
        {
            case OPT_BOOL:
                break;

            case OPT_LONG:
                appendf(&str, " <long>");
                break;

            case OPT_DOUBLE:
                appendf(&str, " <double>");
                break;

            case OPT_STR:
                appendf(&str, " <string>");
                break;
        }
    }
    else
    {
        appendf(&str, " <%s>", opt->name);
    }

    if (opt->flags & OPT_VARIADIC)
        appendf(&str, "...");

    if (opt->flags & OPT_REQUIRED)
        appendf(&str, " [required]");

    if (opt->desc)
        appendf(&str, "  %s", opt->desc);

    return str;
}


MoolahCli *MoolahCliNew(char *prog, char *desc, int flags, char **argv)
{
    prog = prog ? strdup(prog) : strdup(argv[0]);
    SetPrefix(prog);
    MoolahCli *parser = malloc(sizeof(MoolahCli));
    if(!parser) return NULL;

    parser->config = malloc(sizeof(MoolahCliConfig));
    if(!parser->config) {
        free(parser);
        return NULL;
    }
    parser->config->usage = NULL;
    parser->config->program = prog;
    parser->config->description = desc ? strdup(desc) : NULL;
    parser->config->flags = flags;
    parser->lookup = MoolahMapNew(16);

    if(!parser->lookup) {
        free(parser->config->program);
        free(parser->config->description);
        free(parser->config);
        free(parser);
        return NULL;
    }
    parser->options = NULL;
    parser->args = MoolahMapNew(16);
    parser->initialized = 0;

    if(!parser->args) {
        MoolahMapFree(parser->lookup);
        free(parser->config->program);
        free(parser->config->description);
        free(parser->config);
        free(parser);
        return NULL;
    }
    return parser;
}

void MoolahCliDestroy(MoolahCli *parser)
{
    if(!parser) return;
    if(parser->lookup) MoolahMapFree(parser->lookup);
    if(parser->args) MoolahMapFree(parser->args);
    OptionNode *node = parser->options;
    while (node) {
        OptionNode *next = node->next;
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


void MoolahCliAdd(MoolahCli *parser, MoolahCliOptionKind kind, char *name, char *long_flag, char *short_flag, char *desc, int flags, OptionCallback callback, Value *def)
{
    Option *opt = OptionNew(kind, name, long_flag, short_flag, desc, flags, callback);

    if (!opt)
        MoolahCliPanic("Unable to allocate memory for new option\n");

    Value *optval = MoolahValueNew(VALUE_POINTER);

    if (!optval)
    {
        OptionFree(opt);
        MoolahCliPanic("Unable to allocate memory for new option\n");
    }

    optval->p = opt;
    OptionAppend(&parser->options, opt);
    if(def)
    {
        MoolahCliPanicIf(!MoolahMapSet(parser->args, opt->name, *def), "Unable to store default for %s\n", opt->name);
    }
    if (parser->config->flags & MOOLAHCLI_IGNORE_CASE)
    {
        if (opt->flag)
            MoolahCliPanicIf(!MoolahMapSet(parser->lookup, strlwr(opt->flag), *optval), "Unable to register %s\n", opt->name);

        if (opt->short_flag)
            MoolahCliPanicIf(!MoolahMapSet(parser->lookup, strlwr(opt->short_flag), *optval), "Unable to register %s\n", opt->name);
    }
    else
    {
        if (opt->flag) MoolahCliPanicIf(!MoolahMapSet(parser->lookup, opt->flag, *optval), "Unable to register %s\n", opt->name);

        if (opt->short_flag)
            MoolahCliPanicIf(!MoolahMapSet(parser->lookup, opt->short_flag, *optval), "Unable to register %s\n", opt->name);
    }

    free(optval);
}



Value *ParseValue(char *str, MoolahCliOptionKind kind)
{
    ValueKind vkind = VALUE_UNKNOWN;

    switch(kind)
    {
        case OPT_BOOL:
            vkind = VALUE_BOOL;
            break;
        case OPT_DOUBLE:
            vkind = VALUE_DOUBLE;
            break;
        case OPT_LONG:
            vkind = VALUE_LONG;
            break;
        case OPT_STR:
            vkind = VALUE_STRING;
            break;
        default:
            MoolahCliPanic("Invalid Option type\n");
    }

    Value *val = MoolahValueNew(vkind);
    if(!val) MoolahCliPanic("Failed to allocate memory for argument value\n");

    switch(kind)
    {
        case OPT_BOOL:
            val->b = True;
            break;
        case OPT_DOUBLE: {
            char *end = NULL;
            errno = 0;
            val->d = strtod(str, &end);
            if (errno == ERANGE || end == str || *end != '\0')
                MoolahCliPanic("Invalid number: %s\n", str);
            break;
        }
        case OPT_LONG: {
            char *end = NULL;
            errno = 0;
            val->l = strtol(str, &end, 10);
            if (errno == ERANGE || end == str || *end != '\0')
                MoolahCliPanic("Invalid integer: %s\n", str);
            break;
        }
        case OPT_STR:
            val->s = str;
            break;
    }

    return val;
}


void Callback(Context *ctx)
{
    if(ctx->current_option && ctx->current_option->callback)
    {
        ctx->current_option->callback(ctx);
    }
}



char *FlagRepresentation(MoolahCli *parser)
{
    char *repr = strf("");
    int flags = parser->config->flags;

    if (flags & MOOLAHCLI_IGNORE_CASE)
        appendf(&repr, "The parser ignores case when matching flags. ");

    if (flags & MOOLAHCLI_IGNORE_UNKNOWN)
        appendf(&repr, "Unknown flags are ignored. ");

    if (flags & MOOLAHCLI_REMOVE_BUILTINS)
        appendf(&repr, "Built-in options are disabled. ");

    if (flags & MOOLAHCLI_INCLUDE_BUILTINS_USAGE)
        appendf(&repr, "Built-in options are included in the usage information. ");

    return repr;
}



void GenerateUsage(MoolahCli *parser)
{
    char *usage = NULL;

    appendf(&usage, "Usage: %s [options]\n\n", parser->config->program);

    if (parser->config->description)
        appendf(&usage, "%s\n\n", parser->config->description);

    appendf(&usage, "Options:\n");

    for (OptionNode *node = parser->options; node; node = node->next)
    {
        appendf(&usage, "  %s\n", OptionToStr(node->option));
    }
    appendf(&usage, "\n%s\n", FlagRepresentation(parser));

    parser->config->usage = usage;
}


void AddEasterEggs(MoolahCli *parser)
{
    MoolahCliAdd(parser, OPT_BOOL, "fuckyou", "--fuckyou", "-fy", "fucked and cursed easteregg", 0, fuckyoutoo, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "yomomma", "--yomomma", "-ym", "yo momma 😭", 0, yomomma, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "yousuck", "--yousuck", "-ys", "You suck", 0, yousuck, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "skillissue", "--skillissue", "-si", "Diagnose the obvious", 0, skillissue, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "getgood", "--getgood", "-gg", "Professional advice", 0, getgood, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "what", "--what", "-w", "What the fuck did you expect?", 0, what, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "bonk", "--bonk", "-b", "Bonk", 0, bonk, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "perish", "--perish", "-p", "Perish", 0, perish, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "bruh", "--bruh", "-br", "Bruh", 0, bruh, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "touchgrass", "--touchgrass", "-tg", "Attempt to locate grass", 0, touchgrass, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "cope", "--cope", "-c", "Cope harder", 0, cope, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "seethe", "--seethe", "-s", "Seethe", 0, seethe, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "mald", "--mald", "-m", "Mald", 0, mald, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "imagine", "--imagine", "-i", "Imagine needing help", 0, imagine, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "developer", "--developer", "-dev", "Works on my machine", 0, developer, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "worksforme", "--worksforme", "-wfm", "Works for me", 0, worksforme, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "amogus", "--amogus", "-a", "ඞ", 0, amogus, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "ohio", "--ohio", "-oh", "Enter an unstable geopolitical state", 0, ohio, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "nuclear", "--nuclear", "-n", "Launch absolutely nothing", 0, nuclear, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "fuckit", "--fuckit", "-fi", "Fuck it, we ball", 0, fuckit, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "ball", "--ball", "-ba", "We ball", 0, ball, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "rickroll", "--rickroll", "-rr", "Definitely not a rickroll", 0, rickroll, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "adlib", "--adlib", "-ad", "Lil Wayne is the GOAT!", 0, adlib, NULL);
    MoolahCliAdd(parser, OPT_BOOL, "whatsgood", "--whatsgood", "-wg", "What's good?", 0, whatsgood, NULL);
}


Context *Init(MoolahCli *parser, int argc, char **argv)
{
    Context *ctx = malloc(sizeof(Context));
    if(!ctx)
    {
        MoolahCliPanic("Unable to allocate memory for parser context\n");
    }
    ctx->parser = parser;
    ctx->argc = argc;
    ctx->argv = argv;
    ctx->argv_index = 1;
    ctx->current_option = NULL;
    ctx->current_arg = calloc(1, sizeof(Argument));
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
        MoolahCliAdd(parser, OPT_BOOL, "help", "--help", "-h", "Show this help message", 0, help, NULL);
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



Value *ParseList(int argc, char **argv, Option *opt, char *flag, int *index, char *first_value)
{
    if(opt->flags & OPT_LIST)
    {
        Value *list = MoolahValueNew(VALUE_LIST);

        if(!list)
            MoolahCliPanic("failed to allocate list\n");

        if (first_value) {
            Value *first = ParseValue(first_value, opt->kind);
            list = MoolahValueAppend(list, first);
            free(first);
        }

        for (*index = *index + 1; *index < argc && !is_flag(argv[*index]); (*index)++)
        {
            Value *parsed = ParseValue(argv[*index], opt->kind);

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

            if (!(opt->flags & OPT_VARIADIC) && (size_t)opt->nargs == MoolahValueLen(list))
            {
                opt->flags |= OPT_DONE;
                if (*index < argc && is_flag(argv[*index])) --(*index);
                return list;
            }
        }

        if (!(opt->flags & OPT_VARIADIC) && !(opt->flags & OPT_DONE))
            MoolahCliPanic("not enough positional values for %s\n", flag);

        if (*index < argc && is_flag(argv[*index])) (*index)--;
        return list;
    }

    return NULL;
}


void ParsePositional(MoolahCli *parser, int argc, char **argv, int *index)
{
    for (OptionNode *node = parser->options; node; node = node->next)
    {
        Option *opt = node->option;

        if (!(opt->flags & OPT_POSITIONAL) || (opt->flags & OPT_DONE))
            continue;

        if (opt->flags & OPT_LIST)
        {
            Value *list = ParseList(argc, argv, opt, opt->flag, index, argv[*index]);
            MoolahCliPanicIf(!list, "Unable to parse values for %s\n", opt->flag);

            MoolahCliPanicIf(!MoolahMapSet(parser->args, opt->name, *list), "Unable to store %s\n", opt->name);
            free(list);
            return;
        }

        Value *parsed = ParseValue(argv[*index], opt->kind);
        MoolahCliPanicIf(!parsed, "Unable to parse value for %s\n", opt->flag);

        MoolahCliPanicIf(!MoolahMapSet(parser->args, opt->name, *parsed), "Unable to store %s\n", opt->name);
        opt->flags |= OPT_DONE;
        return;
    }
}


void ParseNonPositional(Context *ctx)
{
    MoolahCli *parser = ctx->parser;
    Argument *arg = ctx->current_arg;
    Option *opt = ctx->current_option;

    if (opt->flags & OPT_LIST)
    {
        Value *value = ParseList(ctx->argc, ctx->argv, opt, arg->flag, &ctx->argv_index, arg->value);
        MoolahCliPanicIf(!value, "Unable to parse list for flag %s\n", arg->flag);

        MoolahCliPanicIf(!MoolahMapSet(parser->args, opt->name, *value), "Unable to store %s\n", opt->name);
        free(value);
        return;
    }

    Value *parsed = ParseValue(arg->value, opt->kind);
    MoolahCliPanicIf(!parsed, "Unable to parse value for flag %s\n", arg->flag);

    MoolahMapSet(parser->args, opt->name, *parsed);
    opt->flags |= OPT_DONE;
    free(parsed);
}
















int FindOption(Context *ctx)
{
    Argument *arg = ctx->current_arg;
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
        strlwr(arg->flag);

    Value *optval = MoolahMapGet(parser->lookup, arg->flag);

    if (!optval)
    {
        MoolahCliPanicIf(!(parser->config->flags & MOOLAHCLI_IGNORE_UNKNOWN), "Unknown flag %s\n", arg->flag);
        return 0;
    }

    ctx->current_option = optval->p;

    if (ctx->current_option->kind == OPT_BOOL)
        return 1;

    if (!arg->value)
    {
        if (ctx->argv_index + 1 >= ctx->argc)
            MoolahCliPanic("No value provided for %s\n", arg->flag);

        arg->value = ctx->argv[++ctx->argv_index];
    }

    return 1;
}








void Parse(Context *ctx)
{
    if (IsPositional(ctx))
    {
        ParsePositional(ctx->parser, ctx->argc, ctx->argv, &ctx->argv_index);
    }
    else if(FindOption(ctx))
    {
        ParseNonPositional(ctx);
        Callback(ctx);
    }
}




void EnsureRequired(MoolahCli *parser)
{
    char *error = NULL;

    for (OptionNode *node = parser->options; node; node = node->next)
    {
        Option *opt = node->option;

        if (opt->flags & OPT_REQUIRED && !MoolahMapGet(parser->args, opt->name))
        {
            const char *label = opt->flag != NULL ? opt->flag : opt->name;
            appendf(&error, "Missing required %s %s\n",
                    (opt->flags & OPT_POSITIONAL) ? "argument" : "option",
                    label != NULL ? label : "(unnamed)");
        }
    }

    if (error) MoolahCliPanic("%s", error);
    free(error);
}


void MoolahCliParse(MoolahCli *parser, int argc, char **argv)
{
    Context *ctx = Init(parser, argc, argv);
    while(!ShouldStop(ctx))
    {
        Parse(ctx);
        UpdateCtx(ctx);
    }

    FreeCtx(ctx);
    EnsureRequired(parser);
}


Value *MoolahCliGet(MoolahCli *parser, const char *name)
{
    return parser && parser->args ? MoolahMapGet(parser->args, name) : NULL;
}

char *MoolahCliGetString(MoolahCli *parser, const char *name)
{
    Value *value = MoolahCliGet(parser, name);
    return value && value->kind == VALUE_STRING ? value->s : NULL;
}

