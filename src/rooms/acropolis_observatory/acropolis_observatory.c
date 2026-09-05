#include "common.h"

#include "main/fs.h"
#include "main/gameflag.h"

#include "rooms/room_common.h"

extern s16 D_8007107A;

/// Message gate for the observatory's two hotspots: copies the incoming record
/// to the outgoing one, then edits the copy according to the message id and the
/// game's progress nibbles.
///
/// Message 9 (the telescope) and message 0xB (the door) both answer with a
/// `field_2` refusal code — 5 and 1 respectively — while the disc has no stream
/// file open (`D_8007107A < 0 || D_8006AC30.sector == 0`) or the message's
/// nibble is not in the state that lets it run once. The first pass through
/// each also advances that nibble, so the refusal only shows on later visits.
/// `field_5` non-zero means "report only", which suppresses both the nibble
/// writes and the refusals.
s32 func_acropolis_observatory_8017D618(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 answer;

    *out = *in;
    if (in->msgId == 9 && in->field_5 == 0) {
        if (D_8007107A < 0 || D_8006AC30.sector == 0) {
            out->field_2 = 5;
        }
        if (GameFlag_GetNibble(0x25) == 0) {
            GameFlag_SetNibble(0x25, 1);
        } else {
            out->field_2 = 5;
        }
        if (in->msgId == 9) {
            if (GameFlag_GetNibble(9) & 1) {
                out->field_3 = 2;
            }
        }
    }
    if (in->msgId == 0xB) {
        if (D_8007107A < 0 || D_8006AC30.sector == 0) {
            if (in->field_5 == 0) {
                out->field_2 = 1;
            }
        }
        if (in->field_5 == 0) {
            if (GameFlag_GetNibble(1) == 3) {
                GameFlag_SetNibble(1, 4);
            } else {
                out->field_2 = 1;
            }
        }
        if (in->msgId == 0xB && in->field_5 == 0) {
            answer = GameFlag_GetNibble(2);
            if (answer == 0) {
                answer = 1;
            } else {
                answer = 2;
            }
            out->field_3 = answer;
        }
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_observatory/acropolis_observatory", D_acropolis_observatory_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_observatory/acropolis_observatory", RoomsShared8017d878Table);
