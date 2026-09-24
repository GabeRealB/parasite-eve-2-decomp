#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, which the cap scripts index.
extern GpMsgEntry D_shelter_b2_operating_room_8018091C[];

/// Installs `D_shelter_b2_operating_room_8018091C` as the task's message
/// table, registers the task in pointer slot 7 and steps it on one state.
void func_shelter_b2_operating_room_8017DD14(Task* task)
{
    task->msgTable = D_shelter_b2_operating_room_8018091C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
