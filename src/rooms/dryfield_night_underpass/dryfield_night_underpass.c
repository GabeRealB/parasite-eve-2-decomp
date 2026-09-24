#include "common.h"

#include "main/task.h"

extern TaskDesc D_dryfield_night_underpass_8017DCD8[];

/// Handler for message 0x13F0: for `arg2` 1 or 2, spawns the room's switch
/// task `func_dryfield_night_underpass_8017D5D0` from the task table, toggling
/// nibble 0x51 with cap command 1 or nibble 0x52 with cap command 2. Any other
/// value spawns nothing. Always returns 0.
s32 func_dryfield_night_underpass_8017D868(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 1:
            Task_SpawnFromTable(D_dryfield_night_underpass_8017DCD8, 0, 0x51, 1);
            break;
        case 2:
            Task_SpawnFromTable(D_dryfield_night_underpass_8017DCD8, 0, 0x52, 2);
            break;
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_underpass/dryfield_night_underpass", D_dryfield_night_underpass_8017D5C4);
