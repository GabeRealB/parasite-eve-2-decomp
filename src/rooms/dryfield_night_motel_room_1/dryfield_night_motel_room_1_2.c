#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"

#include "rooms/dryfield_night_motel_room_1.h"
#include "rooms/room_common.h"

/// The night motel room's drawable points, one 8-byte `SVECTOR` per disc.
extern SVECTOR D_dryfield_night_motel_room_1_8017DA54[];
extern SVECTOR D_dryfield_night_motel_room_1_8017DA5C[];

/// Night motel room 1 draw: queues one of the room's two glowing discs for the
/// camera `Gp_GetViewIndex` selects - views 2, 3, 8 and 9 the disc at
/// `D_...DA54` (texture cell 1, half-extent 0x200), 5 and 6 the one at
/// `D_...DA5C` (cell 2, half-extent 0x180). Views outside the 2..9 span draw
/// nothing. `jump.c` cross-jumps the two trailing sprite calls into one tail.
void func_dryfield_night_motel_room_1_8017D9B0(void)
{
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
        case 3:
        case 8:
        case 9:
            func_dryfield_night_motel_room_1_8017D734(&D_dryfield_night_motel_room_1_8017DA54[0], 1, 0x200);
            break;
        case 5:
        case 6:
            func_dryfield_night_motel_room_1_8017D734(&D_dryfield_night_motel_room_1_8017DA5C[0], 2, 0x180);
            break;
    }
}
