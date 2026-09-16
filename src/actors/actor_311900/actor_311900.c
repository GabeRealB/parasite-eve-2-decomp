#include "common.h"

#include "actors/actor_311900.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "main/tmd.h"

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

INCLUDE_ASM("actors/nonmatchings/actor_311900/actor_311900", func_actor_311900_8016228C);

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
