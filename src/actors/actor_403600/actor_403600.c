#include "common.h"

#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "psyq/libgs.h"
#include "psyq/rand.h"
#include "psyq/inline_c.h"
#include "psyq/gtemac.h"
#include "gte.h"

#include "actors/actors_shared_80131fc8.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "actors/actor.h"
#include "actors/actor_403600.h"

/// A quad of the screen distortion grid. Its texture is the copy of the frame
/// the grid is drawn over, which is wider than one texture page reaches, so a
/// vertex's U is kept relative to a page shifted right by its `pageN` (0 or
/// 0x40 pixels), held in what a POLY_FT4 leaves as padding. Once all four
/// vertices are placed they are brought onto one page, which `tpage` names.
typedef struct Actor403600GridQuad {
    u_long  tag;
    u_char  r0, g0, b0, code;
    short   x0, y0;
    u_char  u0, v0;
    u_short clut;
    short   x1, y1;
    u_char  u1, v1;
    u_short tpage;
    short   x2, y2;
    u_char  u2, v2;
    u_char  page0, page1;
    short   x3, y3;
    u_char  u3, v3;
    u_char  page2, page3;
} Actor403600GridQuad;
STATIC_ASSERT_SIZEOF(Actor403600GridQuad, sizeof(POLY_FT4));

/// The position and texture coordinate of one vertex of a grid quad, which
/// every vertex of a POLY_FT4 lays out alike.
typedef struct Actor403600GridVertex {
    s16 x;
    s16 y;
    u8  u;
    u8  v;
} Actor403600GridVertex;

/// Scratch block `func_actor_403600_80132E40` lays the chains out in.
typedef struct Actor403600ChainScratch {
    SVECTOR a;       // the segment being placed, or a vector being rotated
    SVECTOR b;       // a part's position relative to the view, or the axis a basis is built about
    MATRIX  basis;   // the transposed view rotation, then the basis a segment is turned by
    MATRIX  rot;     // a part's rotation while it is turned
    byte    pad_50[0x10];
    SVECTOR dirs[3]; // direction of each segment, as the joints are placed
    byte    pad_78[8];
    SVECTOR drift;   // sideways push the segments take while the actor's drift is set
} Actor403600ChainScratch;
STATIC_ASSERT_SIZEOF(Actor403600ChainScratch, 0x88);

/// 0x1C-byte scratch block used while building the screen transition grid.
typedef struct Actor403600ScreenScratch {
    /* 0x00 */ u8      pad_0[0x10];
    /* 0x10 */ s32     otz;
    /* 0x14 */ SVECTOR offset;
} Actor403600ScreenScratch;
STATIC_ASSERT_SIZEOF(Actor403600ScreenScratch, 0x1C);

/// 0x78-byte scratch block used to project the radial effect grid.
typedef struct Actor403600EffectScratch {
    /* 0x00 */ s32     dp;
    /* 0x04 */ s32     flag;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     nclip;
    /* 0x10 */ s32     sxy;
    /* 0x14 */ SVECTOR projected;
    /* 0x1C */ SVECTOR vec;
    /* 0x24 */ s32     maxOtz;
    /* 0x28 */ MATRIX  matrix;
    /* 0x48 */ SVECTOR points[3];
    /* 0x60 */ s32     pad_60[2];
    /* 0x68 */ s32     sxy3[3];
    /* 0x74 */ s32     pad_74;
} Actor403600EffectScratch;
STATIC_ASSERT_SIZEOF(Actor403600EffectScratch, 0x78);

/// Scratch block `func_actor_403600_80134398` works in while it steers and
/// draws a projectile.
typedef struct Actor403600ProjectileScratch {
    VECTOR  target;  // world position of the part the projectile homes on
    SVECTOR dir;     // that part relative to the view, then the steering direction
    DVECTOR sxy;     // screen position of the trail point being drawn
    s32     dp;      // depth cue of that point
    s32     flag;    // GTE flag of the projection; negative when it failed
    s32     otz;     // screen depth of that point
    s32     pad_28;
    SVECTOR spin;    // half-extents of the quad being drawn, turned by the point's angle
    MATRIX  viewRot; // the view rotation, transposed to take view space back to world space
} Actor403600ProjectileScratch;
STATIC_ASSERT_SIZEOF(Actor403600ProjectileScratch, 0x54);

extern const SVECTOR D_actor_403600_80131E24;
extern GpU16Pair     D_actor_403600_801420F0;
extern s32           D_actor_403600_80142120[];

void func_actor_403600_80134398(Task* arg0);

extern TaskDesc      D_actor_403600_801421A0;
extern s32           D_actor_403600_80160698;
extern s32           D_actor_403600_8016069C;
extern s32           D_actor_403600_801606A0;
extern const SVECTOR D_actor_403600_80131E2C;
extern const CVECTOR D_actor_403600_80131E34;

void func_actor_403600_801353D0(ActorEffectState* arg0, GpCoord* arg1);
void func_actor_403600_80132A18(Task* arg0, Actor403600Work* arg1, Actor403600FxWork* arg2, Actor403600FxWork* arg3);
void func_actor_403600_80132E40(Task* arg0, Actor403600Work* arg1, Actor403600FxWork* arg2);

