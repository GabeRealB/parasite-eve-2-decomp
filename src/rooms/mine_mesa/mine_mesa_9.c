#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern Task* RoomsShared8018459cTask;
extern Task* D_mine_mesa_80189B58;
extern Task* RoomsShared8017e8a8Task;

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

/// The mesa's emitter placements, one `SVECTOR` per position, 8 bytes apart.
/// The runs overlap: `864F0`'s fourth and seventh positions are `864F0` itself
/// plus 0x18 and 0x30, which lie inside the `86508` run, and view 8's single
/// position is `864F0[0]` while view 4 draws the same base's 0x30.
extern SVECTOR D_mine_mesa_801864C8[];
extern SVECTOR D_mine_mesa_801864D0[];
extern SVECTOR D_mine_mesa_801864D8[];
extern SVECTOR D_mine_mesa_801864F0[];
extern SVECTOR D_mine_mesa_80186508[];

void func_mine_mesa_8017EB38(void)
{
    RoomsShared8018459cTask = NULL;
    D_mine_mesa_80189B58    = NULL;
    RoomsShared8017e8a8Task = NULL;
}

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_9", func_mine_mesa_8017EB54);

/// Publishes the mesa's three effect ids as `Gp_State1C::field_A` variant `2`
/// on the task's first tick, then draws every emitter the current camera view
/// shows: one `Room_Draw17` quad per position, UV column `arg1` and half-extent
/// 0x200 - 0x300 for the two column-0 positions of views 2 and 5. Each view is
/// one run of that base's positions; views 4 and 11 both end on 0x30, so the
/// compiler merges their last two calls into one shared tail, which in turn
/// ends on the single `Room_Draw17` every other view finishes with.
void func_mine_mesa_8017ED08(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758                 = 0x600E9;
        D_8011572C                 = 0x600EB;
        D_80115750                 = 0x600EC;
        Gp_State1C->roomEffectMode = 2;
        arg0->state                = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p = D_mine_mesa_801864D0;
            Room_Draw17(&p[0], 0, 0x300);
            Room_Draw17(&p[1], 1, 0x200);
            Room_Draw17(&p[2], 1, 0x200);
            Room_Draw17(&p[3], 1, 0x200);
            break;
        }
        case 4: {
            SVECTOR* p = D_mine_mesa_801864F0;
            Room_Draw17(&p[0], 1, 0x200);
            Room_Draw17(&p[1], 1, 0x200);
            Room_Draw17(&p[2], 1, 0x200);
            Room_Draw17(&p[3], 1, 0x200);
            Room_Draw17(&p[6], 1, 0x200);
            break;
        }
        case 5: {
            SVECTOR* p = D_mine_mesa_801864C8;
            Room_Draw17(&p[0], 0, 0x300);
            Room_Draw17(&p[1], 0, 0x300);
            Room_Draw17(&p[5], 1, 0x200);
            Room_Draw17(&p[6], 1, 0x200);
            Room_Draw17(&p[8], 1, 0x200);
            Room_Draw17(&p[9], 1, 0x200);
            break;
        }
        case 6: {
            SVECTOR* p = D_mine_mesa_801864F0;
            Room_Draw17(&p[0], 1, 0x200);
            Room_Draw17(&p[1], 1, 0x200);
            Room_Draw17(&p[4], 1, 0x200);
            Room_Draw17(&p[5], 1, 0x200);
            break;
        }
        case 8: {
            SVECTOR* p = D_mine_mesa_801864F0;
            Room_Draw17(&p[0], 1, 0x200);
            break;
        }
        case 9: {
            SVECTOR* p = D_mine_mesa_80186508;
            Room_Draw17(&p[0], 1, 0x200);
            Room_Draw17(&p[1], 1, 0x200);
            Room_Draw17(&p[2], 1, 0x200);
            break;
        }
        case 10: {
            SVECTOR* p = D_mine_mesa_801864D8;
            Room_Draw17(&p[0], 1, 0x200);
            Room_Draw17(&p[2], 1, 0x200);
            break;
        }
        case 11: {
            SVECTOR* p = D_mine_mesa_801864F0;
            Room_Draw17(&p[0], 1, 0x200);
            Room_Draw17(&p[1], 1, 0x200);
            Room_Draw17(&p[2], 1, 0x200);
            Room_Draw17(&p[3], 1, 0x200);
            Room_Draw17(&p[4], 1, 0x200);
            Room_Draw17(&p[5], 1, 0x200);
            Room_Draw17(&p[6], 1, 0x200);
            break;
        }
    }
}

INCLUDE_RODATA("rooms/nonmatchings/mine_mesa/mine_mesa_9", D_mine_mesa_8017D624);
