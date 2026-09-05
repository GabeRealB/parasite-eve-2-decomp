#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "weapons/m4a1_hammer.h"
#include "main/tmd.h"

extern u32 Gp_LcgState;

/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix, no translation.
/// The `inline_c.h` macro of that name assembles to a different word, so spell
/// the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// Per-frame task for the hammer's charge flare. `Task::spawnArg2` is the
/// `Gp_State1C` work block, `Task::extra` reaches the coordinate the flare
/// hangs on, and `Task::spawnArg1` is the charge phase the firing code drives.
/// Any room fade of 2 or more, and the player being in the state flagged by
/// `TmdObject::field_C & 0x80`, freeze the task outright.
///
/// - State 0 hangs the coordinate off `GpEffWork::field_8` at the fixed offset
///   `D_m4a1_hammer_8011EB60` with an identity rotation, publishes the task as
///   `D_m4a1_hammer_8012D660` and moves to state 1.
/// - State 1 first republishes the flare's world position as
///   `D_m4a1_hammer_8012D668`, then dispatches on the charge phase. Phase 1
///   idles the flare: it re-rolls the spin angle every 16 frames and the radius
///   every frame, draws it on even frames and claims room-light slot 1 as a
///   narrow (`0x80` / `0x400`) light. Phase 2 charges: on the first frame it
///   seeds the eight sparks in `D_m4a1_hammer_8012D630`, and on every even
///   frame it walks each spark, rotates its offset through the flare's frame
///   and draws it, then widens the light to `0x400` / `0x4000`; five charge
///   frames drop back to phase 1. Phase 3 tears the flare down. A room fade
///   winds `field_22` back down and redraws instead of advancing.
void func_m4a1_hammer_8011D1E0(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* light;
    GpCoord64*     base;
    GpCoordTail*   slot;
    GpMtxWords*    dstm;
    s32            i;
    s32            j;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    base  = &Gp_RoomCoords[1];
    light = &base->coord;
    slot  = (GpCoordTail*)light;

    if ((((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_C & 0x80) == 0 && Gp_State1C->field_4 < 2) {
        work->field_22 = (u16)work->field_22 + 1;
        switch (task->state) {
            case 0:
                dstm              = (GpMtxWords*)&coord->coord;
                coord->sub        = work->field_8;
                dstm->w0          = 0x1000;
                dstm->w2          = 0x1000;
                dstm->h4          = 0x1000;
                dstm->w1          = 0;
                dstm->w3          = 0;
                coord->coord.t[0] = D_m4a1_hammer_8011EB60.vx;
                coord->coord.t[1] = D_m4a1_hammer_8011EB60.vy;
                coord->coord.t[2] = D_m4a1_hammer_8011EB60.vz;
                coord->flg        = 0;

                D_m4a1_hammer_8012D660 = task;
                Gp_UpdateCoord(coord);
                task->state = 1;
                return;
            case 1:
                D_m4a1_hammer_8012D668.vx = coord->workm.t[0];
                D_m4a1_hammer_8012D668.vy = coord->workm.t[1];
                D_m4a1_hammer_8012D668.vz = coord->workm.t[2];
                switch (task->spawnArg1) {
                    case 0:
                        break;
                    case 1:
                        if (Gp_State1C->field_4 != 0) {
                            work->field_22 = (u16)work->field_22 - 1;
                            if ((work->field_22 & 1) == 0) {
                                func_m4a1_hammer_8011D904(coord->workm.t, work->field_22 >> 1, work->field_28,
                                                          work->field_26);
                            }
                            return;
                        }
                        Gp_UpdateCoord(coord);
                        if ((work->field_22 & 0xF) == 0) {
                            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                            work->field_26 = (Gp_LcgState >> 16) & 0xFFF;
                        }
                        Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                        work->field_28 = ((Gp_LcgState >> 16) & 0xFF) + 0xC0;
                        if ((work->field_22 & 1) == 0) {
                            func_m4a1_hammer_8011D904(coord->workm.t, work->field_22 >> 1, work->field_28,
                                                      work->field_26);
                        }
                        base->field_0  = 4;
                        slot->field_58 = 0x80;
                        slot->field_5C = 0x400;
                        Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                        slot->field_54 = ((Gp_LcgState >> 16) & 0x700) + 0x400;
                        slot->field_50 = (u16)slot->field_54 >> 1;
                        slot->field_52 = (u16)slot->field_54 >> 1;
                        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
                        light->flg     = 0;
                        work->field_20 = 0;
                        return;
                    case 2:
                        if (Gp_State1C->field_4 != 0) {
                            work->field_22 = (u16)work->field_22 - 1;
                            if ((work->field_22 & 1) == 0) {
                                func_m4a1_hammer_8011DE60(coord, work->field_22 >> 1, work->field_28,
                                                          work->field_26);
                            }
                            return;
                        }
                        Gp_UpdateCoord(coord);
                        if (work->field_20 == 0) {
                            for (i = 0; i < 8; i++) {
                                Gp_LcgState                    = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[i]      = (i << 9) + ((Gp_LcgState >> 16) & 0x1FF);
                                Gp_LcgState                    = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[i + 8]  = ((Gp_LcgState >> 16) & 0x7FF) + 0x200;
                                Gp_LcgState                    = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[i + 16] = (Gp_LcgState >> 16) & 0x3FF;
                            }
                        }
                        if ((work->field_22 & 0xF) == 0) {
                            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                            work->field_26 = (Gp_LcgState >> 16) & 0xFFF;
                        }
                        Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                        work->field_28 = ((Gp_LcgState >> 16) & 0x3FF) + 0x400;
                        if ((work->field_22 & 1) == 0) {
                            func_m4a1_hammer_8011DE60(coord, work->field_22 >> 1, work->field_28, work->field_26);
                            for (i = 0; i < 8; i++) {
                                j                          = i + 8;
                                Gp_LcgState                = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[i] -= ((Gp_LcgState >> 16) & 0x1FF) - 0x100;
                                Gp_LcgState                = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[j] += (Gp_LcgState >> 16) & 0xFF;
                                work->field_18 =
                                    (D_m4a1_hammer_8012D630[i + 16] * rsin(D_m4a1_hammer_8012D630[i])) >> 12;
                                work->field_1C =
                                    (D_m4a1_hammer_8012D630[i + 16] * rcos(D_m4a1_hammer_8012D630[i])) >> 12;
                                work->field_1A = D_m4a1_hammer_8012D630[j];
                                gte_SetRotMatrix(&coord->workm);
                                gte_ldv0(&work->field_18);
                                gte_rtv0_real();
                                gte_stsv(&work->field_18);
                                work->field_18 = (u16)work->field_18 + (u16)D_m4a1_hammer_8012D668.vx;
                                work->field_1A = (u16)work->field_1A + (u16)D_m4a1_hammer_8012D668.vy;
                                work->field_1C = (u16)work->field_1C + (u16)D_m4a1_hammer_8012D668.vz;
                                func_m4a1_hammer_8011E29C(coord, (SVECTOR*)&work->field_18, work->field_22, 0x280);
                            }
                        }
                        base->field_0  = 4;
                        slot->field_58 = 0x400;
                        slot->field_5C = 0x4000;
                        Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                        slot->field_54 = ((Gp_LcgState >> 16) & 0x700) + 0x800;
                        slot->field_50 = (u16)slot->field_54 >> 1;
                        slot->field_52 = (s16)(u16)slot->field_54 >> 1;
                        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
                        light->flg     = 0;
                        work->field_20 = (u16)work->field_20 + 1;
                        if (work->field_20 >= 5) {
                            task->spawnArg1 = 1;
                        }
                        return;
                    case 3:
                        Gp_ReleaseState1CMem(work, task);
                        return;
                }
                return;
        }
    }
}

/// Draws the hammer's charging flare: one `POLY_FT4` centred on `arg0`, the
/// effect coordinate's world translation, projected with a single `RTPS`.
/// `arg1` picks the animation frame out of the texture page's eight 24-pixel
/// columns, `arg2` is the radius and `arg3` the spin angle. The quad's corners
/// are the radius rotated by `arg3` and by `arg3 + 0x400`, so the sprite spins
/// in screen space; nothing is drawn if the centre projects off-screen.
void func_m4a1_hammer_8011D904(s32* arg0, u16 arg1, u16 arg2, s16 arg3)
{
    void**                  scratch;
    u8*                     head;
    M4a1HammerFlareScratch* block;
    M4a1HammerFlareScratch* vecp;
    POLY_FT4*               prim;
    u16                     vz;
    s32                     u;

    scratch                                          = (void**)G_SCRATCH_HEAD;
    head                                             = *scratch;
    ((M4a1HammerFlareScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0[0];
    block                                            = (M4a1HammerFlareScratch*)(head - 0x1C);
    block->vec.vy                                    = *(u16*)&arg0[1];
    vz                                               = *(u16*)&arg0[2];
    *scratch                                         = block;
    block->vec.vz                                    = vz;
    vecp                                             = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    gte_stsxy(&((M4a1HammerFlareScratch*)(head - 0x1C))->sxy0);
    gte_stflg(&((M4a1HammerFlareScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((M4a1HammerFlareScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x28;
        prim->clut  = 0x430C;
        u           = (arg1 & 7) * 24;
        prim->u0    = u;
        prim->v0    = 0x88;
        prim->u1    = u + 0x17;
        prim->v1    = 0x88;
        prim->u2    = u;
        prim->v2    = 0x9F;
        prim->u3    = u + 0x17;
        prim->v3    = 0x9F;
        block->dx   = (((arg2 * 23) / block->otz) * rsin(arg3)) >> 12;
        block->dy   = (((arg2 * 23) / block->otz) * rcos(arg3)) >> 12;
        prim->x0    = *(u16*)&block->sxy0.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy0.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy0.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy0.vy + *(u16*)&block->dy;
        block->dx   = (((arg2 * 23) / block->otz) * rsin(arg3 + 0x400)) >> 12;
        block->dy   = (((arg2 * 23) / block->otz) * rcos(arg3 + 0x400)) >> 12;
        prim->x1    = *(u16*)&block->sxy0.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy0.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy0.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy0.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

void func_m4a1_hammer_8011DD08(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    mem->field_22++;
    switch (arg0->state) {
        case 0:
            Task_Reparent(D_m4a1_hammer_8012D660, arg0);
            if (arg0->spawnArg1 != 0) {
                parent            = mem->field_8;
                coord->coord.t[0] = 0;
                coord->coord.t[1] = 0;
                coord->coord.t[2] = 0;
                coord->flg        = 0;
                coord->sub        = parent;
                Gp_UpdateCoord(coord);
                arg0->state = 1;
            }
            mem->field_24 = 0x80;
            Gp_LcgState   = (Gp_LcgState * 5) + 0x71357911;
            mem->field_26 = (Gp_LcgState >> 16) & 0xFFF;
            /* fallthrough */
        case 1:
            if (mem->field_22 & 1) {
                func_m4a1_hammer_8011DE60(coord, ++mem->field_20, 0x400, mem->field_26);
                if (mem->field_22 < 8) {
                    func_m4a1_hammer_8011E29C(coord, &D_m4a1_hammer_8012D668, mem->field_20, 0x280);
                }
            }
            if (mem->field_22 >= 0x19) {
                Gp_ReleaseState1CMem(mem, arg0);
            }
            break;
    }
}

/// Draws the hammer's expanding billboard: one `POLY_FT4` centred on `arg0`'s
/// world translation, projected with a single `RTPS`. `arg1` picks the frame
/// out of the texture page's six 40-pixel columns, `arg2` is the radius and
/// `arg3` the spin angle. The quad's corners are the radius rotated by `arg3`
/// and by `arg3 + 0x400`, so the sprite spins in screen space; nothing is
/// drawn if the centre projects off-screen. Same shape as
/// `func_m4a1_hammer_8011D904` on a wider, brighter page.
void func_m4a1_hammer_8011DE60(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    void**                  scratch;
    u8*                     head;
    M4a1HammerFlareScratch* block;
    M4a1HammerFlareScratch* vecp;
    POLY_FT4*               prim;
    u16                     vz;
    s32                     u;

    scratch                                          = (void**)G_SCRATCH_HEAD;
    head                                             = *scratch;
    ((M4a1HammerFlareScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                            = (M4a1HammerFlareScratch*)(head - 0x1C);
    block->vec.vy                                    = *(u16*)&arg0->workm.t[1];
    vz                                               = *(u16*)&arg0->workm.t[2];
    *scratch                                         = block;
    block->vec.vz                                    = vz;
    vecp                                             = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    gte_stsxy(&((M4a1HammerFlareScratch*)(head - 0x1C))->sxy0);
    gte_stflg(&((M4a1HammerFlareScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((M4a1HammerFlareScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2A;
        prim->clut  = 0x4293;
        u           = (s16)(arg1 % 6) * 40;
        prim->u0    = u;
        prim->v0    = 0x38;
        prim->u1    = u + 0x27;
        prim->v1    = 0x38;
        prim->u2    = u;
        prim->v2    = 0x5F;
        prim->u3    = u + 0x27;
        prim->v3    = 0x5F;
        block->dx   = (((arg2 * 39) / block->otz) * rsin(arg3)) >> 12;
        block->dy   = (((arg2 * 39) / block->otz) * rcos(arg3)) >> 12;
        prim->x0    = *(u16*)&block->sxy0.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy0.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy0.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy0.vy + *(u16*)&block->dy;
        block->dx   = (((arg2 * 39) / block->otz) * rsin(arg3 + 0x400)) >> 12;
        block->dy   = (((arg2 * 39) / block->otz) * rcos(arg3 + 0x400)) >> 12;
        prim->x1    = *(u16*)&block->sxy0.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy0.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy0.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy0.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

void func_m4a1_hammer_8011E29C(GsCOORDINATE2* coord, SVECTOR* arg1, s32 arg2, s16 arg3)
{
    void**                  scratch;
    u8*                     head;
    M4a1HammerTrailScratch* block;
    M4a1HammerTrailScratch* vecp;
    POLY_FT4*               prim;
    s16                     ang;
    u16                     vz;

    scratch                                          = (void**)G_SCRATCH_HEAD;
    head                                             = *scratch;
    ((M4a1HammerTrailScratch*)(head - 0x20))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                            = (M4a1HammerTrailScratch*)(head - 0x20);
    block->vec.vy                                    = *(u16*)&coord->workm.t[1];
    vz                                               = *(u16*)&coord->workm.t[2];
    *scratch                                         = block;
    block->vec.vz                                    = vz;
    vecp                                             = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    gte_stsxy(&((M4a1HammerTrailScratch*)(head - 0x20))->sxy0);
    gte_stflg(&((M4a1HammerTrailScratch*)(head - 0x20))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((M4a1HammerTrailScratch*)(head - 0x20))->otz);
        block->otz++;
        gte_ldv0(arg1);
        gte_rtps_real();
        gte_stsxy(&((M4a1HammerTrailScratch*)(head - 0x20))->sxy1);
        gte_stflg(&((M4a1HammerTrailScratch*)(head - 0x20))->flag);
        if (block->flag >= 0) {
            block->otz++;
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            prim->clut  = 0x4287;
            prim->u0    = (arg2 & 1) << 7;
            prim->v0    = ((u32)(arg2 & 3) >> 1) * 24 - 0x30;
            prim->u1    = ((arg2 & 1) << 7) + 0x7F;
            prim->v1    = ((u32)(arg2 & 3) >> 1) * 24 - 0x30;
            prim->u2    = (arg2 & 1) << 7;
            prim->v2    = ((u32)(arg2 & 3) >> 1) * 24 - 0x19;
            prim->u3    = ((arg2 & 1) << 7) + 0x7F;
            prim->v3    = ((u32)(arg2 & 3) >> 1) * 24 - 0x19;
            ang         = ratan2(block->sxy1.vy - block->sxy0.vy, block->sxy1.vx - block->sxy0.vx);
            block->dx   = (((arg3 * 23) / block->otz) * rsin(ang)) >> 12;
            block->dy   = (((arg3 * 23) / block->otz) * rcos(ang)) >> 12;
            prim->x0    = *(u16*)&block->sxy0.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy1.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy0.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy1.vy + *(u16*)&block->dy;
            block->dx   = (((arg3 * 23) / block->otz) * rsin(ang + 0x400)) >> 12;
            block->dy   = (((arg3 * 23) / block->otz) * rcos(ang + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy1.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy0.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy1.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy0.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x20;
}