/// Links, at ordering-table depth `otz`, the primitives that copy the frame
/// drawn so far into the 320x240 VRAM rectangle at (0x1C0, 0x100). Linked at
/// one slot they run in reverse: the draw area and offset move to that
/// rectangle, the mask bit is set, a near-black tile clears it and the current
/// draw buffer is copied over it as two 160-pixel 15-bit sprites; then the
/// mask bit is cleared and the draw offset and area return to the draw buffer,
/// clipped to the view's sprite rectangle when its depth lies in front of
/// `otz`. A 0x14-byte block on the scratch stack holds the area and offset for
/// the duration of the call.
void func_actor_403600_801320F8(s32 otz)
{
    u8*                head;
    u8*                allocated;
    ActorsDrawScratch* scratch;
    GpDrawAreaRec*     extra;
    DR_AREA*           area;
    DR_STP*            stp;
    DR_OFFSET*         off;
    SPRT*              sprt;
    DR_TPAGE*          tpage;
    TILE*              tile;
    RECT*              clip;
    u_short*           ofs;
    s32                val;
    s32                z;

    extra              = Gp_GetViewSprtExtra();
    head               = SCRATCH_HEAD(u8);
    area               = (DR_AREA*)gGpuPrimCursor;
    allocated          = head - 0x14;
    SCRATCH_HEAD(void) = allocated;
    gGpuPrimCursor     = (DR_TPAGE*)(area + 1);
    USE_REG(allocated);
    scratch      = (ActorsDrawScratch*)allocated;
    scratch->otz = otz;
    if (extra != NULL) {
        val = (extra->depth << gDisplayState.otDepthShift) & 0x3FFF;
        z   = otz;
        SOFT_TOUCH_REG(z);
        if ((val >> 4) < z) {
            scratch->rect   = extra->rect;
            scratch->rect.y = (u16)scratch->rect.y + gDisplayState.drawBuffer * 0x110;
        } else {
            goto block_4;
        }
    } else {
    block_4:
        scratch->rect.x = 0;
        scratch->rect.y = gDisplayState.drawBuffer * 0x110;
        scratch->rect.w = 0x140;
        scratch->rect.h = 0xF0;
    }
    clip = &scratch->rect;
    SetDrawArea(area, clip);
    addPrim(&gGpuCurrentOt[scratch->otz], area);

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[scratch->otz], stp);

    ofs             = scratch->ofs;
    off             = (DR_OFFSET*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(off + 1);
    scratch->ofs[0] = 0xA0;
    scratch->ofs[1] = gDisplayState.drawBuffer * 0x110 + 0x78;
    SetDrawOffset(off, ofs);
    addPrim(&gGpuCurrentOt[scratch->otz], off);

    sprt           = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(sprt + 1);
    sprt->x0       = -0xA0;
    sprt->y0       = -0x78;
    sprt->w        = 0xA0;
    sprt->h        = 0xF0;
    sprt->u0       = 0;
    sprt->v0       = gDisplayState.drawBuffer * 0x10;
    setlen(sprt, 4);
    setcode(sprt, 0x65);
    addPrim(&gGpuCurrentOt[scratch->otz], sprt);

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0, gDisplayState.drawBuffer << 8));
    addPrim(&gGpuCurrentOt[scratch->otz], tpage);

    sprt           = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(sprt + 1);
    sprt->x0       = 0;
    sprt->y0       = -0x78;
    sprt->w        = 0xA0;
    sprt->h        = 0xF0;
    sprt->u0       = 0x20;
    sprt->v0       = gDisplayState.drawBuffer * 0x10;
    setlen(sprt, 4);
    setcode(sprt, 0x65);
    addPrim(&gGpuCurrentOt[scratch->otz], sprt);

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0x80, gDisplayState.drawBuffer << 8));
    addPrim(&gGpuCurrentOt[scratch->otz], tpage);

    tile           = (TILE*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(tile + 1);
    setlen(tile, 3);
    setcode(tile, 0x60);
    tile->b0 = 2;
    tile->g0 = 2;
    tile->r0 = 2;
    tile->x0 = -0xA0;
    tile->y0 = -0x78;
    tile->w  = 0x140;
    tile->h  = 0xF0;
    addPrim(&gGpuCurrentOt[scratch->otz], tile);

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[scratch->otz], stp);

    off             = (DR_OFFSET*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(off + 1);
    scratch->ofs[0] = 0x260;
    scratch->ofs[1] = 0x178;
    SetDrawOffset(off, ofs);
    addPrim(&gGpuCurrentOt[scratch->otz], off);

    area            = (DR_AREA*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(area + 1);
    scratch->rect.x = 0x1C0;
    scratch->rect.y = 0x100;
    scratch->rect.w = 0x140;
    scratch->rect.h = 0xF0;
    SetDrawArea(area, clip);
    addPrim(&gGpuCurrentOt[scratch->otz], area);

    SCRATCH_POP_BYTES(0x14);
}

void func_actor_403600_801327A0(Actor403600GridQuad* arg0)
{
    s32 temp_a0;
    s32 temp_a1;
    s32 temp_t1;
    s32 temp_t2;
    s32 max;
    s32 min;
    u8  adjust;

    temp_t2 = arg0->u0 + arg0->page0;
    min     = temp_t2;
    max     = temp_t2;
    temp_t1 = arg0->u1 + arg0->page1;
    temp_a1 = arg0->u2 + arg0->page2;
    temp_a0 = arg0->u3 + arg0->page3;
    if (temp_t1 < min) {
        min = temp_t1;
    } else if (max < temp_t1) {
        max = temp_t1;
    }
    if (temp_a1 < min) {
        min = temp_a1;
    } else if (max < temp_a1) {
        max = temp_a1;
    }
    if (temp_a0 < min) {
        min = temp_a0;
    } else if (max < temp_a0) {
        max = temp_a0;
    }
    if ((max >= 0x100) || (adjust = 0, min >= 0x40)) {
        adjust = 0x40;
    }
    arg0->tpage = (s16)(((u32)(adjust + 0x1C0) >> 6) | 0x110);
    arg0->u0    = (u8)(temp_t2 - adjust);
    arg0->u1    = (u8)(temp_t1 - adjust);
    arg0->u2    = (u8)(temp_a1 - adjust);
    arg0->u3    = (u8)(temp_a0 - adjust);
    arg0->page3 = adjust;
    arg0->page2 = adjust;
    arg0->page1 = adjust;
    arg0->page0 = adjust;
}

/* The quad arrives as an integer because the same variable then holds the
 * vertex's screen x: sharing it is what gives x the argument register. The
 * third argument's value is never read; the vertex pointer replaces it. */
void func_actor_403600_8013289C(s32 arg0, s32 arg1, Actor403600GridVertex* arg2, s32 arg3)
{
    s16 temp_t3;
    s16 temp_t5;
    s16 var_v0;
    s16 var_v0_3;
    s32 temp_a1;
    s32 temp_v0;
    s32 temp_v1;
    s32 temp_v1_2;
    s32 var_v0_2;
    s32 var_v0_4;
    s32 var_t0;
    u8  temp_v0_2;
    u8* var_t2;

    switch (arg1) {
        case 0:
            arg2   = (Actor403600GridVertex*)&((Actor403600GridQuad*)arg0)->x0;
            var_t2 = &((Actor403600GridQuad*)arg0)->page0;
            break;
        case 1:
            arg2   = (Actor403600GridVertex*)&((Actor403600GridQuad*)arg0)->x1;
            var_t2 = &((Actor403600GridQuad*)arg0)->page1;
            break;
        case 2:
            arg2   = (Actor403600GridVertex*)&((Actor403600GridQuad*)arg0)->x2;
            var_t2 = &((Actor403600GridQuad*)arg0)->page2;
            break;
        default:
            arg2   = (Actor403600GridVertex*)&((Actor403600GridQuad*)arg0)->x3;
            var_t2 = &((Actor403600GridQuad*)arg0)->page3;
            break;
    }
    SOFT_TOUCH_REG(arg2);
    temp_t3                 = arg2->x;
    temp_t5                 = arg2->y;
    arg0                    = temp_t3 + 0xA0;
    var_t0                  = temp_t5 + 0x78;
    temp_a1                 = (D_actor_403600_80160698 * 5) + 0x71357911;
    D_actor_403600_80160698 = temp_a1;
    if (((temp_a1 >> 0x10) & 0xFFF) < (arg3 + 0x400)) {
        temp_v1                 = (temp_a1 * 5) + 0x71357911;
        arg0                    = temp_t3 + 0x9C;
        arg0                   += (temp_v1 >> 0x10) & 7;
        temp_v0                 = (temp_v1 * 5) + 0x71357911;
        D_actor_403600_80160698 = temp_v0;
        temp_v1_2               = temp_t5 + 0x74;
        var_t0                  = temp_v1_2 + ((temp_v0 >> 0x10) & 7);
    }
    if (var_t0 >= 0xF0) {
        var_v0 = ((u16)arg2->y + 0xEF) - var_t0;
        var_t0 = 0xEF;
        goto block_15;
    }
    var_v0_2 = arg0 < 0x140;
    if (var_t0 < 0) {
        var_v0 = (u16)arg2->y - var_t0;
        var_t0 = 0;
    block_15:
        arg2->y  = var_v0;
        var_v0_2 = arg0 < 0x140;
    }
    if (var_v0_2 == 0) {
        var_v0_3 = ((u16)arg2->x + 0x13F) - arg0;
        arg0     = 0x13F;
        goto block_20;
    }
    var_v0_4 = arg0 < 0x100;
    if (arg0 < 0) {
        var_v0_3 = (u16)arg2->x - arg0;
        arg0     = 0;
    block_20:
        arg2->x  = var_v0_3;
        var_v0_4 = arg0 < 0x100;
    }
    *var_t2 = 0;
    if (var_v0_4 == 0) {
        *var_t2 = 0x40;
    }
    temp_v0_2 = *var_t2;
    arg2->v   = var_t0;
    arg2->u   = (s8)(arg0 - temp_v0_2);
}

void func_actor_403600_80132A18(Task* arg0, Actor403600Work* arg1, Actor403600FxWork* arg2, Actor403600FxWork* arg3)
{
    s32                       fade;
    s32                       x;
    s32                       y;
    s32*                      ot_entry;
    s32                       fade_step;
    s32                       green;
    s32                       red;
    s32                       seed;
    s32                       fade_delta;
    s32                       red_sum;
    u32                       sign;
    TILE*                     tile;
    DR_TPAGE*                 draw_mode;
    Actor403600GridQuad*      poly;
    Actor403600ScreenScratch* scratch;
    s8*                       head;
    Actor403600GridQuad*      previous;
    Actor403600GridQuad*      previous_row;
    Actor403600GridQuad*      previous_top;
    u_long*                   mode_ot;
    u32                       mask;
    u32                       mask_hi;
    u32                       tpage;
    u32                       color;
    s32*                      ot;
    u32                       high, low, high2, low2;

    head               = SCRATCH_HEAD(void) - 0x1C;
    SCRATCH_HEAD(void) = head;
    scratch            = (Actor403600ScreenScratch*)head;
    if (Gp_StateF0.field_4 == 0) {
        seed                    = rand();
        D_actor_403600_80160698 = seed;
        arg2->gridSeed          = seed;
    } else {
        D_actor_403600_80160698 = arg2->gridSeed;
    }
    scratch->offset.vx = 0;
    scratch->offset.vy = 0;
    scratch->offset.vz = 0;
    fade               = arg1->field_708;
    y                  = -0x78;
    fade_delta         = fade - 0xC00;
    fade_step          = fade_delta >> 3;
    sign               = (u32)fade_delta >> 0x1F;
    SCHED_BARRIER();
    x = -0xA0;
    do {
        green = ((((s32)(fade_step + sign) >> 1) + 0x80) & 0xFF) << 8;
        __asm__("addiu %0, %1, 0x7F"
                : "=r"(red_sum)
                : "r"(fade_step), "r"(green));
        __asm__("andi %0, %1, 0xFF" : "=r"(red) : "r"(red_sum));
        SOFT_USE_REG(red);
    next_quad:
        poly                    = (Actor403600GridQuad*)D_actor_403600_8016069C;
        D_actor_403600_8016069C = (s32)(poly + 1);
        previous                = poly - 1;
        if (x == -0xA0) {
            poly->x2 = x;
            poly->y2 = (s16)(y + 0x10);
            func_actor_403600_8013289C((s32)poly, 2, (Actor403600GridVertex*)&scratch->offset, fade);
        } else {
            PRIM_XY_WORD(poly, 2) = PRIM_XY_WORD(previous, 3);
            poly->u2              = previous->u3;
            poly->v2              = previous->v3;
            poly->page2           = previous->page3;
        }
        if (y == -0x78) {
            previous_top = poly - 1;
            if (x == -0xA0) {
                poly->x0 = x;
                poly->y0 = y;
                func_actor_403600_8013289C((s32)poly, 0, (Actor403600GridVertex*)&scratch->offset, fade);
            } else {
                PRIM_XY_WORD(poly, 0) = PRIM_XY_WORD(previous_top, 1);
                poly->u0              = previous_top->u1;
                poly->v0              = previous_top->v1;
                poly->page0           = previous_top->page1;
            }
            poly->x1 = (s16)(x + 0x10);
            poly->y1 = y;
            func_actor_403600_8013289C((s32)poly, 1, (Actor403600GridVertex*)&scratch->offset, fade);
        } else {
            previous_row          = poly - 20;
            PRIM_XY_WORD(poly, 0) = PRIM_XY_WORD(previous_row, 2);
            poly->u0              = previous_row->u2;
            poly->v0              = previous_row->v2;
            poly->page0           = previous_row->page2;
            PRIM_XY_WORD(poly, 1) = PRIM_XY_WORD(previous_row, 3);
            poly->u1              = previous_row->u3;
            poly->v1              = previous_row->v3;
            poly->page1           = previous_row->page3;
        }
        poly->x3 = (s16)(x + 0x10);
        poly->y3 = (s16)(y + 0x10);
        func_actor_403600_8013289C((s32)poly, 3, (Actor403600GridVertex*)&scratch->offset, fade);
        func_actor_403600_801327A0(poly);
        if (fade < 0xC00) {
            ((u8*)&poly->tag)[3] = 9;
            poly->code           = 0x2D;
        } else {
            PRIM_COLOR_WORD(poly, 0) = (green | PRIM_RGBC(0, 0, 0x80, 0)) | red;
            ((u8*)&poly->tag)[3]     = 9;
            poly->code               = 0x2C;
        }
        __asm__ volatile("lui %0, 0xFF; ori %0, %0, 0xFFFF" : "=r"(mask));
        ot = (s32*)gGpuCurrentOt;
        __asm__ volatile("lui %0, 0xFF00" : "=r"(mask_hi));
        scratch->otz = 0;
        high         = poly->tag & mask_hi;
        low          = *ot & mask;
        SOFT_USE_REG2(high, mask);
        poly->tag = high | low;
        x        += 0x10;
        ot_entry  = (s32*)((*(volatile s32*)&scratch->otz * 4) + (s32)ot);
        high2     = *ot_entry & mask_hi;
        low2      = (s32)poly & mask;
        USE_REG3(high2, high2, low2);
        *ot_entry = high2 | low2;
        if (x < 0xA0) {
            goto next_quad;
        }
        y += 0x10;
        x  = -0xA0;
    } while (y < 0x78);
    TOUCH_REG(x);
    if (fade == 0x1000) {
        color                                 = 0x2060C0;
        tpage                                 = 0xE1000000;
        tile                                  = (TILE*)D_actor_403600_8016069C;
        D_actor_403600_8016069C               = (s32)(tile + 1);
        tile->x0                              = -0xA0;
        tile->y0                              = -0x78;
        tile->w                               = 0x140;
        tile->h                               = 0xF0;
        ((u8*)&tile->tag)[3]                  = 3;
        PRIM_COLOR_WORD(tile, 0)              = color;
        tile->code                            = 0x62;
        draw_mode                             = gGpuPrimCursor;
        gGpuPrimCursor                        = draw_mode + 1;
        tile->tag                             = (tile->tag & mask_hi) | (((POLY_FT4*)(gGpuCurrentOt - 1))->tag & mask);
        ((POLY_FT4*)(gGpuCurrentOt - 1))->tag = (((POLY_FT4*)(gGpuCurrentOt - 1))->tag & mask_hi) | ((u32)tile & mask);
        ((u8*)&draw_mode->tag)[3]             = 1;
        mode_ot                               = gGpuCurrentOt - 1;
        draw_mode->code[0]                    = tpage | 0x220;
        draw_mode->tag                        = (draw_mode->tag & mask_hi) | (((POLY_FT4*)(gGpuCurrentOt - 1))->tag & mask);
        ((POLY_FT4*)mode_ot)->tag             = (((POLY_FT4*)mode_ot)->tag & mask_hi) | ((u32)draw_mode & mask);
    }
    func_actor_403600_801320F8(0);
    SCRATCH_POP_BYTES(0x1C);
}

/// Rotates `v` in place by `m`.
static inline void _actor403600RotateSv(MATRIX* m, SVECTOR* v)
{
    SVECTOR in;

    in = *v;
    gte_ApplyMatrixSV(m, &in, v);
}

/// Rotates `in` by `m` into `out`.
static inline void _actor403600ApplyMatrixSv(MATRIX* m, SVECTOR* in, SVECTOR* out)
{
    gte_SetRotMatrix(m);
    gte_ldv0(in);
    gte_rtv0();
    gte_stsv(out);
}

void func_actor_403600_80132E40(Task* arg0, Actor403600Work* arg1, Actor403600FxWork* arg2)
{
    Task*                    actor;
    GpCoord*                 center;
    u8*                      head;
    Actor403600ChainScratch* scratch;
    s32                      i;

    actor  = arg0->parent;
    center = &actor->extra.tmd->coords[8];
    if (Gp_StateF0.field_4 == 0) {
        head    = SCRATCH_HEAD(u8);
        scratch = (Actor403600ChainScratch*)(SCRATCH_HEAD(u8) = head - sizeof(Actor403600ChainScratch));
        Gp_UpdateCoord(&actor->extra.tmd->coords[11]);
        if (arg2->chainsSet == 0) {
            TransposeMatrix(&gGfxViewCoord.workm, &scratch->basis);
            scratch->b.vx = center->workm.t[0] - gGfxViewCoord.workm.t[0];
            scratch->b.vy = center->workm.t[1] - gGfxViewCoord.workm.t[1];
            scratch->b.vz = center->workm.t[2] - gGfxViewCoord.workm.t[2];

            _actor403600RotateSv(&scratch->basis, &scratch->b);

            scratch->a.vx = 0;
            scratch->a.vy = 0;
            scratch->a.vz = -0x485;
            _actor403600RotateSv(&center->workm, &scratch->a);

            _actor403600RotateSv(&scratch->basis, &scratch->a);

            i = 0;
            do {
                arg2->chain[i]     = scratch->b;
                arg2->chain[i].vx += scratch->a.vx * i;
                arg2->chain[i].vy += scratch->a.vy * i;
                arg2->chain[i].vz += scratch->a.vz * i;
                i++;
            } while (i < 4);

            i = 0;
            do {
                GpCoord* limb = &actor->extra.tmd->coords[i * 4 + 15];
                Gp_UpdateCoord(limb);
                scratch->b.vx = limb->workm.t[0] - gGfxViewCoord.workm.t[0];
                scratch->b.vy = limb->workm.t[1] - gGfxViewCoord.workm.t[1];
                scratch->b.vz = limb->workm.t[2] - gGfxViewCoord.workm.t[2];
                _actor403600RotateSv(&scratch->basis, &scratch->b);

                scratch->a.vx = 0;
                scratch->a.vy = 0x898;
                scratch->a.vz = 0;
                _actor403600RotateSv(&center->workm, &scratch->a);

                _actor403600RotateSv(&scratch->basis, &scratch->a);
                arg2->limbTips[i].vx = scratch->b.vx + scratch->a.vx;
                arg2->limbTips[i].vy = scratch->b.vy + scratch->a.vy;
                arg2->limbTips[i].vz = scratch->b.vz + scratch->a.vz;
                i++;
            } while (i < 2);
            arg2->chainsSet += 1;
        } else {
            TransposeMatrix(&gGfxViewCoord.workm, &scratch->basis);
            scratch->b.vx = center->workm.t[0] - gGfxViewCoord.workm.t[0];
            scratch->b.vy = center->workm.t[1] - gGfxViewCoord.workm.t[1];
            scratch->b.vz = center->workm.t[2] - gGfxViewCoord.workm.t[2];

            _actor403600RotateSv(&scratch->basis, &scratch->b);
            arg2->chain[0] = scratch->b;

            scratch->b.vx = 0;
            scratch->b.vy = 0;
            scratch->b.vz = -(arg1->field_70C + 0x200);
            _actor403600RotateSv(&center->workm, &scratch->b);
            _actor403600RotateSv(&scratch->basis, &scratch->b);

            if (arg1->field_70A != 0) {
                gte_lddp(arg1->field_70A);
                gte_ldsv(&scratch->b);
                gte_gpf12();
                gte_stsv(&scratch->drift);
            }

            i = 0;
            do {
                scratch->a.vx   = arg2->chain[i + 1].vx - arg2->chain[i].vx;
                scratch->a.vy   = arg2->chain[i + 1].vy - arg2->chain[i].vy;
                scratch->a.vz   = arg2->chain[i + 1].vz - arg2->chain[i].vz;
                scratch->a.vx  += scratch->b.vx;
                scratch->a.vy  += scratch->b.vy;
                scratch->a.vz  += scratch->b.vz;
                scratch->b.vx >>= 1;
                scratch->b.vy >>= 1;
                scratch->b.vz >>= 1;
                VectorNormalSS(&scratch->a, &scratch->a);
                scratch->dirs[i] = scratch->a;
                gte_lddp(0x485);
                gte_ldsv(&scratch->a);
                gte_gpf12();
                gte_stsv(&scratch->a);
                arg2->chain[i + 1].vx = arg2->chain[i].vx + scratch->a.vx;
                arg2->chain[i + 1].vy = arg2->chain[i].vy + scratch->a.vy;
                arg2->chain[i + 1].vz = arg2->chain[i].vz + scratch->a.vz;
                i++;
            } while (i < 3);

            i = 0;
            do {
                GpCoord* segment = &actor->extra.tmd->coords[i + 9];
                _actor403600ApplyMatrixSv(&gGfxViewCoord.workm, &scratch->dirs[i], &scratch->a);
                TransposeMatrix(&center->workm, &scratch->rot);
                _actor403600RotateSv(&scratch->rot, &scratch->a);
                scratch->b.vx = 0;
                scratch->b.vy = 0x1000;
                scratch->b.vz = 0;
                scratch->a.vx = -scratch->a.vx;
                scratch->a.vy = -scratch->a.vy;
                scratch->a.vz = -scratch->a.vz;
                Gfx_OrthonormalBasis(&scratch->basis, &scratch->a, &scratch->b);
                gte_MulMatrix0(&scratch->rot, &segment->workm, &scratch->rot);
                gte_MulMatrix0(&scratch->basis, &scratch->rot, &scratch->rot);
                gte_MulMatrix0(&center->workm, &scratch->rot, &scratch->rot);
                TransposeMatrix(&segment->sub->workm, &scratch->basis);
                gte_MulMatrix0(&scratch->basis, &scratch->rot, &segment->coord);
                segment->flg = 0;
                Gp_UpdateCoord(segment);
                i++;
            } while (i < 3);

            i = 0;
            do {
                GpCoord* limb = &actor->extra.tmd->coords[i * 4 + 15];
                TransposeMatrix(&gGfxViewCoord.workm, &scratch->basis);
                Gp_UpdateCoord(limb);
                scratch->b.vx = limb->workm.t[0] - gGfxViewCoord.workm.t[0];
                scratch->b.vy = limb->workm.t[1] - gGfxViewCoord.workm.t[1];
                scratch->b.vz = limb->workm.t[2] - gGfxViewCoord.workm.t[2];
                _actor403600RotateSv(&scratch->basis, &scratch->b);

                scratch->a.vx = 0;
                scratch->a.vy = arg1->field_70E + 0x200;
                scratch->a.vz = 0;
                _actor403600RotateSv(&limb->workm, &scratch->a);
                _actor403600RotateSv(&scratch->basis, &scratch->a);

                scratch->a.vx += arg2->limbTips[i].vx - scratch->b.vx;
                scratch->a.vy += arg2->limbTips[i].vy - scratch->b.vy;
                scratch->a.vz += arg2->limbTips[i].vz - scratch->b.vz;
                if (arg1->field_70A != 0) {
                    scratch->a.vx += scratch->drift.vx;
                    scratch->a.vy += scratch->drift.vy;
                    scratch->a.vz += scratch->drift.vz;
                }
                VectorNormalSS(&scratch->a, &scratch->a);
                scratch->dirs[i] = scratch->a;
                gte_lddp(0x898);
                gte_ldsv(&scratch->a);
                gte_gpf12();
                gte_stsv(&scratch->a);
                arg2->limbTips[i].vx = scratch->b.vx + scratch->a.vx;
                arg2->limbTips[i].vy = scratch->b.vy + scratch->a.vy;
                arg2->limbTips[i].vz = scratch->b.vz + scratch->a.vz;

                _actor403600ApplyMatrixSv(&gGfxViewCoord.workm, &scratch->dirs[i], &scratch->a);
                TransposeMatrix(&limb->workm, &scratch->rot);
                _actor403600RotateSv(&scratch->rot, &scratch->a);
                scratch->b.vx = 0;
                scratch->b.vy = 0;
                scratch->b.vz = 0x1000;
                Gfx_OrthonormalBasis(&scratch->basis, &scratch->a, &scratch->b);
                gte_ReadMatrixColumn(&scratch->basis, 2, &scratch->b);

                scratch->basis.m[0][0] = -scratch->basis.m[0][0];
                scratch->basis.m[1][0] = -scratch->basis.m[1][0];
                scratch->basis.m[2][0] = -scratch->basis.m[2][0];
                scratch->basis.m[0][2] = scratch->basis.m[0][1];
                scratch->basis.m[1][2] = scratch->basis.m[1][1];
                scratch->basis.m[2][2] = scratch->basis.m[2][1];
                scratch->basis.m[0][1] = scratch->b.vx;
                scratch->basis.m[1][1] = scratch->b.vy;
                scratch->basis.m[2][1] = scratch->b.vz;

                gte_MulMatrix0(&scratch->basis, &limb->coord, &limb->coord);
                limb->flg = 0;
                Gp_UpdateCoord(limb);
                i++;
            } while (i < 2);
        }
        SCRATCH_POP_BYTES(sizeof(Actor403600ChainScratch));
    }
}

void func_actor_403600_80134288(Task* arg0)
{
    Task*                       temp_v0_2;
    Actor403600FxWork*          temp_a3;
    register Actor403600FxWork* temp_v0 asm("a3");
    register Actor403600Work*   var_a2 asm("a2");

    var_a2 = (Actor403600Work*)arg0->parent->work;
    if (arg0->state == 0) {
        temp_v0 = memCalloc(0x11C, false);
        if (temp_v0 == NULL) {
            Task_CallExit(arg0);
            return;
        }
        gGameSession->field_80 = 0;
        arg0->work             = temp_v0;
        temp_v0_2              = Task_SpawnFromTable(&D_actor_403600_801421A0, 2, 0, 0);
        if (temp_v0_2 != NULL) {
            Task_Reparent(arg0, temp_v0_2);
        }
        var_a2                  = (Actor403600Work*)arg0->parent->work;
        var_a2->field_710       = arg0;
        D_actor_403600_801606A0 = 0;
        arg0->state            += 1;
        goto block_6;
    }
block_6:
    temp_a3                 = arg0->work;
    D_actor_403600_8016069C = (s32)D_8005C374 + (gDisplayState.otBuffer * 0xC000);
    {
        register s32 field_742 asm("v1");

        field_742 = var_a2->field_742;
        if ((field_742 != 1) && (var_a2->field_708 > 0)) {
            func_actor_403600_80132A18(arg0, var_a2, temp_a3, temp_a3);
        }
    }
}
const SVECTOR D_actor_403600_80131E24 = { -100, 700, -280, 0 };

/// Links primitive `p` at the head of ordering-table entry `ot`, as `addPrim`
/// does, but by masking the two tag words directly instead of through its
/// bitfields.
#define _ACTOR403600_LINK_PRIM(ot, p)                         \
    ((p)->tag = ((p)->tag & 0xFF000000) | (*(ot) & 0xFFFFFF), \
     *(ot)    = (*(ot) & 0xFF000000) | ((u32)(p) & 0xFFFFFF))

void func_actor_403600_80134398(Task* arg0)
{
    MATRIX*       gteValue1;
    SVECTOR*      gteValue2;
    DVECTOR*      gteValue3;
    s32*          gteValue4;
    s32*          gteValue5;
    s32*          gteValue6;
    s32           gteValue7;
    SVECTOR       sp10;
    SVECTOR       sp18;
    SVECTOR*      firstVector;
    SVECTOR*      cameraVector;
    Task*         player;
    s32           sp24;
    s32           sp28;
    DisplayState* ds;
    s16           temp_s0_6;
    s16           temp_v1_10;
    s16           temp_v1_11;
    s16           temp_v1_8;
    s16           temp_v1_9;
    s32           temp_a0_4;
    s32           temp_v1_13;
    s32           temp_v1_6;
    s32           var_a0;
    GpCoord*      var_a1_2;
    s32           var_fp;
    s32           var_s4;
    s32           temp_s0_3;
    s32           temp_s0_5;
    s8            temp_v1_12;
    u16           temp_v1_3;
    s32           var_v0;
    s32           historyDst, historySrc;
    SVECTOR*      historyOut;
    s32           weightedX, weightedY, weightedZ;
    s32           scratchX, scratchY, scratchZ;
    s32           velocityX, velocityY, velocityZ;
    u8            temp_v0_5;
    u8            temp_v1_4;
    u8            temp_v1_5;
    u8            temp_v1_7;
    GpCoord*      ownerCoord;
    Task*         motionParent;
    Task*         temp_a0_3;
    GpObj*        obj;
    GpRec18*      recs;
    SVECTOR*      temp_s0_4;
    SVECTOR*      temp_s1;
    GpActorD4Rec* newShape;
    GpCoord*      target;
    GpCoord*      view;
    GpActorD4Rec* shape;
    /* One local holds the owner's work block, then the steering pass count,
     * then the address of the primitive being written: all three share a
     * single register allocation. */
    s32                           shared;
    Task*                         owner;
    Actor403600ProjectileWork*    work;
    GpCoord*                      coord;
    Actor403600ProjectileScratch* scratch;
    Actor403600ProjectileWork*    newWork;
    SVECTOR*                      temp_v0_4;
    SVECTOR*                      temp_v1;
    SVECTOR*                      point;

    coord  = arg0->extra.tmd->coords;
    sp10   = D_actor_403600_80131E24;
    player = Gp_ActorSlots[0];
    if (player == NULL) {
        Task_CallExit(arg0);
        return;
    }
    if (((Actor403600Work*)((Task*)arg0->spawnArg2)->work)->field_742 == 1) {
        Task_CallExit(arg0);
        return;
    }
    SCRATCH_PUSH(Actor403600ProjectileScratch);
    scratch = SCRATCH_HEAD(Actor403600ProjectileScratch);
    if (arg0->state == 0) {
        newWork = memCalloc(0x15C, 0);
        if (newWork == NULL) {
            Task_CallExit(arg0);
            SCRATCH_POP(Actor403600ProjectileScratch);
            return;
        }
        arg0->work          = newWork;
        coord->sub          = &gGfxViewCoord;
        coord->coord.t[2]   = 0;
        coord->coord.t[1]   = 0;
        coord->coord.t[0]   = 0;
        coord->flg          = 0;
        owner               = arg0->spawnArg2;
        arg0->killCountdown = 1;
        arg0->status        = 1;
        arg0->extraState    = 0;
        if (owner == NULL) {
            newWork->velocity.vx = 0;
            newWork->velocity.vy = -0x1000;
            newWork->velocity.vz = 0;
        } else {
            shared               = (s32)owner->work;
            newWork->velocity.vy = -0x1B8;
            firstVector          = &sp18;
            temp_s1              = &newWork->velocity;

            newWork->velocity.vx = 0;
            newWork->velocity.vz = 0x4B0;
            sp18                 = newWork->velocity;
            ownerCoord           = &((Actor403600Work*)shared)->field_4B8;
            shared               = (s32) & ((Actor403600Work*)shared)->field_4B8.coord;
            gte_SetRotMatrix(shared);
            gte_ldv0(firstVector);
            gte_rtv0();
            gte_stsv(temp_s1);
            coord->coord.t[0]    = ownerCoord->coord.t[0] + newWork->velocity.vx;
            coord->coord.t[1]    = ownerCoord->coord.t[1] + newWork->velocity.vy;
            coord->coord.t[2]    = ownerCoord->coord.t[2] + newWork->velocity.vz;
            newWork->velocity.vx = (s16)((rand() & 0x1FF) - 0x100);
            newWork->velocity.vy = (s16)((rand() & 0x1FF) - 0x100);
            newWork->velocity.vz = 0x1000;
            sp18                 = newWork->velocity;
            gte_SetRotMatrix(shared);
            gte_ldv0(firstVector);
            gte_rtv0();
            gte_stsv(temp_s1);

            if (arg0->spawnArg1 == 0x1100) {
                Gp_CopyCoordOffset(arg0, &owner->extra.tmd->coords[14], &sp10);
            } else {
                Gp_CopyCoordOffset(arg0, &owner->extra.tmd->coords[18], &sp10);
            }
            if (arg0->spawnArg1 < 0x1000) {
                Gp_SpawnEff(0x601BB, coord, 0x20, 0);
            }
            arg0->status        = 3;
            arg0->killCountdown = 0x20;
        }
        var_s4 = 0;
        if (arg0->spawnArg1 >= 0x1000) {
            arg0->status = 4;
        }
        do {
            newWork->trail[var_s4].vx = (u16)coord->coord.t[0];
            newWork->trail[var_s4].vy = (u16)coord->coord.t[1];
            newWork->trail[var_s4].vz = (u16)coord->coord.t[2];
            var_s4                   += 1;
        } while (var_s4 < 0x20);
        newShape = &newWork->shape;
        if (arg0->spawnArg1 < 0x1000) {
            obj                    = &newWork->obj;
            obj->coord             = coord;
            obj->ctx.d4rec         = newShape;
            obj->pos.vx            = 0;
            obj->pos.vy            = 0;
            obj->pos.vz            = 0;
            obj->radius            = 0;
            recs                   = newWork->recs;
            obj->key               = Gp_PackPair(&D_actor_403600_801420F0, arg0->spawnArg1 & 0xF);
            obj->flags             = 3;
            newShape->recs         = recs;
            newShape->end1.vx      = 0;
            newShape->end1.vy      = 0;
            newShape->end1.vz      = 0;
            newWork->shape.end0.vx = 0;
            newShape->end0.vy      = 0;
            newShape->end0.vz      = 0;
            newShape->end0Radius   = 0xC8;
            newShape->end1Radius   = 0xC8;
            Gp_InitRec18Table(recs, 1, 0);
            Gp_LinkObj(3, obj);
            obj->flags         = obj->flags | 0xC000;
            arg0->exitCallback = func_actor_403600_80138C68;
        }
        newWork->life = 0x12C;
        arg0->state   = arg0->state + 1;
        goto block_22;
    }
block_22:
    work   = arg0->work;
    target = &player->extra.tmd->coords[1];
    Gp_UpdateCoord(target);
    TransposeMatrix(&gGfxViewCoord.workm, &scratch->viewRot);
    view            = &gGfxViewCoord;
    var_s4          = target->workm.t[0] - view->workm.t[0];
    scratch->dir.vx = (s16)var_s4;
    var_s4          = target->workm.t[1] - view->workm.t[1];
    scratch->dir.vy = (s16)var_s4;
    shape           = &work->shape;
    cameraVector    = &sp18;
    temp_v1         = &scratch->dir;
    var_s4          = target->workm.t[2] - view->workm.t[2];
    scratch->dir.vz = (s16)var_s4;
    *cameraVector   = scratch->dir;
    gteValue1       = &scratch->viewRot;
    gte_SetRotMatrix(gteValue1);
    gte_ldv0(cameraVector);
    gte_rtv0();
    gte_stsv(temp_v1);
    scratch->target.vx = scratch->dir.vx;
    scratch->target.vy = scratch->dir.vy;
    scratch->target.vz = scratch->dir.vz;
    if (Gp_StateF0.field_4 == 0) {
        if (arg0->spawnArg1 < 0x1000) {
            work->life = work->life - 1;
        } else {
            arg0->killCountdown = (u16)arg0->killCountdown + 1;
        }
        var_s4 = 0;
        do {
            work->trail[31 - var_s4] = work->trail[30 - var_s4];
            var_s4                  += 1;
        } while (var_s4 < 0x1F);
        arg0->extraState   ^= 1;
        temp_v1_3           = (u16)arg0->killCountdown - 1;
        arg0->killCountdown = temp_v1_3;
        if ((temp_v1_3 << 0x10) <= 0) {
            temp_v1_4 = arg0->status;
            if (temp_v1_4 == 2) {
                Task_CallExit(arg0);
            } else {
                if (temp_v1_4 == 3) {
                    temp_s0_3 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(0x54160009, temp_s0_3, (s8)gpGetObjDepth(coord));
                    var_v0 = (rand() & 0xF) + 0x10;
                    goto block_34;
                }
                var_v0 = 0x7FFF;
                if (temp_v1_4 == 0) {
                block_34:
                    arg0->killCountdown = var_v0;
                    arg0->status        = 1;
                } else {
                    arg0->killCountdown = (u16)((rand() & 0xF) + 0x10);
                    arg0->status        = (u8)(arg0->status ^ 1);
                }
                goto block_36;
            }
        } else {
        block_36:
            if (arg0->status != 2) {
                shared    = 0;
                temp_s0_4 = &scratch->dir;
                do {
                    if (arg0->status == 0) {
                        scratch->dir.vx = (s16)((scratch->target.vx - coord->coord.t[0]) >> 2);
                        scratch->dir.vy = (s16)((s32)(scratch->target.vy - coord->coord.t[1]) >> 2);
                        scratch->dir.vz = (s16)((s32)(scratch->target.vz - coord->coord.t[2]) >> 2);
                        VectorNormalSS(temp_s0_4, temp_s0_4);
                        /* Keep scratch loads live across the weighted shifts. */
                        velocityX = work->velocity.vx;
                        weightedX = velocityX * 8;
                        scratchX  = scratch->dir.vx;
                        SOFT_TOUCH_REG_USE(weightedX, scratchX);
                        weightedX      -= velocityX;
                        scratch->dir.vx = (scratchX + weightedX) >> 4;
                        velocityY       = work->velocity.vy;
                        weightedY       = velocityY * 8;
                        scratchY        = scratch->dir.vy;
                        SOFT_TOUCH_REG_USE(weightedY, scratchY);
                        weightedY      -= velocityY;
                        scratch->dir.vy = (scratchY + weightedY) >> 4;
                        velocityZ       = work->velocity.vz;
                        weightedZ       = velocityZ * 8;
                        scratchZ        = scratch->dir.vz;
                        SOFT_TOUCH_REG_USE(weightedZ, scratchZ);
                        weightedZ      -= velocityZ;
                        scratch->dir.vz = (scratchZ + weightedZ) >> 4;
                        VectorNormalSS(temp_s0_4, temp_s0_4);
                        work->velocity.vx = (s16)(u16)scratch->dir.vx;
                        work->velocity.vy = (s16)(u16)scratch->dir.vy;
                        work->velocity.vz = (s16)(u16)scratch->dir.vz;
                    }
                    temp_v1_5 = arg0->status;
                    if (temp_v1_5 < 3U) {
                        gte_lddp(100);
                        gteValue2 = &work->velocity;
                        gte_ldsv(gteValue2);
                        gte_gpf12();
                        gte_stsv(temp_s0_4);
                        shape->end1.vx    = (s16) - (s16)(u16)scratch->dir.vx;
                        shape->end1.vy    = (s16) - (s16)(u16)scratch->dir.vy;
                        shape->end1.vz    = (s16) - (s16)(u16)scratch->dir.vz;
                        coord->coord.t[0] = coord->coord.t[0] + scratch->dir.vx;
                        coord->coord.t[1] = coord->coord.t[1] + scratch->dir.vy;
                        coord->coord.t[2] = coord->coord.t[2] + scratch->dir.vz;
                        goto block_51;
                    }
                    if (temp_v1_5 == 3) {
                        motionParent = arg0->spawnArg2;
                        if ((s16)arg0->killCountdown >= 7) {
                            var_a1_2 = &motionParent->extra.tmd->coords[18];
                            Gp_CopyCoordOffset(arg0, var_a1_2, &sp10);
                            var_s4 = 0;
                            do {
                                work->trail[var_s4].vx = (u16)coord->coord.t[0];
                                work->trail[var_s4].vy = (u16)coord->coord.t[1];
                                work->trail[var_s4].vz = (u16)coord->coord.t[2];
                                var_s4                += 1;
                            } while (var_s4 < 0x20);
                            shared += 1;
                        } else {
                            goto block_51;
                        }
                    } else {
                        temp_a0_3 = arg0->spawnArg2;
                        if (arg0->spawnArg1 == 0x1000) {
                            Gp_CopyCoordOffset(arg0, &temp_a0_3->extra.tmd->coords[18], &sp10);
                        } else {
                            Gp_CopyCoordOffset(arg0, &temp_a0_3->extra.tmd->coords[14], &sp10);
                        }
                    block_51:
                        shared += 1;
                    }
                } while (shared < 2);
            }
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            work->trail[0].vx  = (u16)coord->coord.t[0];
            work->trail[0].vy  = (u16)coord->coord.t[1];
            work->trail[0].vz  = (u16)coord->coord.t[2];
            work->trail[0].pad = rand();
            goto block_54;
        }
    } else {
    block_54:
        if ((arg0->spawnArg1 < 0x1000) && (Gp_FindRec18(work->recs, 0) != 0)) {
            temp_s0_5 = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(0x5416000A, temp_s0_5, (s8)gpGetObjDepth(coord));
            work->life = -1;
        }
        if (work->life < 0) {
            if (arg0->spawnArg1 < 0x1000) {
                Gp_ClearRec18Occupied(work->recs);
                work->obj.flags = work->obj.flags & 0x3FFF;
            }
            work->life          = 0x7FFFFFFF;
            arg0->status        = 2;
            arg0->killCountdown = 0x20;
        }
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        temp_v1_6 = arg0->spawnArg1 & 0xF;
        switch (temp_v1_6) {
            case 0:
                sp24 = 0x808000;
                break;
            case 1:
                sp24 = 0x808080;
                break;
            case 2:
                sp24 = 0x80;
                break;
            default:
                sp24 = 0x8080;
                break;
        }
        sp28      = 1;
        temp_v1_7 = arg0->status;
        var_fp    = 0;
        if (temp_v1_7 == 2) {
            var_fp = 0x20 - (s16)arg0->killCountdown;
        } else if (temp_v1_7 == 3) {
            sp28 = 4;
        }
        var_s4 = var_fp;
        if (var_s4 < 0x20) {
            ds = &gDisplayState;
            do {
                shared                  = D_actor_403600_8016069C;
                D_actor_403600_8016069C = shared + sizeof(POLY_FT4);
                point                   = &work->trail[var_s4];
                gte_ldv0(point);
                gte_rtps();
                gteValue3 = &scratch->sxy;
                gte_stsxy(gteValue3);
                gteValue4 = &scratch->dp;
                gte_stdp(gteValue4);
                gteValue5 = &scratch->flag;
                gte_stflg(gteValue5);
                gteValue6 = &scratch->otz;
                gte_stszotz(gteValue6);
                if (scratch->flag >= 0) {
                    if (var_s4 == 0) {
                        if (arg0->status != 2) {
                            temp_a0_4 = scratch->otz;
                            if (temp_a0_4 >= 0) {
                                scratch->spin.vx = (u16)((s32)(ds->screenDistance * 0x96) / temp_a0_4);
                            } else {
                                scratch->spin.vx = 0x1000U;
                            }
                            temp_v1_8                  = (u16)scratch->sxy.vx - (u16)scratch->spin.vx;
                            ((POLY_FT4*)shared)->x2    = temp_v1_8;
                            ((POLY_FT4*)shared)->x0    = temp_v1_8;
                            temp_v1_9                  = (u16)scratch->sxy.vx + (u16)scratch->spin.vx;
                            ((POLY_FT4*)shared)->x3    = temp_v1_9;
                            ((POLY_FT4*)shared)->x1    = temp_v1_9;
                            temp_v1_10                 = (u16)scratch->sxy.vy - (u16)scratch->spin.vx;
                            ((POLY_FT4*)shared)->y1    = temp_v1_10;
                            ((POLY_FT4*)shared)->y0    = temp_v1_10;
                            temp_v1_11                 = (u16)scratch->sxy.vy + (u16)scratch->spin.vx;
                            ((POLY_FT4*)shared)->tpage = 0x29;
                            ((POLY_FT4*)shared)->y3    = temp_v1_11;
                            ((POLY_FT4*)shared)->y2    = temp_v1_11;
                            if (arg0->extraState != 0) {
                                ((POLY_FT4*)shared)->u2   = 0x70U;
                                ((POLY_FT4*)shared)->u0   = 0x70U;
                                ((POLY_FT4*)shared)->clut = 0x428B;
                            } else {
                                ((POLY_FT4*)shared)->u2   = 0xA8U;
                                ((POLY_FT4*)shared)->u0   = 0xA8U;
                                ((POLY_FT4*)shared)->clut = 0x428C;
                            }
                            ((POLY_FT4*)shared)->v1                 = 0xC9;
                            ((POLY_FT4*)shared)->v0                 = 0xC9;
                            ((POLY_FT4*)shared)->v3                 = 0xFF;
                            ((POLY_FT4*)shared)->v2                 = 0xFF;
                            PRIM_COLOR_WORD(((POLY_FT4*)shared), 0) = sp24;
                            temp_v1_12                              = ((POLY_FT4*)shared)->u0 + 0x37;
                            setlen((POLY_FT4*)shared, 9);
                            ((POLY_FT4*)shared)->code = 0x2E;
                            ((POLY_FT4*)shared)->u3   = temp_v1_12;
                            ((POLY_FT4*)shared)->u1   = temp_v1_12;
                            addPrim((u_long*)(((((u32)scratch->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                    (POLY_FT4*)shared);
                        }
                    } else if (var_s4 >= (var_fp - 4)) {
                        temp_s0_6        = (u16)point->pad;
                        scratch->spin.vx = rsin(temp_s0_6);
                        scratch->spin.vy = rcos(temp_s0_6);
                        scratch->spin.vz = 0;
                        if (scratch->otz >= 0) {
                            var_a0 = (var_s4 * 2) + 0x78;
                            if ((var_fp >= var_s4) && (arg0->status == 2)) {
                                var_a0 = (var_s4 * 20) + 0x78;
                            } else if (arg0->status == 4) {
                                var_a0 *= 2;
                            }
                            gteValue7 = (var_a0 * ds->screenDistance) / scratch->otz;
                            gte_lddp(gteValue7);
                            temp_v0_4 = &scratch->spin;
                            gte_ldsv(temp_v0_4);
                            gte_gpf12();
                            gte_stsv(temp_v0_4);
                        }
                        ((POLY_FT4*)shared)->x0    = (s16)((u16)scratch->sxy.vx + (u16)scratch->spin.vx);
                        ((POLY_FT4*)shared)->y0    = (s16)((u16)scratch->sxy.vy + (u16)scratch->spin.vy);
                        ((POLY_FT4*)shared)->x1    = (s16)((u16)scratch->sxy.vx + (u16)scratch->spin.vy);
                        ((POLY_FT4*)shared)->y1    = (s16)((u16)scratch->sxy.vy - (u16)scratch->spin.vx);
                        ((POLY_FT4*)shared)->x2    = (s16)((u16)scratch->sxy.vx - (u16)scratch->spin.vy);
                        ((POLY_FT4*)shared)->y2    = (s16)((u16)scratch->sxy.vy + (u16)scratch->spin.vx);
                        ((POLY_FT4*)shared)->x3    = (s16)((u16)scratch->sxy.vx - (u16)scratch->spin.vx);
                        ((POLY_FT4*)shared)->y3    = (s16)((u16)scratch->sxy.vy - (u16)scratch->spin.vy);
                        temp_v1_13                 = (u8)(u16)point->pad & 0x20;
                        ((POLY_FT4*)shared)->v1    = 0x18;
                        ((POLY_FT4*)shared)->v0    = 0x18;
                        ((POLY_FT4*)shared)->v3    = 0x37;
                        ((POLY_FT4*)shared)->v2    = 0x37;
                        temp_v0_5                  = temp_v1_13 + 0x60;
                        temp_v1_13                += 0x7F;
                        ((POLY_FT4*)shared)->u2    = temp_v0_5;
                        ((POLY_FT4*)shared)->u0    = temp_v0_5;
                        ((POLY_FT4*)shared)->u3    = temp_v1_13;
                        ((POLY_FT4*)shared)->u1    = temp_v1_13;
                        ((POLY_FT4*)shared)->tpage = 0x2A;
                        ((POLY_FT4*)shared)->clut  = 0x42CC;
                        var_a0                     = var_s4;
                        if (arg0->status == 2) {
                            if (var_fp >= var_s4) {
                                var_a0 = var_fp;
                            }
                        }
                        temp_v1_13 = D_actor_403600_80142120[var_a0];
                        setlen((POLY_FT4*)shared, 9);
                        PRIM_COLOR_WORD(((POLY_FT4*)shared), 0) = temp_v1_13;
                        ((POLY_FT4*)shared)->code               = 0x2E;
                        _ACTOR403600_LINK_PRIM((u32*)(((((u32)scratch->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                               (POLY_FT4*)shared);
                    }
                }
                var_s4 += sp28;
            } while (var_s4 < 0x20);
        }
    }
    SCRATCH_POP(Actor403600ProjectileScratch);
}

void func_actor_403600_801353D0(ActorEffectState* arg0, GpCoord* arg1)
{
    s32                       radii[16];
    s32                       heights[16];
    s32                       corner[4];
    s32                       i;
    s32                       j;
    s32                       firstAngle;
    s32                       angle;
    s32                       index;
    s32                       value;
    s32                       firstRadius;
    s32                       rotation;
    s32                       mirrorXY;
    s32                       projectedX;
    s32                       projectedY;
    s32                       screenX;
    s32                       screenY;
    s32                       min;
    s32                       max;
    s32                       adjust;
    s32                       radiusOffset;
    s32                       scale;
    s32                       scanCount;
    s32                       otz;
    s32*                      height;
    MATRIX*                   matrix;
    s32*                      heightBase;
    s32*                      scan;
    u32                       mask;
    u32                       maskHi;
    u_long*                   ot;
    u16                       oldY;
    u8*                       head;
    u8*                       newHead;
    Actor403600GridQuad*      after;
    Actor403600GridQuad*      previous;
    Actor403600GridQuad*      mirror;
    Actor403600GridQuad*      poly;
    SVECTOR*                  vec;
    Actor403600EffectScratch* scratch;

    head               = SCRATCH_HEAD(u8);
    newHead            = head - 0x78;
    SCRATCH_HEAD(void) = newHead;
    scratch            = (Actor403600EffectScratch*)newHead;
    Gp_UpdateCoord(arg1);
    gte_SetRotMatrix(&arg1->workm);
    gte_SetTransMatrix(&arg1->workm);

    scratch->points[1].vz = 0x1000;
    scratch->points[2].vx = 0x1000;
    scratch->points[0].vx = 0;
    scratch->points[0].vy = 0;
    scratch->points[0].vz = 0;
    scratch->points[1].vx = 0;
    scratch->points[1].vy = 0;
    scratch->points[2].vy = 0;
    scratch->points[2].vz = 0;
    gte_ldv3(&scratch->points[0], &scratch->points[1], &scratch->points[2]);
    gte_rtpt();
    gte_stsxy3(&scratch->sxy3[0], &scratch->sxy3[1], &scratch->sxy3[2]);
    gte_stdp(&scratch->dp);
    gte_stflg(&scratch->flag);
    gte_stszotz(&scratch->otz);
    gte_nclip();
    gte_stopz(&scratch->nclip);

    i = 0;
    do {
        firstAngle  = arg0->field_80 + i * 2;
        index       = firstAngle % 32;
        firstRadius = (rsin(arg0->field_0[index]) * arg0->field_40[index]) >> 12;
        value       = (firstRadius * (16 - i)) / 16;
        firstRadius = value >> 3;
        if (arg0->field_E0 == 1) {
            firstRadius = value >> 8;
        }
        radii[i]   = firstRadius;
        value      = (arg0->field_40[index] * (15 - i)) >> 10;
        heights[i] = value;
        if (scratch->nclip > 0) {
            heights[i] = -value;
        }
        i++;
        j = 0;
    } while (i < 16);

    scratch->maxOtz = 0;
    matrix          = &scratch->matrix;
    vec             = &scratch->vec;
    heightBase      = heights;
    mask            = 0xFFFFFF;
    do {
        scratch->matrix = arg1->workm;
        Gfx_RotMatrixY(matrix, (j << 12) / 12, 0);
        gte_SetTransMatrix(&arg1->workm);
        gte_SetRotMatrix(matrix);
        scale        = 0x14;
        i            = 0;
        height       = heightBase;
        radiusOffset = 0;
        angle        = arg0->field_80;
        do {
            angle                   %= 32;
            poly                     = (Actor403600GridQuad*)D_actor_403600_8016069C;
            D_actor_403600_8016069C += sizeof(POLY_FT4);
            rotation                 = -rcos(arg0->field_0[angle]) >> 3;
            scratch->vec.vx          = rsin(rotation);
            scratch->vec.vy          = rcos(rotation);
            scratch->vec.vz          = 0;
            gte_ldv0(vec);
            gte_rtv0();
            scratch->projected.vx = scale;
            scratch->projected.vz = 0;
            /* A byte offset stepped beside `i`: indexing `radii` by `i` frees
             * that register and moves the allocation of the whole loop. */
            scratch->projected.vy = *(s32*)((u8*)radii + radiusOffset);
            gte_stsv(vec);
            gte_ldv0(&scratch->projected);
            gte_rtps();
            gte_stsxy(&scratch->sxy);
            gte_stdp(&scratch->dp);
            gte_stflg(&scratch->flag);
            gte_stszotz(&scratch->otz);
            gte_lddp(*height);
            gte_ldsv(vec);
            gte_gpf12();
            gte_stsv(vec);

            PRIM_XY_WORD(poly, 0) = scratch->sxy;
            oldY                  = poly->y0;
            projectedX            = scratch->vec.vx + 0xA0;
            screenX               = (s16)poly->x0 + projectedX;
            projectedY            = scratch->vec.vy + 0x78;
            screenY               = (s16)poly->y0 + projectedY;
            if (screenY >= 0xF0) {
                poly->y0 = oldY + 0xEF - screenY;
                screenY  = 0xEF;
            } else if (screenY < 0) {
                poly->y0 = oldY - screenY;
                screenY  = 0;
            }
            if (screenX >= 0x140) {
                poly->x0 = (u16)poly->x0 + 0x13F - screenX;
            } else if (screenX < 0) {
                poly->x0 = (u16)poly->x0 - screenX;
                screenX  = 0;
            }
            poly->page0 = 0;
            if (screenX >= 0x100) {
                poly->page0 = 0x40;
            }
            poly->v0 = screenY;
            poly->u0 = screenX - poly->page0;
            /* The next height is read at a byte offset scaled apart from the
             * base: indexing `heightBase[i + 1]` folds the +1 into the load. */
            if (scratch->flag >= 0 && i != 15 && (*height != 0 || (index = i + 1, index *= 4, *(s32*)((s32)heightBase + index) != 0))) {
                setlen(poly, 9);
                poly->code   = 0x2D;
                scratch->otz = (scratch->otz << gDisplayState.otDepthShift & 0x3FFF) >> 4;
                if (scratch->maxOtz < scratch->otz) {
                    scratch->maxOtz = scratch->otz;
                }
                ot     = gGpuCurrentOt;
                otz    = scratch->otz;
                maskHi = 0xFF000000;
                SOFT_TOUCH_REG_USE(maskHi, otz);
                poly->tag        = (poly->tag & maskHi) | (ot[otz] & mask);
                ot[scratch->otz] = (ot[scratch->otz] & maskHi) | ((u32)poly & mask);
            }
            previous = poly - 1;
            if (i != 0) {
                PRIM_XY_WORD(previous, 1) = PRIM_XY_WORD(poly, 0);
                previous->u1              = poly->u0;
                do {
                    previous->v1    = poly->v0;
                    previous->page1 = poly->page0;
                    if (j != 0) {
                        mirrorXY = PRIM_XY_WORD(previous, 0);
                        mirror   = poly - 17;
                    } else {
                        mirrorXY = PRIM_XY_WORD(previous, 0);
                        mirror   = poly + 175;
                    }
                    PRIM_XY_WORD(mirror, 2) = mirrorXY;
                    mirror->u2              = previous->u0;
                } while (0);
                mirror->v2              = previous->v0;
                mirror->page2           = previous->page0;
                PRIM_XY_WORD(mirror, 3) = PRIM_XY_WORD(previous, 1);
                mirror->u3              = previous->u1;
                mirror->v3              = previous->v1;
                mirror->page3           = previous->page1;
            }
            height++;
            radiusOffset += 4;
            i++;
            angle += 2;
            scale += 0x9B;
        } while (i < 16);
        j++;
    } while (j < 12);

    j = 0;
    do {
        i = 0;
        /* Walked one quad ahead of the quad it adjusts, so every field is
         * reached at a negative displacement, as the build requires. */
        after = poly + 1;
        do {
            corner[0] = after[-1].u0 + after[-1].page0;
            corner[1] = after[-1].u1 + after[-1].page1;
            corner[2] = after[-1].u2 + after[-1].page2;
            corner[3] = after[-1].u3 + after[-1].page3;
            min       = corner[0];
            max       = corner[0];
            for (scanCount = 1; scanCount < 4; scanCount++) {
                if (corner[scanCount] < min) {
                    min = corner[scanCount];
                } else if (max < corner[scanCount]) {
                    max = corner[scanCount];
                }
            }
            if (max >= 0x100 || min >= 0x40) {
                adjust = 0x40;
            } else {
                adjust = 0;
            }
            after[-1].tpage = ((u32)(adjust + 0x1C0) >> 6) | 0x110;
            after[-1].u0    = corner[0] - adjust;
            after[-1].u1    = corner[1] - adjust;
            poly--;
            after[-1].u2 = corner[2] - adjust;
            i++;
            after[-1].u3 = corner[3] - adjust;
            after--;
        } while (i < 16);
        j++;
    } while (j < 12);
    func_actor_403600_801320F8(scratch->maxOtz + 1);
    SCRATCH_POP_BYTES(0x78);
}

const SVECTOR D_actor_403600_80131E2C = { 0, 0x578, 0, 0 };

void func_actor_403600_80135C28(Task* arg0)
{
    SVECTOR           sp10;
    s16*              temp_a0_2;
    s16*              temp_a0_6;
    ActorEffectState* temp_s0;
    s16*              temp_v0_11;
    s16*              temp_v0_13;
    ActorEffectState* temp_v0_2;
    s16*              temp_v0_4;
    s16*              temp_v0_7;
    s16*              temp_v1_5;
    s16*              temp_v1_8;
    s16*              var_a0_2;
    GpCoord*          temp_s4;
    s32               temp_a0_3;
    s32               temp_a0_4;
    s32               temp_a0_7;
    s32               temp_v0_10;
    s32               temp_v0_12;
    s32               temp_v0_3;
    s32               temp_v0_5;
    s32               temp_v0_6;
    s32               temp_v0_8;
    s32               temp_v0_9;
    s32               temp_v1_10;
    s32               temp_v1_11;
    s32               temp_v1_12;
    s32               temp_v1_2;
    s32               temp_v1_3;
    s32               temp_v1_4;
    s32               temp_v1_6;
    s32               temp_v1_7;
    s32               temp_v1_9;
    s32               var_a1;
    s32               var_a1_2;
    s32               var_v0;
    s32               var_v0_2;
    s32               var_v0_3;
    s32               var_v0_4;
    s32               var_v0_5;
    s32               var_v0_6;
    s32               var_v0_7;
    s32               var_v1;
    Task*             temp_a0;
    TmdObject*        temp_a0_5;
    TmdObject*        temp_a1;
    Task*             temp_s2;
    TmdObject*        temp_v0;
    Actor403600Work*  temp_v1;

    temp_a0 = arg0->spawnArg2;
    temp_v1 = temp_a0->work;
    temp_s4 = arg0->extra.tmd->coords;
    temp_s2 = temp_v1->field_710;
    if (temp_v1->field_742 == 1) {
        temp_v0                 = temp_a0->extra.tmd;
        D_actor_403600_801606A0 = NULL;
        temp_v0->flags          = (u16)(temp_v0->flags & 0xFF7F);
        Task_CallExit(arg0);
        return;
    }
    if (arg0->state == 0) {
        temp_v0_2 = memCalloc(0xE8, 0);
        if (temp_v0_2 != NULL) {
            arg0->work          = (TaskIdMap*)temp_v0_2;
            temp_v0_2->field_E0 = 0;
            sp10                = D_actor_403600_80131E2C;
            Gp_CopyCoordOffset(arg0, &temp_s2->parent->extra.tmd->coords[1], &sp10);
            temp_a0_2                      = &temp_v0_2->field_90.coord.m[0][0];
            *(s32*)&temp_a0_2[0]           = 0x1000;
            *(s32*)&temp_a0_2[2]           = 0;
            *(s32*)&temp_a0_2[4]           = 0x1000;
            *(s32*)&temp_a0_2[6]           = 0;
            temp_a0_2[8]                   = 0x1000;
            temp_v0_2->field_90.coord.t[0] = 0;
            temp_v0_2->field_90.coord.t[1] = 0;
            temp_v0_2->field_90.coord.t[2] = 0;
            temp_v0_2->field_90.flg        = 0;
            temp_v0_2->field_90.sub        = temp_s4;
            temp_v1_2                      = arg0->spawnArg1;
            arg0->killCountdown            = 0x10;
            switch (temp_v1_2) {
                case 1:
                    temp_v0_2->field_8E = (s16)temp_v1_2;
                    var_a1_2            = 0;
                    do {
                        temp_a0_4           = temp_v0_2->field_80;
                        temp_v1_6           = temp_a0_4 + 0x1F;
                        var_v0_3            = temp_v1_6;
                        temp_v0_2->field_80 = temp_v1_6;
                        if (temp_v1_6 < 0) {
                            var_v0_3 = temp_a0_4 + 0x3E;
                        }
                        temp_v0_6 = temp_v1_6 - ((var_v0_3 >> 5) << 5);
                        __asm__("move %0,%1" : "=r"(temp_a0_4) : "r"(temp_v0_6));
                        temp_v0_2->field_80 = temp_v0_6;
                        temp_v0_7           = &temp_v0_2->field_0[temp_a0_4];
                        temp_v0_7[0]        = 0;
                        temp_v0_7[0x20]     = 0U;
                        if (temp_v0_2->field_8E != 0) {
                            if (temp_v0_2->field_8C == 0) {
                                temp_v0_2->field_84 = 0;
                            }
                            temp_v1_7 = temp_v0_2->field_88;
                            if (temp_v1_7 < 0x1000) {
                                temp_v0_2->field_88 = (s32)(temp_v1_7 + 0x200);
                            }
                        } else {
                            temp_v0_8 = temp_v0_2->field_88;
                            if (temp_v0_8 > 0) {
                                temp_v0_2->field_88 = (s32)(temp_v0_8 - 0x80);
                            }
                        }
                        temp_v0_2->field_8C = (s16)(u16)temp_v0_2->field_8E;
                        if (temp_v0_2->field_88 != 0) {
                            temp_v1_8       = &temp_v0_2->field_0[temp_a0_4];
                            temp_v1_8[0]    = (s16)(u16)temp_v0_2->field_84;
                            temp_v1_8[0x20] = (u16)temp_v0_2->field_88;
                            if (temp_v0_2->field_E0 == 0) {
                                var_v0_4 = temp_v0_2->field_84 + 0x180;
                            } else {
                                var_v0_4 = temp_v0_2->field_84 + 0x100;
                            }
                            temp_v0_2->field_84 = var_v0_4;
                        }
                        var_a1_2 += 1;
                    } while (var_a1_2 < 0x10);
                    temp_v0_2->field_E4 = 8;
                    break;
                case 2:
                    arg0->killCountdown = 0x2E;
                    temp_v0_2->field_E4 = 0x1F;
                    Gfx_RotMatrixZ(temp_a0_2, 0x800, 0);
                    temp_s4->flg = 0;
                    break;
                default:
                    temp_v0_2->field_8E = 1;
                    var_a1              = 0;
                    do {
                        temp_a0_3           = temp_v0_2->field_80;
                        temp_v1_3           = temp_a0_3 + 0x1F;
                        var_v0              = temp_v1_3;
                        temp_v0_2->field_80 = temp_v1_3;
                        if (temp_v1_3 < 0) {
                            var_v0 = temp_a0_3 + 0x3E;
                        }
                        temp_v0_3 = temp_v1_3 - ((var_v0 >> 5) << 5);
                        __asm__("move %0,%1" : "=r"(temp_a0_3) : "r"(temp_v0_3));
                        temp_v0_2->field_80 = temp_v0_3;
                        temp_v0_4           = &temp_v0_2->field_0[temp_a0_3];
                        temp_v0_4[0]        = 0;
                        temp_v0_4[0x20]     = 0U;
                        if (temp_v0_2->field_8E != 0) {
                            if (temp_v0_2->field_8C == 0) {
                                temp_v0_2->field_84 = 0;
                            }
                            temp_v1_4 = temp_v0_2->field_88;
                            if (temp_v1_4 < 0x1000) {
                                temp_v0_2->field_88 = (s32)(temp_v1_4 + 0x200);
                            }
                        } else {
                            temp_v0_5 = temp_v0_2->field_88;
                            if (temp_v0_5 > 0) {
                                temp_v0_2->field_88 = (s32)(temp_v0_5 - 0x80);
                            }
                        }
                        temp_v0_2->field_8C = (s16)(u16)temp_v0_2->field_8E;
                        if (temp_v0_2->field_88 != 0) {
                            temp_v1_5       = &temp_v0_2->field_0[temp_a0_3];
                            temp_v1_5[0]    = (s16)(u16)temp_v0_2->field_84;
                            temp_v1_5[0x20] = (u16)temp_v0_2->field_88;
                            if (temp_v0_2->field_E0 == 0) {
                                var_v0_2 = temp_v0_2->field_84 + 0x180;
                            } else {
                                var_v0_2 = temp_v0_2->field_84 + 0x100;
                            }
                            temp_v0_2->field_84 = var_v0_2;
                        }
                        var_a1 += 1;
                    } while (var_a1 < 0x10);
                    break;
            }
            arg0->state = (s32)(arg0->state + 1);
        } else {
            Task_CallExit(arg0);
            return;
        }
    }
    temp_s0 = (ActorEffectState*)arg0->work;
    if (Gp_StateF0.field_4 == 0) {
        temp_v1_9 = arg0->spawnArg1;
        switch (temp_v1_9) { /* switch 1; irregular */
            case 1:          /* switch 1 */
                temp_v0_9         = temp_s0->field_E4 - 1;
                temp_s0->field_E4 = temp_v0_9;
                if (temp_v0_9 == 0) {
                    temp_a0_5               = ((Task*)arg0->spawnArg2)->extra.tmd;
                    D_actor_403600_801606A0 = NULL;
                    temp_a0_5->flags        = (u16)(temp_a0_5->flags | 0x80);
                    goto block_57;
                } else if (temp_v0_9 > 0) {
                    D_actor_403600_801606A0 = (s32)&temp_s0->field_90;
                    Gp_UpdateCoord(&temp_s0->field_90);
                    goto block_57;
                }
                goto block_57;
            case 2: /* switch 1 */
                temp_v1_10        = temp_s0->field_E4 - 1;
                temp_s0->field_E4 = temp_v1_10;
                if (temp_v1_10 != 0) {
                    goto block_52;
                }
                temp_a1                 = ((Task*)arg0->spawnArg2)->extra.tmd;
                temp_a0_6               = (s16*)&temp_s0->field_90;
                D_actor_403600_801606A0 = temp_a0_6;
                temp_a1->flags          = (u16)(temp_a1->flags & 0xFF7F);
                Gp_UpdateCoord((GpCoord*)temp_a0_6);
                goto block_57;
            block_52:
                if (temp_v1_10 < -7) {
                    goto block_55;
                }
                D_actor_403600_801606A0 = (s32)&temp_s0->field_90;
                Gp_UpdateCoord(&temp_s0->field_90);
                goto block_57;
            block_55:
                if (temp_v1_10 == -8) {
                    D_actor_403600_801606A0 = NULL;
                }
            block_57:
                break;
        }
        if (arg0->killCountdown > 0) {
            temp_s0->field_8E = 1;
        } else {
            temp_s0->field_8E = 0;
        }
        arg0->killCountdown = (s16)((u16)arg0->killCountdown - 1);
        temp_a0_7           = temp_s0->field_80;
        temp_v1_11          = temp_a0_7 + 0x1F;
        var_v0_5            = temp_v1_11;
        temp_s0->field_80   = temp_v1_11;
        if (temp_v1_11 < 0) {
            var_v0_5 = temp_a0_7 + 0x3E;
        }
        temp_v0_10 = temp_v1_11 - ((var_v0_5 >> 5) << 5);
        __asm__("move %0,%1" : "=r"(temp_a0_7) : "r"(temp_v0_10));
        temp_s0->field_80 = temp_v0_10;
        temp_v0_11        = &temp_s0->field_0[temp_a0_7];
        temp_v0_11[0]     = 0;
        temp_v0_11[0x20]  = 0U;
        if (temp_s0->field_8E != 0) {
            if (temp_s0->field_8C == 0) {
                temp_s0->field_84 = 0;
            }
            temp_v1_12 = temp_s0->field_88;
            if (temp_v1_12 < 0x1000) {
                temp_s0->field_88 = (s32)(temp_v1_12 + 0x200);
            }
        } else {
            temp_v0_12 = temp_s0->field_88;
            if (temp_v0_12 > 0) {
                temp_s0->field_88 = (s32)(temp_v0_12 - 0x80);
            }
        }
        temp_s0->field_8C = (s16)(u16)temp_s0->field_8E;
        if (temp_s0->field_88 != 0) {
            temp_v0_13       = &temp_s0->field_0[temp_a0_7];
            temp_v0_13[0]    = (s16)(u16)temp_s0->field_84;
            temp_v0_13[0x20] = (u16)temp_s0->field_88;
            if (temp_s0->field_E0 != 0) {
                goto block_73;
            }
            var_v0_6 = temp_s0->field_84 + 0x180;
            goto block_74;
        block_72:
            var_v0_7 = 0;
            goto block_78;
        block_73:
            var_v0_6 = temp_s0->field_84 + 0x100;
        block_74:
            temp_s0->field_84 = var_v0_6;
        }
    }
    func_actor_403600_801353D0(temp_s0, temp_s4);
    var_v1 = 0;
    if (arg0->killCountdown <= 0) {
        var_a0_2 = temp_s0->field_0;
    loop_78:
        var_v1 += 1;
        if (*var_a0_2 != 0) {
            goto block_72;
        }
        var_a0_2 += 1;
        if (var_v1 >= 0x20) {
            var_v0_7 = 1;
        } else {
            goto loop_78;
        }
    block_78:
        if (var_v0_7 != 0) {
            Task_CallExit(arg0);
        }
    }
}

u32* func_actor_403600_80136224(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    CVECTOR       col;
    s16           upper_y;
    s16           lower_y;
    POLY_GT3*     poly;
    s32           upper_delta;
    s32           upper_calc;
    s32           lower_delta;
    s32           light;
    s32           upper_limit;
    s32*          opz;
    u32           mask;
    u16*          rec;
    u8*           verts;
    u8*           norms;
    DisplayState* ds;

    poly  = (POLY_GT3*)arg0->primWrite;
    col   = D_actor_403600_80131E34;
    light = arg0->obj->lightLevel;
    if (arg0->elemCount-- > 0) {
        opz         = &arg0->gteResult;
        upper_limit = 0x168 - light;
        ds          = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&arg0->gteFlag);
            if (arg0->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (arg0->gteResult > 0) {
                    gte_stsxy3_gt3(poly);
                    gte_avsz3();
                    upper_delta = 0;
                    if (light != 0) {
                        upper_y = poly->y0;
                        if (upper_limit < upper_y) {
                            upper_calc  = upper_y - 0x168;
                            upper_delta = (upper_calc + light) * 2;
                        }
                    }
                    if (upper_delta >= 0x81) {
                        PRIM_COLOR_WORD(poly, 0) = 0;
                        PRIM_COLOR_WORD(poly, 1) = 0;
                        PRIM_COLOR_WORD(poly, 2) = 0;
                    } else {
                        col.r = -0x80 - upper_delta;
                        col.g = -0x80 - upper_delta;
                        col.b = -0x80 - upper_delta;
                        gte_ldrgb(&col);
                        norms = (u8*)arg0->normals;
                        gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8),
                                 norms + (rec[5] & 0xFFF8));
                        gte_ncct();
                        gte_strgb3_gt3(poly);
                    }
                    if (light != 0) {
                        lower_y = poly->y0;
                        if (upper_limit < lower_y) {
                            lower_delta  = lower_y;
                            lower_delta -= 0x168;
                            lower_delta += light;
                            lower_delta *= 2;
                            poly->y1    -= lower_delta;
                            poly->y2    -= lower_delta;
                            poly->y0    -= lower_delta;
                        }
                    }
                    setlen(poly, 9);
                    setcode(poly, 0x36);
                    gte_stotz(opz);
                    mask      = 0xFFFFFF;
                    poly->tag = (poly->tag & 0xFF000000) |
                                (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) &
                                             0xFFC) +
                                            (s32)arg0->ot) &
                                 mask);
                    *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                               (s32)arg0->ot) =
                        (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                    (s32)arg0->ot) &
                         0xFF000000) |
                        ((u32)poly & mask);
                }
            }
            poly++;
            arg2 += arg0->elemStride;
        } while (arg0->elemCount-- > 0);
    }
    arg0->primWrite = (u8*)poly;
    return arg2;
}

