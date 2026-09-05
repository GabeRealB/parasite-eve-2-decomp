#include "common.h"

#include "main/session.h"
#include "main/task.h"
#include "rooms/rooms_shared_8017db84.h"

void RoomsShared8017db84(Task* task)
{
    task->field_24 = RoomsShared8017db84Msgs;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
