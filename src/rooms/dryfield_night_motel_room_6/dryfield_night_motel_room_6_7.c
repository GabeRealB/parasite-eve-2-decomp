#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "rooms/dryfield_night_motel_room_6.h"
#include "rooms/room_common.h"

/// World position the room's highlight marker is drawn at.
extern SVECTOR D_dryfield_night_motel_room_6_80182EF8[];

/// Draws the room's highlight for the current camera view: the diamond marker
/// in views 3 and 4, the glow disc in view 12, nothing otherwise.
void func_dryfield_night_motel_room_6_80182AE0(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 3:
        case 4:
            func_dryfield_night_motel_room_6_80181CD8(&D_dryfield_night_motel_room_6_80182EF8[0], 0x60, 0x60);
            break;
        case 12:
            func_dryfield_night_motel_room_6_80182158(&D_dryfield_night_motel_room_6_80182EF8[0], 0x60, 0x80);
            break;
    }
}
