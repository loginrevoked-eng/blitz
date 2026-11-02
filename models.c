#include "models.h"
#include "config.h"
#include "logging.h"
#include "pool/init.h"
#include "str/str.h"
#include "fs.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

static PVOID volatile active_context;
static volatile LONG active_ctrl_handlers;

static BOOL WINAPI BotConsoleCtrlHandler(DWORD ctrl_type)
{
    if (ctrl_type != CTRL_C_EVENT && ctrl_type != CTRL_BREAK_EVENT)
        return FALSE;

    InterlockedIncrement(&active_ctrl_handlers);
    BotContext *context = (BotContext *)InterlockedCompareExchangePointer(
        &active_context, NULL, NULL);
    if (context != NULL) {
        InterlockedExchange(&context->interrupted, 1);
        IpPicker_RequestStop(context->picker);
    }
    InterlockedDecrement(&active_ctrl_handlers);
    return context != NULL;
}

static int WriteBucketLists(const IpPicker *picker)
{
    char *json = BucketListsDumpStr(picker->eligible, picker->cooldown);
    if (json == NULL)
        return 0;

    int written = Fs_WriteFile("buckets.json", json, strlen(json));
    free(json);
    return written;
}

IntList *IntListFromValue(const Value *value)
{
    if (value == NULL || value->kind != VALUE_LIST)
        return NULL;

    IntList *list = calloc(1, sizeof *list);
    if (list == NULL)
        return NULL;

    list->count = ValueLen((Value *)value);
    if (list->count != 0) {
        list->list = malloc(list->count * sizeof *list->list);
        if (list->list == NULL) {
            free(list);
            return NULL;
        }
    }

    size_t i = 0;
    for (const ValueNode *node = value->list; node != NULL; node = node->next) {
        if (node->value.kind != VALUE_LONG || node->value.l < 1 || node->value.l > 65535) {
            free(list->list);
            free(list);
            return NULL;
        }
        list->list[i++] = (int)node->value.l;
    }
    return list;
}

char *IntListRepr(IntList *list)
{
    if (list == NULL)
        return NULL;

    char *repr = strf("");
    if (repr == NULL)
        return NULL;

    for (size_t i = 0; i < list->count; i++) {
        if (appendf(&repr, "%s%d", i == 0 ? "" : ", ", list->list[i]) == NULL) {
            free(repr);
            return NULL;
        }
    }
    return repr;
}

BotContext *BotContextOrDie(int argc, char **argv)
{
    BotContext *context = calloc(1, sizeof *context);
    if (context == NULL)
        Panic("Failed to allocate bot context\n");

    context->config = ConfigOrDie(argc, argv, NULL);
    if (context->config->cidr == NULL)
        Panic("Missing CIDR text\n");
    context->cidr_text = strdup(context->config->cidr);
    if (context->cidr_text == NULL)
        Panic("Failed to copy CIDR text\n");

    if (context->config->cooldown_period > UINT32_MAX / 1000U)
        Panic("Cooldown interval is too large\n");
    context->picker = IpPicker_Initialize(context->cidr_text,
                                          context->config->bucket_width,
                                          context->config->cooldown_period * 1000U,
                                          context->config->exclude_private_ranges,
                                          NULL, 0);
    if (context->picker == NULL) {
        Panic("Failed to create IP picker\n");
    }
    InterlockedExchangePointer(&active_context, context);
    if (!SetConsoleCtrlHandler(BotConsoleCtrlHandler, TRUE)) {
        InterlockedExchangePointer(&active_context, NULL);
        Panic("Failed to install Ctrl+C handler\n");
    }
    return context;
}

BotContext *BotInitialize(int argc, char **argv)
{
    srand((unsigned)time(NULL));
    BotContext *context = BotContextOrDie(argc, argv);
    SetLogLevel(context->config->log_level);
    uint64_t exhaustion_count =
        IpPicker_InitialExhaustionCount(context->picker);
    if (context->config->cooldown_period != 0 && exhaustion_count != 0 &&
        context->config->limit > exhaustion_count) {
        Log("With buckets %u IPs wide, the picker has about %llu ready buckets. "
            "If it uses them faster than their cooldown expires, it may pause "
            "after about %llu picks and wait up to %u seconds for a bucket "
            "to become ready again. Probes already in flight will finish "
            "during that pause.\n",
            context->config->bucket_width,
            (unsigned long long)exhaustion_count,
            (unsigned long long)exhaustion_count,
            context->config->cooldown_period);
    }
    if (!ProbeManagerInitialize(&context->probe_manager,
                                context->config->probe_semaphore))
        Panic("Failed to initialize probe manager\n");
    if (context->config->out_path != NULL &&
        !ProbeManagerOpenOutput(&context->probe_manager,
                                context->config->out_path))
        Panic("Failed to open output file: %s\n", context->config->out_path);
    context->probe_manager.filter_by = context->config->filter_by;
    if (context->config->limit != 0 &&
        !UIBridgeInitialize(&context->ui, &context->probe_manager,
                            context->picker, GetStdHandle(STD_OUTPUT_HANDLE),
                            24, '|', 8, 1000)) {
        Panic("Failed to start scan UI\n");
    }
    return context;
}

int BotFinalize(BotContext *context)
{
    if (context == NULL)
        return 1;
    InterlockedExchangePointer(&active_context, NULL);
    SetConsoleCtrlHandler(BotConsoleCtrlHandler, FALSE);
    while (InterlockedCompareExchange(&active_ctrl_handlers, 0, 0) != 0)
        Sleep(1);

    int error = context->error;
    ProbeManagerFinalize(&context->probe_manager);
    if (context->probe_manager.error)
        error = 1;
    UIBridgeFinalize(&context->ui);
    IpPicker_Stop(context->picker);
    if (!WriteBucketLists(context->picker)) {
        Warn("Failed to write picker bucket lists\n");
        error = 1;
    }
    IpPicker_Finalize(context->picker);
    int interrupted = context->interrupted != 0;
    free(context->cidr_text);
    FreeConfig(context->config);
    free(context);
    if (error)
        return error;
    return interrupted ? 130 : 0;
}
