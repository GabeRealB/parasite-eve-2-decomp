#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/dryfield_night_parking_lot.h"

/// The `GpAreaApplyRec` list the 0x11 answer applies when the event fires.
extern GpAreaApplyRec D_dryfield_night_parking_lot_8018155C[];

/// Handler for message 0x13EE in the room's message table: the room's two
/// reports and its two events. The incoming record is first copied to `out`.
///
/// Messages 2 and 0x1D answer in `out->field_3` (only when `field_5` is clear):
/// message 2 gives nibble 0x61 plus one while nibble 0x7A is under 4, and 3
/// once it is not; message 0x1D gives 1 while nibble 0x61 is clear and 3 once
/// it is set.
///
/// Messages 0x11 and 0x12 are the events: each builds a request for the
/// room's event gate `func_dryfield_night_parking_lot_8017D5FC` - message 0x11
/// on nibble 0x40 with item 0x12, message 0x12 on nibble 0x35 with item 0x10.
/// When the gate reports the event fired, 0x11 applies the area records above
/// and sets nibbles 0x46 and 0x97, while 0x12 sets item-seen bit 0x110. Any
/// other message returns 1.
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
        ret         = func_dryfield_night_parking_lot_8017D5FC(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (D_dryfield_night_parking_lot_8018156C != 0) {
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
        ret         = func_dryfield_night_parking_lot_8017D5FC(&req, out);
        if (D_dryfield_night_parking_lot_8018156C != 0) {
            Gp_SetItemSeenBit(0x110, 1);
        }
    } else {
        return 1;
    }
    return ret;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_parking_lot/dryfield_night_parking_lot", D_dryfield_night_parking_lot_8017D5DC);
