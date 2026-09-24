#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b1_sterilization_room.h"

void func_shelter_b1_sterilization_room_80180518(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_sterilization_room_8017D6A4;
    sp.funcs[task->state](task);
}