u32* func_actor_403600_80136500(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT3*     poly;
    s32*          opz;
    DisplayState* ds;
    u32           mask;
    u32           clip_mask;
    u16*          rec;
    s32           light;
    s32           upper_limit;
    s32           upper_delta;
    s32           upper_calc;
    s32           lower_delta;
    s32           sz;
    s32           idx;
    u8*           sz_table;
    s16           upper_y;
    s16           lower_y;

    poly  = (POLY_GT3*)arg0->preXformWrite;
    light = arg0->obj->lightLevel;
    if (arg0->elemCount-- > 0) {
        opz         = &arg0->gteResult;
        clip_mask   = 0x80000000;
        upper_limit = 0x168 - light;
        ds          = &gDisplayState;
        mask        = 0xFFFFFF;
        do {
            rec = (u16*)arg2;
            gte_ldSXYP(PRIM_XY_WORD(poly, 0));
            gte_ldSXYP(PRIM_XY_WORD(poly, 1));
            gte_ldSXYP(PRIM_XY_WORD(poly, 2));
            gte_nclip();
            gte_stopz(opz);
            if (arg0->gteResult > 0) {
                sz_table = (u8*)arg0->szTable;
                idx      = rec[0] & 0xFFFC;
                sz       = *(s32*)(idx + (s32)sz_table);
                if (!(sz & clip_mask)) {
                    gte_ldSZ1(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = *(s32*)(idx + (s32)sz_table);
                    if (!(sz & clip_mask)) {
                        gte_ldSZ2(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = *(s32*)(idx + (s32)sz_table);
                        if (!(sz & clip_mask)) {
                            gte_ldSZ3(sz);
                            gte_avsz3();
                            upper_delta = 0;
                            if (light != 0) {
                                upper_y = poly->y0;
                                if (upper_limit < upper_y) {
                                    upper_calc  = upper_y - 0x168;
                                    upper_delta = (upper_calc + light) * 2;
                                }
                                if (upper_delta >= 0x81) {
                                    PRIM_COLOR_WORD(poly, 0) = 0;
                                    PRIM_COLOR_WORD(poly, 1) = 0;
                                    PRIM_COLOR_WORD(poly, 2) = 0;
                                } else {
                                    s32 faded;

                                    upper_delta = 0x80 - upper_delta;
                                    faded       = (poly->r0 * upper_delta) >> 7;
                                    poly->r0    = faded;
                                    faded       = (poly->g0 * upper_delta) >> 7;
                                    poly->g0    = faded;
                                    faded       = (poly->b0 * upper_delta) >> 7;
                                    poly->b0    = faded;
                                    faded       = (poly->r1 * upper_delta) >> 7;
                                    poly->r1    = faded;
                                    faded       = (poly->g1 * upper_delta) >> 7;
                                    poly->g1    = faded;
                                    faded       = (poly->b1 * upper_delta) >> 7;
                                    poly->b1    = faded;
                                    faded       = (poly->r2 * upper_delta) >> 7;
                                    poly->r2    = faded;
                                    faded       = (poly->g2 * upper_delta) >> 7;
                                    poly->g2    = faded;
                                    faded       = (poly->b2 * upper_delta) >> 7;
                                    poly->b2    = faded;
                                }
                            }
                            if (light != 0) {
                                lower_y = poly->y0;
                                if (upper_limit < lower_y) {
                                    lower_delta  = lower_y;
                                    lower_delta -= 0x168;
                                    lower_delta += light;
                                    lower_delta *= 2;
                                    poly->y1    -= lower_delta;
                                    poly->y2    -= lower_delta;
                                    poly->y0    -= lower_delta;
                                }
                            }
                            setlen(poly, 9);
                            setcode(poly, 0x36);
                            gte_stotz(opz);
                            poly->tag = (poly->tag & 0xFF000000) |
                                        (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                                    (s32)arg0->ot) &
                                         mask);
                            *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                       (s32)arg0->ot) =
                                (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                            (s32)arg0->ot) &
                                 0xFF000000) |
                                ((u32)poly & mask);
                        }
                    }
                }
            }
            poly++;
            arg2 += arg0->elemStride;
        } while (arg0->elemCount-- > 0);
    }
    arg0->preXformWrite = (u8*)poly;
    return arg2;
}

