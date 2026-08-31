#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"

s32 func_neo_ark_power_plant_2_8017D61C(s32 arg0, s32 arg1, s32 arg2)
{
    s32 cmd;

    switch (arg2) {
        case 2:
            if (GameFlag_GetNibble(0xDF) == 0) {
                cmd = 2;
            } else {
                cmd = 5;
            }
            break;
        case 3:
            cmd = 7;
            if (Gp_StateF0.field_0 != 2) {
                cmd = GameFlag_GetNibble(0x147) != 0 ? 6 : 3;
            }
            break;
        default:
            goto done;
    }
    Gp_RunCapCmd1(cmd);
done:
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_power_plant_2/neo_ark_power_plant_2", D_neo_ark_power_plant_2_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_power_plant_2/neo_ark_power_plant_2", D_neo_ark_power_plant_2_8017D5C4);
