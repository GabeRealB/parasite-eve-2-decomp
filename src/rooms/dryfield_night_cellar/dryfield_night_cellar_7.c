#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, installed on the room entry task.
extern GpMsgEntry D_dryfield_night_cellar_8017DAA8[];

/// The room entry task's first state: installs the room's message table, hands
/// the task to pointer slot 7 and moves on to the next state.
void func_dryfield_night_cellar_8017D6FC(Task* task)
{
    task->msgTable = D_dryfield_night_cellar_8017DAA8;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
