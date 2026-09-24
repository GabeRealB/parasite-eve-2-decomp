#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b1_underground_parking.h"

/// Two-state dispatcher of the room's action-prompt task: builds the handler
/// pair on the stack and calls the entry `Task::state` names.
void func_shelter_b1_underground_parking_80184234(Task* task)
{
    TaskFunc states[2] = { func_shelter_b1_underground_parking_801848BC, func_shelter_b1_underground_parking_80183CEC };

    states[task->state](task);
}
