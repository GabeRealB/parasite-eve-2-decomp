#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern void func_80179B14(RoomEventMsg* in, RoomEventMsg* out);

s32 func_neo_ark_north_promenade_8017D5D8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179B14(in, out);
    if (in->msgId != 0xB) {
        return 1;
    }
    if (GameFlag_GetNibble(0xF6) != 0) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    Gp_SetNibbleIf(in->field_6, 2);
    Gp_RunCapCmd1(1);
    return 0;
}

s32 func_neo_ark_north_promenade_8017D66C(void)
{
    return 0;
}

s32 func_neo_ark_north_promenade_8017D674(void)
{
    return 0;
}
INCLUDE_RODATA("rooms/nonmatchings/neo_ark_north_promenade/neo_ark_north_promenade", RoomsShared8017d878Table);
