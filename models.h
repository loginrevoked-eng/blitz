#ifndef MODELS_H
#define MODELS_H

#include "cidr.h"
#include "cmdline/value.h"
#include "probe/probe.h"
#include "picker.h"
#include "uibridge.h"
#include <stddef.h>

typedef struct Config Config;

typedef struct {
    int *list;
    size_t count;
} IntList;

typedef struct {
    int error;
    volatile LONG interrupted;
    char *cidr_text;
    Config *config;
    CidrRange *range;
    IpPicker *picker;
    ProbeManager probe_manager;
    UIBridge ui;
} BotContext;

IntList *IntListFromValue(const Value *value);
char *IntListRepr(IntList *list);
BotContext *BotContextOrDie(int argc, char **argv);
BotContext *BotInitialize(int argc, char **argv);
int BotFinalize(BotContext *context);

#endif
