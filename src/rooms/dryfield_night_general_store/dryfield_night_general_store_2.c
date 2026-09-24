#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern TaskDesc D_dryfield_night_general_store_8017E798[];

/// Message handler for actions 0x18 and 9. Action 0x18 hands
/// `Gp_SpawnIfCapIdle` 0x18 when pointer slot 0xA holds a task and 0x19
/// otherwise; action 9 spawns the room's CAP-command task to run CAP command 9
/// and toggle flag nibble 0x53. Always returns 0.
s32 func_dryfield_night_general_store_8017DD88(s32 arg0, s32 arg1, s32 arg2)
{
    s32   arg;
    void* slot;

    if (arg2 == 0x18) {
        slot = gameGetPtrSlot(0xA);
        arg  = 0x19;
        if (slot != 0) {
            arg = 0x18;
        }
        Gp_SpawnIfCapIdle(arg, 0);
    }
    if (arg2 == 9) {
        Task_SpawnFromTable(D_dryfield_night_general_store_8017E798, 0, 0x53, 9);
    }
    return 0;
}
