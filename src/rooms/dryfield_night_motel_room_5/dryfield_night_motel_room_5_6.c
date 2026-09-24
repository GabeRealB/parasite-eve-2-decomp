#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, published at `Task::msgTable` by the room task.
extern GpMsgEntry D_dryfield_night_motel_room_5_8017DA30[];

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and advances to the next state.
void func_dryfield_night_motel_room_5_8017D684(Task* task)
{
    task->msgTable = D_dryfield_night_motel_room_5_8017DA30;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
