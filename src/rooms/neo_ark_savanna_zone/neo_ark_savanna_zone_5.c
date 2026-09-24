#include "common.h"

#include "main/task.h"

/// State table of the room setup task, indexed by `Task::state`.
extern const TaskFuncTable3 D_neo_ark_savanna_zone_8017D5D8;

/// The room setup task: runs the state handler its state selects, through a
/// copy of the state table on the stack.
void func_neo_ark_savanna_zone_8017D954(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_savanna_zone_8017D5D8;
    sp.funcs[task->state](task);
}
