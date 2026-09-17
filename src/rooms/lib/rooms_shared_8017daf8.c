#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"
#include "rooms/rooms_shared_8017daf8.h"

s32 RoomsShared8017daf8(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
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
        ret         = RoomsShared8017d638(&req, msg);
        if (ret == 0) {
            ret = 2;
        }
        if (RoomsShared8017d638Flag != 0) {
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
        if (Game_Session->field_7 == 3) {
            return 1;
        }
        if (GameFlag_GetNibble(0x32) != 2) {
            return 0;
        }
    }
    return 1;
}
