#include "common.h"

#include <psyq/libgte.h>

#include "main/session.h"
#include "rooms/room_common.h"

/// The night motel room's drawable points, one 8-byte `SVECTOR` per disc.
extern SVECTOR D_dryfield_night_motel_room_5_8017DA58[1];
extern SVECTOR D_dryfield_night_motel_room_5_8017DA60[1];
extern SVECTOR D_dryfield_night_motel_room_5_8017DA68[1];

/// Night motel room 5 draw: queues one of the room's three glowing discs for
/// the view the session byte `gGameSession->at4.loc.view` names - views 3 and 8 the
/// disc at `D_...DA58` with half-extent 0x200, 2 and 7 the one at `D_...DA60`
/// with 0x240, 4 and 9 the one at `D_...DA68` with 0x200. Views outside the 2..9
/// span draw nothing. `jump.c` cross-jumps the 3/8 call into the 4/9 one, whose
/// `jal` the 2/7 call also redirects to.
void func_dryfield_night_motel_room_5_8017D9A4(void)
{
    switch (gGameSession->at4.loc.view) {
        case 3:
        case 8:
            Room_Draw20(&D_dryfield_night_motel_room_5_8017DA58[0], 1, 0x200);
            break;
        case 2:
        case 7:
            Room_Draw20(&D_dryfield_night_motel_room_5_8017DA60[0], 1, 0x240);
            break;
        case 4:
        case 9:
            Room_Draw20(&D_dryfield_night_motel_room_5_8017DA68[0], 1, 0x200);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_room_5/dryfield_night_motel_room_5_3", D_dryfield_night_motel_room_5_8017D5F0);
