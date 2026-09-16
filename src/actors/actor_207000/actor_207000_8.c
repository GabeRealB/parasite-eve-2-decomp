#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's four main-body handlers, dispatched through by state.
extern GpEnemyTaskFuncTable4 D_actor_207000_80149E5C;

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_8", func_actor_207000_8014F8D8);

void func_actor_207000_8014FC8C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = D_actor_207000_80149E5C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
