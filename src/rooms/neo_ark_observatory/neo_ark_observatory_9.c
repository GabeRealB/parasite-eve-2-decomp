#include "common.h"

#include "main/task.h"

/// State handlers of the room entry task, indexed by `Task::state`: the set-up
/// tick, the arrival-line tick, and `taskKill`.
extern const TaskFuncTable3 D_neo_ark_observatory_8017D698;

/// Room entry task: dispatches through a stack copy of its state table.
void func_neo_ark_observatory_8017FDDC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_observatory_8017D698;
    sp.funcs[task->state](task);
}
