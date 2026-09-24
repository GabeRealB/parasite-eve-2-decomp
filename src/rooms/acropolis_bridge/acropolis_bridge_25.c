#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "rooms/acropolis_bridge.h"

/// Runs the bridge enemy's current state handler - setup, per-frame tick or
/// teardown - copying the table onto the stack before the call.
void func_acropolis_bridge_80187D80(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_acropolis_bridge_8017D6E8;
    sp.funcs[task->state](task->spawnArg2, task);
}
