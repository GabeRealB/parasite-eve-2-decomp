#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

/// Third entry of the room's task table: waits for the CAP command to finish,
/// then, unless it ended on event key 0xC, sets the game-flag nibble named by
/// the task's spawn argument to 2, and ends the task.
void func_neo_ark_eve_access_tunnel_8017DED0(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() != 0xC) {
            GameFlag_SetNibble(arg0->spawnArg1, 2);
        }
        taskKill(arg0);
    }
}
