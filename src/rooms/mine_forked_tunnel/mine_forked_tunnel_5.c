#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "rooms/room_common.h"

/// The tunnel's per-view effect anchors, projected by
/// `func_mine_forked_tunnel_8017E78C` with `Room_Draw17` (half-extent 0x300).
/// Views 2 and 3 share the first anchor, view 4 draws the second and third
/// (the tunnel fork's two arms) and view 5 the fourth.
extern SVECTOR D_mine_forked_tunnel_80183614[];
extern SVECTOR D_mine_forked_tunnel_8018361C[];
extern SVECTOR D_mine_forked_tunnel_8018362C[];

/// Room effect tick. Marks the effect state (`field_A` = 2, the value
/// `actor_400100_text` and `Gp_EffCtlTaskAC` test) and projects the light
/// anchor belonging to the camera's view index, so the fork's light follows
/// whichever branch the player is looking down.
void func_mine_forked_tunnel_8017E78C(void)
{
    s32 idx;

    Gp_State1C->roomEffectMode = 2;
    idx                        = Gp_GetViewIndex() & 0xFF;

    switch (idx) {
        case 2:
        case 3:
            Room_Draw17(D_mine_forked_tunnel_80183614, 1, 0x300);
            break;
        case 4:
            Room_Draw17(&D_mine_forked_tunnel_8018361C[0], 1, 0x300);
            Room_Draw17(&D_mine_forked_tunnel_8018361C[1], 1, 0x300);
            break;
        case 5:
            Room_Draw17(D_mine_forked_tunnel_8018362C, 1, 0x300);
            break;
        default:
            return;
    }
}
