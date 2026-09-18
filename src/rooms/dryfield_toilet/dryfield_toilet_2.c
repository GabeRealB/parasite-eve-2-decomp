#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"

#include "rooms/room_common.h"

/// The pair of cutscene blocks `func_800E8634` hands to `Task_Spawn` (bank 9,
/// type 7): the one the running scene starts and the one it parks in
/// `D_801156D0` for the task that follows it.
extern s32 D_dryfield_toilet_80180C58;
extern s32 D_dryfield_toilet_80180F40;

/// The room task's message table (published in `Task::msgTable` for
/// `Gp_DispatchMsg` to walk) and the four-byte payload `func_dryfield_toilet_8017D940`
/// hands that call as `arg2`.
extern s32 D_dryfield_toilet_801802A4;
extern s32 D_dryfield_toilet_801802D4;

void func_dryfield_toilet_8017D5E4(void);

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
/// (`gGameSession::at4.loc.place`) and that has not yet latched nibble 0x60, the
/// toilet starts its cutscene pair and latches the nibble. The outgoing record
/// is never written: this handler only consumes the message.
s32 func_dryfield_toilet_8017D8C8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 subId = in->field_2;

    if (subId == 1 && GameFlag_GetNibble(0x60) == 0 && gGameSession->at4.loc.place == subId) {
        func_800E8634((s32)&D_dryfield_toilet_80180C58, 1, (s32)&D_dryfield_toilet_80180F40);
        GameFlag_SetNibble(0x60, 1);
    }
    return 0;
}

/// The room task's entry state: publish the message table, claim game pointer
/// slot 7, and on the visit that agrees with the session's sub-id
/// (`gGameSession::at4.loc.place` == 1) and has not yet latched nibble 0x60, post
/// message `0x7DA` with the room's payload and run the scene setup. Advance to
/// the next state either way.
void func_dryfield_toilet_8017D940(Task* arg0)
{
    arg0->msgTable = &D_dryfield_toilet_801802A4;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x60) == 0 && gGameSession->at4.loc.place == 1) {
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&D_dryfield_toilet_801802D4, 0x7DB);
        func_dryfield_toilet_8017D5E4();
    }
    arg0->state = arg0->state + 1;
}
