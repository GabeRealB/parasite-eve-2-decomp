#include "common.h"

#include "main/display.h"
#include "main/task.h"

extern TaskDesc D_actor_151000_80133360;
extern s32      D_actor_151000_8013D378;

void func_actor_151000_80131E2C(Task* task)
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
        taskKill(task);
    }
}

void func_actor_151000_80131EE0(s32 arg0)
{
    D_actor_151000_8013D378 = arg0;
    if (arg0 != 0) {
        Task_SpawnFromTable(&D_actor_151000_80133360, 0, 0, 0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", ActorsShared80131f9cSub0);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_80132084);

INCLUDE_RODATA("actors/nonmatchings/actor_151000/actor_151000", D_actor_151000_80131E20);
