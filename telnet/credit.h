#ifndef TELNET_CREDIT_H
#define TELNET_CREDIT_H

#include <windows.h>

typedef struct {
    char *username;
    char *password;
} Cred;

BOOL GetNextCred(Cred *out_cred);

#endif
