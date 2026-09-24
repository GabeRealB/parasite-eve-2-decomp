#include "common.h"

#include "main/task.h"

/// State handlers of the room's entry task, indexed by its state through
/// `func_neo_ark_island_8017EB10`: set-up, idle, then kill.
extern const TaskFuncTable3 D_neo_ark_island_8017D614;

/// Task tick that dispatches on the task's state through the three-entry
/// handler table `D_neo_ark_island_8017D614`, copied to the stack first.
void func_neo_ark_island_8017EB10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_island_8017D614;
    sp.funcs[task->state](task);
}
