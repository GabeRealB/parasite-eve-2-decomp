#include "common.h"

#include <psyq/libgte.h>

#include "main/session.h"
#include "rooms/room_common.h"

/// The night motel room's drawable points, one 8-byte `SVECTOR` per disc. The
/// visit the pair belongs to is `gGameSession->field_4`.
extern SVECTOR D_dryfield_night_motel_room_2_8017DA44[];
extern SVECTOR D_dryfield_night_motel_room_2_8017DA54[];

/// Night motel room 2 draw: queues the room's glowing discs for the visit
/// `gGameSession->field_4` selects - visits 2 and 3 a pair at one point,
/// 5 and 6 a single one at another. Visits outside those ranges draw nothing.
/// `jump.c` cross-jumps the two trailing `Room_Draw20` calls into one tail.
void func_dryfield_night_motel_room_2_8017D990(void)
{
    switch (gGameSession->field_4) {
        case 2:
        case 3: {
            SVECTOR* p = D_dryfield_night_motel_room_2_8017DA44;
            Room_Draw20(&p[0], 1, 0x200);
            Room_Draw20(&p[1], 1, 0x240);
            break;
        }
        case 5:
        case 6: {
            SVECTOR* p = D_dryfield_night_motel_room_2_8017DA54;
            Room_Draw20(&p[0], 2, 0x180);
            break;
        }
    }
}
