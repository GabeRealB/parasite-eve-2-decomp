#include "common.h"

#include "actors/actor_535700.h"
#include "gameplay/1BC.h"
#include "main/mc.h"
#include "main/task.h"

extern s16 D_80071076;

void func_actor_535700_80131E2C(Task* task)
{
    TILE* tile;
    s32   count;

    if (count != 0) {
        tile           = (TILE*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
        SetTile(tile);
        tile->r0 = 0;
        tile->g0 = 0;
        tile->b0 = 0;
        tile->x0 = -0xA0;
        tile->y0 = -0x80;
        tile->w  = 0x140;
        tile->h  = 0x100;
        addPrim(Gpu_CurrentOt + 0xA, tile);
    } else {
        Task_Kill(task);
    }
    D_actor_535700_80146840--;
}

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
