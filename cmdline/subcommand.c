#include "subcommand.h"

static MoolahCli *MoolahCliFindSubcommand(MoolahCli *parser,
    const char *name)
{
    if (parser == NULL || name == NULL)
        return NULL;
    MoolahValue *value = MoolahHashMap_Get(parser->subcommands, name);
    if (value == NULL || value->kind != MOOLAH_VALUE_POINTER)
        return NULL;
    return value->p;
}

int MoolahCliIsSubcommand(MoolahCliContext *ctx)
{
    if (ctx == NULL || ctx->argv_index >= ctx->argc)
        return 0;
    char *argument = ctx->argv[ctx->argv_index];
    MoolahCli *lookup = ctx->level == 0 ? ctx->root : ctx->parser;
    return argument != NULL && !MoolahCliIsFlag(argument) &&
        MoolahCliFindSubcommand(lookup, argument) != NULL;
}

int MoolahCliSwapParser(MoolahCliContext *ctx)
{
    if (!MoolahCliIsSubcommand(ctx))
        return 0;
    MoolahCli *lookup = ctx->level == 0 ? ctx->root : ctx->parser;
    MoolahCli *child = MoolahCliFindSubcommand(lookup,
        ctx->argv[ctx->argv_index]);
    if (child == NULL)
        return 0;
    ctx->parser = child;
    ctx->level = 1;
    return 1;
}
