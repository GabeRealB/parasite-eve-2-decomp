#include "common.h"

#include "actors/actors_shared_80138404.h"

/// Runs the actor's current state handler: spawn/setup, per-frame tick or
/// teardown. The table is copied onto the stack before the call, as everywhere
/// else this dispatch shape appears.
void ActorsShared80138404(Task* task)
{
    TaskFuncTable3 sp;

    sp = ActorsShared80138404Table;
    sp.funcs[task->state](task);
}
