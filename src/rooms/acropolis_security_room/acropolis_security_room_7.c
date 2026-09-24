#include "common.h"

#include "main/task.h"

/// State table of the room's message task: register the room's message table,
/// idle, then kill the task.
extern const TaskFuncTable3 D_acropolis_security_room_8017D5C4;

/// Runs the room's message task's current state through a stack copy of its
/// three-entry state table.
void func_acropolis_security_room_8017D984(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_security_room_8017D5C4;
    sp.funcs[task->state](task);
}
