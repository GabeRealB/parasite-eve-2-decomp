#include "common.h"

#include <psyq/libgte.h>

#include "main/session.h"

#include "rooms/dryfield_night_motel_room_4.h"

/// The room's sprite points for views 2, 3 and 5: three `SVECTOR`s.
extern SVECTOR D_dryfield_night_motel_room_4_8017DA70[];

/// The sprite point view 4 draws. Only its first `SVECTOR` is read here; the
/// words after it are other room data.
extern SVECTOR D_dryfield_night_motel_room_4_8017DA88[];

/// Per-frame effect: queues the room's flickering sprites for the view
/// `gGameSession->at4.loc.view` selects - views 2 and 3 the first two points,
/// view 4 the point at `D_dryfield_night_motel_room_4_8017DA88`, view 5 the
/// first point plus the third, drawn from texture cell 2 with a smaller
/// half-width. Other views draw nothing.
///
/// `jump.c` cross-jumps the trailing sprite calls into one tail here rather
/// than two, because the view-5 group shares its first call with the view-2/3
/// group.
void func_dryfield_night_motel_room_4_8017D990(void)
{
    switch (gGameSession->at4.loc.view) {
        case 2:
        case 3: {
            SVECTOR* p = D_dryfield_night_motel_room_4_8017DA70;
            func_dryfield_night_motel_room_4_8017D714(&p[0], 1, 0x200);
            func_dryfield_night_motel_room_4_8017D714(&p[1], 1, 0x240);
            break;
        }
        case 4: {
            SVECTOR* p = D_dryfield_night_motel_room_4_8017DA88;
            func_dryfield_night_motel_room_4_8017D714(&p[0], 1, 0x200);
            break;
        }
        case 5: {
            SVECTOR* p = D_dryfield_night_motel_room_4_8017DA70;
            func_dryfield_night_motel_room_4_8017D714(&p[0], 1, 0x200);
            func_dryfield_night_motel_room_4_8017D714(&p[2], 2, 0x180);
            break;
        }
    }
}
