#include "common.h"

#include "actors/actors_shared_801385d4.h"

/// Runs the enemy's current state handler. The table is copied onto the stack
/// before the call, as everywhere else this dispatch shape appears.
void ActorsShared801385d4(Task* task)
{
    GpEnemyTaskFuncTable5 sp;

    sp = ActorsShared801385d4Table;
    sp.funcs[task->state](task->spawnArg2, task);
}
