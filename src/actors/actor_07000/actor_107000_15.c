#include "common.h"

#include "actors/actor_107000.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Task handler of the caged specimen: runs the entry of `Actor07000_D00004`
/// for the task's state - spawn, per-frame update or teardown - with the
/// enemy and the task. The table is copied onto the stack before the call.
void Actor07000_Fn02548(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor07000_D00004;
    sp.funcs[task->state](task->spawnArg2, task);
}
