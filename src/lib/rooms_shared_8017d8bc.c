#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "rooms/rooms_shared_8017d638.h"
#include "rooms/rooms_shared_8017d8bc.h"

extern s8 D_8007272D;

s32 RoomsShared8017d8bc(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
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
        ret         = RoomsShared8017d638(&req, out);
        if (RoomsShared8017d638Flag != 0) {
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
        ret         = RoomsShared8017d638(&req, out);
        if (RoomsShared8017d638Flag != 0) {
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
        ret         = RoomsShared8017d638(&req, out);
        if (RoomsShared8017d638Flag != 0) {
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
