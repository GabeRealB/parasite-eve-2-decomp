#include "common.h"

#include "main/task.h"

/// The room task's three states.
extern const TaskFuncTable3 D_dryfield_motel_lobby_8017D644;

/// The room task's update: copies the state table and runs the current state.
void func_dryfield_motel_lobby_8017F498(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_lobby_8017D644;
    sp.funcs[task->state](task);
}
