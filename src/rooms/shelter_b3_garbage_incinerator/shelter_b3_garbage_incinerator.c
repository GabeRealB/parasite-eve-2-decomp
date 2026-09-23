#include "common.h"
#include "main/sound.h"

extern u16 D_shelter_b3_garbage_incinerator_801855DC;

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator", func_shelter_b3_garbage_incinerator_8017D6EC);

s32 func_shelter_b3_garbage_incinerator_8017D838(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator", func_shelter_b3_garbage_incinerator_8017D840);

s32 func_shelter_b3_garbage_incinerator_8017D9B4(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator", func_shelter_b3_garbage_incinerator_8017D9BC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator", func_shelter_b3_garbage_incinerator_8017DA74);

s32 func_shelter_b3_garbage_incinerator_8017DB2C(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 9:
            SndEvt_EnqueueType6(0x54280009, 0, 0);
            break;
        case 10:
            SndEvt_EnqueueType6(0x5428000A, 0, 0);
            break;
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator", func_shelter_b3_garbage_incinerator_8017DB7C);

void func_shelter_b3_garbage_incinerator_8017DC54(void)
{
    char pad[0x10];

    if (D_shelter_b3_garbage_incinerator_801855DC < 0x3D) {
        D_shelter_b3_garbage_incinerator_801855DC++;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator", jtbl_shelter_b3_garbage_incinerator_8017D5E8);
