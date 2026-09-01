#include "common.h"
#include "gameplay/3CD8.h"

void func_actor_143400_80131E24(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(2);
        func_800E6D4C(0x140, 0x100);
        return;
    }
    Gp_ResetCap();
}

INCLUDE_RODATA("actors/nonmatchings/actor_143400/actor_143400", D_actor_143400_80131E20);
