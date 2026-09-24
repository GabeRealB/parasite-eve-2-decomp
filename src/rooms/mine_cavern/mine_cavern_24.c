#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "rooms/mine_cavern.h"

/// Runs the current state handler of one of the room's enemies from its
/// three-entry table - setup (`func_mine_cavern_801836D0`), per-frame tick
/// (`func_mine_cavern_80183AD4`) or teardown (`Gp_DestroyEnemy`) - copying the
/// table onto the stack before the call.
void func_mine_cavern_80183C10(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_mine_cavern_8017D80C;
    sp.funcs[task->state](task->spawnArg2, task);
}
