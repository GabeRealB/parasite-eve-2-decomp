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
#include "main/session.h"
#include "rooms/room_common.h"

/// The room's per-view table: `Gp_State1C->roomEffectMode` latches the entry the
/// current camera index selects, and the room's effect tasks read it back.
extern u16 D_dryfield_night_parking_lot_8017EDBC[];

/// The parking lot's drawable points, one 8-byte `SVECTOR` per prop. The
/// phase each one belongs to is `gGameSession->at4.loc.view` (the room's stage).
extern SVECTOR D_dryfield_night_parking_lot_8017EDCC[];
extern SVECTOR D_dryfield_night_parking_lot_8017EDDC[];
extern SVECTOR D_dryfield_night_parking_lot_8017EDE4[];
extern SVECTOR D_dryfield_night_parking_lot_8017EDEC[];
extern SVECTOR D_dryfield_night_parking_lot_8017EDFC[];

void func_dryfield_night_parking_lot_8017DE10(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_dryfield_night_parking_lot_8017E08C(SVECTOR* arg0, SVECTOR* arg1, s32 arg2);

/// Parking-lot room draw: latches the view's entry of the room's per-view table
/// into `Gp_State1C->roomEffectMode`, then queues the props of the room phase
/// `gGameSession->at4.loc.view` selects - 2, 4 and 5 several points each, 3 and 6 a
/// single one. Every phase ends with the same semi-transparent sprite call,
/// which `jump.c` cross-jumps into one tail block after the last case.
void func_dryfield_night_parking_lot_8017DC88(void)
{
    u8 view;

    view                       = Gp_GetViewIndex();
    Gp_State1C->roomEffectMode = D_dryfield_night_parking_lot_8017EDBC[view - 1];
    switch (gGameSession->at4.loc.view) {
        case 2: {
            SVECTOR* p = D_dryfield_night_parking_lot_8017EDCC;
            func_dryfield_night_parking_lot_8017DE10(&p[0], 0, 0x300);
            func_dryfield_night_parking_lot_8017DE10(&p[1], 0, 0x300);
            func_dryfield_night_parking_lot_8017DE10(&p[2], 0, 0x330);
            func_dryfield_night_parking_lot_8017DE10(&p[6], 1, 0x380);
            func_dryfield_night_parking_lot_8017DE10(&p[7], 1, 0x380);
            break;
        }
        case 3: {
            SVECTOR* p = D_dryfield_night_parking_lot_8017EDFC;
            func_dryfield_night_parking_lot_8017DE10(&p[0], 1, 0x380);
            break;
        }
        case 4: {
            SVECTOR* p = D_dryfield_night_parking_lot_8017EDEC;
            func_dryfield_night_parking_lot_8017E08C(&p[0], &p[1], 0x180);
            func_dryfield_night_parking_lot_8017DE10(&p[2], 1, 0x380);
            func_dryfield_night_parking_lot_8017DE10(&p[3], 1, 0x380);
            break;
        }
        case 5: {
            SVECTOR* p = D_dryfield_night_parking_lot_8017EDDC;
            func_dryfield_night_parking_lot_8017DE10(&p[0], 0, 0x300);
            func_dryfield_night_parking_lot_8017DE10(&p[1], 0, 0x300);
            func_dryfield_night_parking_lot_8017DE10(&p[5], 1, 0x380);
            func_dryfield_night_parking_lot_8017DE10(&p[6], 1, 0x380);
            break;
        }
        case 6: {
            SVECTOR* p = D_dryfield_night_parking_lot_8017EDE4;
            func_dryfield_night_parking_lot_8017DE10(&p[0], 0, 0x300);
            break;
        }
    }
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the OTZ is at least 0x11, queues one semi-transparent `POLY_FT4` (tpage
/// 0x2B, clut `(arg1 & 0x3F) | 0x4380`) centred on it. `(s16)arg1` also selects
/// the 40-texel UV column `arg1 * 40`, rows 0..0x27. `arg2` is a signed
/// half-extent; the on-screen radius is `(s16)arg2 * 39 / otz`. All three
/// colour channels take the flickering grey `((animFrame & 1) * 16) + 0x20`.
void func_dryfield_night_parking_lot_8017DE10(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw25Scratch* block;
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
    tmp     = head - 0xC;
    block   = (RoomDraw25Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomDraw25Scratch*)(head - 0xC))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - 0xC))->otz >= 0x11) {
        ds          = &gDisplayState;
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
        ((RoomDraw25Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw25Scratch*)(head - 0xC))->otz;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sx - *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sx + *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sy - *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sy + *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw25Scratch*)(head - 0xC))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0xC);
}

/// Projects `arg0` and `arg1` through the view matrix and, when the far point is
/// past the near clip, queues gouraud wedges: a fan around the first point, a
/// band joining the two, and a fan around the second. Each point's radius is
/// `(s16)arg2 * 64` over its depth; the inner colour pulses with the display
/// frame parity.
void func_dryfield_night_parking_lot_8017E08C(SVECTOR* arg0, SVECTOR* arg1, s32 arg2)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(arg1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg2 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        ang       = 0;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 16) | 0x20;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = rgb;
            p->g2    = rgb;
            prim->b2 = rgb;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin((ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos((ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin((ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos((ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            SCHED_BARRIER();
            t3             = 0x1000 - ang;
            prim           = (POLY_G4*)gGpuPrimCursor;
            t              = 0x1000 - ang;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            t        = 0xE00;
            t       -= ang;
            prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            t        = 0xC00;
            t       -= ang;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x18);
}
