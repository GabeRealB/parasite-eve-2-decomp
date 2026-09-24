#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// The walkway's message table, installed as the room task's `msgTable`.
extern GpMsgEntry D_shelter_b2_south_maintenance_walkway_80182550[];

/// The room task's setup state: installs the walkway's message table,
/// registers the task in pointer slot 7 and advances to the idle state.
void func_shelter_b2_south_maintenance_walkway_8017DC20(Task* task)
{
    task->msgTable = D_shelter_b2_south_maintenance_walkway_80182550;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
