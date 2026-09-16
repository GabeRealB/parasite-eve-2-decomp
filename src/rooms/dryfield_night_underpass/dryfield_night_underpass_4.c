#include "common.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/room_common.h"

extern SVECTOR D_dryfield_night_underpass_8017DD20[8];
extern s16     D_dryfield_night_underpass_8017DD60[8];

/// Per-frame effect: draws the glow anchors the current visit lights, one per
/// offset in `D_...DD20` whose `D_...DD60` bitmask contains the visit's bit
/// (`Game_Session->field_4`). The whole effect is skipped unless the room flag
/// (`GameFlag_GetNibble(0x53)`) is clear.
void func_dryfield_night_underpass_8017DC3C(void)
{
    s32      mask;
    s32      i;
    SVECTOR* vec;
    s16*     flags;

    mask = 1 << Game_Session->field_4;
    if (GameFlag_GetNibble(0x53) == 0) {
        i     = 0;
        vec   = D_dryfield_night_underpass_8017DD20;
        flags = D_dryfield_night_underpass_8017DD60;
        do {
            if (mask & *flags) {
                Room_Draw17(vec, 0, 0x280);
            }
            vec++;
            i++;
            flags++;
        } while (i < 8);
    }
}
