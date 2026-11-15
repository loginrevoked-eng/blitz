#include <stdio.h>
#include <string.h>
#include "public.h"

static int verbose_callback_called;

static void on_verbose(MoolahCliContext *ctx)
{
    (void)ctx;
    verbose_callback_called = 1;
}

static void print_list(const MoolahValue *value)
{
    if (!value || value->kind != MOOLAH_VALUE_LIST) {
        puts("(none)");
        return;
    }

    const MoolahValueNode *node = value->list;
    if (!node) {
        puts("(empty)");
        return;
    }

    for (; node; node = node->next) {
        if (node->value.kind == MOOLAH_VALUE_STRING)
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

    MoolahValue default_count = { .kind = MOOLAH_VALUE_LONG, .l = 1 };
    MoolahValue default_scale = { .kind = MOOLAH_VALUE_DOUBLE, .d = 1.0 };

    /* Scalar option types, long and short spellings, and a required option. */
    MoolahCliAdd(parser, MOOLAHCLI_OPT_STR, "name", "--name", "-n", "Name to greet (required)",
              MOOLAHCLI_OPT_REQUIRED, NULL, NULL, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_LONG, "count", "--count", "-c", "Greeting count (default: 1)",
              0, NULL, &default_count, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_DOUBLE, "scale", "--scale", "-s", "Floating point example (default: 1.0)",
              0, NULL, &default_scale, NULL);
    MoolahCliAdd(parser, MOOLAHCLI_OPT_BOOL, "verbose", "--verbose", "-v", "Run a callback and enable verbose output",
              0, on_verbose, NULL, NULL, NULL);

    /* A variadic list consumes values through the next option. */
    MoolahCliAdd(parser, MOOLAHCLI_OPT_STR, "tags", "--tag", "-t", "One or more labels",
              MOOLAHCLI_OPT_LIST | MOOLAHCLI_OPT_VARIADIC, NULL, NULL, NULL);

    /* Positional arguments use the same value kinds and required flag. */
    MoolahCliAdd(parser, MOOLAHCLI_OPT_STR, "input", "<input>", NULL, "Optional positional input file",
              MOOLAHCLI_OPT_POSITIONAL, NULL, NULL, NULL);

    MoolahCliParse(parser, argc, argv);

    MoolahValue *name = MoolahCliGet(parser, "name");
    MoolahValue *count = MoolahCliGet(parser, "count");
    MoolahValue *scale = MoolahCliGet(parser, "scale");
    MoolahValue *verbose = MoolahCliGet(parser, "verbose");
    MoolahValue *input = MoolahCliGet(parser, "input");
    MoolahValue *tags = MoolahCliGet(parser, "tags");

    printf("Name: %s\n", name && name->kind == MOOLAH_VALUE_STRING ? name->s : "(missing)");
    printf("Count: %ld\n", count && count->kind == MOOLAH_VALUE_LONG ? count->l : 0L);
    printf("Scale: %.3f\n", scale && scale->kind == MOOLAH_VALUE_DOUBLE ? scale->d : 0.0);
    printf("Verbose flag: %s\n", verbose && verbose->kind == MOOLAH_VALUE_BOOL && verbose->b == MOOLAH_TRUE ? "on" : "off");
    printf("Callback invoked: %s\n", verbose_callback_called ? "yes" : "no");
    printf("Input: %s\n", input && input->kind == MOOLAH_VALUE_STRING ? input->s : "(not provided)");
    printf("Tags: ");
    print_list(tags);
    printf("MoolahCli mode: %s\n", minimal ? "built-ins removed" : "case-insensitive; unknown flags ignored; built-in usage included");

    MoolahCliDestroy(parser);
    return 0;
}

