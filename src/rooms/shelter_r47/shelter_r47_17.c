#include "common.h"

#include "main/task.h"
#include "rooms/shelter_r47.h"

extern TaskFuncTable14 D_shelter_r47_8017D6C8;

void func_shelter_r47_80182B18(Task* task)
{
    TaskFuncTable14 states;

    states = D_shelter_r47_8017D6C8;
    states.funcs[task->state](task);
}