u32* func_actor_403600_8013685C(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    CVECTOR       col;
    s16           upper_y;
    s16           lower_y;
    POLY_GT4*     poly;
    s32           upper_delta;
    s32           upper_calc;
    s32           lower_delta;
    s32           light;
    s32           upper_limit;
    s32*          opz;
    s32*          flg;
    u32           mask;
    u32           clip_mask;
    u16*          rec;
    u8*           verts;
    u8*           norms;
    DisplayState* ds;

    poly  = (POLY_GT4*)arg0->primWrite;
    col   = D_actor_403600_80131E34;
    light = arg0->obj->lightLevel;
    gte_ldrgb(&col);
    if (arg0->elemCount-- > 0) {
        flg         = &arg0->gteFlag;
        clip_mask   = 0x80000000;
        opz         = &arg0->gteResult;
        upper_limit = 0x168 - light;
        ds          = &gDisplayState;
        mask        = 0xFFFFFF;
        SOFT_TOUCH_REG(mask);
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if (!(arg0->gteFlag & clip_mask)) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_gt4(poly);
                gte_ldv0((u8*)arg0->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if (!(arg0->gteFlag & clip_mask)) {
                    if (arg0->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (arg0->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        upper_delta = 0;
                        if (light != 0) {
                            upper_y = poly->y0;
                            if (upper_limit < upper_y) {
                                upper_calc  = upper_y - 0x168;
                                upper_delta = (upper_calc + light) * 2;
                            }
                        }
                        if (upper_delta >= 0x81) {
                            PRIM_COLOR_WORD(poly, 0) = 0;
                            PRIM_COLOR_WORD(poly, 1) = 0;
                            PRIM_COLOR_WORD(poly, 2) = 0;
                            PRIM_COLOR_WORD(poly, 3) = 0;
                        } else {
                            col.r = -0x80 - upper_delta;
                            col.g = -0x80 - upper_delta;
                            col.b = -0x80 - upper_delta;
                            gte_ldrgb(&col);
                            norms = (u8*)arg0->normals;
                            gte_ldv3(norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8),
                                     norms + (rec[6] & 0xFFF8));
                            gte_ncct();
                            gte_strgb3_gt4(poly);
                            gte_ldv0((u8*)arg0->normals + (rec[7] & 0xFFF8));
                            gte_nccs();
                            gte_strgb(&poly->r3);
                        }
                        if (light != 0) {
                            lower_y = poly->y0;
                            if (upper_limit < lower_y) {
                                lower_delta  = lower_y;
                                lower_delta -= 0x168;
                                lower_delta += light;
                                lower_delta *= 2;
                                poly->y3    -= lower_delta;
                                poly->y2    -= lower_delta;
                                poly->y1    -= lower_delta;
                                poly->y0    -= lower_delta;
                            }
                        }
                        setlen(poly, 12);
                        setcode(poly, 0x3E);
                        gte_stotz(opz);
                        poly->tag = (poly->tag & 0xFF000000) |
                                    (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) &
                                                 0xFFC) +
                                                (s32)arg0->ot) &
                                     mask);
                        *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                   (s32)arg0->ot) =
                            (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                        (s32)arg0->ot) &
                             0xFF000000) |
                            ((u32)poly & mask);
                    }
                }
            }
            poly++;
            arg2 += arg0->elemStride;
        } while (arg0->elemCount-- > 0);
    }
    arg0->primWrite = (u8*)poly;
    return arg2;
}

