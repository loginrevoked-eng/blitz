#include "ssh-util.h"

#include <stdlib.h>
#include <string.h>

int SshUtilParseOpenSshVersion(const char *banner,
    SshOpenSshVersion *version)
{
    if (banner == NULL || version == NULL)
        return 0;

    const char *identification = strstr(banner, "SSH-2.0-");
    if (identification == NULL)
        return 0;

    const char *line_end = strpbrk(identification, "\r\n");
    if (line_end == NULL)
        line_end = identification + strlen(identification);

    const char *software = identification + strlen("SSH-2.0-");
    if (software >= line_end || strncmp(software, "OpenSSH_",
            strlen("OpenSSH_")) != 0)
        return 0;
    const char *number = software + strlen("OpenSSH_");

    char *major_end = NULL;
    unsigned long major = strtoul(number, &major_end, 10);
    if (major_end == number || major_end >= line_end || *major_end != '.')
        return 0;

    const char *minor_start = major_end + 1;
    char *minor_end = NULL;
    unsigned long minor = strtoul(minor_start, &minor_end, 10);
    if (minor_end == minor_start || minor_end > line_end)
        return 0;

    const char *token_end = minor_end;
    if (token_end < line_end && (*token_end == 'p' || *token_end == 'P')) {
        ++token_end;
        const char *patch_start = token_end;
        while (token_end < line_end && *token_end >= '0' && *token_end <= '9')
            ++token_end;
        if (token_end == patch_start)
            token_end = patch_start - 1;
    }

    int has_addendum = token_end < line_end &&
        *token_end != ' ' && *token_end != '\t';
    while (token_end < line_end && (*token_end == ' ' || *token_end == '\t'))
        ++token_end;
    if (token_end < line_end)
        has_addendum = 1;

    version->major = major;
    version->minor = minor;
    version->has_addendum = has_addendum;
    return 1;
}
