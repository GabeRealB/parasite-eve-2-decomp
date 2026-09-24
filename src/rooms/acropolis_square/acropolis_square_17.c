#include "common.h"

#include "main/task.h"

/// State handlers of the room entry task `func_acropolis_square_80182308`,
/// indexed by `Task::state`: the set-up tick, the idle tick, and `taskKill`.
extern const TaskFuncTable3 D_acropolis_square_8017D6B4;

/// Room entry task: runs the state handler `D_acropolis_square_8017D6B4`
/// names for `Task::state`, through a copy of the table taken onto the stack.
void func_acropolis_square_80182308(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_square_8017D6B4;
    sp.funcs[task->state](task);
}
