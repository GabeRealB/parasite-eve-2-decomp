#include "common.h"

#include "main/session.h"

#include "rooms/room_common.h"

extern SVECTOR D_dryfield_night_toilet_8017DAA0[];
extern SVECTOR D_dryfield_night_toilet_8017DAA8[];

/// Toilet room draw: the room phase `gGameSession->loc.view` selects picks one
/// of the room's two point sets - phase 4 draws the second, phases 5 and 9 the
/// first.
void func_dryfield_night_toilet_8017D9F8(void)
{
    switch (gGameSession->loc.view) {
        case 4:
            Room_Draw20(&D_dryfield_night_toilet_8017DAA8[0], 1, 0x200);
            break;
        case 5:
        case 9:
            Room_Draw20(&D_dryfield_night_toilet_8017DAA0[0], 1, 0x200);
            break;
    }
}
