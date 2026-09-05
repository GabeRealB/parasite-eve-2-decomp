#include "common.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"

extern s8       D_8007106B;
extern TaskDesc D_acropolis_plaza_80183824;

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017DD90);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", D_acropolis_plaza_8017D5E0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017DE24);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017DFE0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017E7E4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017E9A8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017ECF8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017F48C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017F620);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017F770);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017F9EC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017FB50);

/// Draws the cinematic letterbox: two black 0x140x0x18 `TILE` bars spanning the
/// full screen width at the top (y -0x78) and bottom (y 0x60), linked into
/// `Gpu_CurrentOt[3]`.
void func_acropolis_plaza_8017FF18(void)
{
    TILE* tile;

    tile           = (TILE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
    SetTile(tile);
    tile->b0 = 0;
    tile->g0 = 0;
    tile->r0 = 0;
    tile->x0 = -0xA0;
    tile->y0 = -0x78;
    tile->w  = 0x140;
    tile->h  = 0x18;
    addPrim(Gpu_CurrentOt + 3, tile);

    tile           = (TILE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
    SetTile(tile);
    tile->b0 = 0;
    tile->g0 = 0;
    tile->r0 = 0;
    tile->x0 = -0xA0;
    tile->y0 = 0x60;
    tile->w  = 0x140;
    tile->h  = 0x18;
    addPrim(Gpu_CurrentOt + 3, tile);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_80180054);

void func_acropolis_plaza_80180270(Task* arg0)
{
    Display_SpawnWithOt(&D_acropolis_plaza_80183824, 0xA, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_801802C0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_801811D0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_80182054);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8018251C);
