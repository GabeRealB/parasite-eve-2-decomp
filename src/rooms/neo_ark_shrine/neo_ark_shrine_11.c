#include "common.h"

#include "main/task.h"
#include "rooms/neo_ark_shrine.h"

/// Task callback of the action-prompt cursor: state 0 resets both prompt slots,
/// state 1 moves the cursor from the pad every frame after.
void func_neo_ark_shrine_8017EA70(Task* task)
{
    TaskFunc states[2] = { func_neo_ark_shrine_8017F80C, func_neo_ark_shrine_8017E528 };

    states[task->state](task);
}
