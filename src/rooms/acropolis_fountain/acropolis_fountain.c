#include "common.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern TaskDesc D_acropolis_fountain_8017E78C;
extern u8       D_acropolis_fountain_80183BB0;
extern u8       D_acropolis_fountain_80183BB1;

/// Message gate for the fountain's hotspot: copies the incoming record to the
/// outgoing one, then edits the copy's `field_3` (the answer the caller acts
/// on) according to the message id and the room's progress nibbles. Message 3
/// before nibble 0 reaches 5 hands the record's first two bytes to
/// `D_acropolis_fountain_80183BB0`/`BB1` and spawns the room's own task,
/// consuming the message (returns 0); from nibble 0 == 5 on it only answers.
s32 func_acropolis_fountain_8017D604(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 msgId;

    *out  = *in;
    msgId = in->msgId;
    if (msgId == 3) {
        if (GameFlag_GetNibble(0) < 5) {
            if (in->field_5 == 0) {
                D_acropolis_fountain_80183BB0 = in->field_2;
                D_acropolis_fountain_80183BB1 = in->field_3;
                Task_SpawnFromTable(&D_acropolis_fountain_8017E78C, 0, 0, 0);
            }
            return 0;
        }
        if (in->msgId == msgId && in->field_5 == 0) {
            if (GameFlag_GetNibble(0) < 2) {
                if (GameFlag_GetNibble(0x21) < 2) {
                    out->field_3 = 1;
                } else {
                    out->field_3 = 2;
                }
            } else {
                out->field_3 = msgId;
            }
        }
    } else if (msgId == 9) {
        if (GameFlag_GetNibble(9) & 1) {
            out->field_3 = 2;
        }
        if (in->field_5 == 0 && GameFlag_GetNibble(0x13) == 0) {
            GameFlag_SetNibble(0x13, 1);
        }
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_fountain/acropolis_fountain", D_acropolis_fountain_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_fountain/acropolis_fountain", D_acropolis_fountain_8017D5C4);
