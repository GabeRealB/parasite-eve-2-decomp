#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b4_upper_sewer.h"

void func_shelter_b4_upper_sewer_8017DC30(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b4_upper_sewer_8017D5C4;
    sp.funcs[task->state](task);
}
