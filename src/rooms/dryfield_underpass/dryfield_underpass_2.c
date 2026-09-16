#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/room_common.h"

extern s32 D_dryfield_underpass_8017E8D8;

s32 func_dryfield_underpass_8017D908(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp_v1;

    temp_v1 = in->field_2;
    if ((temp_v1 == 1) && (Game_Session->field_9 == temp_v1) && (GameFlag_GetNibble(0xC9) == 0)) {
        GameFlag_SetNibble(0xC9, 1);
        func_800E8614((s32)&D_dryfield_underpass_8017E8D8, 0);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_underpass/dryfield_underpass_2", func_dryfield_underpass_8017D970);

void func_dryfield_underpass_8017DA00(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_underpass/dryfield_underpass_2", func_dryfield_underpass_8017DA08);
