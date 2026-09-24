#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_neo_ark_garden_8017D614;

/// Tick of the room's entry task: copies the three-entry state table
/// `D_neo_ark_garden_8017D614` (set-up, idle, kill) to the stack and calls the
/// handler for the task's state.
void func_neo_ark_garden_8017EA44(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_garden_8017D614;
    sp.funcs[task->state](task);
}
