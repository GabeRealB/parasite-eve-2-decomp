#include "common.h"

#include "main/sound.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", func_shelter_b2_main_corridor_8017D6BC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", func_shelter_b2_main_corridor_8017D82C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", func_shelter_b2_main_corridor_8017D9C4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", func_shelter_b2_main_corridor_8017DC88);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", func_shelter_b2_main_corridor_8017DEB0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", func_shelter_b2_main_corridor_8017E0FC);

s32 func_shelter_b2_main_corridor_8017E1CC(void)
{
    return 0;
}

s32 func_shelter_b2_main_corridor_8017E1D4(void)
{
    return 0;
}

s32 func_shelter_b2_main_corridor_8017E1DC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 9) {
        SndEvt_EnqueueType6(0x54210000 | 9, 0, 0);
    }
    return 0;
}
