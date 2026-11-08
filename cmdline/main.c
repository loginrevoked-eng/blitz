#include <stdio.h>
#include <string.h>
#include "public.h"

static int verbose_callback_called;

static void on_verbose(Context *ctx)
{
    (void)ctx;
    verbose_callback_called = 1;
}

static void print_list(const Value *value)
{
    if (!value || value->kind != VALUE_LIST) {
        puts("(none)");
        return;
    }

    const ValueNode *node = value->list;
    if (!node) {
        puts("(empty)");
        return;
    }

    for (; node; node = node->next) {
        if (node->value.kind == VALUE_STRING)
            printf("%s%s", node->value.s, node->next ? ", " : "\n");
    }
}

/*
 * MoolahCli feature tour
 *
 * Typical run:
 *   parser_example input.txt --NAME Ada --count 3 --scale 1.25 \
 *       --tag science math --verbose --unknown=ignored
 *
 * Try --help to see generated usage. Prefix the run with --minimal to
 * disable the built-in help/easter-egg options; that selector is consumed
 * here before the parser sees the remaining arguments.
 */
int main(int argc, char **argv)
{
    int minimal = 0;
    int kept = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--minimal") == 0) {
            minimal = 1;
        } else {
            argv[kept++] = argv[i];
        }
    }
    argc = kept;

    int parser_flags = minimal
        ? MOOLAHCLI_REMOVE_BUILTINS
        : MOOLAHCLI_IGNORE_CASE | MOOLAHCLI_IGNORE_UNKNOWN | MOOLAHCLI_INCLUDE_BUILTINS_USAGE;

    MoolahCli *parser = MoolahCliNew(argv[0], "Example of the parser's supported features", parser_flags, argv);
    if (!parser) {
        fputs("Could not create parser\n", stderr);
        return 1;
    }

    Value default_count = { .kind = VALUE_LONG, .l = 1 };
    Value default_scale = { .kind = VALUE_DOUBLE, .d = 1.0 };

    /* Scalar option types, long and short spellings, and a required option. */
    MoolahCliAdd(parser, OPT_STR, "name", "--name", "-n", "Name to greet (required)",
              OPT_REQUIRED, NULL, NULL);
    MoolahCliAdd(parser, OPT_LONG, "count", "--count", "-c", "Greeting count (default: 1)",
              0, NULL, &default_count);
    MoolahCliAdd(parser, OPT_DOUBLE, "scale", "--scale", "-s", "Floating point example (default: 1.0)",
              0, NULL, &default_scale);
    MoolahCliAdd(parser, OPT_BOOL, "verbose", "--verbose", "-v", "Run a callback and enable verbose output",
              0, on_verbose, NULL);

    /* A variadic list consumes values through the next option. */
    MoolahCliAdd(parser, OPT_STR, "tags", "--tag", "-t", "One or more labels",
              OPT_LIST | OPT_VARIADIC, NULL, NULL);

    /* Positional arguments use the same value kinds and required flag. */
    MoolahCliAdd(parser, OPT_STR, "input", "<input>", NULL, "Optional positional input file",
              OPT_POSITIONAL, NULL, NULL);

    MoolahCliParse(parser, argc, argv);

    Value *name = MoolahCliGet(parser, "name");
    Value *count = MoolahCliGet(parser, "count");
    Value *scale = MoolahCliGet(parser, "scale");
    Value *verbose = MoolahCliGet(parser, "verbose");
    Value *input = MoolahCliGet(parser, "input");
    Value *tags = MoolahCliGet(parser, "tags");

    printf("Name: %s\n", name && name->kind == VALUE_STRING ? name->s : "(missing)");
    printf("Count: %ld\n", count && count->kind == VALUE_LONG ? count->l : 0L);
    printf("Scale: %.3f\n", scale && scale->kind == VALUE_DOUBLE ? scale->d : 0.0);
    printf("Verbose flag: %s\n", verbose && verbose->kind == VALUE_BOOL && verbose->b == True ? "on" : "off");
    printf("Callback invoked: %s\n", verbose_callback_called ? "yes" : "no");
    printf("Input: %s\n", input && input->kind == VALUE_STRING ? input->s : "(not provided)");
    printf("Tags: ");
    print_list(tags);
    printf("MoolahCli mode: %s\n", minimal ? "built-ins removed" : "case-insensitive; unknown flags ignored; built-in usage included");

    MoolahCliDestroy(parser);
    return 0;
}

