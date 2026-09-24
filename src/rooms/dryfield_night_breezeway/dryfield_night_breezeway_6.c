#include "common.h"

#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_night_breezeway.h"

/// State 0 of the room's event task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7 and advances to the
/// next state.
void func_dryfield_night_breezeway_8017D634(Task* task)
{
    task->msgTable = D_dryfield_night_breezeway_8017E67C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
