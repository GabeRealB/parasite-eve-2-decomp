#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"
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

/// Fixed offset from the parent coordinate that the hammer effect starts at.
SVECTOR D_m4a1_hammer_8011EB60 = { 0, 0x280, 0x20, 0 };

/// Per-frame task for the hammer's charge flare. `Task::spawnArg2` is the
/// `Gp_State1C` work block, `Task::extra` reaches the coordinate the flare
/// hangs on, and `Task::spawnArg1` is the charge phase the firing code drives.
/// Any room fade of 2 or more, and the player being in the state flagged by
/// `TmdObject::flags & 0x80`, freeze the task outright.
///
/// - State 0 hangs the coordinate off `GpEffWork::parent` at the fixed offset
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
///   winds `age` back down and redraws instead of advancing.
void func_m4a1_hammer_8011D1E0(Task* task)
{
    GpEffWork*    work;
    GpCoord*      coord;
    GpCoord*      light;
    GpCoord64*    base;
    GpPointLight* slot;
    GpMtxWords*   dstm;
    s32           i;
    s32           j;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    base  = &Gp_RoomCoords[1];
    light = &base->data.coord;
    slot  = &base->data.light;

    if (((gameGetPtrSlot(3))->extra.tmd->flags & 0x80) == 0 && Gp_State1C->eventState < 2) {
        work->age = work->age + 1;
        switch (task->state) {
            case 0:
                dstm              = (GpMtxWords*)&coord->coord;
                coord->sub        = work->parent;
                dstm->m00_m01     = 0x1000;
                dstm->m11_m12     = 0x1000;
                dstm->m22         = 0x1000;
                dstm->m02_m10     = 0;
                dstm->m20_m21     = 0;
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
                        if (Gp_State1C->eventState != 0) {
                            work->age = work->age - 1;
                            if ((work->age & 1) == 0) {
                                func_m4a1_hammer_8011D904(coord->workm.t, work->age >> 1, work->period,
                                                          work->angle);
                            }
                            return;
                        }
                        Gp_UpdateCoord(coord);
                        if ((work->age & 0xF) == 0) {
                            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                            work->angle = (Gp_LcgState >> 16) & 0xFFF;
                        }
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        work->period = ((Gp_LcgState >> 16) & 0xFF) + 0xC0;
                        if ((work->age & 1) == 0) {
                            func_m4a1_hammer_8011D904(coord->workm.t, work->age >> 1, work->period,
                                                      work->angle);
                        }
                        base->framesLeft = 4;
                        slot->inner      = 0x80;
                        slot->outer      = 0x400;
                        Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
                        slot->head.b     = ((Gp_LcgState >> 16) & 0x700) + 0x400;
                        slot->head.r     = (u16)slot->head.b >> 1;
                        slot->head.g     = (u16)slot->head.b >> 1;
                        Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &light->coord);
                        light->flg  = 0;
                        work->index = 0;
                        return;
                    case 2:
                        if (Gp_State1C->eventState != 0) {
                            work->age = work->age - 1;
                            if ((work->age & 1) == 0) {
                                func_m4a1_hammer_8011DE60(coord, work->age >> 1, work->period,
                                                          work->angle);
                            }
                            return;
                        }
                        Gp_UpdateCoord(coord);
                        if (work->index == 0) {
                            for (i = 0; i < 8; i++) {
                                Gp_LcgState                    = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[i]      = (i << 9) + ((Gp_LcgState >> 16) & 0x1FF);
                                Gp_LcgState                    = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[i + 8]  = ((Gp_LcgState >> 16) & 0x7FF) + 0x200;
                                Gp_LcgState                    = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[i + 16] = (Gp_LcgState >> 16) & 0x3FF;
                            }
                        }
                        if ((work->age & 0xF) == 0) {
                            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                            work->angle = (Gp_LcgState >> 16) & 0xFFF;
                        }
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        work->period = ((Gp_LcgState >> 16) & 0x3FF) + 0x400;
                        if ((work->age & 1) == 0) {
                            func_m4a1_hammer_8011DE60(coord, work->age >> 1, work->period, work->angle);
                            for (i = 0; i < 8; i++) {
                                j                          = i + 8;
                                Gp_LcgState                = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[i] -= ((Gp_LcgState >> 16) & 0x1FF) - 0x100;
                                Gp_LcgState                = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[j] += (Gp_LcgState >> 16) & 0xFF;
                                work->pos.vx =
                                    (D_m4a1_hammer_8012D630[i + 16] * rsin(D_m4a1_hammer_8012D630[i])) >> 12;
                                work->pos.vz =
                                    (D_m4a1_hammer_8012D630[i + 16] * rcos(D_m4a1_hammer_8012D630[i])) >> 12;
                                work->pos.vy = D_m4a1_hammer_8012D630[j];
                                gte_SetRotMatrix(&coord->workm);
                                gte_ldv0(&work->pos);
                                gte_rtv0();
                                gte_stsv(&work->pos);
                                work->pos.vx = work->pos.vx + (u16)D_m4a1_hammer_8012D668.vx;
                                work->pos.vy = work->pos.vy + (u16)D_m4a1_hammer_8012D668.vy;
                                work->pos.vz = work->pos.vz + (u16)D_m4a1_hammer_8012D668.vz;
                                func_m4a1_hammer_8011E29C(coord, &work->pos, work->age, 0x280);
                            }
                        }
                        base->framesLeft = 4;
                        slot->inner      = 0x400;
                        slot->outer      = 0x4000;
                        Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
                        slot->head.b     = ((Gp_LcgState >> 16) & 0x700) + 0x800;
                        slot->head.r     = (u16)slot->head.b >> 1;
                        slot->head.g     = (s16)(u16)slot->head.b >> 1;
                        Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &light->coord);
                        light->flg  = 0;
                        work->index = work->index + 1;
                        if (work->index >= 5) {
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
    u8*              head;
    GpFxQuadScratch* block;
    GpFxQuadScratch* vecp;
    POLY_FT4*        prim;
    u16              vz;
    s32              u;

    head                                      = SCRATCH_HEAD(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0[1];
    vz                                        = (u16)arg0[2];
    SCRATCH_HEAD(GpFxQuadScratch)             = block;
    block->vec.vz                             = vz;
    vecp                                      = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
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
        prim->x0    = block->sx + (u16)block->dx;
        prim->x3    = block->sx - (u16)block->dx;
        prim->y0    = block->sy - (u16)block->dy;
        prim->y3    = block->sy + (u16)block->dy;
        block->dx   = (((arg2 * 23) / block->otz) * rsin(arg3 + 0x400)) >> 12;
        block->dy   = (((arg2 * 23) / block->otz) * rcos(arg3 + 0x400)) >> 12;
        prim->x1    = block->sx + (u16)block->dx;
        prim->x2    = block->sx - (u16)block->dx;
        prim->y1    = block->sy - (u16)block->dy;
        prim->y2    = block->sy + (u16)block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    }
    SCRATCH_POP_BYTES(0x1C);
}

void func_m4a1_hammer_8011DD08(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;
    GpCoord*   parent;

    mem   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
    mem->age++;
    switch (arg0->state) {
        case 0:
            Task_Reparent(D_m4a1_hammer_8012D660, arg0);
            if (arg0->spawnArg1 != 0) {
                parent            = mem->parent;
                coord->coord.t[0] = 0;
                coord->coord.t[1] = 0;
                coord->coord.t[2] = 0;
                coord->flg        = 0;
                coord->sub        = parent;
                Gp_UpdateCoord(coord);
                arg0->state = 1;
            }
            mem->scale  = 0x80;
            Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
            mem->angle  = (Gp_LcgState >> 16) & 0xFFF;
            /* fallthrough */
        case 1:
            if (mem->age & 1) {
                func_m4a1_hammer_8011DE60(coord, ++mem->index, 0x400, mem->angle);
                if (mem->age < 8) {
                    func_m4a1_hammer_8011E29C(coord, &D_m4a1_hammer_8012D668, mem->index, 0x280);
                }
            }
            if (mem->age >= 0x19) {
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
void func_m4a1_hammer_8011DE60(GpCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    GpFxQuadScratch* vecp;
    POLY_FT4*        prim;
    u16              vz;
    s32              u;

    head                                      = SCRATCH_HEAD(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    SCRATCH_HEAD(GpFxQuadScratch)             = block;
    block->vec.vz                             = vz;
    vecp                                      = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
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
        prim->x0    = block->sx + (u16)block->dx;
        prim->x3    = block->sx - (u16)block->dx;
        prim->y0    = block->sy - (u16)block->dy;
        prim->y3    = block->sy + (u16)block->dy;
        block->dx   = (((arg2 * 39) / block->otz) * rsin(arg3 + 0x400)) >> 12;
        block->dy   = (((arg2 * 39) / block->otz) * rcos(arg3 + 0x400)) >> 12;
        prim->x1    = block->sx + (u16)block->dx;
        prim->x2    = block->sx - (u16)block->dx;
        prim->y1    = block->sy - (u16)block->dy;
        prim->y2    = block->sy + (u16)block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    }
    SCRATCH_POP_BYTES(0x1C);
}

void func_m4a1_hammer_8011E29C(GpCoord* coord, SVECTOR* arg1, s32 arg2, s16 arg3)
{
    u8*                     head;
    M4a1HammerTrailScratch* block;
    M4a1HammerTrailScratch* vecp;
    POLY_FT4*               prim;
    s16                     ang;
    u16                     vz;

    head                                             = SCRATCH_HEAD(u8);
    ((M4a1HammerTrailScratch*)(head - 0x20))->vec.vx = (u16)coord->workm.t[0];
    block                                            = (M4a1HammerTrailScratch*)(head - 0x20);
    block->vec.vy                                    = (u16)coord->workm.t[1];
    vz                                               = (u16)coord->workm.t[2];
    SCRATCH_HEAD(M4a1HammerTrailScratch)             = block;
    block->vec.vz                                    = vz;
    vecp                                             = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps();
    gte_stsxy(&((M4a1HammerTrailScratch*)(head - 0x20))->sxy0);
    gte_stflg(&((M4a1HammerTrailScratch*)(head - 0x20))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((M4a1HammerTrailScratch*)(head - 0x20))->otz);
        block->otz++;
        gte_ldv0(arg1);
        gte_rtps();
        gte_stsxy(&((M4a1HammerTrailScratch*)(head - 0x20))->sxy1);
        gte_stflg(&((M4a1HammerTrailScratch*)(head - 0x20))->flag);
        if (block->flag >= 0) {
            block->otz++;
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
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
            prim->x0    = (u16)block->sxy0.vx + (u16)block->dx;
            prim->x3    = (u16)block->sxy1.vx - (u16)block->dx;
            prim->y0    = (u16)block->sxy0.vy - (u16)block->dy;
            prim->y3    = (u16)block->sxy1.vy + (u16)block->dy;
            block->dx   = (((arg3 * 23) / block->otz) * rsin(ang + 0x400)) >> 12;
            block->dy   = (((arg3 * 23) / block->otz) * rcos(ang + 0x400)) >> 12;
            prim->x1    = (u16)block->sxy1.vx + (u16)block->dx;
            prim->x2    = (u16)block->sxy0.vx - (u16)block->dx;
            prim->y1    = (u16)block->sxy1.vy - (u16)block->dy;
            prim->y2    = (u16)block->sxy0.vy + (u16)block->dy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
        }
    }
    SCRATCH_POP_BYTES(0x20);
}
