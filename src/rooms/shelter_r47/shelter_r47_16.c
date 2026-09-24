#include "common.h"

#include "main/task.h"
#include "rooms/shelter_r47.h"

extern const TaskFuncTable3 D_shelter_r47_8017D6A4;

void func_shelter_r47_801807B4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_r47_8017D6A4;
    sp.funcs[task->state](task);
}
