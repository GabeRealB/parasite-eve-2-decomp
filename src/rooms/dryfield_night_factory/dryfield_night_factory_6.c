#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

extern TaskDesc* D_dryfield_night_factory_8018A7E4;
extern TaskDesc  D_dryfield_night_factory_80186E4C[];

/// Command handler for the night factory room, reached from the room's command
/// table (`D_dryfield_night_factory_80186E64`, id 0x13F0) with the command in
/// `$a2`.
///
/// Cases 1/2/3/5/12 spawn an actor out of whichever spawn table the session
/// selected (`D_..._A7E4`, written by `func_dryfield_night_factory_80180438`)
/// at index 2/3/1/0/6, handing the command on as `Task_SpawnFromTable`'s third
/// argument. Case 6 silences both characters' weapons and spawns the factory's
/// own table `D_..._80186E4C` at index 0 instead -- that table's task is the
/// `func_dryfield_night_factory_8018076C` poller. Case 12 only acts while
/// progress flag 0x49 is 1, and silences the player's and the ally's weapon
/// before spawning. Every other command does nothing.
///
/// The `goto`s are the target's shape: every path shares the single `return 0`
/// at `end`, so the exit block is the only place `$v0` is zeroed.
s32 func_dryfield_night_factory_80180814(s32 arg0, s32 arg1, s32 cmd)
{
    TaskDesc* table;
    s32       idx;

    switch (cmd) {
        case 1:
            table = D_dryfield_night_factory_8018A7E4;
            idx   = 2;
            break;
        case 2:
            table = D_dryfield_night_factory_8018A7E4;
            idx   = 3;
            break;
        case 3:
            table = D_dryfield_night_factory_8018A7E4;
            idx   = 1;
            break;
        case 5:
            table = D_dryfield_night_factory_8018A7E4;
            idx   = 0;
            break;
        case 6:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            Gp_MsgAllyWeapon(0);
            Gp_MsgAlly3F3(0);
            Task_SpawnFromTable(D_dryfield_night_factory_80186E4C, 0, 0, 0);
            goto end;
        case 12:
            if (GameFlag_GetNibble(0x49) == 1) {
                Gp_MsgPlayerWeapon(0);
                Gp_MsgAllyWeapon(0);
                table = D_dryfield_night_factory_8018A7E4;
                idx   = 6;
                break;
            }
            goto end;
        default:
            goto end;
    }
    Task_SpawnFromTable(table, idx, cmd, 0);
end:
    return 0;
}
