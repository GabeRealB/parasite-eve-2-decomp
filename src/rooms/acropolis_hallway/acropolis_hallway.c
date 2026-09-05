#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"

#include "rooms/room_common.h"

/// Message gate for the hallway's first hotspot: copies the incoming record to
/// the outgoing one, then edits the copy's `field_3` (the answer the caller
/// acts on) according to the message id and the room's progress nibbles.
/// Returning 0 means the message was consumed.
s32 func_acropolis_hallway_8017D5D0(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u16 msgId;

    *out = *in;
    if (in->msgId == 8) {
        if ((GameFlag_GetNibble(9) & 2) && in->field_5 == 0) {
            out->field_3 = 2;
        }
    }
    if (in->msgId == 4 && in->field_2 == 3 && GameFlag_GetNibble(0) < 3) {
        if (in->field_5 == 0) {
            Gp_RunCapCmd1(1);
        }
        return 0;
    }
    if (in->msgId == 8 && GameFlag_GetNibble(0) == 3) {
        return 1;
    }
    msgId = in->msgId;
    if (msgId == 4 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0) >= 3) {
            out->field_3 = msgId;
        }
        if (GameFlag_GetNibble(0) == 2) {
            out->field_3 = 3;
        }
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_hallway/acropolis_hallway", D_acropolis_hallway_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_hallway/acropolis_hallway", RoomsShared8017d878Table);
