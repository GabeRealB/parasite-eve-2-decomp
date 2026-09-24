#include "common.h"

#include "main/session.h"
#include "main/task.h"
#include "rooms/shelter_b2_elevator_hall.h"

void func_shelter_b2_elevator_hall_8017DCBC(Task* task)
{
    task->msgTable = D_shelter_b2_elevator_hall_801837A8;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
