#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_1f_bulwark_8018032C[];

/// First state of the room's controller task: installs the room's message
/// table, registers the task in pointer slot 7 and advances to the idle state.
void func_shelter_1f_bulwark_8017DBD4(Task* task)
{
    task->msgTable = D_shelter_1f_bulwark_8018032C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
