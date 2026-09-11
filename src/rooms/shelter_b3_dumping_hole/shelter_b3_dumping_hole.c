#include "common.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole", func_shelter_b3_dumping_hole_8017D760);

s32 func_shelter_b3_dumping_hole_8017D82C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x12) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0x11D) != 0 ? 0x12 : 0x17, 1);
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole", D_shelter_b3_dumping_hole_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole", D_shelter_b3_dumping_hole_8017D5C4);
