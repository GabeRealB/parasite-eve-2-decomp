#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s32 Gp_LcgState;

INCLUDE_ASM("pe/nonmatchings/metabolism/metabolism", func_metabolism_8012EF34);

/// Metabolism billboard. State 0 seeds the spin from the spawn argument and
/// picks the draw path: the plain additive quad (state 1), or, one roll in
/// three when the level's difficulty band allows it, the alternate
/// `func_800EB6E8` quad that fades its colour by 0x18 a frame (state 2).
/// Both states lift the frame and draw on odd ticks until it runs out.
void func_metabolism_8012F5A0(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            y;
    s16            step;
    u16            kind;
    u16            roll;

    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            mem->field_10 = 0;
            mem->field_12 = 8;
            mem->field_14 = 0;
            mem->field_26 = arg0->spawnArg1 & 0xFFF;
            kind          = Gp_StateC08.field_0 % 10U;
            if (kind - 1 < 2 ||
                (Gp_LcgState = Gp_LcgState * 5 + 0x71357911,
                 roll        = ((u32)Gp_LcgState >> 16) % 3U, roll != 0)) {
                arg0->state   = 1;
                mem->field_28 = 0x1000;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_24 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            } else {
                arg0->state   = 2;
                mem->field_24 = 0xC0;
                mem->field_28 = 0x3000;
            }
            return;
        case 1:
            step              = mem->field_12;
            y                 = coord->coord.t[1] + step;
            coord->flg        = 0;
            coord->coord.t[1] = y;
            Gp_UpdateCoord(coord);
            if (!((u16)mem->field_22 & 1)) {
                mem->field_20 = (u16)mem->field_20 + 1;
            }
            if (mem->field_20 < 8) {
                if ((u16)mem->field_22 & 1) {
                    Gp_DrawFxQuad(coord, (u16)mem->field_20, mem->field_26,
                                  (u16)mem->field_24 | (u16)mem->field_28);
                    return;
                }
            } else {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            break;
        case 2:
            step              = mem->field_12;
            y                 = coord->coord.t[1] + step;
            coord->flg        = 0;
            coord->coord.t[1] = y;
            Gp_UpdateCoord(coord);
            if (!((u16)mem->field_22 & 1)) {
                mem->field_20 = (u16)mem->field_20 + 1;
            }
            if (mem->field_20 < 8) {
                if ((u16)mem->field_22 & 1) {
                    func_800EB6E8(coord, (u16)mem->field_20, (u16)mem->field_26,
                                  (u16)mem->field_24 | (u16)mem->field_28);
                    mem->field_24 = (u16)mem->field_24 - 0x18;
                    return;
                }
            } else {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            break;
    }
}

/// Draws one wedge of the metabolism fan as a Gouraud triangle. `arg0`'s
/// origin is projected once through `GsWSMATRIX`; the two outer corners sit
/// `arg1` screen units away at `arg2 - 0x20` and `arg2 + 0x20`. Apex colour
/// is a single channel: red is halved, green is `arg3`, blue is shifted by
/// the low bit of `Display_State.field_8`. The rim fades to black. A
/// negative `gte_stflg` drops the wedge.
void func_metabolism_8012F840(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    SVECTOR*       vec;
    POLY_G3*       prim;
    s32            ang;
    s32            ang2;
    s32            color;
    u16            vz;

    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = *(u16*)&arg0->workm.t[1];
    vz                                      = *(u16*)&arg0->workm.t[2];
    color                                   = arg3;
    SOFT_TOUCH_REG(color);
    *scratch      = block;
    block->vec.vz = vz;
    vec           = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_G3*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setPolyG3(prim);
        setRGB0(prim, (s16)arg3 >> 1, color, (s16)arg3 >> (Display_State.field_8 & 1));
        setRGB1(prim, 0, 0, 0);
        setRGB2(prim, 0, 0, 0);
        block->step = ((s16)arg1 * 128) / block->otz;
        ang         = (s16)arg2;
        ang2        = ang - 0x20;
        prim->x0    = *(u16*)&block->sx;
        prim->y0    = *(u16*)&block->sy;
        prim->x1    = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
        prim->y1    = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
        ang        += 0x20;
        prim->x2    = *(u16*)&block->sx + ((block->step * rsin(ang)) >> 12);
        prim->y2    = *(u16*)&block->sy + ((block->step * rcos(ang)) >> 12);
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

INCLUDE_RODATA("pe/nonmatchings/metabolism/metabolism", D_metabolism_8012EF30);
