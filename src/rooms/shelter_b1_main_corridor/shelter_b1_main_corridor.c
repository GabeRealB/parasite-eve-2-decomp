#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_main_corridor/shelter_b1_main_corridor", func_shelter_b1_main_corridor_8017D8F4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_main_corridor/shelter_b1_main_corridor", func_shelter_b1_main_corridor_8017DA8C);

s32 func_shelter_b1_main_corridor_8017DCEC(void)
{
    return 0;
}

s32 func_shelter_b1_main_corridor_8017DCF4(void)
{
    return 0;
}

s32 func_shelter_b1_main_corridor_8017DCFC(void)
{
    return 0;
}

s32 func_shelter_b1_main_corridor_8017DD04(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 sndId;

    if (arg2 != 0xD) {
        if (arg2 == 0xE) {
            sndId = 0x540F0000 | 0xE;
            goto play;
        }
    } else {
        sndId = 0x540F000D;
    play:
        SndEvt_EnqueueType6(sndId, 0, 0);
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_main_corridor/shelter_b1_main_corridor", RoomsShared8017d878Table);
