#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, handed to its event task in state 0.
extern GpMsgEntry D_shelter_1f_airlock_8017E494[];

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances to state 1.
void func_shelter_1f_airlock_8017D62C(Task* task)
{
    task->msgTable = D_shelter_1f_airlock_8017E494;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
