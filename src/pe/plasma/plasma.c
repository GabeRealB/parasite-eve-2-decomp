#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/plasma.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// This overlay's id. Every package opens with one: a u16 in a u32
/// slot, distinct across all 448, with the families in contiguous blocks.
const u32 D_plasma_8012EF30 = 52;

/// Per-level geometry for the plasma ring: rows are PE levels 1-3. `rInner` is
/// the inner radius, `yOff` the height above the caster, `rExtra` how far the
/// ring grows before it breaks up.
PlasmaRingScale D_plasma_8012FF34[] = {
    { 0x0100, 0x0800, 0x0200 },
    { 0x0200, 0x0600, 0x0300 },
    { 0x0300, 0x0400, 0x0400 },
};

/// The `SndEvt_EnqueueType6` id for each `D_plasma_8012FF34` row.
s32 D_plasma_8012FF48[] = { 0xE0160001, 0xE0190001, 0xE01C0001 };

/// Per-vertex jitter the ring walks each frame; three banks of 16.
PlasmaJitter D_plasma_8012FF54 = { 0 };

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern s32 Gp_LcgState;

/// Plasma PE ring. `Task::spawnArg2` is the `GpEffWork` block (`field_24`
/// brightness, `field_20` combo index, `field_22` tick / inner radius);
/// `Task::extra` reaches the coordinate. Cancel (`Gp_StateC08.field_3 == -2`
/// or `Gp_State1C->field_E >= 4`) releases the pool block.
///
/// State 0 seeds brightness, the combo index, and three 16-entry LCG columns
/// in `D_plasma_8012FF54`, plays the combo-indexed cue, and starts a pad
/// lerp. States 1 and 2 decay brightness and draw three rings via
/// `func_plasma_8012FB10` (the third only when `field_20 != 0`) after
/// `func_plasma_8012F568` has applied each jitter column. State 1 is the
/// weaker combo (`field_20 < 2`). Either state releases once brightness
/// drops below 9.
void func_plasma_8012EF34(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpStateC08*    state;
    s32            pan;
    s32            i;
    s16*           row;
    s32            st;
    u16            prev;
    u16            next;
    u8             rgb[3];
    u8             bright;
    s16            span;

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((state->field_3 == -2) || (Gp_State1C->field_E >= 4)) {
        goto release;
    }

    coord->flg = 0;
    Gp_UpdateCoord(coord);
    prev          = mem->field_22;
    next          = prev + 1;
    mem->field_22 = next;
    USE_REG(arg0);
    USE_REG(arg0);
    switch (arg0->state) {
        case 0:
            mem->field_24 = 0xA0;
            mem->field_20 = (Gp_StateC08.field_0 % 10) - 1;
            i             = 0;
            do {
                row                     = &D_plasma_8012FF54.a + i;
                Gp_LcgState             = Gp_LcgState * 5 + 0x71357911;
                ((PlasmaJitter*)row)->a = ((u32)Gp_LcgState >> 16) & 0xFF;
                Gp_LcgState             = Gp_LcgState * 5 + 0x71357911;
                ((PlasmaJitter*)row)->b = ((u32)Gp_LcgState >> 16) & 0xFF;
                Gp_LcgState             = Gp_LcgState * 5 + 0x71357911;
                ((PlasmaJitter*)row)->c = ((u32)Gp_LcgState >> 16) & 0xFF;
                i                      += 1;
            } while (i < 0x10);
            st = 2;
            if (mem->field_20 < 2) {
                st = 1;
            }
            arg0->state = st;
            pan         = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(D_plasma_8012FF48[(u16)(Gp_StateC08.field_0 % 10) - 1], pan,
                                (s8)Gp_GetObjDepth((GpObj38*)coord));
            Gp_SpawnPadLerp((s16)(mem->field_20 * 4 + 0x10), 0xFF, 8);
            return;
        case 1:
            if (mem->field_24 < 9) {
                goto release;
            }
            if (Gp_State1C->field_E == 0) {
                if ((s16)next == 8) {
                    state->field_6 |= 8;
                }
                mem->field_24 = (u16)mem->field_24 - 8;
                mem->field_26 = (u16)mem->field_26 + 0x60 + (mem->field_20 * 0x30);
                mem->field_28 = (u16)mem->field_28 - 0x20;
                mem->field_2A = (u16)mem->field_2A + 0x20;
            } else {
                mem->field_22 = prev;
            }
            func_plasma_8012F568(mem, coord, 0);
            func_plasma_8012F568(mem, coord, 1);
            func_plasma_8012F568(mem, coord, 2);
            bright             = *(u8*)&mem->field_24;
            rgb[1]             = bright;
            rgb[0]             = bright;
            rgb[2]             = mem->field_24 * 3 / 2;
            coord->workm.t[1] -= mem->field_22 << 6;
            {
                s32 a1tmp;
                s32 a2tmp;

                a1tmp = mem->field_22;
                a2tmp = mem->field_20;
                func_plasma_8012FB10(coord, (s16)(a1tmp << 6), (s16)((a2tmp << 7) + 0x100), rgb);
            }
            {
                GsCOORDINATE2* c;
                unsigned int   r;
                unsigned int   g;
                unsigned int   b;
                s32            a1tmp;
                s32            a2tmp;
                u8*            color;

                c = coord;
                COPY_REG_EC(c, coord);
                color = rgb;
                SOFT_TOUCH_REG(color);
                r      = rgb[0];
                b      = rgb[2];
                rgb[0] = r >> 1;
                SOFT_COMPILER_BARRIER();
                g                  = rgb[1];
                rgb[2]             = b >> 1;
                rgb[1]             = g >> 1;
                coord->workm.t[1] -= mem->field_22 << 6;
                a1tmp              = mem->field_22;
                a2tmp              = mem->field_20;
                func_plasma_8012FB10(c, (s16)(a1tmp << 7), (s16)((a2tmp << 7) + 0x100), color);
            }
            if (mem->field_20 != 0) {
                GsCOORDINATE2* c;
                unsigned int   r;
                unsigned int   g;
                unsigned int   b;
                s32            a1tmp;
                s32            a2tmp;
                u8*            color;

                c = coord;
                COPY_REG_EC(c, coord);
                color = rgb;
                SOFT_TOUCH_REG(color);
                r      = rgb[0];
                b      = rgb[2];
                rgb[0] = r >> 1;
                SOFT_COMPILER_BARRIER();
                g              = rgb[1];
                rgb[2]         = b >> 1;
                rgb[1]         = g >> 1;
                c->workm.t[1] -= mem->field_22 << 6;
                a1tmp          = mem->field_22;
                a2tmp          = mem->field_20;
                func_plasma_8012FB10(c, (s16)(a1tmp * 0xC0), (s16)((a2tmp << 7) + 0x100), color);
            }
            return;
        case 2:
            if (mem->field_24 < 9) {
                goto release;
            }
            if (Gp_State1C->field_E == 0) {
                if ((s16)next == 8) {
                    state->field_6 |= 8;
                }
                mem->field_24 = (u16)mem->field_24 - 8;
                mem->field_26 = (u16)mem->field_26 + 0xC0;
                mem->field_28 = (u16)mem->field_28 - 0x20;
                mem->field_2A = (u16)mem->field_2A + 0x20;
            } else {
                mem->field_22 = prev;
            }
            func_plasma_8012F568(mem, coord, 0);
            func_plasma_8012F568(mem, coord, 1);
            func_plasma_8012F568(mem, coord, 2);
            bright             = *(u8*)&mem->field_24;
            rgb[1]             = bright;
            rgb[0]             = bright;
            rgb[2]             = mem->field_24 * 3 / 2;
            coord->workm.t[1] -= mem->field_22 << 7;
            {
                s32 a1tmp;

                a1tmp = mem->field_22;
                span  = (s16)(a1tmp << 6);
                func_plasma_8012FB10(coord, span, span, rgb);
            }
            {
                GsCOORDINATE2* c;
                unsigned int   r;
                unsigned int   g;
                unsigned int   b;
                s32            a1tmp;

                c = coord;
                COPY_REG_EC(c, coord);
                r = rgb[0];
                b = rgb[2];
                TOUCH_REG2(r, b);
                rgb[0]             = r >> 1;
                g                  = rgb[1];
                rgb[2]             = b >> 1;
                rgb[1]             = g >> 1;
                coord->workm.t[1] -= mem->field_22 << 7;
                a1tmp              = mem->field_22;
                span               = (s16)(a1tmp << 7);
                func_plasma_8012FB10(c, span, span, rgb);
            }
            if (mem->field_20 != 0) {
                GsCOORDINATE2* c;
                unsigned int   r;
                unsigned int   g;
                unsigned int   b;
                s32            a1tmp;

                c = coord;
                COPY_REG_EC(c, coord);
                r = rgb[0];
                b = rgb[2];
                TOUCH_REG2(r, b);
                rgb[0]         = r >> 1;
                g              = rgb[1];
                rgb[2]         = b >> 1;
                rgb[1]         = g >> 1;
                c->workm.t[1] -= mem->field_22 << 7;
                a1tmp          = mem->field_22;
                span           = (s16)(a1tmp * 0xC0);
                func_plasma_8012FB10(c, span, span, rgb);
            }
            return;
    }
    return;
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_plasma_8012F568(GpEffWork* arg0, GsCOORDINATE2* arg1, s32 arg2)
{
    void**           scratch;
    u8*              head;
    GpBandScratch*   block;
    SVECTOR*         op;
    POLY_FT4*        prim;
    PlasmaRingScale* row;
    s32              i;
    s32              next;
    s32              ang;
    s32              u;
    s16              idx;
    s16              r0;
    s16              r1;
    u16              y;
    u16              f28;

    row      = &D_plasma_8012FF34[arg2];
    f28      = (u16)arg0->field_28;
    r1       = (u16)arg0->field_26;
    y        = f28 + (u16)row->yOff;
    r1      += (u16)row->rInner;
    r0       = r1 + (u16)arg0->field_2A + (u16)row->rExtra;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch;
    *scratch = head - 0x118;
    block    = (GpBandScratch*)(head - 0x118);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = -y;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&arg1->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0_real();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&arg1->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&arg1->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&arg1->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = 0;
        op->vz             = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&arg1->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0_real();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&arg1->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&arg1->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&arg1->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps_real();
        idx = ((&D_plasma_8012FF54.a + i)[arg2 * 16] + arg0->field_22) % 6;
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt_real();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyFT4(prim);
            setRGB0(prim, *(u8*)&arg0->field_24, *(u8*)&arg0->field_24, *(u8*)&arg0->field_24);
            setSemiTrans(prim, 1);
            prim->tpage = 0x2A;
            prim->clut  = 0x42C1;
            u           = idx * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = *(u16*)&block->sxy0.vx;
            prim->y0 = *(u16*)&block->sxy0.vy;
            prim->x1 = *(u16*)&block->sxy1.vx;
            prim->y1 = *(u16*)&block->sxy1.vy;
            prim->x2 = *(u16*)&block->sxy2.vx;
            prim->y2 = *(u16*)&block->sxy2.vy;
            prim->x3 = *(u16*)&block->sxy3.vx;
            prim->y3 = *(u16*)&block->sxy3.vy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x118;
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a ring. `arg1` is the inner half-extent and `arg2` the extra outer
/// width; on-screen radii are `(s16)arg1 * 64 / (otz + 1)` and
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`. The RGB triple tints the inner edge
/// so each wedge fades to a black outer rim. Byte-identical to the rooms
/// family's `Room_Draw07` (src/rooms/lib/room_draw07.c).
void func_plasma_8012FB10(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    PlasmaRingScratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register s32       hi asm("a1");
    register u8*       head asm("t0");
    register s32       sum asm("a1");
    register s32       otz asm("v0");
    register s32       rOuter asm("a0");
    register s32       rInner asm("v1");
    register u8*       color asm("s4");
    s32                t;
    u16                vz;
    u32                maskLo;
    u32                maskHi;

    saved = arg1;
    asm("lui %0, 0x1F80" : "=r"(hi) : "r"(saved));
    asm("ori %0, %1, 0x3FC" : "=r"(scratch) : "r"(hi));
    color = rgb;
    head  = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((PlasmaRingScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (PlasmaRingScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((PlasmaRingScratch*)(head - 0x1C))->sx);
    gte_stflg(&((PlasmaRingScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((PlasmaRingScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        otz        = block->otz + 1;
        rOuter     = ((s16)saved * 64) / otz;
        rInner     = (s16)sum * 64;
        block->otz = otz;
        SOFT_BARRIER();
        rInner        = rInner / otz;
        ang           = 0;
        block->rOuter = rOuter;
        block->rInner = rInner;

        do {
            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            maskLo   = 0xFFFFFF;
            maskHi   = 0xFF000000;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG2(maskLo, maskHi);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
