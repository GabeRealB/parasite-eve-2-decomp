#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, which its cap scripts index.
extern GpMsgEntry D_shelter_b1_sleeping_quarters_80180518[];

/// First state of the room task: installs the room's message table, publishes
/// the task in pointer slot 7 and advances to the next state.
void func_shelter_b1_sleeping_quarters_8017D83C(Task* task)
{
    task->msgTable = D_shelter_b1_sleeping_quarters_80180518;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
