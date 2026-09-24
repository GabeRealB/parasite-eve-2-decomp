#include "common.h"

#include "main/task.h"
#include "rooms/acropolis_bridge.h"

/// Fourteen-state dispatcher of the room's cutscene task: copies the handler
/// table onto the stack and calls the entry named by `Task::state`.
void func_acropolis_bridge_8017D8D0(Task* task)
{
    TaskFuncTable14 states;

    states = D_acropolis_bridge_8017D5DC;
    states.funcs[task->state](task);
}
