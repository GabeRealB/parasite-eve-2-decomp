#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_acropolis_hallway_8017E238[];

/// State 0 of the room task: installs the room's message table, publishes the
/// task in pointer slot 7 and advances to the next state.
void func_acropolis_hallway_8017D784(Task* task)
{
    task->msgTable = D_acropolis_hallway_8017E238;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
