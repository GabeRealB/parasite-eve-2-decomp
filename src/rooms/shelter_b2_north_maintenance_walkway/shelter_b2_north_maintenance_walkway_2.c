#include "common.h"

#include "main/sound.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_north_maintenance_walkway/shelter_b2_north_maintenance_walkway_2", func_shelter_b2_north_maintenance_walkway_8017DA88);

s32 func_shelter_b2_north_maintenance_walkway_8017DC44(void)
{
    return 0;
}

s32 func_shelter_b2_north_maintenance_walkway_8017DC4C(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_north_maintenance_walkway/shelter_b2_north_maintenance_walkway_2", func_shelter_b2_north_maintenance_walkway_8017DC54);

s32 func_shelter_b2_north_maintenance_walkway_8017DCE4(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 7) {
        SndEvt_EnqueueType6(0x541E0000 | 7, 0, 0);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_north_maintenance_walkway/shelter_b2_north_maintenance_walkway_2", func_shelter_b2_north_maintenance_walkway_8017DD18);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_north_maintenance_walkway/shelter_b2_north_maintenance_walkway_2", RoomsShared8017d878Table);
