#ifndef PANIC_H
#define PANIC_H

void MoolahCliPanic(const char *fmt, ...);
void MoolahCliPanicIf(int condition, const char *fmt, ...);
void SetPrefix(char *prefix);
#endif