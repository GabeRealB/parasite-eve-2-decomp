#include "common.h"

#include "gameplay/268.h"
#include "main/gameflag.h"
#include "main/sound.h"

#include "rooms/room_common.h"

extern s32 D_acropolis_roof_garden_80186E94;

/// Keeps the roof garden's ambience (sound id 0x510D0005) in step with the
/// session's weather/time state: state 5 plays it at 0x1E, state 7 at full
/// 0x64 and anything else silences it. `D_acropolis_roof_garden_80186E94`
/// latches the volume currently playing, so the task only talks to the sound
/// driver on a change - starting the loop, fading it out, or ramping it to the
/// new level. The driver wants attenuation rather than volume, hence the
/// `(0x64 - vol) * 127 / 100` conversion.
void func_acropolis_roof_garden_8017D5D4(Task* task)
{
    s32 vol;
    u8  state;
    s32 prev;

    switch (task->state) {
        case 0:
            D_acropolis_roof_garden_80186E94 = 0;
            task->state                      = task->state + 1;
            return;
        case 1:
            break;
        default:
            return;
    }

    state = (u8)Game_Session->field_4;
    if (state != 5) {
        vol = 0;
        if (state == 7) {
            vol = 0x64;
        }
    } else {
        vol = 0x1E;
    }

    prev = D_acropolis_roof_garden_80186E94;
    if (vol == prev) {
        return;
    }
    if (prev == 0) {
        SndEvt_EnqueueType6(0x510D0005, 0, (s8)(((0x64 - vol) * 127) / 100));
    } else if (vol == 0) {
        SndEvt_EnqueueType7(0x510D0005, 0x1E);
    } else {
        SndEvt_EnqueueTypeA(0x510D0005, 0, (s8)(((0x64 - vol) * 127) / 100));
    }
    D_acropolis_roof_garden_80186E94 = vol;
}

/// Message gate for the roof garden's hotspot: copies the incoming record to
/// the outgoing one, then runs the message's one-shot side effect.
///
/// Message 0xC, when not a "report only" query (`field_5 == 0`) and its nibble
/// is still clear, advances nibble 7 to 2 and sets collection bit 0x13 to 2.
/// The copy itself is unedited, so the answer is always "allowed".
s32 func_acropolis_roof_garden_8017D71C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 0xC && in->field_5 == 0 && GameFlag_GetNibble(7) == 0) {
        GameFlag_SetNibble(7, 2);
        Gp_SetCurBit2Flag(0x13, 2);
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden", D_acropolis_roof_garden_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden", D_acropolis_roof_garden_8017D5C4);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden", D_acropolis_roof_garden_8017D5D0);
