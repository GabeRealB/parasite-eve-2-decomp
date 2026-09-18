#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/room_common.h"

extern u8         D_8007216D;
extern GpMsgEntry D_dryfield_underpass_8017E830[];
extern s32        D_dryfield_underpass_8017E89C;
extern s32        D_dryfield_underpass_8017E8D8;

s32 func_dryfield_underpass_8017D908(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp_v1;

    temp_v1 = in->field_2;
    if ((temp_v1 == 1) && (gGameSession->at4.loc.place == temp_v1) && (GameFlag_GetNibble(0xC9) == 0)) {
        GameFlag_SetNibble(0xC9, 1);
        func_800E8614((s32)&D_dryfield_underpass_8017E8D8, 0);
    }
    return 0;
}

void func_dryfield_underpass_8017D970(Task* arg0)
{
    arg0->field_24 = D_dryfield_underpass_8017E830;
    Game_SetPtrSlot(arg0, 7);
    if ((gGameSession->at4.loc.place == 1) && (GameFlag_GetNibble(0xC9) == 0)) {
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&D_dryfield_underpass_8017E89C, 0x7DB);
    }
    arg0->state = arg0->state + 1;
}

void func_dryfield_underpass_8017DA00(void)
{
}

/// Builds the message 0x26 record the room asks `Room_Script09` for - the same
/// nibble 0xC9 / 0x53 / 0x51 answer in `field_3` - and publishes it: the answer
/// is the room index stored in `gGameSession.at4.loc.room` (and the area-record id
/// `D_8007216D`), then the session is told to rebuild through
/// `Gp_RoomObjState1`. `Room_Script01`'s script-mode branch is the same code
/// with a task wrapper around it.
void func_dryfield_underpass_8017DA08(void)
{
    RoomEventMsg  src;
    RoomEventMsg  dst;
    RoomEventMsg* s;
    RoomEventMsg* d;
    GameSession*  session;
    u8            room;

    d           = &dst;
    s           = &src;
    *(u16*)&src = 0x26;
    src.field_5 = 0;
    if (s->field_5 == 0) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) != 0) {
                d->field_3 = 2;
            } else {
                d->field_3 = 1;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                dst.field_3 = dst.field_3 + 2;
            }
        } else {
            if (GameFlag_GetNibble(0x51) != 0) {
                d->field_3 = 5;
            } else {
                d->field_3 = 6;
            }
        }
    }
    session                     = gGameSession;
    room                        = dst.field_3;
    session->at4.loc.room       = room;
    D_8007216D                  = room;
    gGameSession->roomObjsDirty = 1;
}
