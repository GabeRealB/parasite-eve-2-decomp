#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_dryfield_driveway_8017E754[];

/// State 0 of the room task: attach the room's message table, publish the task
/// in pointer slot 7 and advance to the next state.
void func_dryfield_driveway_8017DDC0(Task* task)
{
    task->msgTable = D_dryfield_driveway_8017E754;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
