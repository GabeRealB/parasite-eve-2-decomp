#include "common.h"

#include "actors/actors_shared_80135df4.h"

/// Runs the enemy's current state handler: spawn/setup, per-frame tick or
/// teardown. The table is copied onto the stack before the call, as everywhere
/// else this dispatch shape appears.
void ActorsShared80135df4(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = ActorsShared80135df4Table;
    sp.funcs[task->state](task->spawnArg2, task);
}
