#include "common.h"

#include "main/sound.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_elevator_hall/shelter_b2_elevator_hall", func_shelter_b2_elevator_hall_8017D8E4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_elevator_hall/shelter_b2_elevator_hall", func_shelter_b2_elevator_hall_8017DAD4);

s32 func_shelter_b2_elevator_hall_8017DC70(void)
{
    return 0;
}

s32 func_shelter_b2_elevator_hall_8017DC78(void)
{
    return 0;
}

s32 func_shelter_b2_elevator_hall_8017DC80(void)
{
    return 0;
}

s32 func_shelter_b2_elevator_hall_8017DC88(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        SndEvt_EnqueueType6(0x541B0000 | 1, 0, 0);
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_elevator_hall/shelter_b2_elevator_hall", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_elevator_hall/shelter_b2_elevator_hall", jtbl_shelter_b2_elevator_hall_8017D5FC);
