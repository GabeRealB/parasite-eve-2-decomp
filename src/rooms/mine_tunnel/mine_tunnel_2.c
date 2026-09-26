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

/// The tunnel's five light anchors, one `SVECTOR` each in one run; each view
/// draws a subset of them.
extern SVECTOR D_mine_tunnel_8017E12C[];

void func_mine_tunnel_8017D8CC(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Room effect tick: sets `Gp_State1C->roomEffectMode` to 2 and draws the
/// light anchors the current view index shows - anchor 2 in view 2, all five
/// in view 3, anchors 2 and 3 in view 4, anchors 1 and 4 in view 5, none
/// otherwise.
void func_mine_tunnel_8017D7D4(void)
{
    s32 idx;

    Gp_State1C->roomEffectMode = 2;
    idx                        = Gp_GetViewIndex() & 0xFF;

    switch (idx) {
        case 2:
            func_mine_tunnel_8017D8CC(&D_mine_tunnel_8017E12C[2], 1, 0x300);
            break;
        case 3:
            func_mine_tunnel_8017D8CC(&D_mine_tunnel_8017E12C[0], 1, 0x300);
            func_mine_tunnel_8017D8CC(&D_mine_tunnel_8017E12C[1], 1, 0x300);
            func_mine_tunnel_8017D8CC(&D_mine_tunnel_8017E12C[2], 1, 0x300);
            func_mine_tunnel_8017D8CC(&D_mine_tunnel_8017E12C[3], 1, 0x300);
            func_mine_tunnel_8017D8CC(&D_mine_tunnel_8017E12C[4], 1, 0x300);
            break;
        case 4:
            func_mine_tunnel_8017D8CC(&D_mine_tunnel_8017E12C[2], 1, 0x300);
            func_mine_tunnel_8017D8CC(&D_mine_tunnel_8017E12C[3], 1, 0x300);
            break;
        case 5:
            func_mine_tunnel_8017D8CC(&D_mine_tunnel_8017E12C[1], 1, 0x300);
            func_mine_tunnel_8017D8CC(&D_mine_tunnel_8017E12C[4], 1, 0x300);
            break;
        case 6:
            break;
    }
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// `gte_stflg` is non-negative, queues one semi-transparent `POLY_FT4` (tpage
/// 0x2B, clut `(arg1 & 0x3F) | 0x4380`). `arg1` selects the 40-texel UV column
/// `(s16)arg1 * 40` at v=0..0x27. `arg2` is a signed half-extent; the
/// on-screen radius is `(s16)arg2 * 39 / otz`. RGB is the frame-counter blend
/// byte `((animFrame & 1) * 16) + 0x20` on all three channels. The room's
/// effect tick draws its light anchors with it.
void func_mine_tunnel_8017D8CC(SVECTOR* arg0, s32 arg1, s32 arg2)
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
