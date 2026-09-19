#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "main/mc.h"
#include "rooms/room_common.h"

/// The tunnel's per-view emitter placements, one `SVECTOR` per position, 8 bytes
/// apart. The runs overlap: `DB18`'s fifth position, `DB30`'s third and
/// `DB38`'s second are all the same `0x8017DB40`, reached through whichever
/// base the view's case names.
extern SVECTOR D_mine_tunnel_entrance_8017DB18[];
extern SVECTOR D_mine_tunnel_entrance_8017DB30[];
extern SVECTOR D_mine_tunnel_entrance_8017DB38[];
extern SVECTOR D_mine_tunnel_entrance_8017DB48[];

/// Publishes variant `2` as the room's `Gp_State1C->roomEffectMode` index, then draws
/// every emitter the current camera view shows: one `Room_Draw17` quad per
/// position, UV column `arg1` and half-extent 0x300. Views 2-5 each end on the
/// same call, which the compiler merges into one shared tail; view 2 draws
/// three column-0 positions from `DB18` plus the shared `DB38[1]`, and view 6 is
/// the only one whose second quad uses half-extent 0x200.
void func_mine_tunnel_entrance_8017D720(void)
{
    Gp_State1C->roomEffectMode = 2;
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB18;
            Room_Draw17(&p[0], 0, 0x300);
            Room_Draw17(&p[1], 0, 0x300);
            Room_Draw17(&p[2], 1, 0x300);
            Room_Draw17(&p[5], 1, 0x300);
            break;
        }
        case 3: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB30;
            Room_Draw17(&p[0], 1, 0x300);
            Room_Draw17(&p[1], 1, 0x300);
            Room_Draw17(&p[2], 1, 0x300);
            break;
        }
        case 4: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB30;
            Room_Draw17(&p[0], 1, 0x300);
            Room_Draw17(&p[1], 1, 0x300);
            break;
        }
        case 5: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB38;
            Room_Draw17(&p[0], 1, 0x300);
            break;
        }
        case 6: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB48;
            Room_Draw17(&p[0], 1, 0x300);
            Room_Draw17(&p[1], 1, 0x200);
            break;
        }
    }
}

INCLUDE_RODATA("rooms/nonmatchings/mine_tunnel_entrance/mine_tunnel_entrance_3", D_mine_tunnel_entrance_8017D5E8);
