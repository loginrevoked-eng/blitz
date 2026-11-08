#ifndef PROBE_IO_SIGNAL_H
#define PROBE_IO_SIGNAL_H

typedef void (*ProbeSignalCallback)(int signal_number, void *context);

int ProbeSignalHook(ProbeSignalCallback callback, void *context);
int ProbeSignalDispatch(void);
void ProbeSignalUnhook(void);

#endif
