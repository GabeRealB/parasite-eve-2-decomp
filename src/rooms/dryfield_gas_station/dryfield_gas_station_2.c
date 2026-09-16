#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// Gas station message handler. Every call copies the incoming record onto the
/// outgoing one first, so a handler that does not consume the message still
/// hands back the request unchanged.
///
/// Message 2 (the gas station door) reports how far the story has progressed
/// through the station: nibble 0x7A below 4 answers with nibble 0x61 plus one,
/// otherwise the flat value 3. Message 2 is also re-tested after the message-3
/// block, where the answer 0 is returned when nibble 0x45 reads 1 and the
/// request is not report-only (`field_5 == 0`), running cap command 8 first.
///
/// Message 3 asks a background cutscene to advance. It is answered with 2 and
/// cap command 0x15 only while the session is on this very message
/// (`Game_Session::field_7 == msgId`) in play mode (`field_9 == 1`) with
/// `Gp_StateF0.field_0` in the same state; otherwise nibble 0x3B decides
/// between returning 0 (after cap command 7, which re-arms nibble
/// `field_6`) and falling through to the message-2 test. 1 means the room
/// script did not consume the message.
s32 func_dryfield_gas_station_8017FA20(Task* task, s32 msgId, RoomEventMsg* arg2, RoomEventMsg* arg3)
{
    s32 n;
    s32 v;

    *arg3 = *arg2;
    if ((arg2->msgId == 2) && (arg2->field_5 == 0)) {
        n = GameFlag_GetNibble(0x7A);
        if (n < 4) {
            v = 3;
            TOUCH_REG(v);
            v = GameFlag_GetNibble(0x61) + 1;
        } else {
            v = 3;
        }
        arg3->field_3 = v;
    }
    if (arg2->msgId == 3) {
        if ((Game_Session->field_7 == arg2->msgId) && (Game_Session->field_9 == 1) &&
            (Gp_StateF0.field_0 == Game_Session->field_9)) {
            if (arg2->field_5 == 0) {
                Gp_RunCapCmd1(0x15);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0x3B) == 0) {
            if (arg2->field_5 == 0) {
                Gp_RunCapCmd1(7);
                Gp_SetNibbleIf(arg2->field_6, 2);
            }
            return 0;
        }
    }
    if (arg2->msgId == 2) {
        if (GameFlag_GetNibble(0x45) == 1) {
            if (arg2->field_5 == 0) {
                Gp_RunCapCmd1(8);
            }
            return 0;
        }
    }
    return 1;
}

/// Maps a cap (cutscene) script event key to the stage sound it should play in
/// the gas station, then enqueues it as a type-6 sound event. Event key 0x83
/// only plays if a cap script is still reporting an event key. Keys with no
/// sound are ignored. Always returns 0.
s32 func_dryfield_gas_station_8017FB94(s32 arg0, s32 arg1, s32 arg2)
{
    s32 id;

    switch (arg2) {
        case 5:
            id = 0x52010005;
            goto play;
        case 7:
            id = 0x52010007;
            goto play;
        case 0xA:
            id = 0x5201000A;
            goto play;
        case 0xD:
            id = 0x5201000D;
            goto play;
        case 0x11:
            id = 0x52010011;
            goto play;
        case 0x13:
            id = 0x52010013;
            goto play;
        case 0x6D:
        case 0x82:
            id = 0x5201000B;
            goto play;
        case 0x73:
            id = 0x5201000E;
            goto play;
        case 0x83:
            if (Gp_GetCapEventKey() == 0) {
                break;
            }
            id = 0x52010012;
        play:
            Gp_EnqueueStageSnd6(id, 0, 0);
            break;
    }
    return 0;
}
