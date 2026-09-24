#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, `(msgId, handler)` pairs ending at 0x7FFFFFFF,
/// which the entry task installs as its own `Task::msgTable`.
extern GpMsgEntry D_dryfield_night_g_r_kitchen_8017E254[];

/// State 0 of the room entry task: installs the room's message table,
/// registers the task in game pointer slot 7 and advances to the idle state.
void func_dryfield_night_g_r_kitchen_8017D958(Task* task)
{
    task->msgTable = D_dryfield_night_g_r_kitchen_8017E254;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
