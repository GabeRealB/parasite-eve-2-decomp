#include "common.h"

#include "main/gameflag.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"

#include "rooms/mine_refuge.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

INCLUDE_ASM("rooms/nonmatchings/mine_refuge/mine_refuge_5", func_mine_refuge_8018029C);

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues a glow of gouraud `POLY_G4` wedges
/// around the projected centre: an eight-wedge disc of radius
/// `(s16)arg2 * 64 / otz`, each wedge paired with a half-radius copy, then four
/// wedges reaching between that radius and an inner one of `(s16)arg2 * 8 /
/// otz`. Only the centre vertex is lit, on green and blue, with a level of
/// `rsin(animFrame * (s16)arg1) / 34 + 0x78` so the glow pulses; the half-radius
/// copies take that level and every other wedge half of it. The scratch block is returned to
/// `G_SCRATCH_HEAD` on exit.
void func_mine_refuge_80180710(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                 scratch;
    u8*                    head;
    MineRefugeGlowScratch* block;
    POLY_G4*               prim;
    s32                    pulse;
    s32                    color;
    s32                    half;
    s32                    size;
    s32                    ang;
    s32                    t;
    s32                    t2;
    s32                    u;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    block   = (MineRefugeGlowScratch*)(*scratch = head - 0x1C);

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps_real();
    gte_stsxy(&((MineRefugeGlowScratch*)(head - 0x1C))->sx);
    gte_stflg(&((MineRefugeGlowScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((MineRefugeGlowScratch*)(head - 0x1C))->otz);
        pulse         = rsin(gDisplayState.animFrame * (s16)arg1);
        ang           = 0;
        size          = (s16)arg2;
        block->rOuter = (size * 64) / block->otz;
        block->rInner = (size * 8) / block->otz;
        color         = pulse / 34 + 0x78;
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues three concentric rings of eight
/// gouraud `POLY_G4` wedges around the projected centre. The first ring's
/// radius is `(s16)arg1 * 64 / otz`; each later ring doubles it and halves the
/// centre colour. `arg2` packs three RGB nibbles for the centre vertex, each
/// offset by `(animFrame & 1) << 5` so the glow flickers on alternate frames.
/// Unlike the shared wedge draws it never returns its 0x10-byte scratch block
/// to `G_SCRATCH_HEAD`.
void func_mine_refuge_80181094(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    s32                ring;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                r;
    s32                g;
    s32                b;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    block   = (RoomDraw13Scratch*)(*scratch = head - 0x10);

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1          = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        ring          = 0;
        blend         = ((u8)gDisplayState.animFrame & 1) << 5;
        packed        = arg2 << 16;
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            ang = 0;
            do {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, r, g, b);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
                t        = ang + 0x100;
                prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
                prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
                prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
                t2       = ang + 0x200;
                prim->x2 = block->sx;
                prim->y2 = block->sy;
                prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
                prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
                ang      = t2;
                addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            } while (ang < 0x1000);
            r              = (u8)r >> 1;
            g              = (u8)g >> 1;
            b              = (u8)b >> 1;
            block->radius *= 2;
            ring++;
        } while (ring < 3);
    }
}

void func_mine_refuge_8018029C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_mine_refuge_80180710(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_mine_refuge_80181094(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Draw anchors of the refuge, in the shared data blob at the end of the
/// overlay: `D8` and `E0` are the two wedges of the first view, `E8` heads the
/// run the later views sweep. `E0` is reached both as `D8[1]` (the draw pair
/// below) and by its own name (view 6), and the two forms are different code -
/// indexing emits `D8+8`, naming emits its own `lui` - so it keeps its own
/// declaration.
extern SVECTOR D_mine_refuge_801818D8[];
extern SVECTOR D_mine_refuge_801818E0;
extern SVECTOR D_mine_refuge_801818E8;

/// Draws the refuge for whichever view is current. View 2 draws two wedges in
/// sequence; views 3 and 4/5 sweep one anchor each, but only while progress
/// flag 0xC3 is 1, and view 6 draws on its own. Every case shares the one
/// epilogue, so 6 falls into it and the rest jump.
void func_mine_refuge_80181454(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            Room_Draw17(&D_mine_refuge_801818D8[0], 1, 0x300);
            func_mine_refuge_8018029C(&D_mine_refuge_801818D8[1], 0x60, 0x40);
            break;
        case 3:
            if (GameFlag_GetNibble(0xC3) == 1) {
                func_mine_refuge_80181094(&D_mine_refuge_801818E8, 0x30, 0xF0);
            }
            break;
        case 4:
        case 5:
            if (GameFlag_GetNibble(0xC3) == 1) {
                func_mine_refuge_80181094(&D_mine_refuge_801818E8, 0x60, 0xD0);
            }
            break;
        case 6:
            func_mine_refuge_80180710(&D_mine_refuge_801818E0, 0x60, 0x80);
            break;
    }
}
