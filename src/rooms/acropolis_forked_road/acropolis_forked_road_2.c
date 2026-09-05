#include "common.h"

#include "gameplay/gameplay.h"

#include "main/fs.h"

s32 func_acropolis_forked_road_8017D858(s32 arg0, s32 arg1, s32 arg2)
{
    s32 cmd;

    if (arg2 == 3) {
        if ((Gp_GetCurBit2Flag(0x18) == 0) || (Gp_GetCurBit2Flag(0x18) == 1)) {
            cmd = 3;
        } else {
            cmd = 4;
        }
        Gp_RunCapCmd1(cmd);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road_2", func_acropolis_forked_road_8017D8A8);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road_2", jtbl_acropolis_forked_road_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road_2", D_acropolis_forked_road_8017D5E8);
