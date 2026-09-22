#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

s32 RoomsShared8017db08(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u8           variant;

    *out = *in;
    if (in->msgId == 0x19) {
        variant = gGameSession->at4.loc.stage;
        if (variant == 2) {
            if (in->field_5 == 0) {
                if (GameFlag_GetNibble(0x3A) >= 2) {
                    out->field_3 = variant;
                } else {
                    out->field_3 = 1;
                }
            }
        } else if (in->field_5 == 0) {
            out->field_3 = GameFlag_GetNibble(0x61) + 1;
        }
    }
    if (in->msgId == 0x18) {
        if (in->field_5 == 0) {
            if (GameFlag_GetNibble(0x7A) < 4) {
                out->field_3 = 1;
            } else {
                out->field_3 = 2;
            }
        }
        if (in->msgId == 0x18) {
            if (GameFlag_GetNibble(0x4A) != 2) {
                if (in->field_5 != 0) {
                    return 0;
                }
                Gp_StartCapSlot(4, 1, 0);
                Gp_SetNibbleIf(in->field_6, 2);
                return 0;
            }
        }
    }
    if (in->msgId == 0x16) {
        if (GameFlag_GetNibble(0x37) == 0) {
            if (in->field_5 != 0) {
                return 0;
            }
            Gp_SetNibbleIf(in->field_6, 2);
            Gp_RunCapCmd1(0xD);
            return 0;
        }
    }
    if (in->msgId == 0x19) {
        req.field_0 = 0xE;
        req.field_4 = 0xE;
        req.field_8 = 0x52170013;
        req.field_C = 0x52170003;
        req.flagId  = -0x30;
        req.itemId  = 0;
        return RoomsShared8017d638(&req, in);
    }
    return 1;
}
