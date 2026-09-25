#include "common.h"

#include "actors/actor_210700.h"

/// The actor's task entry: runs the handler for the task's current state out
/// of `D_actor_210700_80149E24` - spawn, per-frame tick or teardown - copying
/// the table onto the stack before the call.
void func_actor_210700_80149F38(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_210700_80149E24;
    sp.funcs[task->state](task);
}
