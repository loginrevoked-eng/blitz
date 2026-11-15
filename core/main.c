#include "initialize.h"
#include "finalize.h"
#include "ui/ui.h"

int main(int argc, char **argv)
{
    ProbeContext context = { 0 };
    ProbeInitializeOrPanic(&context, argc, argv);
    UIHandle *ui = UILaunch(&context);
    int finalized = ProbeFinalize(&context);
    UIClose(ui);
    return !finalized;
}
