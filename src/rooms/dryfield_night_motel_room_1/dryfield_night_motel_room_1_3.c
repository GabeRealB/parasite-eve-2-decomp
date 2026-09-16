#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "rooms/room_common.h"

/// The night motel room's drawable points, one 8-byte `SVECTOR` per disc.
extern SVECTOR D_dryfield_night_motel_room_1_8017DA54[];
extern SVECTOR D_dryfield_night_motel_room_1_8017DA5C[];

/// Night motel room 1 draw: queues the room's glowing discs for the camera
/// `Gp_GetViewIndex` selects - views 2, 3, 8 and 9 the pair at one point, 5 and
/// 6 the single one at another. Views outside the 2..9 span draw nothing.
/// `jump.c` cross-jumps the two trailing `Room_Draw20` calls into one tail.
void func_dryfield_night_motel_room_1_8017D9B0(void)
{
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
        case 3:
        case 8:
        case 9:
            Room_Draw20(&D_dryfield_night_motel_room_1_8017DA54[0], 1, 0x200);
            break;
        case 5:
        case 6:
            Room_Draw20(&D_dryfield_night_motel_room_1_8017DA5C[0], 2, 0x180);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_room_1/dryfield_night_motel_room_1_3", D_dryfield_night_motel_room_1_8017D5F0);
