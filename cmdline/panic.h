#ifndef MOOLAHCLI_PANIC_H
#define MOOLAHCLI_PANIC_H

void MoolahCliPanic(const char *fmt, ...);
void MoolahCliPanicIf(int condition, const char *fmt, ...);
void MoolahCliSetPrefix(char *prefix);
#endif
