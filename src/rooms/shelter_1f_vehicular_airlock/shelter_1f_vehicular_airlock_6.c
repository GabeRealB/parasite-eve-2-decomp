#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, which its cap scripts index.
extern GpMsgEntry D_shelter_1f_vehicular_airlock_80182034[];

/// State 0 of the room's message task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7 and advances to
/// state 1.
void func_shelter_1f_vehicular_airlock_8017D9FC(Task* task)
{
    task->msgTable = D_shelter_1f_vehicular_airlock_80182034;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
