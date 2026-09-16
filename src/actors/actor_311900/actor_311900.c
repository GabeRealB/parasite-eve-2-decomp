#include "common.h"

#include "actors/actor_311900.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// The animation data `func_800B3F84` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_311900_8016EBE8[];

void func_actor_311900_8016228C(GpEnemy* enemy, Task* task);
void func_actor_311900_801623B0(GpEnemy* enemy, Task* task);

void func_actor_311900_80161E3C(Task* task, s32 arg1, s32 arg2);
void func_actor_311900_80162100(Task* task);

INCLUDE_ASM("actors/nonmatchings/actor_311900/actor_311900", func_actor_311900_80161E3C);

INCLUDE_ASM("actors/nonmatchings/actor_311900/actor_311900", func_actor_311900_80162100);

INCLUDE_RODATA("actors/nonmatchings/actor_311900/actor_311900", D_actor_311900_80161E20);

/// The actor's three state handlers - spawn, per-frame tick and teardown -
/// dispatched through by state. splat migrates the table into the `.s` of the
/// function that reads it, so it is written out here to keep the block in the
/// unit's `.rodata` now that `func_actor_311900_8016222C` is decompiled.
const GpEnemyTaskFuncTable3 D_actor_311900_80161E24 = {
    func_actor_311900_8016228C,
    func_actor_311900_801623B0,
    Gp_DestroyEnemy,
};

/// Runs the actor's state handler that `Task::state` selects. Copies the
/// table onto the stack first, the same local jump table `Gp_EnemyDispatch`
/// builds for the shared `Gp_EnemyWaitFuncs`, so the call goes through the
/// stack copy rather than the overlay's own `.rodata`.
void func_actor_311900_8016222C(Task* task)
{
    GpEnemy*              enemy;
    GpEnemyTaskFuncTable3 sp;

    enemy = task->spawnArg2;
    sp    = D_actor_311900_80161E24;
    sp.funcs[task->state](enemy, task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_311900/actor_311900", ActorsShared80135df4Table);

/// The actor's first setup path, reached through `D_actor_311900_80161E24`. It
/// tears the enemy down instead while game flag 0xA's nibble 2 -- the bit
/// `func_actor_311900_801623B0` raises once the view reaches 0xA -- is already
/// up, or when the 0x4CC-byte work block cannot be allocated into
/// `Task::idMap` (that slot is not a `TaskIdMap` here).
///
/// Otherwise it splats the light / colour pair `func_actor_311900_8016278C`
/// writes onto the model root's `field_1C` / `field_20` slots, points
/// `GpEnemy::field_4` at the root coordinate's matrix, re-parents that root to
/// `Gfx_ViewCoord`, builds the animation context `func_800B3F84` over the
/// block's slot array and packed-pose run, seeds the tick's two work halfwords
/// 0x474 / 0x478 and zeroes the 0x4C4 / 0x4C6 pair it counts in, and publishes
/// the view-dependent light level exactly as the tick does.
void func_actor_311900_8016228C(GpEnemy* enemy, Task* task)
{
    Actor311900Work* work;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    if ((GameFlag_GetNibble(0xA) & 2) ||
        (work = Mem_Calloc(0x4CC, 0), task->idMap = (TaskIdMap*)work, work == NULL)) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    func_actor_311900_8016278C(task);
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    obj->field_C    = 0;
    func_800B3F84((GpAnimCtx*)work, D_actor_311900_8016EBE8, (GpAnimObj*)obj, work->anim.poses,
                  work->anim.slots);
    coord->sub      = &Gfx_ViewCoord;
    work->field_474 = 2;
    work->field_478 = 1;
    work->field_4C4 = 0;
    work->field_4C6 = 0;
    if ((Gp_GetViewIndex() & 0xFF) == 0xA) {
        obj->field_C = 0;
    } else {
        obj->field_C = 0x80;
    }
    func_actor_311900_80162100(task);
    task->state += 1;
}

/// The actor's per-frame tick. Publishes the view-dependent light level into
/// `TmdObject::field_C` (0 at view 0xA, 0x80 otherwise), and while the work
/// block's `field_4C6` latch is up, counts frames in `field_4C4` and nudges the
/// model along the coordinate part `func_actor_311900_80162658` walks. The
/// counter reaching 0x5A raises game flag 0x102 and advances the state.
void func_actor_311900_801623B0(GpEnemy* enemy, Task* task)
{
    Actor311900Work* work;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;

    obj   = task->extra;
    work  = (Actor311900Work*)task->idMap;
    coord = obj->field_8;
    func_actor_311900_80161E3C(task, 2, 0);
    if ((Gp_GetViewIndex() & 0xFF) == 0xA) {
        GameFlag_SetNibble(0xA, GameFlag_GetNibble(0xA) | 2);
        obj->field_C    = 0;
        work->field_4C6 = 1;
    } else {
        obj->field_C = 0x80;
    }
    if ((s16)work->field_4C6 == 1) {
        work->field_4C4++;
        func_actor_311900_80162658(coord, 0x24);
    }
    func_actor_311900_80162100(task);
    if ((s16)work->field_4C4 >= 0x5A) {
        GameFlag_SetNibble(0x102, 1);
        task->state++;
    }
}
