#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

void func_actor_311900_8016228C(GpEnemy* enemy, Task* task);
void func_actor_311900_801623B0(GpEnemy* enemy, Task* task);

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

INCLUDE_ASM("actors/nonmatchings/actor_311900/actor_311900", func_actor_311900_801623B0);
