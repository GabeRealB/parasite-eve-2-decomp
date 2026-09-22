#include "common.h"

#include "main/task.h"

extern TaskDesc D_dryfield_underpass_8017E818;

/// Spawns the room's script task for the two start-up commands the room's cap
/// table issues: command 1 spawns script 0x51 with `spawnArg2` 1, command 2
/// spawns 0x52 with 2. Any other command spawns nothing.
s32 func_dryfield_underpass_8017D868(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 1:
            Task_SpawnFromTable(&D_dryfield_underpass_8017E818, 0, 0x51, 1);
            break;
        case 2:
            Task_SpawnFromTable(&D_dryfield_underpass_8017E818, 0, 0x52, 2);
            break;
    }
    return 0;
}
INCLUDE_RODATA("rooms/nonmatchings/dryfield_underpass/dryfield_underpass", RoomsShared8017d878Table);
