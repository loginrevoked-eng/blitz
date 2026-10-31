#ifndef BOT_CLI_SPINNER_H
#define BOT_CLI_SPINNER_H

#include <stdarg.h>
#include <stddef.h>
#include <windows.h>

#include "progress.h"

typedef struct Spinner Spinner;

Spinner *SpinnerCreate(StringCycle *frames, const char *verb, HANDLE out, DWORD interval);
Spinner *SpinnerCreateStderr(StringCycle *frames, const char *verb, DWORD interval);
Spinner *SpinnerCreateVA(const char *verb, HANDLE out, DWORD interval, size_t count, va_list ap);
Spinner *SpinnerCreateV(const char *verb, HANDLE out, DWORD interval, size_t count, ...);
Spinner *SpinnerCreateVStderr(const char *verb, DWORD interval, size_t count, ...);
int SpinnerStart(Spinner *sp);
void SpinnerStop(Spinner *sp);
int SpinnerIsRunning(const Spinner *sp);
DWORD SpinnerGetInterval(Spinner *sp);
size_t SpinnerGetVerb(Spinner *sp, char *dst, size_t cap);
int SpinnerSetInterval(Spinner *sp, DWORD interval);
int SpinnerSetVerb(Spinner *sp, const char *verb);
int SpinnerClearVerb(Spinner *sp);
int SpinnerSetFrames(Spinner *sp, StringCycle *frames);
void SpinnerDelete(Spinner *sp);

#endif
