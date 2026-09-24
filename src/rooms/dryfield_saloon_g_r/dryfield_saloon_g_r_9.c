#include "common.h"

#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_saloon_g_r.h"

/// Entry state of the room task: installs the room's message table, registers
/// the task in pointer slot 7 and advances to the idle state.
void func_dryfield_saloon_g_r_8017D9CC(Task* task)
{
    task->msgTable = D_dryfield_saloon_g_r_8017ECBC;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}
