#include "common.h"

#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_g_r_kitchen.h"

/// Entry state of the room task: installs the room's message table, registers
/// the task in pointer slot 7 and advances to the idle state.
void func_dryfield_g_r_kitchen_8017D958(Task* task)
{
    task->msgTable = D_dryfield_g_r_kitchen_8017EBC0;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
