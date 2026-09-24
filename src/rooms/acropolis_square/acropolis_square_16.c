#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, installed on the room entry task.
extern GpMsgEntry D_acropolis_square_801837C4[];

/// First state of the room entry task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances the state.
void func_acropolis_square_80182260(Task* task)
{
    task->msgTable = D_acropolis_square_801837C4;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
