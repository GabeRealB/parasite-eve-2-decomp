#include "common.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/room_common.h"

/// Handler for message 0x13EE in the room's message table. It copies the
/// incoming record to `out` and, unless `in->field_5` is set, answers two
/// queries in `out->field_3`:
///
/// - 0x19: while the session's stage is 2, 2 once progress nibble 0x3A has
///   reached 2 and 1 before; in any other stage, nibble 0x61 plus one.
/// - 0x26: with nibble 0xC9 set, 2 or 1 by nibble 0x53, plus 2 while nibble
///   0x51 is clear; with 0xC9 clear, 5 or 6 by whether nibble 0x51 is set.
///
/// Always returns 1.
s32 func_dryfield_water_hole_8017D5F0(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp;

    *out = *in;
    if (in->msgId == 0x19) {
        temp = gGameSession->at4.loc.stage;
        if (temp == 2) {
            if (in->field_5 == 0) {
                if (GameFlag_GetNibble(0x3A) >= 2) {
                    out->field_3 = temp;
                } else {
                    out->field_3 = 1;
                }
            }
        } else if (in->field_5 == 0) {
            out->field_3 = GameFlag_GetNibble(0x61) + 1;
        }
    }
    if (in->msgId == 0x26 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) != 0) {
                out->field_3 = 2;
            } else {
                out->field_3 = 1;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                out->field_3 += 2;
            }
        } else {
            if (GameFlag_GetNibble(0x51) != 0) {
                out->field_3 = 5;
            } else {
                out->field_3 = 6;
            }
        }
    }
    return 1;
}
