#include "common.h"

#include "main/sound.h"

s32 func_dryfield_night_motel_room_6_80181BF8(void)
{
    return 0;
}

s32 func_dryfield_night_motel_room_6_80181C00(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x531E000C, 0, 0);
    }
    return 0;
}
