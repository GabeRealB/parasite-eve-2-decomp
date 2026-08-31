#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_corridor/shelter_b6_corridor", func_shelter_b6_corridor_8017D5D0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_corridor/shelter_b6_corridor", func_shelter_b6_corridor_8017DEA8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_corridor/shelter_b6_corridor", func_shelter_b6_corridor_8017DEB0);

s32 func_shelter_b6_corridor_8017DF48(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 2:
            if (GameFlag_GetNibble(0x144) != 0) {
                Gp_RunCapCmd1(5);
            } else if (Gp_StateF0.field_0 == 1) {
                Gp_RunCapCmd1(2);
            } else {
                Gp_RunCapCmd1(8);
            }
            break;
        case 3:
            if (GameFlag_GetNibble(0x145) != 0) {
                Gp_RunCapCmd1(6);
            } else if (Gp_StateF0.field_0 == 1) {
                Gp_RunCapCmd1(3);
            } else {
                Gp_RunCapCmd1(9);
            }
            break;
        case 4:
            if (GameFlag_GetNibble(0x146) != 0) {
                Gp_RunCapCmd1(7);
            } else if (Gp_StateF0.field_0 != 1) {
                Gp_RunCapCmd1(0xA);
            } else {
                Gp_RunCapCmd1(4);
            }
            break;
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_corridor/shelter_b6_corridor", func_shelter_b6_corridor_8017E020);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_corridor/shelter_b6_corridor", func_shelter_b6_corridor_8017E028);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_corridor/shelter_b6_corridor", func_shelter_b6_corridor_8017E064);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_corridor/shelter_b6_corridor", func_shelter_b6_corridor_8017E12C);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_corridor/shelter_b6_corridor", D_shelter_b6_corridor_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_corridor/shelter_b6_corridor", func_shelter_b6_corridor_8017E144);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_corridor/shelter_b6_corridor", func_shelter_b6_corridor_8017E19C);
