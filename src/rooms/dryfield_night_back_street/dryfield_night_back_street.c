#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/room_common.h"

/// Message handler for the back street's two events. Copies the incoming
/// record to the outgoing one and answers by editing `field_3` of the copy; a
/// non-zero `field_5` suppresses the side effects, as for every handler.
///
/// The response byte is the session's stage (`Game_Session.field_7`), read once
/// into a local and reused: the stage-2-only message 7 keeps that byte when
/// event nibble 0x3C is set and answers 1 when it is clear.
///
/// Message 9 is the room's progress gate -- with nibble 0x3F clear it runs CAP
/// command 2 on stage 2 (9 otherwise), writes the record's nibble and answers
/// 0. Any other message plays the "refused" sound when the session is on stage
/// 2 and answers 1.
///
/// The stage load is scheduled above the prologue, so this function's `.text`
/// starts 8 bytes before its `addiu $sp` - the `text` cut in the manifest.
s32 func_dryfield_night_back_street_8017D5D0(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    GameSession* session = Game_Session;
    u8           response;

    *out     = *in;
    response = session->field_7;
    if (response == 2) {
        if (in->msgId == 7 && in->field_5 == 0) {
            if (GameFlag_GetNibble(0x3C) == 0) {
                out->field_3 = 1;
            } else {
                out->field_3 = response;
            }
        }
    }
    if (in->msgId == 9 && GameFlag_GetNibble(0x3F) == 0) {
        if (in->field_5 == 0) {
            Gp_RunCapCmd1(Game_Session->field_7 == 2 ? 2 : 9);
            Gp_SetNibbleIf(in->field_6, 2);
        }
        return 0;
    }
    if (in->field_5 == 0 && Game_Session->field_7 == 2) {
        SndEvt_EnqueueType7(0x52050006, 0xF);
    }
    return 1;
}

s32 func_dryfield_night_back_street_8017D724(void)
{
    return 0;
}

s32 func_dryfield_night_back_street_8017D72C(void)
{
    return 0;
}

s32 func_dryfield_night_back_street_8017D734(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_back_street/dryfield_night_back_street", D_dryfield_night_back_street_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_back_street/dryfield_night_back_street", RoomsShared8017d878Table);
