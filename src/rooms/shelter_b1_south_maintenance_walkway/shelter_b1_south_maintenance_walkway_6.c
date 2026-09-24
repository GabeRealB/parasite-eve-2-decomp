#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_b1_south_maintenance_walkway_80182308[];

/// Installs the room's message table on `task`, publishes the task in pointer
/// slot 7 and steps it to its next state.
void func_shelter_b1_south_maintenance_walkway_8017D9E8(Task* task)
{
    task->msgTable = D_shelter_b1_south_maintenance_walkway_80182308;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
