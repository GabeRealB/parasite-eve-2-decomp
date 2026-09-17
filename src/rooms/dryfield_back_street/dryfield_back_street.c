#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern GpMsgEntry D_dryfield_back_street_8017F964[];
extern TaskDesc   D_dryfield_back_street_8017F98C[];

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

INCLUDE_ASM("rooms/nonmatchings/dryfield_back_street/dryfield_back_street", func_dryfield_back_street_8017D5D0);

/// Message gate for the room's hotspot. It copies the incoming record to the
/// outgoing one and writes the answer the caller acts on to the copy's
/// `field_3`, returning 0 when the message was consumed and 1 when it was not.
///
/// The copy is the `RoomEventMsg` assignment; the rest is two independent id
/// checks. While the session is in the room (`Game_Session->field_7` is 2), a
/// type-7 record with no sub-id answers 1, or the session's own value when flag
/// nibble 0x3C is set. A type-9 record with flag nibble 0x3F clear runs CAP
/// command 9 -- or 2 while the session is in the room -- and arms nibble 2 of
/// the record's flag index, both only when the sub-id is clear; everything else
/// is left to the caller and answers 1, ringing the type-7 event while the
/// session is in the room.
s32 func_dryfield_back_street_8017D748(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 s1;

    *out = *in;
    s1   = Game_Session->field_7;
    if (s1 == 2) {
        if (in->msgId == 7) {
            if (in->field_5 == 0) {
                if (GameFlag_GetNibble(0x3C) == 0) {
                    out->field_3 = 1;
                } else {
                    out->field_3 = s1;
                }
            }
        }
    }
    if ((in->msgId == 9) && (GameFlag_GetNibble(0x3F) == 0)) {
        if (in->field_5 == 0) {
            s32 cmd = 9;

            if (Game_Session->field_7 == 2) {
                cmd = 2;
            }
            Gp_RunCapCmd1(cmd);
            Gp_SetNibbleIf(in->field_6, 2);
        }
        return 0;
    }
    if (in->field_5 == 0) {
        if (Game_Session->field_7 == 2) {
            SndEvt_EnqueueType7(0x52050006, 0xF);
        }
    }
    return 1;
}

s32 func_dryfield_back_street_8017D89C(void)
{
    return 0;
}

s32 func_dryfield_back_street_8017D8A4(void)
{
    return 0;
}

s32 func_dryfield_back_street_8017D8AC(void)
{
    return 0;
}

void func_dryfield_back_street_8017D8B4(Task* arg0)
{
    arg0->field_24 = D_dryfield_back_street_8017F964;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_dryfield_back_street_8017F98C, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_dryfield_back_street_8017D910(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_back_street/dryfield_back_street", D_dryfield_back_street_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_back_street/dryfield_back_street", RoomsShared8017d878Table);
