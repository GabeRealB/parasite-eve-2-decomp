#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

extern TaskDesc* D_dryfield_factory_8018A3C4;
extern TaskDesc  D_dryfield_factory_801826BC[];

/// Command handler of the room's message table. Commands 1, 2, 3, 5 and 12
/// spawn entries 2, 3, 1, 0 and 6 of the spawn table the entry task selected,
/// passing the command on; 12 only while progress nibble 0x49 is 1, after
/// silencing both weapons. Command 6 silences both characters and spawns the
/// room's own table instead. Every command answers 0.
///
/// The `goto`s give every path the single `return 0` at `end`, the target's
/// shape.
s32 func_dryfield_factory_8017DDA8(s32 arg0, s32 arg1, s32 cmd)
{
    TaskDesc* table;
    s32       idx;

    switch (cmd) {
        case 1:
            table = D_dryfield_factory_8018A3C4;
            idx   = 2;
            break;
        case 2:
            table = D_dryfield_factory_8018A3C4;
            idx   = 3;
            break;
        case 3:
            table = D_dryfield_factory_8018A3C4;
            idx   = 1;
            break;
        case 5:
            table = D_dryfield_factory_8018A3C4;
            idx   = 0;
            break;
        case 6:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            Gp_MsgAllyWeapon(0);
            Gp_MsgAlly3F3(0);
            Task_SpawnFromTable(D_dryfield_factory_801826BC, 0, 0, 0);
            goto end;
        case 12:
            if (GameFlag_GetNibble(0x49) == 1) {
                Gp_MsgPlayerWeapon(0);
                Gp_MsgAllyWeapon(0);
                table = D_dryfield_factory_8018A3C4;
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

INCLUDE_RODATA("rooms/nonmatchings/dryfield_factory/dryfield_factory_2", D_dryfield_factory_8017D61C);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_factory/dryfield_factory_2", D_dryfield_factory_8017D628);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_factory/dryfield_factory_2", D_dryfield_factory_8017D634);
