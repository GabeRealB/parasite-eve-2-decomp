#include "common.h"

#include "main/session.h"

#include "rooms/dryfield_night_toilet.h"
#include "rooms/room_common.h"

extern SVECTOR D_dryfield_night_toilet_8017DAA0[];
extern SVECTOR D_dryfield_night_toilet_8017DAA8[];

/// Draws the room's glow sprite (texture cell 1, half-extent 0x200) at the
/// point the current camera view (`gGameSession->at4.loc.view`) shows: view 4
/// uses the second point, views 5 and 9 the first, and every other view draws
/// nothing.
void func_dryfield_night_toilet_8017D9F8(void)
{
    switch (gGameSession->at4.loc.view) {
        case 4:
            func_dryfield_night_toilet_8017D77C(&D_dryfield_night_toilet_8017DAA8[0], 1, 0x200);
            break;
        case 5:
        case 9:
            func_dryfield_night_toilet_8017D77C(&D_dryfield_night_toilet_8017DAA0[0], 1, 0x200);
            break;
    }
}
