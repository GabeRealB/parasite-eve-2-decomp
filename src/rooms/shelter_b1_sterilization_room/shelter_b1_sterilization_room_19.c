#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b1_sterilization_room.h"

void func_shelter_b1_sterilization_room_801811E0(Task* task)
{
    TaskFuncTable4 states;

    states = D_shelter_b1_sterilization_room_8017D700;
    states.funcs[task->state](task);
}