u32* func_actor_403600_80136C00(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT4*     poly;
    s32*          opz;
    DisplayState* ds;
    u32           mask;
    u32           clip_mask;
    u16*          rec;
    s32           light;
    s32           upper_limit;
    s32           upper_delta;
    s32           upper_calc;
    s32           lower_delta;
    s32           sz;
    s32           idx;
    u8*           sz_table;
    s16           upper_y;
    s16           lower_y;

    poly  = (POLY_GT4*)arg0->preXformWrite;
    light = arg0->obj->lightLevel;
    if (arg0->elemCount-- > 0) {
        opz         = &arg0->gteResult;
        clip_mask   = 0x80000000;
        upper_limit = 0x168 - light;
        ds          = &gDisplayState;
        mask        = 0xFFFFFF;
        SOFT_TOUCH_REG(mask);
        do {
            rec = (u16*)arg2;
            gte_ldSXYP(PRIM_XY_WORD(poly, 0));
            gte_ldSXYP(PRIM_XY_WORD(poly, 1));
            gte_ldSXYP(PRIM_XY_WORD(poly, 2));
            gte_nclip();
            gte_stopz(opz);
            if (arg0->gteResult > 0) {
                goto draw;
            }
            gte_ldSXYP(PRIM_XY_WORD(poly, 3));
            gte_nclip();
            gte_stopz(opz);
            if (arg0->gteResult < 0) {
            draw:
                sz_table = (u8*)arg0->szTable;
                idx      = rec[0] & 0xFFFC;
                sz       = *(s32*)(idx + (s32)sz_table);
                if (!(sz & clip_mask)) {
                    gte_ldSZ0(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = *(s32*)(idx + (s32)sz_table);
                    if (!(sz & clip_mask)) {
                        gte_ldSZ1(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = *(s32*)(idx + (s32)sz_table);
                        if (!(sz & clip_mask)) {
                            gte_ldSZ2(sz);
                            idx = rec[3] & 0xFFFC;
                            sz  = *(s32*)(idx + (s32)sz_table);
                            if (!(sz & clip_mask)) {
                                gte_ldSZ3(sz);
                                gte_avsz4();
                                upper_delta = 0;
                                if (light != 0) {
                                    upper_y = poly->y0;
                                    if (upper_limit < upper_y) {
                                        upper_calc  = upper_y - 0x168;
                                        upper_delta = (upper_calc + light) * 2;
                                    }
                                    if (upper_delta >= 0x81) {
                                        PRIM_COLOR_WORD(poly, 0) = 0;
                                        PRIM_COLOR_WORD(poly, 1) = 0;
                                        PRIM_COLOR_WORD(poly, 2) = 0;
                                        PRIM_COLOR_WORD(poly, 3) = 0;
                                    } else {
                                        s32 faded;

                                        // Vertex 1's colour is faded into vertex 3 and then faded
                                        // again with it, while vertex 1 keeps its own colour; the
                                        // original writes it that way.
                                        upper_delta = 0x80 - upper_delta;
                                        faded       = (poly->r0 * upper_delta) >> 7;
                                        poly->r0    = faded;
                                        faded       = (poly->g0 * upper_delta) >> 7;
                                        poly->g0    = faded;
                                        faded       = (poly->b0 * upper_delta) >> 7;
                                        poly->b0    = faded;
                                        faded       = (poly->r1 * upper_delta) >> 7;
                                        poly->r3    = faded;
                                        faded       = (poly->g1 * upper_delta) >> 7;
                                        poly->g3    = faded;
                                        faded       = (poly->b1 * upper_delta) >> 7;
                                        poly->b3    = faded;
                                        faded       = (poly->r2 * upper_delta) >> 7;
                                        poly->r2    = faded;
                                        faded       = (poly->g2 * upper_delta) >> 7;
                                        poly->g2    = faded;
                                        faded       = (poly->b2 * upper_delta) >> 7;
                                        poly->b2    = faded;
                                        faded       = (poly->r3 * upper_delta) >> 7;
                                        poly->r3    = faded;
                                        faded       = (poly->g3 * upper_delta) >> 7;
                                        poly->g3    = faded;
                                        faded       = (poly->b3 * upper_delta) >> 7;
                                        poly->b3    = faded;
                                    }
                                }
                                if (light != 0) {
                                    lower_y = poly->y0;
                                    if (upper_limit < lower_y) {
                                        lower_delta  = lower_y;
                                        lower_delta -= 0x168;
                                        lower_delta += light;
                                        lower_delta *= 2;
                                        poly->y3    -= lower_delta;
                                        poly->y2    -= lower_delta;
                                        poly->y1    -= lower_delta;
                                        poly->y0    -= lower_delta;
                                    }
                                }
                                setlen(poly, 12);
                                setcode(poly, 0x3E);
                                gte_stotz(opz);
                                gte_stotz(opz);
                                poly->tag = (poly->tag & 0xFF000000) |
                                            (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                                        (s32)arg0->ot) &
                                             mask);
                                *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                           (s32)arg0->ot) =
                                    (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                                (s32)arg0->ot) &
                                     0xFF000000) |
                                    ((u32)poly & mask);
                            }
                        }
                    }
                }
            }
            poly++;
            arg2 += arg0->elemStride;
        } while (arg0->elemCount-- > 0);
    }
    arg0->preXformWrite = (u8*)poly;
    return arg2;
}

