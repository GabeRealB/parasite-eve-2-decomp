#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"

void func_actor_260500_80149E38(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(2);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

INCLUDE_ASM("actors/nonmatchings/actor_260500/actor_260500", func_actor_260500_80149E80);

INCLUDE_RODATA("actors/nonmatchings/actor_260500/actor_260500", D_actor_260500_80149E20);
