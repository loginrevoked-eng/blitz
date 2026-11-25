// telnet.h
#ifndef TELNET_H
#define TELNET_H
#include <windows.h>
#include <stdint.h>
/* Queues a session and returns immediately. TRUE means CONNECT was submitted. */
BOOL TelnetCreateSession(char *ip, uint16_t port, char *username, char *password);
BOOL TelnetCreateSessionHardcoded(char *ip, uint16_t port, BOOL *exhaused);
BOOL TelnetCreateSessionHardcodedII(uint64_t ip, uint16_t port, BOOL *exhaused);

/* Waits for all submitted sessions to finish; intended for simple standalone callers. */
BOOL TelnetWaitForSessions(DWORD timeout_ms);
#endif