u32* func_actor_403600_8013700C(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    CVECTOR       col;
    s16           upper_y;
    s16           lower_y;
    POLY_GT3*     poly;
    s32           upper_delta;
    s32           upper_calc;
    s32           lower_delta;
    s32           light;
    s32           upper_limit;
    s32*          opz;
    u32           mask;
    u16*          rec;
    u8*           verts;
    u8*           norms;
    DisplayState* ds;

    poly  = (POLY_GT3*)arg0->primWrite;
    col   = D_actor_403600_80131E34;
    light = arg0->obj->lightLevel;
    if (arg0->elemCount-- > 0) {
        opz         = &arg0->gteResult;
        upper_limit = 0x168 - light;
        ds          = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&arg0->gteFlag);
            if (arg0->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (arg0->gteResult > 0) {
                    gte_stsxy3_gt3(poly);
                    gte_avsz3();
                    upper_delta = 0;
                    if (light != 0) {
                        upper_y = poly->y0;
                        if (upper_limit < upper_y) {
                            upper_calc  = upper_y - 0x168;
                            upper_delta = (upper_calc + light) * 2;
                        }
                    }
                    if (upper_delta >= 0x81) {
                        PRIM_COLOR_WORD(poly, 0) = 0;
                        PRIM_COLOR_WORD(poly, 1) = 0;
                        PRIM_COLOR_WORD(poly, 2) = 0;
                    } else {
                        col.r = -0x80 - upper_delta;
                        col.g = -0x80 - upper_delta;
                        col.b = -0x80 - upper_delta;
                        gte_ldrgb(&col);
                        norms = (u8*)arg0->normals;
                        gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8),
                                 norms + (rec[5] & 0xFFF8));
                        gte_ncct();
                        gte_strgb3_gt3(poly);
                    }
                    setlen(poly, 9);
                    setcode(poly, 0x34);
                    if (light != 0) {
                        lower_y = poly->y0;
                        if (lower_y < (light - 0x168)) {
                            lower_delta  = lower_y;
                            lower_delta += 0x168;
                            lower_delta -= light;
                            lower_delta *= 2;
                            poly->y1    += lower_delta;
                            poly->y2    += lower_delta;
                            poly->y0    += lower_delta;
                            poly->code  |= 2;
                        }
                    }
                    poly->code &= 0xFE;
                    gte_stotz(opz);
                    mask      = 0xFFFFFF;
                    poly->tag = (poly->tag & 0xFF000000) |
                                (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) &
                                             0xFFC) +
                                            (s32)arg0->ot) &
                                 mask);
                    *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                               (s32)arg0->ot) =
                        (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                    (s32)arg0->ot) &
                         0xFF000000) |
                        ((u32)poly & mask);
                }
            }
            poly++;
            arg2 += arg0->elemStride;
        } while (arg0->elemCount-- > 0);
    }
    arg0->primWrite = (u8*)poly;
    return arg2;
}

