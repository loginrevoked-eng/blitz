#ifndef HANDLERS_SSH_UTIL_H
#define HANDLERS_SSH_UTIL_H

typedef struct {
    unsigned long major;
    unsigned long minor;
    int has_addendum;
} SshOpenSshVersion;

int SshUtilParseOpenSshVersion(const char *banner,
    SshOpenSshVersion *version);

#endif
