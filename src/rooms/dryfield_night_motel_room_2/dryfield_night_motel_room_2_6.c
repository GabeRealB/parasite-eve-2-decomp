#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, which the event task installs in state 0.
extern GpMsgEntry D_dryfield_night_motel_room_2_8017DA1C[];

/// State 0 of the room's event task: installs the room's message table, puts
/// the task in pointer slot 7 and moves on to state 1.
void func_dryfield_night_motel_room_2_8017D670(Task* task)
{
    task->msgTable = D_dryfield_night_motel_room_2_8017DA1C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
