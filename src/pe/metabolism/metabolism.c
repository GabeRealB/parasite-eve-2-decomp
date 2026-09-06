#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/metabolism.h"

/// Per-level tuning for the metabolism drain: rows are PE levels 1-3.
MetabolismStep D_metabolism_8012FB54[] = {
    { 0x0008, 0x0080, 0x0020, 0x0400 },
    { 0x000C, 0x00B0, 0x0030, 0x0500 },
    { 0x0010, 0x00E0, 0x0040, 0x0600 },
};

/// The `SndEvt_EnqueueType6` id for each `D_metabolism_8012FB54` row.
s32 D_metabolism_8012FB6C[] = { 0xE01F0001, 0xE0220001, 0xE0250001 };

/// Scratch for the drain ring (was its own _work unit).
s16 D_metabolism_8012FB78[16] = { 0 };

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// `mvmva 1, 0, 0, 3, 0`. The `inline_c.h` macro of that name assembles to a
/// different word, so spell the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern s32 Gp_LcgState;

void func_metabolism_8012F840(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);

/// Runs one frame of the metabolism cast. Cancel (`Gp_StateC08.field_3 == -2`
/// or `Gp_State1C->field_E >= 4`) releases the work block. State 0 parents the
/// coordinate to the player with an identity rotation lifted 0x400 above it,
/// picks the intensity row from the combo counter, seeds one random angle per
/// fan wedge into `D_metabolism_8012FB78`, and plays the combo-indexed cue.
/// State 1 grows brightness and radius, and each frame spins the coordinate to
/// three random yaws, rotating `GpEffWork.field_10` through the new frame and
/// then overwriting it with the `field_26` circle at `field_2A`, to parent
/// three `0x60013` sparks; it hands over to state 2 once the radius reaches
/// the row's `field_6`. State 2 shrinks brightness by 0x10 a frame and drops
/// to state 3 - release - below 0x11. States 1 and 2 both draw the fan wedges,
/// two rings and two or three arcs, each arc on a colour halved again from the
/// last.
void func_metabolism_8012EF34(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    GpEffWork*     spawned;
    s32            pan;
    s32            bright;
    s32            i;
    s32            temp_lo;
    u8             rgb[3];

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((Gp_StateC08.field_3 == -2) || (Gp_State1C->field_E >= 4)) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }

    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            rot               = (GpMtxWords*)&coord->coord;
            coord->sub        = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
            rot->w0           = 0x1000;
            rot->w1           = 0;
            rot->w2           = 0x1000;
            rot->w3           = 0;
            rot->h4           = 0x1000;
            coord->coord.t[0] = 0;
            coord->coord.t[1] = -0x400;
            coord->coord.t[2] = 0;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            arg0->state   = 1;
            mem->field_20 = (Gp_StateC08.field_0 % 10) - 1;
            mem->field_26 = 0x80;
            {
                s32 rng;

                for (i = 0; i < D_metabolism_8012FB54[mem->field_20].field_0; i++) {
                    rng                      = Gp_LcgState * 5 + 0x71357911;
                    D_metabolism_8012FB78[i] = (i << 10) + (((u32)rng >> 16) & 0x3FF);
                    Gp_LcgState              = rng;
                }
            }
            Gp_StateC08.field_6 |= 8;
            pan                  = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(D_metabolism_8012FB6C[mem->field_20], pan,
                                (s8)Gp_GetObjDepth((GpObj38*)coord));
            /* fallthrough */
        case 1:
            Gp_UpdateCoord(coord);
            bright = mem->field_24;
            if (bright < D_metabolism_8012FB54[mem->field_20].field_2) {
                bright += 0x10;
            }
            mem->field_24 = bright;
            mem->field_26 = (u16)mem->field_26 + D_metabolism_8012FB54[mem->field_20].field_4;
            {
                s32 rng;
                s32 rng2;

                for (i = 0; i < 3; i++) {
                    rng           = Gp_LcgState * 5 + 0x71357911;
                    rng2          = rng * 5 + 0x71357911;
                    Gp_LcgState   = rng;
                    mem->field_2A = ((u32)rng >> 16) & 0xFFF;
                    Gp_LcgState   = rng2;
                    Gfx_RotMatrixY(&coord->coord, ((u32)rng2 >> 16) & 0xFFF, 0);
                    gte_SetRotMatrix(&coord->coord);
                    gte_ldv0(&mem->field_10);
                    gte_rtv0_real();
                    gte_stsv(&mem->field_10);
                    mem->field_10 = (rcos(mem->field_2A) * mem->field_26) >> 12;
                    temp_lo       = rsin(mem->field_2A) * mem->field_26;
                    mem->field_14 = 0;
                    mem->field_12 = temp_lo >> 12;
                    spawned       = Gp_SpawnEff(0x60013, coord,
                                                D_metabolism_8012FB54[mem->field_20].field_6,
                                                (SVECTOR*)&mem->field_10);
                    if (spawned != NULL) {
                        Task_Reparent(arg0, spawned->field_0);
                    }
                }
            }
            if (mem->field_26 >= D_metabolism_8012FB54[mem->field_20].field_6) {
                arg0->state = 2;
            }
            for (i = 0; i < D_metabolism_8012FB54[mem->field_20].field_0; i++) {
                func_metabolism_8012F840(coord, mem->field_26, D_metabolism_8012FB78[i],
                                         mem->field_24);
            }
            goto draw;
        case 2:
            Gp_UpdateCoord(coord);
            for (i = 0; i < D_metabolism_8012FB54[mem->field_20].field_0; i++) {
                func_metabolism_8012F840(coord, mem->field_26, D_metabolism_8012FB78[i],
                                         mem->field_24);
            }
            mem->field_24 = (u16)mem->field_24 - 0x10;
            mem->field_26 = (u16)mem->field_26 + D_metabolism_8012FB54[mem->field_20].field_4;
            if (mem->field_24 < 0x11) {
                arg0->state = 3;
            }
        draw:
            rgb[0] = (u16)mem->field_24 >> 2;
            rgb[1] = *(u8*)&mem->field_24;
            rgb[2] = (u16)mem->field_24 >> 1;
            Gp_DrawRing(coord, (s32)((u16)mem->field_26 << 16) >> 17, rgb);
            Gp_DrawRing(coord, (s32)((u16)mem->field_26 << 16) >> 17, rgb);
            {
                GsCOORDINATE2* c;
                s32            span;
                unsigned int   r;
                unsigned int   g;
                unsigned int   b;

                c = coord;
                COPY_REG_EC(c, coord);
                span = 0x80;
                TOUCH_REG(span);
                r      = rgb[0];
                b      = rgb[2];
                rgb[0] = r >> 1;
                SOFT_COMPILER_BARRIER();
                g      = rgb[1];
                rgb[2] = b >> 1;
                rgb[1] = g >> 1;
                Gp_DrawArc(c, mem->field_26, span, rgb);
            }
            if ((u16)mem->field_22 & 1) {
                unsigned int g;
                unsigned int b;

                g      = rgb[1];
                b      = rgb[2];
                rgb[1] = g >> 1;
                rgb[2] = b << 1;
                Gp_DrawArc(coord, 0x80, mem->field_26, rgb);
            }
            if (mem->field_20 != 0) {
                GsCOORDINATE2* c;
                s32            span;
                unsigned int   r;
                unsigned int   g;
                unsigned int   b;

                c = coord;
                COPY_REG_EC(c, coord);
                span = 0x80;
                TOUCH_REG(span);
                r      = rgb[0];
                b      = rgb[2];
                rgb[0] = r >> 1;
                SOFT_COMPILER_BARRIER();
                g      = rgb[1];
                rgb[2] = b >> 1;
                rgb[1] = g >> 1;
                Gp_DrawArc(c, (s16)((u16)mem->field_26 + 0x200), span, rgb);
            }
            return;
        case 3:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}

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
