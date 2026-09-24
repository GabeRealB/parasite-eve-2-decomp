#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_night_water_tower.h"
#include "rooms/room_common.h"

/// The room's handler for message 0x13EE, the first entry of its message table.
/// It copies the incoming record to `out` and answers by the record's first
/// halfword. For 0x13 it builds the room's event request -- flag nibble 0x34,
/// prerequisite item 0x10, CAP commands 0xA and 6 and two stage sounds -- and
/// hands it to the event gate with the incoming record; the gate's 0 (the
/// prerequisite missing) is answered as 2, and once the gate has latched the
/// event item 0x110 is marked seen. Any other record first drops nibble 0x55
/// from 2 back to 1 unless it is only a query. For 0x15 it also clears nibble
/// 0x4B when it reads 7, and answers 1 on stage 3 and otherwise only while
/// nibble 0x32 is 2. Everything else answers 1.
s32 func_dryfield_night_water_tower_8017D8E0(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *msg;
    if (msg->msgId == 0x13) {
        req.field_0 = 0xA;
        req.field_4 = 6;
        req.field_8 = Gp_PackStageSndId(0x5214000E);
        req.field_C = Gp_PackStageSndId(0x52140003);
        req.flagId  = 0x34;
        req.itemId  = 0x10;
        ret         = func_dryfield_night_water_tower_8017D60C(&req, msg);
        if (ret == 0) {
            ret = 2;
        }
        if (D_dryfield_night_water_tower_80182C58 != 0) {
            Gp_SetItemSeenBit(0x110, 1);
        }
        return ret;
    }
    if (msg->field_5 == 0 && GameFlag_GetNibble(0x55) == 2) {
        GameFlag_SetNibble(0x55, 1);
    }
    if (msg->msgId == 0x15) {
        if (msg->field_5 == 0 && GameFlag_GetNibble(0x4B) == 7) {
            GameFlag_SetNibble(0x4B, 0);
        }
        if (gGameSession->at4.loc.stage == 3) {
            return 1;
        }
        if (GameFlag_GetNibble(0x32) != 2) {
            return 0;
        }
    }
    return 1;
}

/// The room's handler for message 0x13F2: plays the stage sound for script
/// events 8 and 13 and answers 0 for every event.
s32 func_dryfield_night_water_tower_8017DA4C(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 8:
            Gp_EnqueueStageSnd6(0x52140008, 0, 0);
            break;
        case 13:
            Gp_EnqueueStageSnd6(0x5214000D, 0, 0);
            break;
    }
    return 0;
}
