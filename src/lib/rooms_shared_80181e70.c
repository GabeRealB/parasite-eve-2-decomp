#include "common.h"

#include "main/task.h"

#include "rooms/rooms_shared_80181e70.h"

void RoomsShared80181e70(Task* task)
{
    TaskFuncTable4 states;

    states = RoomsShared80181e70Table;
    states.funcs[task->state](task);
}
