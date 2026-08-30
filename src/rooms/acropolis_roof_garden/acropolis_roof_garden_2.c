#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"

extern u8 D_801153F4;

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_2", func_acropolis_roof_garden_8017D7A0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_2", func_acropolis_roof_garden_8017D868);

s32 func_acropolis_roof_garden_8017D8AC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        if ((Gp_GetCurBit2Flag(0x13) == 0) || (Gp_GetCurBit2Flag(0x13) == 1)) {
            Gp_RunCapCmd1(5);
        } else {
            Gp_StartCapSlot(2, 1, 0);
        }
    }
    if (arg2 == 4) {
        if (GameFlag_GetNibble(0x155) < 6) {
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 6);
        }
        D_801153F4 = 1;
        Gp_RunCapCmd(4, 0);
        func_800E3FAC(0xA2, 7);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_2", func_acropolis_roof_garden_8017D970);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_2", func_acropolis_roof_garden_8017DA48);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_2", func_acropolis_roof_garden_8017DAD4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_2", func_acropolis_roof_garden_8017DB74);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_2", func_acropolis_roof_garden_8017DBEC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_2", func_acropolis_roof_garden_8017DC74);
