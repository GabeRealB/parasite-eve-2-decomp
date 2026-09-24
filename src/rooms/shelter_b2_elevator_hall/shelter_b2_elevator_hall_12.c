#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b2_elevator_hall.h"

void func_shelter_b2_elevator_hall_8017DD08(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_elevator_hall_8017D5F0;
    sp.funcs[task->state](task);
}
