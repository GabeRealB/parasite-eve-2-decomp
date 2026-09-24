#include "common.h"

#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_storeroom/shelter_b1_storeroom", func_shelter_b1_storeroom_8017D604);

s32 func_shelter_b1_storeroom_8017D6E0(void)
{
    return 0;
}

s32 func_shelter_b1_storeroom_8017D6E8(void)
{
    return 0;
}

s32 func_shelter_b1_storeroom_8017D6F0(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 8:
            SndEvt_EnqueueType6(0x540B0008, 0, 0);
            break;
        case 0x6A:
            SndEvt_EnqueueType6(0x540B0009, 0, 0);
            break;
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_storeroom/shelter_b1_storeroom", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_storeroom/shelter_b1_storeroom", jtbl_shelter_b1_storeroom_8017D5D0);
