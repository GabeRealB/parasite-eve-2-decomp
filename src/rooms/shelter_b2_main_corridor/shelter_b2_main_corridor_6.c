#include "common.h"

#include "main/task.h"

/// Waits for the capture command the message handler started to finish, then
/// sets the game-flag nibble named by the task's spawn argument to 2 unless
/// the capture ended on event key 0xC, and ends the task.
void func_shelter_b2_main_corridor_8017E210(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() != 0xC) {
            GameFlag_SetNibble(arg0->spawnArg1, 2);
        }
        taskKill(arg0);
    }
}
