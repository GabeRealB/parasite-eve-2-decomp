#include "common.h"
#include "gameplay/D4.h"
#include "main/session.h"

/// Sets or clears `field_4` of the second sprite command in view 13 of the
/// current room's sprite table, from the low byte of `arg0` (zero clears it,
/// anything else sets it to 1).
void func_shelter_b4_upper_sewer_8017E59C(s32 arg0)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtCmd* cmd;

    cmd = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1][12].field_4;
    if ((arg0 & 0xFF) == 0) {
        cmd[1].field_4 = 0;
    } else {
        cmd[1].field_4 = 1;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer_3", func_shelter_b4_upper_sewer_8017E5F8);
