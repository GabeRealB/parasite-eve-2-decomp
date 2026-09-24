#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "rooms/dryfield_motel_room_6.h"
#include "rooms/room_common.h"

extern SVECTOR D_dryfield_motel_room_6_80182D90[];

void func_dryfield_motel_room_6_80182978(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 3:
        case 4:
            func_dryfield_motel_room_6_80181B70(&D_dryfield_motel_room_6_80182D90[0], 0x60, 0x60);
            break;
        case 12:
            func_dryfield_motel_room_6_80181FF0(&D_dryfield_motel_room_6_80182D90[0], 0x60, 0x80);
            break;
    }
}
