#include "common.h"

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern Task* D_actor_341300_80165A2C;

extern TaskDesc D_actor_341300_80165A68;

extern TaskDesc D_actor_341300_80165208;

extern Task* D_actor_341300_80165AA4;

extern s8 D_8007272D;

void func_actor_341300_8016398C(s32 arg0);

void func_actor_341300_801639CC(s32 arg0);

void func_actor_341300_8016241C(void)
{
    D_actor_341300_80165AA4 = Task_SpawnFromTable(&D_actor_341300_80165208, 0, 0, 0);
}

void func_actor_341300_80162450(void)
{
    if (D_actor_341300_80165AA4 != NULL) {
        D_actor_341300_80165AA4->state         = -1;
        D_actor_341300_80165AA4->killCountdown = 0;
        D_actor_341300_80165AA4                = NULL;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_80162478);

void func_actor_341300_80162530(void)
{
    D_actor_341300_80165AA4 = Task_SpawnFromTable(&D_actor_341300_80165208, 1, 0, 0);
}

void func_actor_341300_80162564(s16 arg0)
{
    func_actor_341300_8016398C(arg0);
}

void func_actor_341300_80162588(s16 arg0)
{
    func_actor_341300_801639CC(arg0);
}

/// Offset vector the four `Gp_SpawnEff` calls below spawn on. splat migrates
/// it into `func_actor_341300_801625AC`'s own `.s`, so there is no standalone
/// rodata file to `INCLUDE_RODATA`; defining it here emits it where the
/// function sits, ahead of the `jtbl_actor_341300_80161E6C` include below.
const SVECTOR D_actor_341300_80161E64 = { 100, -200, -100, 0 };

void func_actor_341300_801625AC(void)
{
    SVECTOR        vec   = D_actor_341300_80161E64;
    GsCOORDINATE2* coord = &((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8[2];

    Gp_SpawnEff(0x60055, coord, 0x10013300, &vec);
    Gp_SpawnEff(0x60055, coord, 0x10112280, &vec);
    Gp_SpawnEff(0x60055, coord, 0x10112280, &vec);
    Gp_SpawnEff(0x60055, coord, 0x10112280, &vec);
}

void func_actor_341300_80162680(s8 arg0)
{
    D_8007272D = arg0;
}

void func_actor_341300_8016268C(void)
{
    D_actor_341300_80165AA4 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_80162698);

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_80162878);

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_80163028);

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_801631D4);

void func_actor_341300_8016398C(s32 arg0)
{
    if ((arg0 << 0x10) == 0) {
        D_actor_341300_80165A2C = Task_SpawnFromTable(&D_actor_341300_80165A68, 0, 0, 0);
    }
}

void func_actor_341300_801639CC(s32 arg0)
{
    if (((arg0 << 0x10) == 0) && (D_actor_341300_80165A2C != NULL)) {
        Task_Kill(D_actor_341300_80165A2C);
        D_actor_341300_80165A2C = NULL;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_80163A10);
