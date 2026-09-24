#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, published in `Task::msgTable` by the entry task
/// (ids 0x13EE-0x13F2).
extern GpMsgEntry D_dryfield_parking_lot_8017DC04[];

/// Room entry task state 0: parks the room's message table in `Task::msgTable`,
/// publishes the task in pointer slot 7 and advances the state.
void func_dryfield_parking_lot_8017DB08(Task* task)
{
    task->msgTable = D_dryfield_parking_lot_8017DC04;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
