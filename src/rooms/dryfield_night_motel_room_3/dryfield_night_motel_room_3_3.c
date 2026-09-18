#include "common.h"

#include <psyq/libgte.h>

#include "main/session.h"
#include "rooms/room_common.h"

/// The night motel room's drawable points, one 8-byte `SVECTOR` per disc. Each
/// is reached by its own address, so the compiler materialises it whole into
/// `$a0` rather than indexing one base. The room's script table starts
/// immediately after the third.
extern SVECTOR D_dryfield_night_motel_room_3_8017DA84;
extern SVECTOR D_dryfield_night_motel_room_3_8017DA8C;
extern SVECTOR D_dryfield_night_motel_room_3_8017DA94;

/// Per-frame effect: queues the room's glowing discs for the visit
/// `gGameSession->field_4` selects - visit 3 the second point, visit 4 the
/// second point then the first, visits 10 and 11 the first alone, visit 8 the
/// third with a wider UV column and half-extent. Visits outside those draw
/// nothing.
void func_dryfield_night_motel_room_3_8017D9B4(void)
{
    switch (gGameSession->field_4) {
        case 3:
            Room_Draw20(&D_dryfield_night_motel_room_3_8017DA8C, 1, 0x240);
            break;
        case 4:
            Room_Draw20(&D_dryfield_night_motel_room_3_8017DA8C, 1, 0x240);
            /* fallthrough */
        case 10:
        case 11:
            Room_Draw20(&D_dryfield_night_motel_room_3_8017DA84, 1, 0x200);
            break;
        case 8:
            Room_Draw20(&D_dryfield_night_motel_room_3_8017DA94, 2, 0x180);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_room_3/dryfield_night_motel_room_3_3", D_dryfield_night_motel_room_3_8017D5F4);
