#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

s32 func_shelter_b1_storeroom_8017D604(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0xD && GameFlag_GetNibble(0xA6) == 0) {
        if (in->field_5 != 0) {
            return 0;
        }
        Gp_SetNibbleIf(in->field_6, 2);
        Gp_RunCapCmd1(1);
        return 0;
    }
    if (in->msgId != 0xC && in->msgId != 0xA) {
        return 1;
    }
    if (GameFlag_GetNibble(0x7A) < 6) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    Gp_RunCapCmd1(0xE);
    return 0;
}

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

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_storeroom/shelter_b1_storeroom", D_shelter_b1_storeroom_8017D5C4);
