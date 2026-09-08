#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/task.h"

#include <psyq/libgte.h>

extern s32 D_80115728;
extern s32 Gp_LcgState;

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_102400_80131E24;

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80131E84);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_801323DC);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_8013277C);

INCLUDE_RODATA("actors/nonmatchings/actor_102400/actor_102400", D_actor_102400_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_102400/actor_102400", D_actor_102400_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80132A28);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80133240);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_801333B0);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80133830);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_801339B0);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80133D94);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80133EAC);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80134084);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_801341D4);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80134318);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_801345B0);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80134910);

void func_actor_102400_80134AC4(GsCOORDINATE2* arg0, s32 arg1)
{
    SVECTOR sp10;
    SVECTOR sp18;
    s32     ang;

    if (Gp_State1C->field_4 == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            ang         = ((u32)Gp_LcgState >> 16) & 0xF80;
            memset(&sp18, 0, sizeof(sp18));
            sp18.vx = (u32)(rcos(ang) * 5) >> 5;
            sp18.vz = (u32)(rsin(ang) * 5) >> 5;
            sp10    = sp18;
            Gp_SpawnEff(D_80115728, arg0, arg1 | 0x20100200, &sp10);
        }
    }
}

void func_actor_102400_80134BD0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_102400_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_RODATA("actors/nonmatchings/actor_102400/actor_102400", D_actor_102400_80131E5C);
