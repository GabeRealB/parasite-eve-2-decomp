#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

INCLUDE_ASM("rooms/nonmatchings/mine_gorge/mine_gorge", func_mine_gorge_8017D5F8);

s32 func_mine_gorge_8017D6E8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    if (in->msgId != 2) {
        return 1;
    }
    if (GameFlag_GetNibble(0xB5) != 0) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    Gp_SetNibbleIf(in->field_6, 2);
    Gp_RunCapCmd1(3);
    return 0;
}
INCLUDE_RODATA("rooms/nonmatchings/mine_gorge/mine_gorge", RoomsShared8017d878Table);
