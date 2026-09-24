#include "common.h"

#include "main/task.h"

/// State handlers of the room task `func_neo_ark_r31_8017D990`, indexed by
/// `Task::state`.
extern const TaskFuncTable3 D_neo_ark_r31_8017D5C4;

/// Room task: dispatches through a stack copy of its state table.
void func_neo_ark_r31_8017D990(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_r31_8017D5C4;
    sp.funcs[task->state](task);
}
