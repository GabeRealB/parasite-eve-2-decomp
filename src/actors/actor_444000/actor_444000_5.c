#include "common.h"

#include "actors/actor_444000.h"
#include "actors/actors_shared_80135990.h"

#include "gameplay/1BC.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_444000_80131E90;

extern s16 D_actor_444000_80144A68;
extern s16 D_actor_444000_80144A72;

void func_actor_444000_8014382C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_444000_80131E90;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_444000_80143888(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_444000_80131E90;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_801438E4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80143960);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80143A6C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80143B74);

/// Teardown state of the enemy's handler table: hand the enemy back to
/// `Gp_DestroyEnemy` once `D_actor_444000_80144A68` is set, otherwise step the
/// task on when `D_actor_444000_80144A72` says to and clear the model object's
/// flag word so it stops drawing.
void func_actor_444000_80143BFC(GpEnemy* arg0, Task* arg1)
{
    if (D_actor_444000_80144A68 == 1) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }

    if (D_actor_444000_80144A72 == 1) {
        arg1->state++;
    }

    ((TmdObject*)arg1->extra)->field_C = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80143C64);

s32 func_actor_444000_80143D68(Actor444000* arg0)
{
    return arg0->field_20->field_40 > 0;
}

/// Seeds the enemy's `TmdObject` coordinate frame from `placement`: the three
/// longs become the translation, the Euler angles are applied X/Y/Z unless the
/// work block's state index is 0x12 or 0x13, and the coordinate is marked
/// dirty. Same body as `ActorsShared80135990` with that state gate added.
s32 func_actor_444000_80143D7C(Actor444000* arg0, s32 arg1, ActorShared80135990Placement* placement)
{
    Actor444000Work* work = arg0->field_1C;

    ((TmdObject*)arg0->extra)->field_8->coord.t[0] = placement->pos.vx;
    ((TmdObject*)arg0->extra)->field_8->coord.t[1] = placement->pos.vy;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] = placement->pos.vz;
    if ((u32)(work->field_0 - 0x12) >= 2U) {
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->field_8->coord, placement->rot.vx, 1);
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->field_8->coord, placement->rot.vy, 0);
        Gfx_RotMatrixZ(&((TmdObject*)arg0->extra)->field_8->coord, placement->rot.vz, 0);
    }
    ((TmdObject*)arg0->extra)->field_8->flg = 0;
    return 1;
}

/// Per-frame upkeep for the enemy, dispatched by `arg2`: state 0 bumps the
/// heal counter, files a negative "damage" with `func_800DA6E8` so the HUD
/// shows it as a heal, and tops the enemy's HP back up by 0x64; state 1 ticks
/// the countdown at 0xF1C down, re-arms `field_F16` and drops either tracked
/// enemy whose HP has run out.
s32 func_actor_444000_80143E68(Actor444000* arg0, s32 arg1, s32 arg2)
{
    Actor444000Work* work = arg0->field_1C;
    GpEnemy*         obj  = arg0->field_20;

    switch (arg2) {
        case 0:
            work->field_F1A++;
            func_800DA6E8(&obj->node, -0x64, 0);
            if (obj->field_40 > 0) {
                obj->field_40 += 0x64;
            }
            break;
        case 1:
            if (work->field_F1C > 0) {
                work->field_F1C--;
            }
            work->field_F16 = 2;
            if (work->field_EE8 != NULL && work->field_EE8->field_40 <= 0) {
                work->field_EE8 = NULL;
            }
            if (work->field_EEC != NULL && work->field_EEC->field_40 <= 0) {
                work->field_EEC = NULL;
            }
            break;
    }
    return 1;
}

s32 func_actor_444000_80143F38(Actor444000* arg0)
{
    arg0->field_1C->field_0 = 0;
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80143F4C);
