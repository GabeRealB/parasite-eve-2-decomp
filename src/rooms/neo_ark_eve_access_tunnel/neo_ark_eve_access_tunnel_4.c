#include "common.h"

#include <psyq/libgte.h>

extern s32 Gp_GetViewIndex(void);
void       Room_Draw01(SVECTOR* v, s32 arg1, s32 arg2);

/// Live emitters for the tunnel's views, in the shared data blob at the end of
/// the overlay. `D_..._8017EB48` doubles as case 2's three-entry run and case 6's
/// two-entry one, so both views share one base pointer.
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EAE8[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB08[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB28[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB48[];

INCLUDE_ASM("rooms/nonmatchings/neo_ark_eve_access_tunnel/neo_ark_eve_access_tunnel_4", func_neo_ark_eve_access_tunnel_8017E090);

/// Draws whichever emitters the current view shows: two adjacent positions per
/// drawn wedge, stepping through the view's run. View 4 chains into view 5's
/// emitters (`D_..._8017EB08` then `D_..._8017EB28`); every other view stops at
/// its own.
void func_neo_ark_eve_access_tunnel_8017E15C(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            Room_Draw01(&D_neo_ark_eve_access_tunnel_8017EB48[0], 0x180, 0x444);
            Room_Draw01(&D_neo_ark_eve_access_tunnel_8017EB48[2], 0x180, 0x444);
            Room_Draw01(&D_neo_ark_eve_access_tunnel_8017EB48[4], 0x180, 0x444);
            break;
        case 3:
            Room_Draw01(&D_neo_ark_eve_access_tunnel_8017EAE8[0], 0x180, 0x444);
            Room_Draw01(&D_neo_ark_eve_access_tunnel_8017EAE8[2], 0x180, 0x444);
            break;
        case 4:
            Room_Draw01(&D_neo_ark_eve_access_tunnel_8017EB08[0], 0x180, 0x444);
            Room_Draw01(&D_neo_ark_eve_access_tunnel_8017EB08[2], 0x180, 0x444);
            /* fallthrough */
        case 5:
            Room_Draw01(&D_neo_ark_eve_access_tunnel_8017EB28[0], 0x180, 0x444);
            Room_Draw01(&D_neo_ark_eve_access_tunnel_8017EB28[2], 0x180, 0x444);
            break;
        case 6:
            Room_Draw01(&D_neo_ark_eve_access_tunnel_8017EB48[0], 0x180, 0x444);
            Room_Draw01(&D_neo_ark_eve_access_tunnel_8017EB48[2], 0x180, 0x444);
            break;
    }
}
