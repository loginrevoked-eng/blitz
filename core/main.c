#include "initialize.h"
#include "finalize.h"

int main(int argc, char **argv)
{
    ProbeContext context = { 0 };
    ProbeInitializeOrPanic(&context, argc, argv);
    return !ProbeFinalize(&context);
}
