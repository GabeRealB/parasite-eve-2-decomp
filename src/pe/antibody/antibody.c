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
#include "pe/antibody.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// Per-level tuning for the antibody motes: rows are PE levels 1-3.
AntibodyStep D_antibody_80130BD4[] = {
    { 0x0008, 0x0090, 0x0005, 0x0200, 0x0080, 0x0600, 0x0008 },
    { 0x000C, 0x00C0, 0x0006, 0x0300, 0x0100, 0x0700, 0x0006 },
    { 0x0010, 0x00F0, 0x0007, 0x0400, 0x0180, 0x0800, 0x0004 },
};

/// The `SndEvt_EnqueueType6` id for each `D_antibody_80130BD4` row.
s32 D_antibody_80130C00[] = { 0xE0290001, 0xE02C0001, 0xE02F0001 };

extern s32 Gp_LcgState;

/// `gpf 1` / `rtps`. The `inline_c.h` macros of those names assemble to
/// different words, so spell the instructions out.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")

void PeShared801305c0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_antibody_8012FBB0(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_antibody_8012FFEC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_antibody_80130428(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);

/// Runs one frame of an antibody cast. `Task::spawnArg2` is the `GpEffWork`
/// block and `Task::extra` reaches the effect coordinate. Cancel
/// (`Gp_StateC08.field_3 == -2` or `Gp_State1C->field_E >= 4`) releases the
/// work block.
///
/// State 0 parents the coordinate with an identity rotation at the origin,
/// seeds `field_20` from the combo counter, refills `D_antibody_80130C0C`
/// with one yaw per wedge, and plays the row's cue. State 1 grows the draw
/// parameter `field_24` by the row's `field_4`, draws three rings plus the
/// `field_0` wedges (and an arc above the weakest row), and for the first
/// 0x14 ticks spawns four `0x600F5` motes on a `field_A`-radius circle every
/// `field_C` frames, reparenting each onto this task. Once `field_24` passes
/// the row's `field_2` cap it spawns the `0x800600AC` burst, latches
/// `field_28` and moves to state 2, which shrinks `field_24` by 0x10 a frame
/// and redraws at the capped radius until it drops below 0x11.
/// Scratch for the mote ring's per-frame vertex work.
/// lists an object in the linker script at its first subsegment, and this has
s16 D_antibody_80130C0C[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

void func_antibody_8012EF34(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpStateC08*    state;
    s32            i;
    u8             rgb[3];

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((state->field_3 != -2) && (Gp_State1C->field_E < 4)) {
        mem->field_22 = (u16)mem->field_22 + 1;
        switch (arg0->state) {
            case 0: {
                GpMtxWords* rot;

                rot               = (GpMtxWords*)&coord->coord;
                coord->sub        = mem->field_8;
                rot->w0           = 0x1000;
                rot->w1           = 0;
                rot->w2           = 0x1000;
                rot->w3           = 0;
                rot->h4           = 0x1000;
                coord->coord.t[2] = 0;
                coord->coord.t[1] = 0;
                coord->coord.t[0] = 0;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                Gp_State1C->field_12 &= 0xFDFF;
                state->field_6       |= 8;
                arg0->state           = 1;
                mem->field_20         = (Gp_StateC08.field_0 % 10) - 1;
                i                     = 0;
                if (D_antibody_80130BD4[mem->field_20].field_0 > 0) {
                    do {
                        s16* dst;
                        s32  lo;
                        s32  rng;

                        dst         = D_antibody_80130C0C;
                        lo          = i * (0x1000 / D_antibody_80130BD4[mem->field_20].field_0);
                        rng         = Gp_LcgState * 5 + 0x71357911;
                        dst[i]      = lo + (((u32)rng >> 16) & 0x1FF);
                        Gp_LcgState = rng;
                    } while (++i < D_antibody_80130BD4[mem->field_20].field_0);
                }
                {
                    s32 pan;

                    pan = (s8)Gp_GetObjPan((GpObj38*)coord);
                    SndEvt_EnqueueType6(D_antibody_80130C00[mem->field_20], pan,
                                        (s8)Gp_GetObjDepth((GpObj38*)coord));
                }
                return;
            }
            case 1: {
                AntibodyStep* table;
                AntibodyStep* t2;
                GpEffWork*    eff;
                s32           rng;
                s16           ang;
                s16*          p;
                s16           count;

                table             = D_antibody_80130BD4;
                mem->field_24     = (u16)mem->field_24 + table[mem->field_20].field_4;
                rgb[0]            = *(u8*)&mem->field_24;
                rgb[1]            = *(u8*)&mem->field_24;
                rgb[2]            = (u16)mem->field_24 >> 1;
                coord->coord.t[1] = -0x400;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                Gp_DrawRing(coord, (s16)(mem->field_24 * 4), rgb);
                Gp_DrawRing(coord, (s16)(mem->field_24 * 8), rgb);
                Gp_DrawRing(coord, (s16)(mem->field_24 * 0xC), rgb);
                if (mem->field_20 != 0) {
                    rgb[0] >>= 1;
                    rgb[1] >>= 1;
                    rgb[2] >>= 1;
                    Gp_DrawArc(coord, (s16)(mem->field_24 * 8), 0x80, rgb);
                }
                i     = 0;
                count = table[mem->field_20].field_0;
                if (count > 0) {
                    t2 = table;
                    p  = D_antibody_80130C0C;
                    do {
                        PeShared801305c0(coord, (s16)(mem->field_24 * 6), *p, rgb);
                        p += 1;
                    } while (++i < t2[mem->field_20].field_0);
                }
                coord->coord.t[1] = 0;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                if (mem->field_22 < 0x14) {
                    if ((mem->field_22 % D_antibody_80130BD4[mem->field_20].field_C) == 1) {
                        i = 0;
                        do {
                            rng           = Gp_LcgState * 5 + 0x71357911;
                            ang           = i + (((u32)rng >> 16) & 0x3FF);
                            Gp_LcgState   = rng;
                            mem->field_26 = ang;
                            mem->field_10 =
                                (D_antibody_80130BD4[mem->field_20].field_A * rsin(ang)) >> 12;
                            mem->field_14 = (D_antibody_80130BD4[mem->field_20].field_A *
                                             rcos(mem->field_26)) >>
                                            12;
                            eff = Gp_SpawnEff(0x600F5, coord, 0, (SVECTOR*)&mem->field_10);
                            if (eff != NULL) {
                                Task_Reparent(arg0, eff->field_0);
                            }
                            i += 0x400;
                        } while (i < 0x1000);
                    }
                }
                if (mem->field_24 > D_antibody_80130BD4[mem->field_20].field_2) {
                    Gp_SpawnEff(0x800600AC, coord, 0, 0);
                    mem->field_28 = (u16)mem->field_24;
                    arg0->state   = 2;
                }
                return;
            }
            case 2: {
                AntibodyStep* table;
                AntibodyStep* t2;
                s16*          p;
                s16           count;

                if (mem->field_24 < 0x11) {
                    goto release;
                }
                mem->field_24     = (u16)mem->field_24 - 0x10;
                rgb[0]            = *(u8*)&mem->field_24;
                rgb[1]            = *(u8*)&mem->field_24;
                rgb[2]            = (u16)mem->field_24 >> 1;
                coord->coord.t[1] = -0x400;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                table = D_antibody_80130BD4;
                Gp_DrawRing(coord, (s16)(table[mem->field_20].field_2 * 4), rgb);
                Gp_DrawRing(coord, (s16)(table[mem->field_20].field_2 * 8), rgb);
                Gp_DrawRing(coord, (s16)(table[mem->field_20].field_2 * 0xC), rgb);
                if (mem->field_20 != 0) {
                    if (mem->field_20 == 2) {
                        mem->field_28 = (u16)mem->field_28 + table[mem->field_20].field_4;
                    }
                    rgb[0] >>= 1;
                    rgb[1] >>= 1;
                    rgb[2] >>= 1;
                    Gp_DrawArc(coord, (s16)(mem->field_28 * 8), 0x80, rgb);
                }
                i     = 0;
                count = D_antibody_80130BD4[mem->field_20].field_0;
                if (count > 0) {
                    t2 = D_antibody_80130BD4;
                    p  = D_antibody_80130C0C;
                    do {
                        PeShared801305c0(coord, (s16)(mem->field_28 * 6), *p, rgb);
                        p += 1;
                    } while (++i < t2[mem->field_20].field_0);
                }
                coord->coord.t[1] = 0;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                return;
            }
        }
        return;
    }
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Runs one frame of an antibody mote. State 0 re-bases the effect coordinate
/// on the `GpEffWork.field_8` parent with an identity rotation and the work
/// block's `field_18`..`field_1C` offset, then GPF-scales that offset by 0x100
/// (a sixteenth) into `field_10`..`field_14` as the per-frame step, and seeds
/// the intensity `field_20` from the combo counter, the draw parameter
/// `field_24` from that row's `field_6` and the phase `field_26` from
/// `Gp_LcgState`. State 1 walks the coordinate back down that step every frame
/// and draws with `func_antibody_8012FBB0`; past tick 0x10 it parks a `-0x80`
/// Y drift in `field_12` and moves to state 2, and one frame in sixteen it
/// jumps straight to state 3 instead. State 2 applies that Y drift and keeps
/// drawing; state 3 draws the larger `func_antibody_8012FFEC` /
/// `func_antibody_80130428` pair. All three re-roll `field_24` / `field_26`
/// from the row's `field_8` one frame in eight, and states 2 and 3 release the
/// effect at tick 0x15.
void func_antibody_8012F734(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    s32            rng0;
    s32            rng1a;
    s32            rng1b;
    s32            rng1c;
    s32            rng1d;
    s32            rng2a;
    s32            rng2b;
    s32            rng2c;
    s32            rng3a;
    s32            rng3b;
    s32            rng3c;
    s16            idx;

    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    coord->flg    = 0;
    switch (arg0->state) {
        case 0:
            rot        = (GpMtxWords*)&coord->coord;
            coord->sub = mem->field_8;
            rot->w0    = 0x1000;
            rot->w1    = 0;
            rot->w2    = 0x1000;
            rot->w3    = 0;
            rot->h4    = 0x1000;

            coord->coord.t[0] = mem->field_18;
            coord->coord.t[1] = mem->field_1A;
            coord->coord.t[2] = mem->field_1C;

            gte_lddp(0x100);
            gte_ldsv(&mem->field_18);
            gte_gpf12_real();
            gte_stsv(&mem->field_10);

            arg0->state   = 1;
            rng0          = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState   = rng0;
            idx           = Gp_StateC08.field_0 % 10 - 1;
            mem->field_20 = idx;
            mem->field_24 = D_antibody_80130BD4[idx].field_6;
            mem->field_26 = ((u32)rng0 >> 16) & 0xFFF;
            /* fallthrough */
        case 1:
            rng1a       = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rng1a;
            if ((((u32)rng1a >> 16) & 7) == 0) {
                rng1b       = rng1a * 5 + 0x71357911;
                Gp_LcgState = rng1b;
                mem->field_24 =
                    D_antibody_80130BD4[mem->field_20].field_8 + (((u32)rng1b >> 16) & 0x1FF);
                rng1c         = rng1b * 5 + 0x71357911;
                Gp_LcgState   = rng1c;
                mem->field_26 = ((u32)rng1c >> 16) & 0xFFF;
            }
            coord->coord.t[0] -= mem->field_10;
            coord->coord.t[1] -= mem->field_12;
            coord->coord.t[2] -= mem->field_14;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            func_antibody_8012FBB0(coord, mem->field_22, mem->field_24, mem->field_26);
            if (mem->field_22 >= 0x10) {
                mem->field_12 = -0x80;
                arg0->state   = 2;
                return;
            }
            rng1d       = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rng1d;
            if ((((u32)rng1d >> 16) & 0xF) == 0) {
                arg0->state = 3;
            }
            return;
        case 2:
            rng2a       = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rng2a;
            if ((((u32)rng2a >> 16) & 7) == 0) {
                rng2b       = rng2a * 5 + 0x71357911;
                Gp_LcgState = rng2b;
                mem->field_24 =
                    D_antibody_80130BD4[mem->field_20].field_8 + (((u32)rng2b >> 16) & 0x1FF);
                rng2c         = rng2b * 5 + 0x71357911;
                Gp_LcgState   = rng2c;
                mem->field_26 = ((u32)rng2c >> 16) & 0xFFF;
            }
            coord->coord.t[1] += mem->field_12;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            func_antibody_8012FBB0(coord, mem->field_22, mem->field_24, mem->field_26);
            goto check;
        case 3:
            rng3a       = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rng3a;
            if ((((u32)rng3a >> 16) & 7) == 0) {
                rng3b         = rng3a * 5 + 0x71357911;
                Gp_LcgState   = rng3b;
                mem->field_24 = (s16)D_antibody_80130BD4[mem->field_20].field_8 * 2 +
                                (((u32)rng3b >> 16) & 0x1FF);
                rng3c         = rng3b * 5 + 0x71357911;
                Gp_LcgState   = rng3c;
                mem->field_26 = ((u32)rng3c >> 16) & 0xFFF;
            }
            Gp_UpdateCoord(coord);
            func_antibody_8012FFEC(coord, mem->field_22, mem->field_24, mem->field_26);
            func_antibody_80130428(coord, mem->field_22, mem->field_24);
        check:
            if (mem->field_22 >= 0x15) {
                Gp_ReleaseState1CMem(mem, arg0);
            }
            break;
    }
}

/// Draws one antibody mote as a semi-transparent raw-tex `POLY_FT4` (tpage
/// 0x29, clut 0x42C6) centred on `arg0`'s world translation, projected with a
/// single `RTPS`. `arg1` picks one of six 40-pixel columns, `arg2` is the
/// radius and `arg3` the spin angle. The quad's corners are that radius
/// rotated by `arg3` and by `arg3 + 0x400`; nothing is drawn if the centre
/// projects off-screen.
void func_antibody_8012FBB0(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    void**               scratch;
    u8*                  head;
    AntibodyMoteScratch* block;
    AntibodyMoteScratch* vecp;
    POLY_FT4*            prim;
    u16                  vz;
    s32                  u;
    s32                  ang2;

    scratch                                      = (void**)G_SCRATCH_HEAD;
    head                                         = *scratch;
    ((AntibodyMoteScratch*)(head - 0x1C))->v0.vx = *(u16*)&arg0->workm.t[0];
    block                                        = (AntibodyMoteScratch*)(head - 0x1C);
    block->v0.vy                                 = *(u16*)&arg0->workm.t[1];
    vz                                           = *(u16*)&arg0->workm.t[2];
    *scratch                                     = block;
    block->v0.vz                                 = vz;
    vecp                                         = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->v0);
    gte_rtps_real();
    gte_stsxy(&((AntibodyMoteScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((AntibodyMoteScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((AntibodyMoteScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        prim->clut  = 0x42C6;
        prim->v0    = 0x50;
        prim->v1    = 0x50;
        prim->v2    = 0x77;
        prim->v3    = 0x77;
        u           = (s16)(arg1 % 6) * 40;
        prim->u0    = u;
        prim->u1    = u + 0x27;
        prim->u2    = u;
        prim->u3    = u + 0x27;
        block->dx   = (((arg2 * 39) / block->otz) * rsin(arg3)) >> 12;
        block->dy   = (((arg2 * 39) / block->otz) * rcos(arg3)) >> 12;
        prim->x0    = *(u16*)&block->sx0 + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sx0 - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sy0 - *(u16*)&block->dy;
        ang2        = arg3 + 0x400;
        prim->y3    = *(u16*)&block->sy0 + *(u16*)&block->dy;
        block->dx   = (((arg2 * 39) / block->otz) * rsin(ang2)) >> 12;
        block->dy   = (((arg2 * 39) / block->otz) * rcos(ang2)) >> 12;
        prim->x1    = *(u16*)&block->sx0 + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sx0 - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sy0 - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sy0 + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

/// Draws one antibody mote as a semi-transparent raw-tex `POLY_FT4`
/// (tpage 0x2A, clut 0x42C9). The effect coordinate's world position is
/// projected through `GsWSMATRIX` with one `RTPS`; the quad is a square laid
/// around that point, `arg3` giving the spin applied at that angle and at
/// `+ 0x400`. `arg1 % 6` selects one of six 40x40 texel tiles along the
/// sprite sheet row at v = 0x38..0x5F. `arg2` is a signed half-extent, so the
/// on-screen half-diagonal is `arg2 * 39 / otz`. Nothing is drawn if the
/// projection sets a negative `gte_stflg`.
void func_antibody_8012FFEC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    void**               scratch;
    u8*                  head;
    AntibodyMoteScratch* block;
    POLY_FT4*            prim;
    SVECTOR*             vec;
    u16                  vz;
    s16                  tile;
    s32                  u0;
    s32                  u1;
    s32                  ang2;

    scratch                                      = (void**)G_SCRATCH_HEAD;
    head                                         = *scratch;
    ((AntibodyMoteScratch*)(head - 0x1C))->v0.vx = *(u16*)&arg0->workm.t[0];
    block                                        = (AntibodyMoteScratch*)(head - 0x1C);
    block->v0.vy                                 = *(u16*)&arg0->workm.t[1];
    vz                                           = *(u16*)&arg0->workm.t[2];
    *scratch                                     = block;
    block->v0.vz                                 = vz;
    vec                                          = &block->v0;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((AntibodyMoteScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((AntibodyMoteScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((AntibodyMoteScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        block->otz     = block->otz + 1;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2A;
        prim->clut  = 0x42C9;
        tile        = arg1 % 6;
        u0          = tile * 0x28;
        u1          = u0 + 0x27;
        setUV4(prim, u0, 0x38, u1, 0x38, u0, 0x5F, u1, 0x5F);
        block->dx = (((arg2 * 0x27) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 0x27) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = *(u16*)&block->sx0 + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx0 - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy0 - *(u16*)&block->dy;
        ang2      = arg3 + 0x400;
        prim->y3  = *(u16*)&block->sy0 + *(u16*)&block->dy;
        block->dx = (((arg2 * 0x27) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 0x27) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx0 + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx0 - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy0 - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy0 + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(AntibodyMoteScratch);
}

/// Draws the antibody arc between the effect and the player as one
/// semi-transparent raw-tex `POLY_FT4` (tpage 0x28, clut 0x42C8). The effect
/// coordinate's world position and the player's second part coordinate are
/// each projected through `GsWSMATRIX` with one `RTPS`; the quad is laid
/// along the line joining the two projected points, `ratan2` of their screen
/// delta giving the spin applied at that angle and at `+ 0x400`. `arg1`
/// selects the 128-texel UV tile: u = `(arg1 & 1) * 128`, v =
/// `((arg1 & 3) >> 1) * 24 - 0x30`. `arg2` is a signed half-extent, so the
/// on-screen half-width is `arg2 * 23 / otz`. Nothing is drawn if either
/// projection sets a negative `gte_stflg`.
void func_antibody_80130428(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**              scratch;
    u8*                 head;
    AntibodyArcScratch* block;
    POLY_FT4*           prim;
    SVECTOR*            vec;
    GsCOORDINATE2*      player;
    s32                 u0;
    s32                 u1;
    s32                 va;
    s32                 vb;
    s16                 ang;
    s32                 ang2;
    u16                 vz;

    player                                      = &((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8[1];
    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((AntibodyArcScratch*)(head - 0x28))->v0.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (AntibodyArcScratch*)(head - 0x28);
    block->v0.vy                                = *(u16*)&arg0->workm.t[1];
    block->v0.vz                                = *(u16*)&arg0->workm.t[2];
    block->v1.vx                                = *(u16*)&player->workm.t[0];
    block->v1.vy                                = *(u16*)&player->workm.t[1];
    vz                                          = *(u16*)&player->workm.t[2];
    *scratch                                    = block;
    block->v1.vz                                = vz;
    vec                                         = &block->v0;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((AntibodyArcScratch*)(head - 0x28))->sx0);
    gte_stflg(&((AntibodyArcScratch*)(head - 0x28))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((AntibodyArcScratch*)(head - 0x28))->otz);
        block->otz = block->otz + 1;
        gte_ldv0(&((AntibodyArcScratch*)(head - 0x28))->v1);
        gte_rtps_real();
        gte_stsxy(&((AntibodyArcScratch*)(head - 0x28))->sx1);
        gte_stflg(&((AntibodyArcScratch*)(head - 0x28))->flag);
        if (block->flag >= 0) {
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            prim->clut  = 0x42C8;
            u0          = (arg1 & 1) << 7;
            u1          = u0 + 0x7F;
            va          = ((arg1 & 3) >> 1) * 24 - 0x30;
            vb          = ((arg1 & 3) >> 1) * 24 - 0x19;
            setUV4(prim, u0, va, u1, va, u0, vb, u1, vb);
            ang       = ratan2(block->sy1 - block->sy0, block->sx1 - block->sx0);
            block->dx = (((arg2 * 0x17) / block->otz) * rsin(ang)) >> 12;
            block->dy = (((arg2 * 0x17) / block->otz) * rcos(ang)) >> 12;
            prim->x0  = *(u16*)&block->sx0 + *(u16*)&block->dx;
            prim->x3  = *(u16*)&block->sx1 - *(u16*)&block->dx;
            prim->y0  = *(u16*)&block->sy0 - *(u16*)&block->dy;
            ang2      = ang + 0x400;
            prim->y3  = *(u16*)&block->sy1 + *(u16*)&block->dy;
            block->dx = (((arg2 * 0x17) / block->otz) * rsin(ang2)) >> 12;
            block->dy = (((arg2 * 0x17) / block->otz) * rcos(ang2)) >> 12;
            prim->x1  = *(u16*)&block->sx1 + *(u16*)&block->dx;
            prim->x2  = *(u16*)&block->sx0 - *(u16*)&block->dx;
            prim->y1  = *(u16*)&block->sy1 - *(u16*)&block->dy;
            prim->y2  = *(u16*)&block->sy0 + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(AntibodyArcScratch);
}

INCLUDE_RODATA("pe/nonmatchings/antibody/antibody", D_antibody_8012EF30);
