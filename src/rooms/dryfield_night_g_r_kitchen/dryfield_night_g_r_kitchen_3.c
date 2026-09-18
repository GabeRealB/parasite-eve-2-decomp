#include "common.h"

#include "main/session.h"
#include "rooms/room_common.h"

extern SVECTOR D_dryfield_night_g_r_kitchen_8017E27C[];
extern SVECTOR D_dryfield_night_g_r_kitchen_8017E29C[];

/// Picks the pair of wedges `Room_Draw08` sweeps from the current view index
/// (`gGameSession->loc.view`, 2 or 3); any other view draws nothing.
void func_dryfield_night_g_r_kitchen_8017E1E4(void)
{
    u8 view;

    view = gGameSession->loc.view;
    if (view == 2) {
        Room_Draw08(&D_dryfield_night_g_r_kitchen_8017E27C[0], 0x100);
        Room_Draw08(&D_dryfield_night_g_r_kitchen_8017E27C[2], 0x100);
    } else if (view == 3) {
        Room_Draw08(&D_dryfield_night_g_r_kitchen_8017E29C[0], 0x100);
        Room_Draw08(&D_dryfield_night_g_r_kitchen_8017E29C[2], 0x100);
    }
}
