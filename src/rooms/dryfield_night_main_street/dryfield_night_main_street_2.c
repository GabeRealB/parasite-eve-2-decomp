#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/dryfield_night_main_street.h"

/// Event record the room's own gate latches for its event task, next to the
/// message that triggered it. `flagId` is the game-flag nibble the event sets
/// once it has fired; zero means the event is not latched by a nibble.
typedef struct {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    s8  field_A;
} _LatchedEvent;

extern RoomEventMsg  D_dryfield_night_main_street_80188BAC;
extern s8            D_dryfield_night_main_street_80188BB4;
extern _LatchedEvent D_dryfield_night_main_street_80188BC8;
extern TaskDesc      D_dryfield_night_main_street_8018208C;

/// Message handler for the room. Messages 0x19, 1 and 0xF answer in the copy's
/// `field_3` from story nibbles. Messages 0xB and 0xC run the room's own event
/// gate: unless the event's nibble is already set, the message and event are
/// latched, the nibble is set and the event task is spawned. Messages 0xD and
/// 0xE go through the shared gate and, when it fires, swap collected bits
/// 0x10F / 0x112 for 0x113. Anything else is not consumed.
s32 func_dryfield_night_main_street_8017DA6C(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq  req;
    _LatchedEvent ev;
    s32           ret;

    *out = *msg;
    if (msg->msgId == 0x19) {
        if (gGameSession->at4.loc.stage == 2) {
            if (msg->field_5 == 0) {
                if (GameFlag_GetNibble(0x3A) >= 2) {
                    out->field_3 = 2;
                } else {
                    out->field_3 = 1;
                }
            }
        } else if (msg->field_5 == 0) {
            out->field_3 = GameFlag_GetNibble(0x61) + 1;
        }
    }
    if (msg->msgId == 1 && msg->field_5 == 0) {
        if (GameFlag_GetNibble(0x63) == 0) {
            out->field_3 = 1;
        } else if (GameFlag_GetNibble(0x7A) >= 4) {
            out->field_3 = 4;
        } else {
            out->field_3 = GameFlag_GetNibble(0x61) + 2;
        }
    }
    if (msg->msgId == 0xF && msg->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    if (msg->msgId == 0x19 && GameFlag_GetNibble(0x61) != 0) {
        if (msg->field_5 == 0) {
            Gp_SetNibbleIf(msg->field_6, 2);
            Gp_RunCapCmd1(0x13);
            return 2;
        }
        return 2;
    }
    if (msg->msgId == 0xB) {
        ev.field_0                            = 3;
        ev.field_4                            = 0x52020005;
        ev.flagId                             = 0x57;
        ev.field_A                            = 0;
        D_dryfield_night_main_street_80188BB4 = 0;
        if (GameFlag_GetNibble(ev.flagId) == 0 || ev.flagId == 0) {
            if (out->field_5 == 0) {
                D_dryfield_night_main_street_80188BAC = *out;
                D_dryfield_night_main_street_80188BC8 = ev;
                if (ev.flagId != 0) {
                    GameFlag_SetNibble(ev.flagId, 1);
                }
                Task_SpawnFromTable(&D_dryfield_night_main_street_8018208C, 0, 0, 0);
                D_dryfield_night_main_street_80188BB4 = 1;
                return 2;
            }
            return 2;
        }
        return 1;
    } else if (msg->msgId == 0xC) {
        ev.field_0                            = 4;
        ev.field_4                            = 0x52020005;
        ev.flagId                             = 0x58;
        ev.field_A                            = 0;
        D_dryfield_night_main_street_80188BB4 = 0;
        if (GameFlag_GetNibble(ev.flagId) == 0 || ev.flagId == 0) {
            if (out->field_5 == 0) {
                D_dryfield_night_main_street_80188BAC = *out;
                D_dryfield_night_main_street_80188BC8 = ev;
                if (ev.flagId != 0) {
                    GameFlag_SetNibble(ev.flagId, 1);
                }
                Task_SpawnFromTable(&D_dryfield_night_main_street_8018208C, 0, 0, 0);
                D_dryfield_night_main_street_80188BB4 = 1;
                return 2;
            }
            return 2;
        }
        return 1;
    } else if (msg->msgId == 0xD) {
        req.field_0 = 0xA;
        req.field_4 = 5;
        req.field_8 = Gp_PackStageSndId(0x5202000A);
        req.field_C = Gp_PackStageSndId(0x52020005);
        req.flagId  = 0x41;
        req.itemId  = 0x13;
        ret         = func_dryfield_night_main_street_8017D798(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (D_dryfield_night_main_street_80188BC4 != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
        if (msg->field_5 == 0 && GameFlag_GetNibble(0x93) == 0) {
            Gp_SetNibbleIf(msg->field_6, 0);
        }
        return ret;
    } else if (msg->msgId == 0xE) {
        req.field_0 = 0xB;
        req.field_4 = 6;
        req.field_8 = Gp_PackStageSndId(0x5202000A);
        req.field_C = Gp_PackStageSndId(0x52020005);
        req.flagId  = 0x42;
        req.itemId  = 0x13;
        ret         = func_dryfield_night_main_street_8017D798(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (D_dryfield_night_main_street_80188BC4 != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
        if (msg->field_5 == 0 && GameFlag_GetNibble(0x94) == 0) {
            Gp_SetNibbleIf(msg->field_6, 0);
        }
        return ret;
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_main_street/dryfield_night_main_street_2", D_dryfield_night_main_street_8017D5F4);
