#include "common.h"

#include "main/task.h"

#include "rooms/rooms_shared_8017d8d0.h"

void RoomsShared8017d8d0(Task* task)
{
    TaskFuncTable14 states;

    states = RoomsShared8017d8d0States;
    states.funcs[task->state](task);
}
