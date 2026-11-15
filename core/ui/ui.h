#ifndef CORE_UI_UI_H
#define CORE_UI_UI_H

#include "../probe.h"

typedef struct UIHandle UIHandle;

UIHandle *UILaunch(ProbeContext *ctx);
void UIClose(UIHandle *handle);

#endif
