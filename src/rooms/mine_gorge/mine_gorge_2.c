#include "common.h"

INCLUDE_ASM("rooms/nonmatchings/mine_gorge/mine_gorge_2", func_mine_gorge_8017D784);

s32 func_mine_gorge_8017D7F4(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xA) {
        SndEvt_EnqueueType6(0x54050000 | arg2, 0, 0);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/mine_gorge/mine_gorge_2", func_mine_gorge_8017D828);
