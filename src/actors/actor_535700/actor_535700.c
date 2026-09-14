#include "common.h"

#include "gameplay/1BC.h"
#include "main/mc.h"
#include "main/task.h"

extern s16 D_80071076;

INCLUDE_ASM("actors/nonmatchings/actor_535700/actor_535700", func_actor_535700_80131E2C);

INCLUDE_ASM("actors/nonmatchings/actor_535700/actor_535700", func_actor_535700_80131EF0);

void func_actor_535700_80131F2C(void)
{
    if (Mc_SaveData.field_23 != 9) {
        Mc_SaveData.field_6 = 0x1D;
        Mc_SaveData.field_8 = 5;
        Mc_SaveData.field_5 = 2;
        D_80071076          = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Mc_SaveData.field_5C5 = 6;
        Gp_RestoreStreamRng();
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_535700/actor_535700", ActorsShared80131f9cSub0);

INCLUDE_ASM("actors/nonmatchings/actor_535700/actor_535700", func_actor_535700_80132108);

INCLUDE_RODATA("actors/nonmatchings/actor_535700/actor_535700", D_actor_535700_80131E20);
