#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "rooms/rooms_shared_80183c10.h"

/// Runs the room enemy's current state handler: spawn/setup
/// (`func_acropolis_bridge_80185988`), per-frame tick
/// (`func_acropolis_bridge_80187850`) or teardown (`Gp_DestroyEnemy`). The
/// table is copied onto the stack before the call, as everywhere else this
/// dispatch shape appears.
void RoomsShared80183c10(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = RoomsShared80183c10Table;
    sp.funcs[task->state](task->spawnArg2, task);
}
