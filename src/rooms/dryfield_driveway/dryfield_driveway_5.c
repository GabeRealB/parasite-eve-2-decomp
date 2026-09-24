#include "common.h"

#include "gameplay/3FB8.h"

/// Script callback: stores its argument in the gameplay byte `D_80115768`.
void func_dryfield_driveway_8017DC64(u8 arg0)
{
    D_80115768 = arg0;
}