u32* func_actor_403600_80137300(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    CVECTOR       col;
    s16           upper_y;
    s16           lower_y;
    POLY_GT3*     poly;
    s32           upper_delta;
    s32           upper_calc;
    s32           lower_delta;
    s32           light;
    s32           upper_limit;
    s32*          opz;
    u32           mask;
    u16*          rec;
    u8*           verts;
    u8*           norms;
    DisplayState* ds;

    poly  = (POLY_GT3*)arg0->primWrite;
    col   = D_actor_403600_80131E34;
    light = arg0->obj->lightLevel;
    if (arg0->elemCount-- > 0) {
        opz         = &arg0->gteResult;
        upper_limit = 0x168 - light;
        ds          = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&arg0->gteFlag);
            if (arg0->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (arg0->gteResult > 0) {
                    gte_stsxy3_gt3(poly);
                    gte_avsz3();
                    upper_delta = 0;
                    if (light != 0) {
                        upper_y = poly->y0;
                        if (upper_limit < upper_y) {
                            upper_calc  = upper_y - 0x168;
                            upper_delta = (upper_calc + light) * 2;
                        }
                    }
                    if (upper_delta >= 0x81) {
                        PRIM_COLOR_WORD(poly, 0) = 0;
                        PRIM_COLOR_WORD(poly, 1) = 0;
                        PRIM_COLOR_WORD(poly, 2) = 0;
                    } else {
                        col.r = -0x80 - upper_delta;
                        col.g = -0x80 - upper_delta;
                        col.b = -0x80 - upper_delta;
                        gte_ldrgb(&col);
                        norms = (u8*)arg0->normals;
                        gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8),
                                 norms + (rec[5] & 0xFFF8));
                        gte_ncct();
                        gte_strgb3_gt3(poly);
                    }
                    setlen(poly, 9);
                    setcode(poly, 0x34);
                    if (light != 0) {
                        lower_y = poly->y0;
                        if (lower_y < (light - 0x168)) {
                            lower_delta  = lower_y;
                            lower_delta += 0x168;
                            lower_delta -= light;
                            lower_delta *= 2;
                            poly->y1    += lower_delta;
                            poly->y2    += lower_delta;
                            poly->y0    += lower_delta;
                            poly->code  |= 2;
                        }
                    }
                    poly->code = (poly->code & 0xFE) | 2;
                    gte_stotz(opz);
                    mask      = 0xFFFFFF;
                    poly->tag = (poly->tag & 0xFF000000) |
                                (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) &
                                             0xFFC) +
                                            (s32)arg0->ot) &
                                 mask);
                    *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                               (s32)arg0->ot) =
                        (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                    (s32)arg0->ot) &
                         0xFF000000) |
                        ((u32)poly & mask);
                }
            }
            poly++;
            arg2 += arg0->elemStride;
        } while (arg0->elemCount-- > 0);
    }
    arg0->primWrite = (u8*)poly;
    return arg2;
}

