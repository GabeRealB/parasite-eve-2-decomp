#include "common.h"

#include "main/task.h"

/// States of the room's message task: install the message table and spawn the
/// room's tasks, run the per-frame check, die.
extern const TaskFuncTable3 D_acropolis_fire_escape_8017D6A4;

/// Runs the room's message task: copies the state table onto the stack and
/// calls the entry for the task's current state.
void func_acropolis_fire_escape_8017FF24(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_fire_escape_8017D6A4;
    sp.funcs[task->state](task);
}
