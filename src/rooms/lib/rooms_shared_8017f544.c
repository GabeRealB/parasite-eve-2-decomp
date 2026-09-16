#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"

#include "rooms/room_common.h"

/// Handler for slot-7 msg `0x13EE`, the destination-location request: copies
/// the incoming record to the outgoing one and answers with the room variant to
/// use in `field_3`. Message 2 picks 3, or nibble `0x61` plus one while nibble
/// `0x7A` is still below 4; message 3 runs CAP command 0x15 and returns 2 when
/// the session already sits on it, otherwise CAP command 7 plus a nibble write
/// and returns 0 unless nibble `0x3B` is set. `field_5` suppresses the side
/// effects (the handler only reports what *would* happen). Any other message
/// runs CAP command 8 once nibble `0x45` reads 1 and is otherwise not consumed.
s32 RoomsShared8017f544(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 n;
    s32 val;

    *out = *in;
    if (in->msgId == 2 && in->field_5 == 0) {
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
    if (in->msgId == 3) {
        if ((Game_Session->field_7 == in->msgId) && (Game_Session->field_9 == 1) &&
            (Gp_StateF0.field_0 == Game_Session->field_9)) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(0x15);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0x3B) == 0) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(7);
                Gp_SetNibbleIf(in->field_6, 2);
            }
            return 0;
        }
    }
    if (in->msgId == 2) {
        if (GameFlag_GetNibble(0x45) == 1) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(8);
            }
            return 0;
        }
    }
    return 1;
}
