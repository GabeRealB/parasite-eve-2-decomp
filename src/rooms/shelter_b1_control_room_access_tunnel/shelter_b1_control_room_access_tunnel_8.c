#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, installed on its task by state 0.
extern GpMsgEntry D_shelter_b1_control_room_access_tunnel_80181E74[];

/// State 0 of the room's task: installs the room's message table, publishes
/// the task in pointer slot 7 and advances to state 1.
void func_shelter_b1_control_room_access_tunnel_8017D640(Task* task)
{
    task->msgTable = D_shelter_b1_control_room_access_tunnel_80181E74;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
