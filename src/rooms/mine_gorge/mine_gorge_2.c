#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "rooms/room_common.h"

/// Per-view halfword table, indexed 1-based by `Gp_GetViewIndex()`. The value
/// the room publishes as its `Gp_State1C->roomEffectMode` variant index.
extern u16 D_mine_gorge_8017E760[];

/// The gorge's per-view prop placements, one `SVECTOR` per position, 8 bytes
/// apart and overlapping: the run a view draws starts at whichever of these
/// four bases its case names and reaches at most `0x20` past `E778`.
extern SVECTOR D_mine_gorge_8017E778[];
extern SVECTOR D_mine_gorge_8017E788[];
extern SVECTOR D_mine_gorge_8017E790[];
extern SVECTOR D_mine_gorge_8017E798[];

static void func_mine_gorge_8017DB88(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Publishes the variant index the current camera view maps to, then draws the
/// gorge's props for that view: one `func_mine_gorge_8017DB88` quad per
/// position, UV column 1 and half-extent 0x300. Views share runs of the same
/// table, so `3` and `7` draw five positions from `E778` where `6` draws two,
/// and `10`/`11` draw the single position at `E790`; every case ends on the
/// same call, which the compiler merges into one shared tail.
static void func_mine_gorge_8017D9F8(void)
{
    Gp_State1C->roomEffectMode = D_mine_gorge_8017E760[(Gp_GetViewIndex() & 0xFF) - 1];
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p = D_mine_gorge_8017E798;
            func_mine_gorge_8017DB88(&p[0], 1, 0x300);
            func_mine_gorge_8017DB88(&p[1], 1, 0x300);
            break;
        }
        case 3:
        case 7: {
            SVECTOR* p = D_mine_gorge_8017E778;
            func_mine_gorge_8017DB88(&p[0], 1, 0x300);
            func_mine_gorge_8017DB88(&p[1], 1, 0x300);
            func_mine_gorge_8017DB88(&p[2], 1, 0x300);
            func_mine_gorge_8017DB88(&p[3], 1, 0x300);
            func_mine_gorge_8017DB88(&p[4], 1, 0x300);
            break;
        }
        case 4:
        case 5:
        case 9: {
            SVECTOR* p = D_mine_gorge_8017E788;
            func_mine_gorge_8017DB88(&p[0], 1, 0x300);
            func_mine_gorge_8017DB88(&p[1], 1, 0x300);
            func_mine_gorge_8017DB88(&p[2], 1, 0x300);
            func_mine_gorge_8017DB88(&p[3], 1, 0x300);
            break;
        }
        case 6: {
            SVECTOR* p = D_mine_gorge_8017E778;
            func_mine_gorge_8017DB88(&p[0], 1, 0x300);
            func_mine_gorge_8017DB88(&p[1], 1, 0x300);
            break;
        }
        case 8: {
            SVECTOR* p = D_mine_gorge_8017E788;
            func_mine_gorge_8017DB88(&p[0], 1, 0x300);
            func_mine_gorge_8017DB88(&p[2], 1, 0x300);
            func_mine_gorge_8017DB88(&p[3], 1, 0x300);
            break;
        }
        case 10:
        case 11: {
            SVECTOR* p = D_mine_gorge_8017E790;
            func_mine_gorge_8017DB88(&p[0], 1, 0x300);
            break;
        }
    }
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// `gte_stflg` is non-negative, queues one semi-transparent `POLY_FT4` (tpage
/// 0x2B, clut `(arg1 & 0x3F) | 0x4380`). `arg1` selects the 40-texel UV column
/// `(s16)arg1 * 40` at v=0..0x27. `arg2` is a signed half-extent; the
/// on-screen radius is `(s16)arg2 * 39 / otz`. RGB is the frame-counter blend
/// byte `((animFrame & 1) * 16) + 0x20` on all three channels.
static void func_mine_gorge_8017DB88(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    s32                idx;
    s32                blend;
    s16                xy;

    block = SCRATCH_PUSH(RoomDraw13Scratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)arg1;
        blend       = (((u8)gDisplayState.animFrame & 1) * 16) + 0x20;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        setUVWH(prim, idx * 40, 0, 0x27, 0x27);
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        xy            = block->sx - (u16)block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = block->sx + (u16)block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = block->sy - (u16)block->radius;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = block->sy + (u16)block->radius;
        prim->y3      = xy;
        prim->y2      = xy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}
