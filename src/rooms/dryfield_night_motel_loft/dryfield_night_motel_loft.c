#include "common.h"

#include "gameplay/3CD8.h"
#include "main/task.h"

extern TaskDesc D_dryfield_night_motel_loft_8017EB4C;

/// The room's `D_dryfield_night_motel_loft_8017EB1C` `GpMsgEntry` table lists
/// this as the id 0x13F0 handler, so `Gp_DispatchMsg` calls it with the
/// sender's command in `$a2` (as `func_dryfield_night_factory_80180814`). On
/// command 3 the player's weapon is silenced and the room's own script task
/// (`D_dryfield_night_motel_loft_8017EB4C`, whose callback is
/// `func_dryfield_night_motel_loft_8017D6F8`) is spawned. Every other command
/// does nothing.
s32 func_dryfield_night_motel_loft_8017D67C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 3) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(&D_dryfield_night_motel_loft_8017EB4C, 0, 0, 0);
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_loft/dryfield_night_motel_loft", D_dryfield_night_motel_loft_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_loft/dryfield_night_motel_loft", RoomsShared8017d878Table);
