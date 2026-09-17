#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

/// The `GpAreaApplyRec` list the 0x11 answer applies when the event fires.
extern GpAreaApplyRec D_dryfield_night_parking_lot_8018155C[];

/// The room's two reports and its two events.
///
/// Messages 2 and 0x1D answer a status query in `out->field_3` and are the
/// shared room-script bodies `Room_Script02` / `Room_Script04` verbatim: message
/// 2 answers nibble 0x7A, giving the nibble 0x61 count plus one while it is
/// still under 4 and 3 once it is not - the count the weapon-sale progress
/// meter reads - and message 0x1D answers nibble 0x61, 1 while it is clear and
/// 3 once it is set.
///
/// Messages 0x11 and 0x12 are the events: each builds the request the gate at
/// `RoomsShared8017d638` takes, message 0x11 for nibble 0x40 and message 0x12
/// for nibble 0x35 with collected bit 0x110. When the gate reports the event
/// fired, 0x11 applies the area records above and sets nibbles 0x46 / 0x97
/// while 0x12 sets the collected bit. Anything else is not consumed.
s32 func_dryfield_night_parking_lot_8017D8D0(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;
    s32          val;
    s32          n;

    *out = *msg;
    if ((msg->msgId == 2) && (msg->field_5 == 0)) {
        n = GameFlag_GetNibble(0x7A);
        if (n < 4) {
            val = 3;
            TOUCH_REG(val);
            val = GameFlag_GetNibble(0x61) + 1;
        } else {
            val = 3;
        }
        out->field_3 = val;
    }
    if ((msg->msgId == 0x1D) && (msg->field_5 == 0)) {
        n = GameFlag_GetNibble(0x61);
        if (n == 0) {
            n = 1;
        } else {
            n = 3;
        }
        out->field_3 = n;
    }
    if (msg->msgId == 0x11) {
        req.field_0 = 6;
        req.field_4 = 1;
        req.field_8 = Gp_PackStageSndId(0x520F000B);
        req.field_C = Gp_PackStageSndId(0x520F0007);
        req.flagId  = 0x40;
        req.itemId  = 0x12;
        ret         = RoomsShared8017d638(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (RoomsShared8017d638Flag != 0) {
            Gp_ApplyAreaRecs(D_dryfield_night_parking_lot_8018155C);
            GameFlag_SetNibble(0x46, 1);
            GameFlag_SetNibble(0x97, 1);
        }
    } else if (msg->msgId == 0x12) {
        req.field_0 = 3;
        req.field_4 = 2;
        req.field_8 = Gp_PackStageSndId(0x520F000B);
        req.field_C = Gp_PackStageSndId(0x520F0007);
        req.flagId  = 0x35;
        req.itemId  = 0x10;
        ret         = RoomsShared8017d638(&req, out);
        if (RoomsShared8017d638Flag != 0) {
            Gp_SetItemSeenBit(0x110, 1);
        }
    } else {
        return 1;
    }
    return ret;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_parking_lot/dryfield_night_parking_lot", RoomsShared8017d878Table);
