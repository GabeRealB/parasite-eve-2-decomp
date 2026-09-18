#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/room_common.h"

extern s32 D_dryfield_night_junk_yard_801805A4;

/// Message gate for the junk yard's hotspot. Copies the incoming record to the
/// outgoing one, then edits the copy according to the message id and the game's
/// progress nibbles.
///
/// Message 0x18 first picks the copy's `field_3` answer from nibble 0x7A (2
/// once it has reached 4, else 1). Message 0x1B, while nibble 0x46 is 1, runs
/// the CAP command 4 and arms the nibble in `field_6`, consuming the message
/// (returns 0); otherwise, with nibble 0x64 still clear, it answers 2 and
/// latches that nibble. It then walks the 0x73 / 0x61 / 0x8F chain - only while
/// 0x73 is 1, 0x61 agrees with it and 0x8F is clear - and answers 3 when nibble
/// 0x7A has reached it, latching 0x8F.
///
/// `field_5` non-zero means "report only", which suppresses every side effect.
s32 func_dryfield_night_junk_yard_8017D6AC(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventMsg unused;
    s32          state;
    s32          value;

    if (in->msgId == 0x18 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0x7A) >= 4) {
            out->field_3 = 2;
        } else {
            out->field_3 = 1;
        }
    }
    *out = *in;
    if (in->msgId != 0x1B) {
        return 1;
    }
    if (GameFlag_GetNibble(0x46) == 1) {
        if (in->field_5 != 0) {
            return 0;
        }
        Gp_RunCapCmd1(4);
        Gp_SetNibbleIf(in->field_6, 2);
        return 0;
    }
    if (GameFlag_GetNibble(0x64) == 0 && in->field_5 == 0) {
        out->field_2 = 2;
        GameFlag_SetNibble(0x64, 1);
    }
    state = GameFlag_GetNibble(0x73);
    if (state != 1) {
        return 1;
    }
    if (GameFlag_GetNibble(0x61) != state) {
        return 1;
    }
    if (GameFlag_GetNibble(0x8F) != 0) {
        return 1;
    }
    value = GameFlag_GetNibble(0x7A);
    if (value != 3) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 1;
    }
    out->field_2 = value;
    GameFlag_SetNibble(0x8F, 1);
    return 1;
}

s32 func_dryfield_night_junk_yard_8017D82C(s32 arg0, s32 arg1, RoomEventMsg* in)
{
    if ((in->field_2 == 3) && (gGameSession->loc.place == 1) && (GameFlag_GetNibble(0x9F) == 0)) {
        GameFlag_SetNibble(0x9F, 1);
        func_800E8614((s32)&D_dryfield_night_junk_yard_801805A4, 0);
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_junk_yard/dryfield_night_junk_yard", D_dryfield_night_junk_yard_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_junk_yard/dryfield_night_junk_yard", RoomsShared8017d878Table);
