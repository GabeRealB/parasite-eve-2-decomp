#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"

#include "rooms/room_common.h"

/// The pair of cutscene blocks `func_800E8634` hands to `Task_Spawn` (bank 9,
/// type 7): the one the running scene starts and the one it parks in
/// `D_801156D0` for the task that follows it.
extern s32 D_dryfield_toilet_80180C58;
extern s32 D_dryfield_toilet_80180F40;

s32 func_dryfield_toilet_8017D8B8(void)
{
    return 0;
}

s32 func_dryfield_toilet_8017D8C0(void)
{
    return 0;
}

/// Handler for message `0x13EF` in the room's `(msgId, handler)` table - the
/// direction record `Gp_PostMsg13EF` posts. On the visit whose sub-id
/// (`field_2`) is 1, that agrees with the session's own sub-id
/// (`Game_Session::field_9`) and that has not yet latched nibble 0x60, the
/// toilet starts its cutscene pair and latches the nibble. The outgoing record
/// is never written: this handler only consumes the message.
s32 func_dryfield_toilet_8017D8C8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 subId = in->field_2;

    if (subId == 1 && GameFlag_GetNibble(0x60) == 0 && Game_Session->field_9 == subId) {
        func_800E8634((s32)&D_dryfield_toilet_80180C58, 1, (s32)&D_dryfield_toilet_80180F40);
        GameFlag_SetNibble(0x60, 1);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_toilet/dryfield_toilet_2", func_dryfield_toilet_8017D940);
