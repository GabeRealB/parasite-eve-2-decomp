#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"

void ActorsShared80131f28(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(2);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}
