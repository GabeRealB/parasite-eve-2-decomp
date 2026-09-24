#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "rooms/dryfield_night_saloon_g_r.h"

/// Handler for message 0x13EE in the room's message table, which filters a
/// warp request: copies `in` to `out`, and for area 0xF picks the destination
/// room from game-flag nibble 0x61 (unless `in->field_5` asks for a dry run),
/// then passes the warp through the event gate with the room's own request -
/// nibble 0x35, no item, cap command 2 and two stage sound ids. Any other area
/// answers 1.
s32 func_dryfield_night_saloon_g_r_8017DCA4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
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
            return func_dryfield_night_saloon_g_r_8017D8A0(&req, in);
        }
    }
    return 1;
}
