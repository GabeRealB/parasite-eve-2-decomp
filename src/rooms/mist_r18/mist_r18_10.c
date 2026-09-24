#include "common.h"

#include "main/task.h"
#include "rooms/mist_r18.h"

/// Per-frame entry point of a four-state task: run the handler its state
/// selects from `D_mist_r18_8017D5DC`, copied onto the stack each frame.
void func_mist_r18_8017E854(Task* task)
{
    TaskFuncTable4 states;

    states = D_mist_r18_8017D5DC;
    states.funcs[task->state](task);
}
