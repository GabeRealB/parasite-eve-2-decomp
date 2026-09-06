#include "common.h"

#include "main/task.h"
#include "rooms/rooms_shared_80180294.h"

void RoomsShared80180294(Task* task)
{
    TaskFuncTable16 sp;

    sp = RoomsShared80180294States;
    sp.funcs[task->state](task);
}
