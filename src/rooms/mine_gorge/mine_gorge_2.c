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

void func_mine_gorge_8017DB88(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Publishes the variant index the current camera view maps to, then draws the
/// gorge's props for that view: one `func_mine_gorge_8017DB88` quad per
/// position, UV column 1 and half-extent 0x300. Views share runs of the same
/// table, so `3` and `7` draw five positions from `E778` where `6` draws two,
/// and `10`/`11` draw the single position at `E790`; every case ends on the
/// same call, which the compiler merges into one shared tail.
void func_mine_gorge_8017D9F8(void)
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

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// `gte_stflg` is non-negative, queues one semi-transparent `POLY_FT4` (tpage
/// 0x2B, clut `(arg1 & 0x3F) | 0x4380`). `arg1` selects the 40-texel UV column
/// `(s16)arg1 * 40` at v=0..0x27. `arg2` is a signed half-extent; the
/// on-screen radius is `(s16)arg2 * 39 / otz`. RGB is the frame-counter blend
/// byte `((animFrame & 1) * 16) + 0x20` on all three channels.
void func_mine_gorge_8017DB88(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                tex;
    s32                idx;
    s32                u0;
    s32                u1;
    register s32       sarg asm("v1");
    s32                t;
    s32                field8;
    s32                blend;
    s32                v;
    u8                 code;
    s16                xy;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    tmp     = head - 0x10;
    block   = (RoomDraw13Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (((RoomDraw13Scratch*)tmp)->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ds             = &gDisplayState;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)tex;
        field8      = (u8)ds->animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        prim->u0    = u0;
        prim->u2    = u0;
        SOFT_USE_REG(u0);
        v        = 0x27;
        prim->u1 = u1;
        prim->u3 = u1;
        SOFT_USE_REG(u1);
        sarg     = arg2 << 16;
        prim->v2 = v;
        prim->v3 = v;
        SCHED_BARRIER();
        code     = prim->code;
        sarg     = sarg >> 16;
        prim->v0 = 0;
        prim->v1 = 0;
        blend    = ((field8 & 1) * 16) + 0x20;
        COMPILER_BARRIER();
        prim->code = code | 2;
        t          = sarg * 40;
        setRGB0(prim, blend, blend, blend);
        ((RoomDraw13Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sx - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sx + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sy - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sy + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw13Scratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}
