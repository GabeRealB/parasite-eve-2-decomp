#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_dryfield_night_underpass_8017DCF0[];

/// First state of the room task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7, and advances.
void func_dryfield_night_underpass_8017D910(Task* task)
{
    task->msgTable = D_dryfield_night_underpass_8017DCF0;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
