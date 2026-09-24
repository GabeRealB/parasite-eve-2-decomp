#include "common.h"

#include "main/session.h"
#include "rooms/dryfield_night_g_r_kitchen.h"

extern SVECTOR D_dryfield_night_g_r_kitchen_8017E27C[];
extern SVECTOR D_dryfield_night_g_r_kitchen_8017E29C[];

/// Picks the pair of light shafts `func_dryfield_night_g_r_kitchen_8017D9FC`
/// draws from the current view index (`gGameSession->at4.loc.view`, 2 or 3);
/// any other view draws nothing.
void func_dryfield_night_g_r_kitchen_8017E1E4(void)
{
    u8 view;

    view = gGameSession->at4.loc.view;
    if (view == 2) {
        func_dryfield_night_g_r_kitchen_8017D9FC(&D_dryfield_night_g_r_kitchen_8017E27C[0], 0x100);
        func_dryfield_night_g_r_kitchen_8017D9FC(&D_dryfield_night_g_r_kitchen_8017E27C[2], 0x100);
    } else if (view == 3) {
        func_dryfield_night_g_r_kitchen_8017D9FC(&D_dryfield_night_g_r_kitchen_8017E29C[0], 0x100);
        func_dryfield_night_g_r_kitchen_8017D9FC(&D_dryfield_night_g_r_kitchen_8017E29C[2], 0x100);
    }
}
