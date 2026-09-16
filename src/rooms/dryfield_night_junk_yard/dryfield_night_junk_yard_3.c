#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"

/// Applies game flag nibble 0x9F to the sixth sprite command of view 0 in the
/// current room's sprite record: a zero nibble draws the command, a nonzero one
/// hides it (`Gp_LinkViewSprts` skips OT-linking when `field_4` is set).
void func_dryfield_night_junk_yard_8017D9B8(u8 arg0)
{
    GameSessionFrom4* sess = (GameSessionFrom4*)&Game_Session->field_4;
    GpSprtCmd*        cmd;

    cmd = Gp_SprtTables[sess->field_3 - 1][0].field_0[sess->field_2 - 1][6].field_4;
    if (arg0 == 0) {
        cmd[5].field_4 = 0;
    } else {
        cmd[5].field_4 = 1;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_junk_yard/dryfield_night_junk_yard_3", func_dryfield_night_junk_yard_8017DA14);
