#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"

#include "rooms/room_common.h"

/// Gas station message handler, for slot-7 msg `0x13EE`, the destination-location
/// request, shared by the day and night gas station rooms. Every call copies the
/// incoming record onto the outgoing one first, so a handler that does not
/// consume the message still hands back the request unchanged, and answers with
/// the room variant to use in `field_3`.
///
/// Message 2 (the gas station door) reports how far the story has progressed
/// through the station: while nibble `0x7A` is still below 4 the answer is
/// nibble `0x61` plus one, otherwise the flat value 3. Message 2 is also
/// re-tested after the message-3 block, where 0 is returned when nibble `0x45`
/// reads 1 and the request is not report-only (`field_5 == 0`), running cap
/// command 8 first.
///
/// Message 3 asks a background cutscene to advance: it is answered with 2 and
/// cap command 0x15 only while the session is on this very message
/// (`gGameSession::field_7 == msgId`) in play mode (`field_9 == 1`) with
/// `Gp_StateF0.field_0` in the same state. Otherwise nibble `0x3B` decides
/// between returning 0 - after cap command 7 plus a nibble write, which re-arms
/// nibble `field_6` - and falling through to the message-2 test. `field_5`
/// suppresses the side effects throughout, so the handler only reports what
/// *would* happen, and 1 means the room script did not consume the message.
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
        if ((gGameSession->loc.stage == in->msgId) && (gGameSession->loc.place == 1) &&
            (Gp_StateF0.field_0 == gGameSession->loc.place)) {
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
