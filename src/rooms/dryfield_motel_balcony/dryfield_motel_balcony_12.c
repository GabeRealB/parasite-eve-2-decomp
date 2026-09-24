#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_motel_balcony.h"

/// The room's message table.
extern GpMsgEntry D_dryfield_motel_balcony_8018227C[];

/// Room task state 0: installs the room's message table, registers the task
/// in pointer slot 7 and advances to the next state.
void func_dryfield_motel_balcony_8017DB84(Task* task)
{
    task->msgTable = D_dryfield_motel_balcony_8018227C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
