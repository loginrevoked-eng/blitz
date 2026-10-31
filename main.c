#include "cidr.h"
#include "config/config.h"
#include "models.h"
#include "probe/probe.h"
#include "logging.h"
#include <stdint.h>


int main(int argc, char **argv)
{
    BotContext *context = BotInitialize(argc, argv);
    IcmpConfig icmp_config = IcmpConfig_New();

    for (uint32_t i = 0; i < context->config->limit; i++) {
        Ipv4 target;
        uint64_t picked_index;
        if (!IpPicker_Pick(context->picker, &target, &picked_index))
            break;
        if (!ProbeManagerAddProbe(&context->probe_manager, context->picker,
                                  target, picked_index, PROBE_ICMP, &icmp_config,
                                  UIBridgePrintProbeStats, &context->ui)) {
            Panic("failed to start ICMP probe to %s\n", Ipv4Repr(target));
            context->error = 1;
            break;
        }
    }

    return BotFinalize(context);
}
