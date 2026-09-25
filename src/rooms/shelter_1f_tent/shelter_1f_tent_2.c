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

/// Glow positions `func_shelter_1f_tent_8017FE10` draws per view.
extern SVECTOR D_shelter_1f_tent_80181D04[];
extern SVECTOR D_shelter_1f_tent_80181D0C[];
extern SVECTOR D_shelter_1f_tent_80181D1C[];
extern SVECTOR D_shelter_1f_tent_80181D2C[];
extern SVECTOR D_shelter_1f_tent_80181D3C[];

void func_shelter_1f_tent_8017FFAC(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_1f_tent_801807F0(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_1f_tent_80180B90(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_1f_tent_80181010(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Draws the glows of the current camera view at the room's fixed world
/// points; views without an entry draw nothing.
void func_shelter_1f_tent_8017FE10(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_1f_tent_80181D2C;
            func_shelter_1f_tent_8017FFAC(&p[0], 0x200, 0x111);
            func_shelter_1f_tent_801807F0(&p[-5], 0x280, 0x40);
            func_shelter_1f_tent_801807F0(&p[-4], 0x280, 0x444);
            func_shelter_1f_tent_801807F0(&p[-3], 0x180, 0x22);
            func_shelter_1f_tent_801807F0(&p[-2], 0x280, 0x444);
            func_shelter_1f_tent_801807F0(&p[-1], 0x180, 0x22);
            func_shelter_1f_tent_80180B90(&D_shelter_1f_tent_80181D3C[0], 0x60, 0x80);
            break;
        }
        case 3: {
            SVECTOR* p = D_shelter_1f_tent_80181D04;
            func_shelter_1f_tent_801807F0(&p[0], 0x280, 0x40);
            func_shelter_1f_tent_801807F0(&p[1], 0x280, 0x444);
            func_shelter_1f_tent_801807F0(&p[2], 0x180, 0x22);
            func_shelter_1f_tent_80180B90(&p[7], 0x60, 0x80);
            break;
        }
        case 5: {
            SVECTOR* p = D_shelter_1f_tent_80181D0C;
            func_shelter_1f_tent_801807F0(&p[0], 0x280, 0x444);
            func_shelter_1f_tent_80181010(&p[6], 0x60, 0x80);
            break;
        }
        case 4:
        case 6: {
            SVECTOR* p = D_shelter_1f_tent_80181D1C;
            func_shelter_1f_tent_801807F0(&p[0], 0x280, 0x444);
            func_shelter_1f_tent_801807F0(&p[1], 0x180, 0x22);
            break;
        }
        case 7: {
            SVECTOR* p = D_shelter_1f_tent_80181D04;
            func_shelter_1f_tent_801807F0(&p[0], 0x280, 0x40);
            func_shelter_1f_tent_801807F0(&p[1], 0x280, 0x444);
            func_shelter_1f_tent_801807F0(&p[2], 0x180, 0x22);
            func_shelter_1f_tent_80180B90(&D_shelter_1f_tent_80181D3C[0], 0x60, 0x80);
            break;
        }
    }
}

/// Draws a glowing capsule between the world points `arg0` and `arg0 + 1`,
/// projected through `Gfx_ViewWorldMtx`. Each end is a disc of radius
/// `(s16)arg1 * 64` over its depth; for each 0x400 step across half a turn
/// from the screen-space angle between the ends, one gouraud `POLY_G4` wedge
/// is queued at each end and one band joins them. The lit vertices, at the
/// centres, take the colour packed in `arg2` (one nibble per channel),
/// flickering with the animation frame. Nothing is drawn when either
/// projection flags an error.
void func_shelter_1f_tent_8017FFAC(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      sum;
    u8                       r;
    u8                       g;
    u8                       b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (OverlayPointPairScratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            SCHED_BARRIER();
            ang    = (s16)ang;
            blend  = (*(u8*)&ds->animFrame & 1) * 8;
            packed = arg2 << 16;
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend | tr;
            g      = blend | tg;
            b      = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim = (POLY_G4*)gGpuPrimCursor;
                    USE_REG(prim);
                    t   = ang - angStart;
                    t <<= 1;
                    TOUCH_REG(t);
                    sum            = angStart + t;
                    t              = sum;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();
                    t3   = ang + 0x800;
                    prim = (POLY_G4*)gGpuPrimCursor;
                    SOFT_BARRIER();
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues four gouraud `POLY_G4` wedges around
/// the projected centre. `arg1` is a signed half-extent; the on-screen radius
/// is `(s16)arg1 * 64 / otz`. `arg2` packs three RGB nibbles for the inner
/// vertex, OR'd with `((u8)gDisplayState.animFrame & 1) * 8`.
void func_shelter_1f_tent_801807F0(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    register u8*       tmp asm("v0");
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (RoomDraw13Scratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1 = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        ang  = 0;
        tmp  = (u8*)&gDisplayState;
        SOFT_TOUCH_REG(tmp);
        ds            = (DisplayState*)tmp;
        blend         = (*(u8*)&ds->animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x10);
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues two gouraud `POLY_G4` diamonds and two
/// gouraud `LINE_G3` diagonals around the projected centre, with an on-screen
/// radius of `(s16)arg2 * 48 / otz`. `arg1` scales `gDisplayState.animFrame`
/// into `rsin` so the lit vertex pulses as `rsin(...) / 34 + 0x78` on green and
/// blue.
void func_shelter_1f_tent_80180B90(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    LINE_G3*           line;
    s32                sine;
    s32                pulse;
    s32                radius;
    s32                i;
    s32                t1;
    s32                t2;
    s32                twice;
    u16                sx;
    u16                sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x10);
        SOFT_TOUCH_REG(tmp);
        block = (RoomDraw13Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * (s16)arg1);
        radius        = ((s16)arg2 * 48) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        i             = 0;
        pulse         = sine / 34 + 0x78;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, pulse, pulse);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->radius;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->radius;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->radius) + (block->radius * twice);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = (LINE_G3*)gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, 0, pulse, pulse);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->radius * t1);
            line->y0 = block->sy - (block->radius * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->radius * t1);
            line->y2 = block->sy + (block->radius * t2);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP_BYTES(0x10);
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues a sixteen-wedge gouraud disc plus two
/// inner cross wedges around the projected centre. `arg2` is a signed
/// half-extent; on-screen radii are `(s16)arg2 * 64 / otz` (outer) and
/// `(s16)arg2 * 8 / otz` (inner). `arg1` scales `gDisplayState.animFrame` into
/// `rsin` so the lit vertex pulses as `rsin(...) / 34 + 0x78` on green and
/// blue.
void func_shelter_1f_tent_80181010(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    s32                pulse;
    s32                color;
    s32                half;
    s32                size;
    s32                ang;
    s32                t;
    s32                t2;
    s32                u;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x14);
        SOFT_TOUCH_REG(tmp);
        block = (RoomDraw05Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw05Scratch*)(head - 0x14))->sx);
    gte_stflg(&((RoomDraw05Scratch*)(head - 0x14))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulse         = rsin(gDisplayState.animFrame * (s16)arg1);
        ang           = 0;
        size          = (s16)arg2;
        block->rOuter = (size * 64) / ((RoomDraw05Scratch*)(head - 0x14))->otz;
        color         = pulse / 34 + 0x78;
        block->rInner = (size * 8) / ((RoomDraw05Scratch*)(head - 0x14))->otz;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            half = (s16)color >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, half, half);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        color = half;
        ang   = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x14);
}
