#include "common.h"

#include "actors/actor_107000.h"
#include "main/task.h"

/// Task handler of the specimen's contact effect: runs the entry of
/// `Actor07000_D000E0` for the task's state. The table is copied onto the
/// stack before the call.
void Actor07000_Fn06338(Task* task)
{
    TaskFuncTable3 sp;

    sp = Actor07000_D000E0;
    sp.funcs[task->state](task);
}