u32* func_actor_403600_801375F8(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    CVECTOR       col;
    s16           upper_y;
    s16           lower_y;
    POLY_GT4*     poly;
    s32           upper_delta;
    s32           upper_calc;
    s32           lower_delta;
    s32           light;
    s32           upper_limit;
    s32*          opz;
    s32*          flg;
    u32           mask;
    u32           clip_mask;
    u16*          rec;
    u8*           verts;
    u8*           norms;
    DisplayState* ds;

    poly  = (POLY_GT4*)arg0->primWrite;
    col   = D_actor_403600_80131E34;
    light = arg0->obj->lightLevel;
    gte_ldrgb(&col);
    if (arg0->elemCount-- > 0) {
        flg         = &arg0->gteFlag;
        clip_mask   = 0x80000000;
        opz         = &arg0->gteResult;
        upper_limit = 0x168 - light;
        ds          = &gDisplayState;
        mask        = 0xFFFFFF;
        SOFT_TOUCH_REG(mask);
        do {
            rec   = (u16*)arg2;
            verts = (u8*)arg0->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8),
                     verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if (!(arg0->gteFlag & clip_mask)) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_gt4(poly);
                gte_ldv0((u8*)arg0->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if (!(arg0->gteFlag & clip_mask)) {
                    if (arg0->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (arg0->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        upper_delta = 0;
                        if (light != 0) {
                            upper_y = poly->y0;
                            if (upper_limit < upper_y) {
                                upper_calc  = upper_y - 0x168;
                                upper_delta = (upper_calc + light) * 2;
                            }
                        }
                        if (upper_delta >= 0x81) {
                            PRIM_COLOR_WORD(poly, 0) = 0;
                            PRIM_COLOR_WORD(poly, 1) = 0;
                            PRIM_COLOR_WORD(poly, 2) = 0;
                            PRIM_COLOR_WORD(poly, 3) = 0;
                        } else {
                            col.r = -0x80 - upper_delta;
                            col.g = -0x80 - upper_delta;
                            col.b = -0x80 - upper_delta;
                            gte_ldrgb(&col);
                            norms = (u8*)arg0->normals;
                            gte_ldv3(norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8),
                                     norms + (rec[6] & 0xFFF8));
                            gte_ncct();
                            gte_strgb3_gt4(poly);
                            gte_ldv0((u8*)arg0->normals + (rec[7] & 0xFFF8));
                            gte_nccs();
                            gte_strgb(&poly->r3);
                        }
                        setlen(poly, 12);
                        setcode(poly, 0x3C);
                        if (light != 0) {
                            lower_y = poly->y0;
                            if (lower_y < (light - 0x168)) {
                                lower_delta  = lower_y;
                                lower_delta += 0x168;
                                lower_delta -= light;
                                lower_delta *= 2;
                                poly->y3    += lower_delta;
                                poly->y2    += lower_delta;
                                poly->y1    += lower_delta;
                                poly->y0    += lower_delta;
                                poly->code  |= 2;
                            }
                        }
                        poly->code &= 0xFE;
                        gte_stotz(opz);
                        poly->tag = (poly->tag & 0xFF000000) |
                                    (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) &
                                                 0xFFC) +
                                                (s32)arg0->ot) &
                                     mask);
                        *(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                   (s32)arg0->ot) =
                            (*(u_long*)(((((u32)arg0->gteResult << ds->otDepthShift) >> 2) & 0xFFC) +
                                        (s32)arg0->ot) &
                             0xFF000000) |
                            ((u32)poly & mask);
                    }
                }
            }
            poly++;
            arg2 += arg0->elemStride;
        } while (arg0->elemCount-- > 0);
    }
    arg0->primWrite = (u8*)poly;
    return arg2;
}

/// Scratch-pad frame of a triangle pass that works in the frame of a reference
/// coordinate: the GTE state the pass restores on exit, the model-to-reference
/// matrix it builds, and the element being worked on.
typedef struct {
    VECTOR  trans;    // GTE translation on entry
    SVECTOR offset;   // Entry translation relative to the reference, rotated into its frame
    SVECTOR verts[3]; // The element's vertices in the reference frame, clamped to y <= 0
    s32     index[3]; // The element's vertex indices
    MATRIX  savedRot; // GTE rotation on entry
    MATRIX  local;    // Model-to-reference transform
} _Actor403600TriScratch;

u32* func_actor_403600_801379B4(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    CVECTOR                 color;
    u8*                     head;
    _Actor403600TriScratch* sc;
    GpCoord*                coord;
    POLY_GT3*               poly;
    u16*                    rec;
    s32                     i;

    if (D_actor_403600_801606A0 != NULL) {
        poly  = (POLY_GT3*)ws->primWrite;
        color = D_actor_403600_80131E34;
        head  = SCRATCH_HEAD(u8);
        sc    = SCRATCH_HEAD(_Actor403600TriScratch) =
            (_Actor403600TriScratch*)(head - sizeof(_Actor403600TriScratch));
        gte_sttr(&sc->trans);
        gte_ReadRotMatrix(&sc->savedRot);
        TransposeMatrix(&((GpCoord*)D_actor_403600_801606A0)->workm, &sc->local);
        coord         = (GpCoord*)D_actor_403600_801606A0;
        sc->offset.vx = sc->trans.vx - coord->workm.t[0];
        sc->offset.vy = sc->trans.vy - coord->workm.t[1];
        sc->offset.vz = sc->trans.vz - coord->workm.t[2];
        _actor403600RotateSv(&sc->local, &sc->offset);
        gte_MulMatrix0(&sc->local, &sc->savedRot, &sc->local);
        sc->local.t[0] = sc->offset.vx;
        sc->local.t[1] = sc->offset.vy;
        sc->local.t[2] = sc->offset.vz;
        gte_ldrgb(&color);
        for (; ws->elemCount-- > 0; poly++, stream += ws->elemStride) {
            rec = (u16*)stream;
            gte_SetTransMatrix(&sc->local);
            gte_SetRotMatrix(&sc->local);
            sc->index[0] = rec[0] >> 3;
            sc->index[1] = rec[1] >> 3;
            sc->index[2] = rec[2] >> 3;
            for (i = 0; i < 3; i++) {
                gte_ldv0(&ws->verts[sc->index[i]]);
                gte_rtv0tr();
                gte_stsv(&sc->verts[i]);
                if (sc->verts[i].vy > 0) {
                    sc->verts[i].vy = 0;
                }
            }
            gte_SetRotMatrix(&((GpCoord*)D_actor_403600_801606A0)->workm);
            gte_SetTransMatrix(&((GpCoord*)D_actor_403600_801606A0)->workm);
            gte_ldv3(&sc->verts[0], &sc->verts[1], &sc->verts[2]);
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag & 0x80000000) {
                continue;
            }
            gte_nclip();
            gte_stopz(&ws->gteResult);
            if (ws->gteResult <= 0) {
                continue;
            }
            gte_stsxy3_gt3(poly);
            gte_avsz3();
            gte_ldv3(&ws->normals[rec[3] >> 3], &ws->normals[rec[4] >> 3], &ws->normals[rec[5] >> 3]);
            gte_ncct();
            gte_strgb3_gt3(poly);
            setlen(poly, 9);
            setcode(poly, 0x34);
            gte_stotz(&ws->gteResult);
            addPrim(&ws->ot[((u32)ws->gteResult << gDisplayState.otDepthShift) >> 4 & 0x3FF], poly);
        }
        ws->primWrite = (u8*)poly;
        gte_SetTransVector(&sc->trans);
        gte_SetRotMatrix(&sc->savedRot);
        SCRATCH_POP_BYTES(sizeof(_Actor403600TriScratch));
        return stream;
    }
    return tmdDrawStreamGt3(ws, flags, stream);
}

/// Scratch-pad frame of a quad pass that draws the model in the frame of a
/// reference coordinate: the GTE state the pass restores on exit, the
/// model-to-reference matrix it builds, and the element being drawn.
typedef struct {
    VECTOR  trans;    // GTE translation on entry
    SVECTOR offset;   // Entry translation relative to the reference, rotated into its frame
    SVECTOR verts[4]; // The element's vertices in the reference frame, clamped to y <= 0
    s32     index[4]; // The element's vertex indices
    MATRIX  savedRot; // GTE rotation on entry
    MATRIX  local;    // Model-to-reference transform
} _Actor403600QuadScratch;

u32* func_actor_403600_80138004(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    CVECTOR                  color;
    u8*                      head;
    _Actor403600QuadScratch* sc;
    GpCoord*                 coord;
    POLY_GT4*                poly;
    u16*                     rec;
    s32                      i;

    if (D_actor_403600_801606A0 != NULL) {
        poly  = (POLY_GT4*)ws->primWrite;
        color = D_actor_403600_80131E34;
        head  = SCRATCH_HEAD(u8);
        sc    = SCRATCH_HEAD(_Actor403600QuadScratch) =
            (_Actor403600QuadScratch*)(head - sizeof(_Actor403600QuadScratch));
        gte_sttr(&sc->trans);
        gte_ReadRotMatrix(&sc->savedRot);
        TransposeMatrix(&((GpCoord*)D_actor_403600_801606A0)->workm, &sc->local);
        coord         = (GpCoord*)D_actor_403600_801606A0;
        sc->offset.vx = sc->trans.vx - coord->workm.t[0];
        sc->offset.vy = sc->trans.vy - coord->workm.t[1];
        sc->offset.vz = sc->trans.vz - coord->workm.t[2];
        _actor403600RotateSv(&sc->local, &sc->offset);
        gte_MulMatrix0(&sc->local, &sc->savedRot, &sc->local);
        sc->local.t[0] = sc->offset.vx;
        sc->local.t[1] = sc->offset.vy;
        sc->local.t[2] = sc->offset.vz;
        gte_ldrgb(&color);
        for (; ws->elemCount-- > 0; poly++, stream += ws->elemStride) {
            rec = (u16*)stream;
            gte_SetTransMatrix(&sc->local);
            gte_SetRotMatrix(&sc->local);
            sc->index[0] = rec[0] >> 3;
            sc->index[1] = rec[1] >> 3;
            sc->index[2] = rec[2] >> 3;
            sc->index[3] = rec[3] >> 3;
            for (i = 0; i < 4; i++) {
                gte_ldv0(&ws->verts[sc->index[i]]);
                gte_rtv0tr();
                gte_stsv(&sc->verts[i]);
                if (sc->verts[i].vy > 0) {
                    sc->verts[i].vy = 0;
                }
            }
            gte_SetRotMatrix(&((GpCoord*)D_actor_403600_801606A0)->workm);
            gte_SetTransMatrix(&((GpCoord*)D_actor_403600_801606A0)->workm);
            gte_ldv3(&sc->verts[0], &sc->verts[1], &sc->verts[2]);
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag & 0x80000000) {
                continue;
            }
            gte_nclip();
            gte_stopz(&ws->gteResult);
            gte_stsxy3_gt4(poly);
            gte_ldv0(&sc->verts[3]);
            gte_rtps();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag & 0x80000000) {
                continue;
            }
            /* Drawn when either triangle of the quad faces the camera. */
            if (ws->gteResult <= 0) {
                gte_nclip();
                gte_stopz(&ws->gteResult);
                if (ws->gteResult >= 0) {
                    continue;
                }
            }
            gte_stsxy2(&poly->x3);
            gte_avsz4();
            gte_ldv3(&ws->normals[rec[4] >> 3], &ws->normals[rec[5] >> 3], &ws->normals[rec[6] >> 3]);
            gte_ncct();
            gte_strgb3_gt4(poly);
            gte_ldv0(&ws->normals[rec[7] >> 3]);
            gte_nccs();
            gte_strgb(&poly->r3);
            setlen(poly, 12);
            setcode(poly, 0x3C);
            gte_stotz(&ws->gteResult);
            addPrim(&ws->ot[((u32)ws->gteResult << gDisplayState.otDepthShift) >> 4 & 0x3FF], poly);
        }
        ws->primWrite = (u8*)poly;
        gte_SetTransVector(&sc->trans);
        gte_SetRotMatrix(&sc->savedRot);
        SCRATCH_POP_BYTES(sizeof(_Actor403600QuadScratch));
        return stream;
    }
    return tmdDrawStreamGt4(ws, flags, stream);
}

u32* func_actor_403600_801386EC(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    CVECTOR                 color;
    u8*                     head;
    _Actor403600TriScratch* sc;
    GpCoord*                coord;
    u16*                    rec;
    s32                     previous;

    if (D_actor_403600_801606A0 != NULL) {
        previous = -1;
        color    = D_actor_403600_80131E34;
        if (ws->elemCount == 0) {
            return stream;
        }
        head = SCRATCH_HEAD(u8);
        sc   = SCRATCH_HEAD(_Actor403600TriScratch) =
            (_Actor403600TriScratch*)(head - sizeof(_Actor403600TriScratch));
        gte_sttr(&sc->trans);
        gte_ReadRotMatrix(&sc->savedRot);
        TransposeMatrix(&((GpCoord*)D_actor_403600_801606A0)->workm, &sc->local);
        coord         = (GpCoord*)D_actor_403600_801606A0;
        sc->offset.vx = sc->trans.vx - coord->workm.t[0];
        sc->offset.vy = sc->trans.vy - coord->workm.t[1];
        sc->offset.vz = sc->trans.vz - coord->workm.t[2];
        _actor403600RotateSv(&sc->local, &sc->offset);
        gte_MulMatrix0(&sc->local, &sc->savedRot, &sc->local);
        sc->local.t[0] = sc->offset.vx;
        sc->local.t[1] = sc->offset.vy;
        sc->local.t[2] = sc->offset.vz;
        gte_ldrgb(&color);
        while (ws->elemCount-- > 0) {
            rec = (u16*)stream;
            if (rec[0] != previous) {
                gte_SetTransMatrix(&sc->local);
                gte_SetRotMatrix(&sc->local);
                gte_ldv0(&ws->verts[rec[0] >> 3]);
                gte_rtv0tr();
                gte_stsv(&sc->verts[0]);
                if (sc->verts[0].vy > 0) {
                    sc->verts[0].vy = 0;
                }
                gte_SetRotMatrix(&((GpCoord*)D_actor_403600_801606A0)->workm);
                gte_SetTransMatrix(&((GpCoord*)D_actor_403600_801606A0)->workm);
                gte_ldv0(&sc->verts[0]);
                gte_rtps();
                gte_stsz(&ws->gteResult);
                gte_stflg(&ws->gteFlag);
                if (ws->gteFlag & 0x80000000) {
                    ws->gteResult |= 0x80000000;
                }
                ws->szTable[rec[0] >> 3] = ws->gteResult;
            }
            gte_stsxy(ws->preXformWrite + rec[2]);
            gte_ldv0(&ws->normals[rec[1] >> 3]);
            gte_nccs();
            stream += ws->elemStride;
            gte_strgb(ws->preXformWrite + rec[3]);
            previous = rec[0];
        }
        gte_SetTransVector(&sc->trans);
        gte_SetRotMatrix(&sc->savedRot);
        SCRATCH_POP_BYTES(sizeof(_Actor403600TriScratch));
        return stream;
    }
    return tmdXformStreamVerts(ws, flags, stream);
}

const CVECTOR D_actor_403600_80131E34 = { 0x80, 0x80, 0x80, 0 };
