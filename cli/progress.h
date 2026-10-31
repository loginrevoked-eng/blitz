#ifndef BOT_CLI_PROGRESS_H
#define BOT_CLI_PROGRESS_H

#include <stdarg.h>
#include <stddef.h>
#include <windows.h>

typedef struct StringCycle StringCycle;
typedef struct ProgressBar ProgressBar;

StringCycle *StringCycleCreate(const char **src, size_t count);
StringCycle *StringCycleCreateVA(size_t count, va_list ap);
StringCycle *StringCycleCreateV(size_t count, ...);
const char *StringCycleGet(const StringCycle *c, size_t i, size_t *len);
size_t StringCycleLength(const StringCycle *c);
int StringCycleSet(StringCycle *c, size_t i, const char *s);
const char *StringCycleCurrent(const StringCycle *c, size_t *len);
const char *StringCycleNext(StringCycle *c, size_t *len);
void StringCycleReset(StringCycle *c);
void StringCycleDelete(StringCycle *c);

ProgressBar *ProgressBarCreate(StringCycle *frames, const char *verb, size_t steps);
size_t ProgressBarGetStep(const ProgressBar *bar);
size_t ProgressBarGetSteps(const ProgressBar *bar);
size_t ProgressBarGetPercent(const ProgressBar *bar);
int ProgressBarGetEta(const ProgressBar *bar);
size_t ProgressBarGetVerb(const ProgressBar *bar, char *dst, size_t cap);
int ProgressBarSetStep(ProgressBar *bar, size_t step);
void ProgressBarUpdateSteps(ProgressBar *bar, int delta);
void ProgressBarStep(ProgressBar *bar);
int ProgressBarSetSteps(ProgressBar *bar, size_t steps);
int ProgressBarSetVerb(ProgressBar *bar, const char *verb);
int ProgressBarClearVerb(ProgressBar *bar);
void ProgressBarSetFrames(ProgressBar *bar, StringCycle *frames);
void ProgressBarReset(ProgressBar *bar);
char *ProgressBarDraw(ProgressBar *bar);
int ProgressBarWrite(ProgressBar *bar, HANDLE hndl);
int ProgressBarStepAndWrite(ProgressBar *bar, HANDLE hndl);
int ProgressBarWriteStderr(ProgressBar *bar);
int ProgressBarStepAndWriteStderr(ProgressBar *bar);
void ProgressBarDelete(ProgressBar *bar);

#endif
