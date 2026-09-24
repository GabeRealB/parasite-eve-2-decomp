#include "common.h"

#include "main/task.h"

/// State handlers of the room's entry task, indexed by its state through
/// `func_neo_ark_pyramid_8017DB98`: set-up, per-frame draw, then kill.
extern const TaskFuncTable3 D_neo_ark_pyramid_8017D5C4;

/// Task tick that dispatches on the task's state through the three-entry
/// handler table `D_neo_ark_pyramid_8017D5C4`, copied to the stack first.
void func_neo_ark_pyramid_8017DB98(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_pyramid_8017D5C4;
    sp.funcs[task->state](task);
}
