#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"

#include "main/session.h"

#include "rooms/neo_ark_eve_access_tunnel.h"

extern s32 Gp_GetViewIndex(void);
void       Room_Draw01(SVECTOR* v, s32 arg1, s32 arg2);

/// Live emitters for the tunnel's views, in the shared data blob at the end of
/// the overlay. `D_..._8017EB48` doubles as case 2's three-entry run and case 6's
/// two-entry one, so both views share one base pointer.
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EAE8[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB08[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB28[];
extern SVECTOR D_neo_ark_eve_access_tunnel_8017EB48[];

void func_neo_ark_eve_access_tunnel_8017E090(s32 arg0, s32 arg1)
{
    GameSessionFrom4* sess = &gGameSession->loc;
    NaetSprtRec*      rec  = (NaetSprtRec*)Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    NaetSprtView*     view;
    s32               run = arg0 & 0xFF;
    s32               flag;

    if (run == 0) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            view           = rec->field_1C;
            view->field_24 = 1;
            return;
        }
        if (flag == 1) {
            view           = rec->field_1C;
            view->field_24 = 0;
            return;
        }
    } else if (run == 1) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            view           = rec->field_28;
            view->field_1C = run;
            view           = rec->field_34;
            view->field_14 = run;
            return;
        }
        if (flag == run) {
            view           = rec->field_28;
            view->field_1C = 0;
            view           = rec->field_34;
            view->field_14 = 0;
        }
    }
}

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
