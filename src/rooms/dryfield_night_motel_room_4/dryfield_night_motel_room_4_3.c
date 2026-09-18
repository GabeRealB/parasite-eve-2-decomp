#include "common.h"

#include <psyq/libgte.h>

#include "main/session.h"
#include "rooms/room_common.h"

/// The night motel room's drawable points, one 8-byte `SVECTOR` per disc: the
/// three discs the visit byte below indexes for visits 2, 3 and 5. The visit
/// each point belongs to is `gGameSession->loc.view`.
extern SVECTOR D_dryfield_night_motel_room_4_8017DA70[];

/// The disc only visit 4 draws, written next to the run above. Only its first
/// `SVECTOR` is a draw anchor: the words after it are the room's own script
/// table, which this room's `func_dryfield_night_motel_room_4_8017D660` and
/// `D_dryfield_night_motel_room_4_801802A8` entries reach.
extern SVECTOR D_dryfield_night_motel_room_4_8017DA88[];

/// Per-frame effect: queues the room's glowing discs for the visit
/// `gGameSession->loc.view` selects - visits 2 and 3 the pair at the first two
/// points, visit 4 the single point at the second address, visit 5 the first
/// point again plus a third with a wider UV column and half-extent. Visits
/// outside those draw nothing.
///
/// Sibling of `func_dryfield_night_motel_room_2_8017D990`, the same body over a
/// two-visit case set. `jump.c` cross-jumps the three trailing `Room_Draw20`
/// calls into one tail here rather than two, because the visit-5 group shares
/// its first call with the visit-2/3 group.
void func_dryfield_night_motel_room_4_8017D990(void)
{
    switch (gGameSession->loc.view) {
        case 2:
        case 3: {
            SVECTOR* p = D_dryfield_night_motel_room_4_8017DA70;
            Room_Draw20(&p[0], 1, 0x200);
            Room_Draw20(&p[1], 1, 0x240);
            break;
        }
        case 4: {
            SVECTOR* p = D_dryfield_night_motel_room_4_8017DA88;
            Room_Draw20(&p[0], 1, 0x200);
            break;
        }
        case 5: {
            SVECTOR* p = D_dryfield_night_motel_room_4_8017DA70;
            Room_Draw20(&p[0], 1, 0x200);
            Room_Draw20(&p[2], 2, 0x180);
            break;
        }
    }
}
