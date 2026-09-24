#include "common.h"

#include "main/task.h"

/// State handlers of the room's message task, run by
/// `func_mine_refuge_8017FFBC`.
extern const TaskFuncTable3 D_mine_refuge_8017D6A4;

/// Runs the handler for the task's current state, from a local copy of
/// `D_mine_refuge_8017D6A4`.
void func_mine_refuge_8017FFBC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_refuge_8017D6A4;
    sp.funcs[task->state](task);
}
