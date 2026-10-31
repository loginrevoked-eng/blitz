#include "models.h"
#include "config.h"
#include "logging.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    BotContext *context = BotInitialize(argc, argv);

    for (uint32_t i = 0; i < context->config->limit; i++) {
        Ipv4 ip;
        if (!IpPicker_Pick(context->picker, &ip, NULL))
            break;
        char *ip_repr = Ipv4Repr(ip);
        if (ip_repr == NULL)
            Panic("Failed to format IPv4 address\n");

        printf("%s\n", ip_repr);
        for (size_t port = 0; port < context->config->ports->count; port++)
            printf("   %s:%d\n", ip_repr, context->config->ports->list[port]);
        free(ip_repr);
    }

    return BotFinalize(context);
}
