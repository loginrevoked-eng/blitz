#include "ssh.h"
#include "ssh-util.h"

int HandlerSshBanner(Ipv4 ip, uint16_t port,
    const RegistryFinding *finding, void *context)
{
    (void)ip;
    (void)port;
    (void)context;
    if (finding == NULL || finding->service != REGISTRY_SERVICE_SSH)
        return 0;

    SshOpenSshVersion version;
    if (!SshUtilParseOpenSshVersion(finding->detail.ssh.identification,
            &version) || version.has_addendum)
        return 0;

    if (version.major < 9 || version.major > 10 ||
        (version.major == 10 && version.minor > 3))
        return 0;
    return 1;
}
