#include "common.h"

#include "main/task.h"
#include "rooms/acropolis_bridge.h"

/// Two-state dispatcher of the room's prompt script task: state 0 resets the
/// action prompts, state 1 moves and draws their cursors.
void func_acropolis_bridge_8017F280(Task* task)
{
    TaskFunc states[2] = { func_acropolis_bridge_8017F808, func_acropolis_bridge_8017ED38 };

    states[task->state](task);
}
