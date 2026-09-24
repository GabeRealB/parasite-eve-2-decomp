#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_neo_ark_north_promenade_80181D68[];

/// State 0 of the room's message-driven task: parks the room's message table
/// in `Task::msgTable`, publishes the task in pointer slot 7 and advances to
/// state 1.
void func_neo_ark_north_promenade_8017D67C(Task* task)
{
    task->msgTable = D_neo_ark_north_promenade_80181D68;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
