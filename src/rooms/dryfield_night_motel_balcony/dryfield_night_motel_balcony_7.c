#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/dryfield_night_motel_balcony.h"

extern s8 D_8007272D;

/// The room's message handler. It copies `msg` to `out`, filling `field_3`
/// from game flags for messages 0x1C, 0xF and 0x1F, then routes messages 0x1C,
/// 0x1F and 0x1E through the event gate with each one's request; when the
/// gate fires, it updates the collected and seen item bits (and, for 0x1E, a
/// flag nibble and `D_8007272D`). Any other message answers 1; a gate result
/// of 0 is reported as 2.
s32 func_dryfield_night_motel_balcony_8017D968(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          flagClear;
    s32          ret;

    *out = *msg;
    if (msg->msgId == 0x1C && msg->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    if (msg->msgId == 0xF && msg->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    if (msg->msgId == 0x1F && msg->field_5 == 0) {
        flagClear    = GameFlag_GetNibble(0x96) == 0;
        out->field_3 = flagClear ? 1 : 2;
    }
    if (msg->msgId == 0x1C) {
        req.field_0 = 7;
        req.field_4 = 4;
        req.field_8 = Gp_PackStageSndId(0x521D000A);
        req.field_C = Gp_PackStageSndId(0x521D0001);
        req.flagId  = 0x43;
        req.itemId  = 0x13;
        ret         = func_dryfield_night_motel_balcony_8017D694(&req, out);
        if (D_dryfield_night_motel_balcony_8018F2DC != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
    } else if (msg->msgId == 0x1F) {
        req.field_0 = 5;
        req.field_4 = 2;
        req.field_8 = Gp_PackStageSndId(0x521D000A);
        req.field_C = Gp_PackStageSndId(0x521D0001);
        req.flagId  = 0x44;
        req.itemId  = 0x13;
        ret         = func_dryfield_night_motel_balcony_8017D694(&req, out);
        if (D_dryfield_night_motel_balcony_8018F2DC != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
    } else if (msg->msgId == 0x1E) {
        req.field_0 = 6;
        req.field_4 = 3;
        req.field_8 = Gp_PackStageSndId(0x521D000A);
        req.field_C = Gp_PackStageSndId(0x521D0001);
        req.flagId  = 0x2E;
        req.itemId  = 0xF;
        ret         = func_dryfield_night_motel_balcony_8017D694(&req, out);
        if (D_dryfield_night_motel_balcony_8018F2DC != 0) {
            GameFlag_SetNibble(0x30, 1);
            D_8007272D = 3;
            func_800E3FAC(0xA2, 0xC);
        }
    } else {
        return 1;
    }
    if (ret == 0) {
        ret = 2;
    }
    return ret;
}

/// Plays stage sound 0x521D0008 or 0x521D0009 for events 8 and 9; always
/// answers 0.
s32 func_dryfield_night_motel_balcony_8017DBC8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0x8:
            Gp_EnqueueStageSnd6(0x521D0008, 0, 0);
            break;
        case 0x9:
            Gp_EnqueueStageSnd6(0x521D0009, 0, 0);
            break;
    }
    return 0;
}
