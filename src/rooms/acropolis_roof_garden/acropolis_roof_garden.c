#include "common.h"

#include "gameplay/268.h"
#include "main/gameflag.h"

#include "rooms/room_common.h"

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden", func_acropolis_roof_garden_8017D5D4);

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
