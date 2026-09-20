#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_204600_80149E5C;

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_7", func_actor_204600_8014D28C);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_7", func_actor_204600_8014D778);

void func_actor_204600_8014D9A0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_204600_80149E5C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_7", func_actor_204600_8014D9FC);
