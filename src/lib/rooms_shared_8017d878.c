#include "common.h"

#include "main/task.h"
#include "rooms/rooms_shared_8017d878.h"

void RoomsShared8017d878(Task* task)
{
    TaskFuncTable3 sp;

    sp = RoomsShared8017d878Table;
    sp.funcs[task->state](task);
}
