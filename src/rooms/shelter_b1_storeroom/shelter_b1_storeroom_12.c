#include "common.h"

#include "rooms/shelter_b1_storeroom.h"
#include "main/task.h"

void func_shelter_b1_storeroom_8017D794(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_storeroom_8017D5C4;
    sp.funcs[task->state](task);
}
