#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "rooms/room_common.h"

/// The tunnel's per-view light anchors, one SVECTOR each and all in one run:
/// index 1 is the view-5 anchor, index 2 the view-2/4 one and index 4 the
/// shared closing one. `D_mine_tunnel_8017E134`, `_E13C` and `_E14C` are the
/// names splat gives the interior addresses the %lo pairs below resolve to,
/// so they are indices here, not separate arrays.
extern SVECTOR D_mine_tunnel_8017E12C[];

/// Room effect tick. Marks the effect state (`field_A` = 2, the value
/// `actor_400100_text` and `Gp_EffCtlTaskAC` test) and projects the light
/// anchors belonging to the camera's view index, so the tunnel's lights follow
/// whichever branch the player is looking down. Views 3 and 5 both finish
/// with the run's last anchor, which is why they share a tail.
void func_mine_tunnel_8017D7D4(void)
{
    s32 idx;

    Gp_State1C->roomEffectMode = 2;
    idx                        = Gp_GetViewIndex() & 0xFF;

    switch (idx) {
        case 2:
            Room_Draw17(&D_mine_tunnel_8017E12C[2], 1, 0x300);
            break;
        case 3:
            Room_Draw17(&D_mine_tunnel_8017E12C[0], 1, 0x300);
            Room_Draw17(&D_mine_tunnel_8017E12C[1], 1, 0x300);
            Room_Draw17(&D_mine_tunnel_8017E12C[2], 1, 0x300);
            Room_Draw17(&D_mine_tunnel_8017E12C[3], 1, 0x300);
            Room_Draw17(&D_mine_tunnel_8017E12C[4], 1, 0x300);
            break;
        case 4:
            Room_Draw17(&D_mine_tunnel_8017E12C[2], 1, 0x300);
            Room_Draw17(&D_mine_tunnel_8017E12C[3], 1, 0x300);
            break;
        case 5:
            Room_Draw17(&D_mine_tunnel_8017E12C[1], 1, 0x300);
            Room_Draw17(&D_mine_tunnel_8017E12C[4], 1, 0x300);
            break;
        case 6:
            break;
    }
}
