#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table.
extern GpMsgEntry D_dryfield_motel_lobby_8017F810[];

/// First state of the room task: installs the room's message table, stores the
/// task in session pointer slot 7 and advances.
void func_dryfield_motel_lobby_8017F44C(Task* task)
{
    task->msgTable = D_dryfield_motel_lobby_8017F810;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
