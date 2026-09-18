#include "common.h"

#include <psyq/libgte.h>

#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/room_common.h"

/// The cellar's per-visit glow anchors, projected by
/// `func_dryfield_night_cellar_8017DA28` with `Room_Draw17` (UV column 1,
/// half-extent 0x280). Visit 2 lights the pair at the first address and visit
/// 3 the pair at the second; the two anchors of a pair differ in x and in z
/// respectively.
extern SVECTOR D_dryfield_night_cellar_8017DAD0[];
extern SVECTOR D_dryfield_night_cellar_8017DAE0[];

/// Per-frame effect: once the story flag at nibble 0x52 is set, draws the two
/// glow anchors belonging to the visit the stage-visit byte
/// `gGameSession->field_4` records. Every other visit draws nothing.
void func_dryfield_night_cellar_8017DA28(void)
{
    u8 visit;

    if (GameFlag_GetNibble(0x52) == 1) {
        visit = gGameSession->field_4;
        if (visit == 2) {
            Room_Draw17(&D_dryfield_night_cellar_8017DAD0[0], 1, 0x280);
            Room_Draw17(&D_dryfield_night_cellar_8017DAD0[1], 1, 0x280);
        } else if (visit == 3) {
            Room_Draw17(&D_dryfield_night_cellar_8017DAE0[0], 1, 0x280);
            Room_Draw17(&D_dryfield_night_cellar_8017DAE0[1], 1, 0x280);
        }
    }
}
