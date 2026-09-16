#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"

#include "rooms/rooms_shared_8017d638.h"
#include "rooms/rooms_shared_8017dca4.h"

s32 RoomsShared8017dca4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u16          msgId;

    *out  = *in;
    msgId = in->msgId;
    if (msgId == 0xF) {
        if (in->field_5 == 0) {
            out->field_3 = GameFlag_GetNibble(0x61) + 1;
        }
        if (in->msgId == msgId) {
            req.field_0 = 2;
            req.field_4 = 2;
            req.field_8 = Gp_PackStageSndId(0x52120005);
            req.field_C = Gp_PackStageSndId(0x52120003);
            req.flagId  = 0x35;
            req.itemId  = 0;
            return RoomsShared8017d638(&req, in);
        }
    }
    return 1;
}
