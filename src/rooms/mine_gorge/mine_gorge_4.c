#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "rooms/room_common.h"

/// Per-view halfword table, indexed 1-based by `Gp_GetViewIndex()`. The value
/// the room publishes as its `Gp_State1C::field_A` variant index.
extern u16 D_mine_gorge_8017E760[];

/// The gorge's per-view prop placements, one `SVECTOR` per position, 8 bytes
/// apart and overlapping: the run a view draws starts at whichever of these
/// four bases its case names and reaches at most `0x20` past `E778`.
extern SVECTOR D_mine_gorge_8017E778[];
extern SVECTOR D_mine_gorge_8017E788[];
extern SVECTOR D_mine_gorge_8017E790[];
extern SVECTOR D_mine_gorge_8017E798[];

/// Publishes the variant index the current camera view maps to, then draws the
/// gorge's props for that view: one `Room_Draw17` quad per position, UV column
/// 1 and half-extent 0x300. Views share runs of the same table, so `3` and `7`
/// draw five positions from `E778` where `6` draws two, and `10`/`11` draw the
/// single position at `E790`; every case ends on the same call, which the
/// compiler merges into one shared tail.
void func_mine_gorge_8017D9F8(void)
{
    Gp_State1C->roomEffectMode = D_mine_gorge_8017E760[(Gp_GetViewIndex() & 0xFF) - 1];
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p = D_mine_gorge_8017E798;
            Room_Draw17(&p[0], 1, 0x300);
            Room_Draw17(&p[1], 1, 0x300);
            break;
        }
        case 3:
        case 7: {
            SVECTOR* p = D_mine_gorge_8017E778;
            Room_Draw17(&p[0], 1, 0x300);
            Room_Draw17(&p[1], 1, 0x300);
            Room_Draw17(&p[2], 1, 0x300);
            Room_Draw17(&p[3], 1, 0x300);
            Room_Draw17(&p[4], 1, 0x300);
            break;
        }
        case 4:
        case 5:
        case 9: {
            SVECTOR* p = D_mine_gorge_8017E788;
            Room_Draw17(&p[0], 1, 0x300);
            Room_Draw17(&p[1], 1, 0x300);
            Room_Draw17(&p[2], 1, 0x300);
            Room_Draw17(&p[3], 1, 0x300);
            break;
        }
        case 6: {
            SVECTOR* p = D_mine_gorge_8017E778;
            Room_Draw17(&p[0], 1, 0x300);
            Room_Draw17(&p[1], 1, 0x300);
            break;
        }
        case 8: {
            SVECTOR* p = D_mine_gorge_8017E788;
            Room_Draw17(&p[0], 1, 0x300);
            Room_Draw17(&p[2], 1, 0x300);
            Room_Draw17(&p[3], 1, 0x300);
            break;
        }
        case 10:
        case 11: {
            SVECTOR* p = D_mine_gorge_8017E790;
            Room_Draw17(&p[0], 1, 0x300);
            break;
        }
    }
}
