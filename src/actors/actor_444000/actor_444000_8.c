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

/// A second three-entry handler table, dispatched only while the global game
/// state is not 2.
extern GpEnemyTaskFuncTable3 D_actor_444000_80131E9C;

/// The grab enemy's five handler states, dispatched only while the global game
/// state is neither 1 nor 2.
extern GpEnemyTaskFuncTable5 D_actor_444000_80131EA8;

/// A four-entry handler table, dispatched only while the global game state is
/// neither 1 nor 2.
extern GpEnemyTaskFuncTable4 D_actor_444000_80131F0C;

/// A five-entry handler table, dispatched only while the global game state is
/// neither 1 nor 2.
extern GpEnemyTaskFuncTable5 D_actor_444000_80131F1C;

/// The spinner enemy's four handler states, dispatched while the global game
/// state is neither 1 nor 2.
extern GpEnemyTaskFuncTable4 D_actor_444000_80131F30;

extern u8 D_801153F4;

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

/// Same dispatch as `func_actor_444000_80143888` through the second handler
/// table, skipped while the global game state is 2.
void func_actor_444000_801438E4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_444000_80131E9C;
    switch (D_801153F4) {
        default:
        case 0:
        case 1:
            sp.funcs[arg0->state](arg0->spawnArg2, arg0);
            break;
        case 2:
            break;
    }
}

/// Dispatcher of the grab enemy (`D_actor_444000_80131EA8`): park the model
/// object while the global game state is 1 or 2, otherwise note in the work
/// block whether the state changed since the last step and run the handler for
/// it. The same shape as `func_actor_444000_80143A6C`, except that it guards
/// the bookkeeping on the state being non-zero rather than on the work block
/// existing, and clears the model object's flag word rather than leaving it 2.
void func_actor_444000_80143960(Actor444000Grab* arg0)
{
    GpEnemyTaskFuncTable5 sp;
    Actor444000GrabWork*  work;

    sp   = D_actor_444000_80131EA8;
    work = arg0->field_1C;

    switch (D_801153F4) {
        case 0:
            arg0->extra->flags = 0;
            break;
        case 1:
            arg0->extra->flags = 0;
            return;
        case 2:
            arg0->extra->flags = 0x80;
            return;
    }

    if (arg0->state != 0) {
        if (work->field_1B4 != arg0->state) {
            work->field_1A8 = 1;
        } else {
            work->field_1A8 = 0;
        }
        work->field_1B4 = arg0->state;
    }
    sp.funcs[arg0->state](arg0->spawnArg2, (Task*)arg0);
}

/// Dispatcher of the `D_actor_444000_80131F0C` enemy: park the model object
/// while the global game state is 1 or 2, otherwise note in the work block
/// whether the state changed since the last step and run the handler for it.
/// The same shape as `func_actor_444000_80143C64`, over the other work block
/// and with the model object's flag word left at 2 rather than cleared.
void func_actor_444000_80143A6C(Actor444000F0C* arg0)
{
    GpEnemyTaskFuncTable4 sp;
    Actor444000F0CWork*   work;

    sp = D_actor_444000_80131F0C;

    switch (D_801153F4) {
        case 0:
            arg0->extra->flags = 2;
            break;
        case 1:
            arg0->extra->flags = 2;
            return;
        case 2:
            arg0->extra->flags = 0x80;
            return;
    }

    if (arg0->field_1C != NULL) {
        work = arg0->field_1C;
        if (work->field_1B4 != arg0->state) {
            work->field_1A8 = 1;
        } else {
            work->field_1A8 = 0;
        }
        work->field_1B4 = arg0->state;
    }
    sp.funcs[arg0->state](arg0->spawnArg2, (Task*)arg0);
}

/// Same dispatch again through a five-entry handler table, skipped while the
/// global game state is 1 or 2.
void func_actor_444000_80143B74(Task* arg0)
{
    GpEnemyTaskFuncTable5 sp;

    sp = D_actor_444000_80131F1C;
    switch (D_801153F4) {
        case 0:
        default:
            sp.funcs[arg0->state](arg0->spawnArg2, arg0);
            break;
        case 1:
        case 2:
            break;
    }
}

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

    ((TmdObject*)arg1->extra)->flags = 0;
}

/// Dispatcher of the spinner enemy `D_actor_444000_80131F30` drives: park the
/// model object while the global game state is 1 or 2, otherwise note in the
/// work block whether the state changed since the last step and run the
/// handler for it.
void func_actor_444000_80143C64(Actor444000Spinner* arg0)
{
    GpEnemyTaskFuncTable4   sp;
    Actor444000SpinnerWork* work;

    sp = D_actor_444000_80131F30;

    switch (D_801153F4) {
        case 0:
            arg0->extra->flags = 0;
            break;
        case 1:
            arg0->extra->flags = 0;
            return;
        case 2:
            arg0->extra->flags = 0x80;
            return;
    }

    if (arg0->field_1C != NULL) {
        work = arg0->field_1C;
        if (work->field_94 != arg0->state) {
            work->field_90 = 1;
        } else {
            work->field_90 = 0;
        }
        work->field_94 = arg0->state;
    }
    sp.funcs[arg0->state](arg0->spawnArg2, (Task*)arg0);
}

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

    ((TmdObject*)arg0->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)arg0->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] = placement->pos.vz;
    if ((u32)(work->field_0 - 0x12) >= 2U) {
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords->coord, placement->rot.vx, 1);
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, placement->rot.vy, 0);
        Gfx_RotMatrixZ(&((TmdObject*)arg0->extra)->coords->coord, placement->rot.vz, 0);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
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
            if (work->field_EE8[0] != NULL && work->field_EE8[0]->field_40 <= 0) {
                work->field_EE8[0] = NULL;
            }
            if (work->field_EE8[1] != NULL && work->field_EE8[1]->field_40 <= 0) {
                work->field_EE8[1] = NULL;
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

/// Reset handler: when the work block is asking for a reset, stop the enemy's
/// own model drawing and push that same flag word onto each of the seven
/// escorts' models, then clear the two counters at 0xEF4. Otherwise just run
/// the ordinary re-arm in `func_actor_444000_8013441C`.
void func_actor_444000_80143F4C(Actor444000* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    s16              i;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        escorts                          = arg0->field_1C;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        work->field_EF4 = 0;
        work->field_EF6 = 0;
    } else {
        func_actor_444000_8013441C(arg0);
    }
}
