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

/// Volume last asked of the back street's ambience, or 0 when none is playing.
/// Written by `func_dryfield_back_street_8017D5D0` and cleared by state 0 of the
/// same task.
extern s32 D_dryfield_back_street_80181054;

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

/// Back street ambience: state 0 clears the recorded volume and advances, state
/// 1 maps the current camera view to a target volume and stereo pan - 0x1E/+4,
/// 0x32/-8 and 0x64/-0xC for views 3/4/5, 0 and centre elsewhere - and, whenever
/// the volume differs from the recorded one, enqueues the matching fade event:
/// type 6 to start the track, type 7 to stop it, type A to retune it, then
/// records the new volume.
void func_dryfield_back_street_8017D5D0(Task* task)
{
    s32 vol;
    s32 pan;

    switch (task->state) {
        case 0:
            D_dryfield_back_street_80181054 = 0;
            task->state                     = task->state + 1;
            return;
        case 1:
            break;
        default:
            return;
    }

    switch (Gp_GetViewIndex()) {
        case 3:
            vol = 0x1E;
            pan = 4;
            break;
        case 4:
            vol = 0x32;
            pan = -8;
            break;
        case 5:
            vol = 0x64;
            pan = -0xC;
            break;
        default:
            pan = 0;
            vol = 0;
            break;
    }

    if (vol == D_dryfield_back_street_80181054) {
        return;
    }
    if (D_dryfield_back_street_80181054 == 0) {
        SndEvt_EnqueueType6(0x52050006, pan, (s8)(((0x64 - vol) * 0x7F) / 100));
    } else if (vol == 0) {
        SndEvt_EnqueueType7(0x52050006, 0x1E);
    } else {
        SndEvt_EnqueueTypeA(0x52050006, pan, (s8)(((0x64 - vol) * 0x7F) / 100));
    }
    D_dryfield_back_street_80181054 = vol;
}

/// Message gate for the room's hotspot. It copies the incoming record to the
/// outgoing one and writes the answer the caller acts on to the copy's
/// `field_3`, returning 0 when the message was consumed and 1 when it was not.
///
/// The copy is the `RoomEventMsg` assignment; the rest is two independent id
/// checks. While the session is in the room (`gGameSession->at4.loc.stage` is 2), a
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
    s1   = gGameSession->at4.loc.stage;
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

            if (gGameSession->at4.loc.stage == 2) {
                cmd = 2;
            }
            Gp_RunCapCmd1(cmd);
            Gp_SetNibbleIf(in->field_6, 2);
        }
        return 0;
    }
    if (in->field_5 == 0) {
        if (gGameSession->at4.loc.stage == 2) {
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
