#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

/// Cutscene script blob arguments of `func_800E8634`.
extern s32 D_dryfield_night_dilapidated_house_801868F4;
extern s32 D_dryfield_night_dilapidated_house_80187134;

/// The room task's message table: `{id, handler}` pairs terminated by
/// `0x7FFFFFFF`.
extern GpMsgEntry D_dryfield_night_dilapidated_house_8017E700[];

s32 func_dryfield_night_dilapidated_house_8017D8D4(void)
{
    return 0;
}

/// Handler for the room's `0x13EE` message, the warp destination the gameplay
/// side posts as `Gp_WarpLoc`: copies the incoming payload through to `out` and,
/// when the destination id is 5, offers the gate a request that plays the
/// room's pair of stage sounds under flag nibble 0x3F. Returns 1 for a
/// destination it does not own.
s32 func_dryfield_night_dilapidated_house_8017D8DC(s32 arg0, s32 arg1, RoomEventMsg* in,
                                                   RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    if (in->msgId == 5) {
        req.field_0 = 0xC;
        req.field_4 = 0xC;
        req.field_8 = 0x53090005;
        req.field_C = 0x53090001;
        req.flagId  = 0x3F;
        req.itemId  = 0;
        return RoomsShared8017d638(&req, in);
    }
    return 1;
}

s32 func_dryfield_night_dilapidated_house_8017D960(void)
{
    return 0;
}

s32 func_dryfield_night_dilapidated_house_8017D968(void)
{
    return 0;
}

void func_dryfield_night_dilapidated_house_8017D970(Task* arg0)
{
    arg0->field_24 = D_dryfield_night_dilapidated_house_8017E700;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    if (GameFlag_GetNibble(0x92) == 0) {
        if (Game_GetPtrSlot(0xA) != 0) {
            func_800E8634((s32)&D_dryfield_night_dilapidated_house_801868F4, 0,
                          (s32)&D_dryfield_night_dilapidated_house_80187134);
        }
        GameFlag_SetNibble(0x92, 1);
        GameFlag_SetNibble(0x7A, 3);
        func_800E3FAC(0xA2, 0x11);
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_dilapidated_house/dryfield_night_dilapidated_house", RoomsShared8017d878Table);
