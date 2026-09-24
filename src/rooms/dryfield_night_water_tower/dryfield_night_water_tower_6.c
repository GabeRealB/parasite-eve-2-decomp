#include "common.h"

#include "main/session.h"
#include "main/task.h"

#include "rooms/dryfield_night_water_tower.h"

/// State 0 of the room entry task: installs the room's message table,
/// registers the task in game pointer slot 7 and advances to the idle state.
void func_dryfield_night_water_tower_8017DADC(Task* task)
{
    task->msgTable = D_dryfield_night_water_tower_8017E6EC;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
