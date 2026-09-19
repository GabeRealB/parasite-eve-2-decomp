#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actor_800100.h"

/// Per-frame flare task of the actor: while the player model is visible
/// (`field_C & 0x80` clear) and the room is not fading out
/// (`Gp_State1C->field_4 < 2`) it claims room-light slot 3 as the flare's
/// coordinate. State 0 hangs that coordinate off the actor's own at the fixed
/// offset and zeroes its `field_22`; state 1 then dispatches on `spawnArg1`:
///
/// - 1 draws the flare at the coordinate's `workm.t` every frame and re-aims
///   the light at a random angle in `0x400..0xB00`, arming the flare width in
///   `field_24`.
/// - 2 widens that flare by 0x40 a frame up to 0x180, spawns effect `0x60181`
///   as a child of this task, and re-claims the light with a much wider
///   (`0x400` / `0x4000`) falloff and a `0x800..0xF00` angle.
/// - 3 and 4 switch back to sub-state 1 and 0, and 5 releases the pool block.
///
/// While `Gp_State1C->field_4` is non-zero the two drawing sub-states wind
/// `field_22` back down instead of advancing.
void func_actor_800100_80161F20(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;
    GsCOORDINATE2* light;
    GpMtxWords*    rot;
    GpEffWork*     eff;
    u32            ang;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    base  = &D_8011505C;
    light = &base->coord;
    slot  = (GpCoordTail*)light;
    if ((((GpActorWork*)Game_GetPtrSlot(10))->extra->flags & 0x80) != 0) {
        return;
    }
    if (Gp_State1C->field_4 >= 2) {
        return;
    }
    work->field_22++;
    switch (task->state) {
        case 0:
            rot               = (GpMtxWords*)&coord->coord;
            coord->sub        = work->field_8;
            rot->w0           = 0x1000;
            rot->w1           = 0;
            rot->w2           = 0x1000;
            rot->w3           = 0;
            rot->h4           = 0x1000;
            coord->coord.t[0] = D_actor_800100_80167128.vx;
            coord->coord.t[1] = D_actor_800100_80167128.vy;
            coord->coord.t[2] = D_actor_800100_80167128.vz;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            task->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            switch (task->spawnArg1) {
                case 0:
                    break;
                case 1:
                    if (Gp_State1C->field_4 != 0) {
                        work->field_22--;
                        func_actor_800100_80162264(
                            (VECTOR3*)&coord->workm.t, work->field_22, 0x80);
                        break;
                    }
                    func_actor_800100_80162264(
                        (VECTOR3*)&coord->workm.t, work->field_22, 0x80);
                    base->field_0  = 4;
                    slot->field_58 = 0x80;
                    slot->field_5C = 0x400;
                    ang            = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState    = ang;
                    slot->field_50 = ((ang >> 16) & 0x700) + 0x400;
                    slot->field_52 = (u16)slot->field_50 >> 1;
                    slot->field_54 = slot->field_50 >> 2;
                    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
                    light->flg     = 0;
                    work->field_24 = 0x40;
                    break;
                case 2:
                    if (Gp_State1C->field_4 != 0) {
                        work->field_22--;
                        break;
                    }
                    if (work->field_24 < 0x180) {
                        work->field_24 = (u16)work->field_24 + 0x40;
                    }
                    eff = Gp_SpawnEff(0x60181, coord, work->field_24, NULL);
                    if (eff != NULL) {
                        Task_Reparent(task, eff->field_0);
                    }
                    base->field_0  = 4;
                    slot->field_58 = 0x400;
                    slot->field_5C = 0x4000;
                    ang            = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState    = ang;
                    slot->field_50 = ((ang >> 16) & 0x700) + 0x800;
                    slot->field_52 = (u16)slot->field_50 >> 1;
                    slot->field_54 = slot->field_50 >> 2;
                    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
                    light->flg = 0;
                    break;
                case 3:
                    task->spawnArg1 = 1;
                    break;
                case 4:
                    task->spawnArg1 = 0;
                    break;
                case 5:
                    Gp_ReleaseState1CMem(work, task);
                    break;
            }
            break;
    }
}

/// Links one frame of the flare's animated sprite at the world point `pos`.
/// The point is projected through `GsWSMATRIX` by a single `RTPS` and the quad
/// is dropped when that sets a negative `gte_stflg`. `frame % 6` picks one of
/// the six 0x20-wide texture frames on tpage 0x29 (CLUT 0x430D) and
/// `brightness` sizes it: the corners sit `brightness * 31 / otz` from the
/// projected centre, so the sprite shrinks with distance. Same 0x18-byte
/// `GpEffFt4Scratch` block and axis-aligned quad as the gameplay sprite
/// drawers.
void func_actor_800100_80162264(VECTOR3* pos, u16 frame, s32 brightness)
{
    void**           scratch;
    u8*              head;
    GpEffFt4Scratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s16              x;
    s16              y;
    u16              uv;
    s32              u0;
    s32              u1;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpEffFt4Scratch*)(head - 0x18))->vec.vx = *(u16*)&pos->vx;
    block                                     = (GpEffFt4Scratch*)(head - 0x18);
    block->vec.vy                             = *(u16*)&pos->vy;
    vz                                        = *(u16*)&pos->vz;
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffFt4Scratch*)(head - 0x18))->sx);
    gte_stflg(&((GpEffFt4Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffFt4Scratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        prim->clut  = 0x430D;
        prim->v0    = 0x98;
        prim->v1    = 0x98;
        prim->v2    = 0xB7;
        prim->v3    = 0xB7;
        uv          = frame % 6;
        u0          = uv << 5;
        u1          = u0 + 0x1F;
        prim->u0    = u0;
        prim->u1    = u1;
        prim->u2    = u0;
        prim->u3    = u1;
        block->size = ((u16)brightness * 31) / block->otz;
        x           = *(u16*)&block->sx - *(u16*)&block->size;
        prim->x2    = x;
        prim->x0    = x;
        x           = *(u16*)&block->sx + *(u16*)&block->size;
        prim->x3    = x;
        prim->x1    = x;
        y           = *(u16*)&block->sy - *(u16*)&block->size;
        prim->y1    = y;
        prim->y0    = y;
        y           = *(u16*)&block->sy + *(u16*)&block->size;
        prim->y3    = y;
        prim->y2    = y;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

/// Projectile task of the actor: while the state block says a fade-out is not
/// running it winds `work->field_22` (the animation frame, halved for the
/// draw) forward, and while one is (`Gp_State1C->field_4` non-zero) it just
/// redraws at the coordinate. `field_4 >= 4` tears the task down.
///
/// - State 0 allocates the projectile's `Actor800100Beam`, claims the exit
///   callback, seeds its spin from `Gp_LcgState`, and rotates the scratch
///   `(0, pitch, roll)` vector by the task's own coordinate through the GTE
///   to get the launch direction. It arms the record's payload `0x21C9E`,
///   links the object onto list 1, and falls through.
/// - State 1 steps the coordinate by that direction, redraws, and rolls
///   `Gp_LcgState % 3` to drop a ground impact (`Gp_TraceGroundCoord` plus
///   `func_actor_800100_80162E90` at two thirds of the width) when the room's
///   ground is live. A hit on anything (`func_800DE7CC`) ends the flight into
///   state 2, and a miss after 0x15 frames releases the task.
/// - State 2 keeps falling at four times the speed until the same 0x15.
void func_actor_800100_801624F0(Task* task)
{
    GsCOORDINATE2    ground;
    SVECTOR          after;
    SVECTOR          before;
    GsCOORDINATE2*   coord;
    GpEffWork*       work;
    Actor800100Beam* beam;
    s32              fade;
    u32              ang0;
    u32              ang1;
    u32              ang2;
    u32              ang3;

    beam  = (Actor800100Beam*)task->work;
    work  = task->spawnArg2;
    fade  = Gp_State1C->field_4;
    coord = ((TmdObject*)task->extra)->coords;
    if (fade >= 4) {
        if (task->state != 0) {
            Gp_UnlinkObj(&beam->obj);
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    if (fade != 0) {
        Gp_UpdateCoord(coord);
        func_actor_800100_80162A14((VECTOR3*)coord->workm.t,
                                   ((s16)(u16)work->field_22 >> 1) + 1, work->field_24,
                                   work->field_26);
        return;
    }
    work->field_22 = (u16)work->field_22 + 1;
    switch (task->state) {
        case 0:
            beam = memCalloc(sizeof(Actor800100Beam), 0);
            if (beam == NULL) {
                work->field_22 = 0;
                return;
            }
            task->exitCallback = func_actor_800100_801631C8;
            work->field_10     = 0;
            ang0               = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState        = ang0;
            work->field_12     = (u16)task->spawnArg1 - ((ang0 >> 16) & 0x3F);
            work->field_14     = 0;
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&work->field_10);
            gte_rtv0_real();
            gte_stsv(&work->field_10);
            work->field_24     = (u16)task->spawnArg1 + 0x180;
            ang1               = Gp_LcgState * 5 + 0x71357911;
            work->field_26     = (ang1 >> 16) & 0xFFF;
            task->state        = 1;
            task->work         = (TaskIdMap*)beam;
            beam->obj.coord    = coord;
            beam->obj.ctx.recs = beam->rec;
            beam->obj.key      = 0x21C9E;
            beam->obj.radius   = (s16)(u16)work->field_24 >> 1;
            Gp_LcgState        = ang1;
            beam->obj.flags    = 1;
            Gp_LinkObj(1, &beam->obj);
            beam->rec[0].flags = 2;
            beam->obj.flags   |= 0x8000;
            /* fallthrough */
        case 1:
            work->field_24     = (u16)work->field_24 + 0x10;
            work->field_12     = (u16)work->field_12 + 8;
            before.vx          = coord->workm.t[0];
            before.vy          = coord->workm.t[1];
            before.vz          = coord->workm.t[2];
            coord->coord.t[0] += work->field_10;
            coord->coord.t[1] += work->field_12;
            coord->coord.t[2] += work->field_14;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            after.vx = coord->workm.t[0];
            after.vy = coord->workm.t[1];
            after.vz = coord->workm.t[2];
            func_actor_800100_80162A14((VECTOR3*)coord->workm.t,
                                       ((s16)(u16)work->field_22 >> 1) + 1, work->field_24,
                                       work->field_26);
            ang2        = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = ang2;
            if ((u16)((ang2 >> 16) % 3) == 0 && Gp_State1C->field_6 != 0 &&
                Gp_TraceGroundCoord(coord, &ground) == 1) {
                func_actor_800100_80162E90((VECTOR3*)ground.workm.t,
                                           (s16)((work->field_24 * 2) / 3));
            }
            if (Gp_CountRec18Hi(beam->obj.ctx.recs, 0x30000) != 0) {
                Gp_UnlinkObj(&beam->obj);
                Gp_ReleaseState1CMem(work, task);
                return;
            }
            if (func_800DE7CC(&after, &before, NULL, NULL) == 1) {
                Gp_UnlinkObj(&beam->obj);
                task->state    = 2;
                work->field_10 = (u32)rcos(work->field_26) >> 8;
                work->field_12 = (u32)rsin(work->field_26) >> 8;
                ang3           = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState    = ang3;
                work->field_14 = (u32)rsin((ang3 >> 16) & 0xFFF) >> 8;
                return;
            }
            if (work->field_22 >= 0x15) {
                Gp_UnlinkObj(&beam->obj);
                Gp_ReleaseState1CMem(work, task);
                return;
            }
            Gp_ClearRec18Occupied(beam->rec);
            return;
        case 2:
            work->field_24     = (u16)work->field_24 + 0x40;
            coord->coord.t[0] += work->field_10;
            coord->coord.t[1] += work->field_12;
            coord->coord.t[2] += work->field_14;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            func_actor_800100_80162A14((VECTOR3*)coord->workm.t,
                                       ((s16)(u16)work->field_22 >> 1) + 1, work->field_24,
                                       work->field_26);
            if (work->field_22 >= 0x15) {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Draws one frame of the launched projectile's spinning sprite: a single
/// semi-transparent, textured `POLY_FT4` billboarded on the world point `pos`.
/// `frame` walks the twelve sprite windows of `D_80111E48`, `width` is the
/// flare's half-width (divided down by the projected depth) and `ang` its spin,
/// so the quad is a square rotated by `ang` rather than an axis-aligned sprite.
/// `otz` is biased by one before it is used as the divisor so a point on the
/// near plane cannot divide by zero.
void func_actor_800100_80162A14(VECTOR3* pos, u16 frame, u16 width, s16 ang)
{
    void**                  scratch;
    u8*                     head;
    Actor800100SpinScratch* block;
    Actor800100SpinScratch* vecp;
    POLY_FT4*               prim;
    GpEffUv8*               rec;
    u16                     idx;
    s32                     a;
    u16                     vz;

    scratch                                          = (void**)G_SCRATCH_HEAD;
    head                                             = *scratch;
    ((Actor800100SpinScratch*)(head - 0x1C))->vec.vx = *(u16*)&pos->vx;
    block                                            = (Actor800100SpinScratch*)(head - 0x1C);
    block->vec.vy                                    = *(u16*)&pos->vy;
    vz                                               = *(u16*)&pos->vz;
    *scratch                                         = block;
    block->vec.vz                                    = vz;
    vecp                                             = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    idx = frame % 12;
    gte_stsxy(&((Actor800100SpinScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((Actor800100SpinScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((Actor800100SpinScratch*)(head - 0x1C))->otz);
        block->otz     = block->otz + 1;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        rec         = &D_80111E48[idx];
        prim->clut  = (rec->clutY << 6) | ((rec->clutX >> 4) & 0x3F);
        prim->u0    = rec->u;
        prim->v0    = rec->v;
        prim->u1    = rec->u + 0x27;
        prim->v1    = rec->v;
        prim->u2    = rec->u;
        prim->v2    = rec->v + 0x27;
        prim->u3    = rec->u + 0x27;
        prim->v3    = rec->v + 0x27;
        a           = ang;
        block->dx   = (((width * 0x27) / block->otz) * rsin(a)) >> 12;
        block->dy   = (((width * 0x27) / block->otz) * rcos(a)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        a           = a + 0x400;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        block->dx   = (((width * 0x27) / block->otz) * rsin(a)) >> 12;
        block->dy   = (((width * 0x27) / block->otz) * rcos(a)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

/// Draws the projectile's ground splash at the traced ground point `pos`: the
/// unit quad `D_80111E38` scaled to `width` half-size, laid flat by
/// `Gfx_ViewWorldMtx`, and projected through `GsWSMATRIX` into a 0x30-byte
/// `G_SCRATCH_HEAD` block. The first corner goes through `rtps` and the other
/// three through one `rtpt`; a negative `gte_stflg` drops the quad.
void func_actor_800100_80162E90(VECTOR3* pos, s32 width)
{
    void**                    scratch;
    u8*                       head;
    Actor800100SplashScratch* block;
    POLY_FT4*                 prim;
    GpQuadCorner*             tbl;
    s32                       i;
    s32                       flag;
    s32                       otz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = (u8*)*scratch - 0x30;
    SOFT_TOUCH_REG(head);
    *scratch = head;
    block    = (Actor800100SplashScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    tbl = D_80111E38;
    do {
        block->vec[i].vx = tbl[i].x * width;
        block->vec[i].vy = 0;
        block->vec[i].vz = tbl[i].y * width;
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(&block->vec[i]);
        gte_rtv0_real();
        gte_stsv(&block->vec[i]);
        *(u16*)&block->vec[i].vx = *(u16*)&block->vec[i].vx + *(u16*)&pos->vx;
        *(u16*)&block->vec[i].vy = *(u16*)&block->vec[i].vy + *(u16*)&pos->vy;
        *(u16*)&block->vec[i].vz = *(u16*)&block->vec[i].vz + *(u16*)&pos->vz;
        i++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps_real();
    gte_stsxy(&block->sxy[0]);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt_real();
    gte_stsxy3(&block->sxy[1], &block->sxy[2], &block->sxy[3]);
    gte_stflg(&flag);
    if (flag >= 0) {
        gte_stszotz(&otz);
        otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x40, 0x40, 0x40);
        prim->tpage = 0x29;
        prim->clut  = 0x430F;
        setUV4(prim, 0xE0, 0xC8, 0xFF, 0xC8, 0xE0, 0xE7, 0xFF, 0xE7);
        prim->x0 = block->sxy[0].vx;
        prim->y0 = block->sxy[0].vy;
        prim->x1 = block->sxy[1].vx;
        prim->y1 = block->sxy[1].vy;
        prim->x2 = block->sxy[2].vx;
        prim->y2 = block->sxy[2].vy;
        prim->x3 = block->sxy[3].vx;
        prim->y3 = block->sxy[3].vy;
        addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x30;
}

void func_actor_800100_801631C8(Task* arg0)
{
    GpObj* temp_a0;
    void*  temp_s1;

    temp_a0 = arg0->work;
    temp_s1 = arg0->spawnArg2;
    if (temp_a0 != NULL) {
        Gp_UnlinkObj(temp_a0);
    }
    Gp_ReleaseState1CMem(temp_s1, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80163214);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_801635F4);

/// Texture-upload state of the actor: runs two independent sequences, each a
/// countdown (`field_988` / `field_98B`, reloaded with 4 / 8) that advances a
/// frame index (`field_989` / `field_98C`) through the NULL-terminated image
/// list `D_actor_800100_80167200` / `D_actor_800100_80167210` named by the
/// sequence number (`field_987` / `field_98A`), ending the sequence when its
/// list runs out. Every upload posts its image over an 8-byte `RECT` borrowed
/// from `G_SCRATCH_HEAD` and gives it back at the end of the call.
void func_actor_800100_80163A58(GpActorWork* arg0)
{
    void**      scratch;
    u8*         head;
    s32         temp;
    RECT*       rect;
    GameActor*  actor;
    GpImgRec*** table;
    s32         idx;
    u32         row;
    GpImgRec*   img;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    actor    = arg0->actor;
    temp     = (s32)(head - 8);
    *scratch = (void*)temp;
    rect     = (RECT*)temp;

    if ((s8)actor->field_987 != 0) {
        actor->field_988--;
        if ((s8)actor->field_988 <= 0) {
            table = D_actor_800100_80167200;
            idx   = (s8)actor->field_987 - 1;
            img   = table[idx][(s8)actor->field_989];
            if (img != NULL) {
                ((RECT*)head)[-1].x = 0;
                rect->y             = 0x28;
                rect->w             = 0x15;
                rect->h             = 8;
                Gp_LoadActorImage(arg0, img, rect);
                actor->field_988 = 4;
                actor->field_989++;
            } else {
                actor->field_987 = 0;
            }
        }
    }

    if ((s8)actor->field_98A != 0) {
        actor->field_98B--;
        if ((s8)actor->field_98B <= 0) {
            table = D_actor_800100_80167210;
            idx   = (row = (s8)actor->field_98A - 1);
            img   = table[row][(s8)actor->field_98C];
            if (img != NULL) {
                rect->x = 8;
                rect->y = 0x40;
                rect->w = 0xD;
                rect->h = 0xC;
                Gp_LoadActorImage(arg0, img, rect);
                actor->field_98B = 8;
                actor->field_98C++;
            } else {
                actor->field_98A = 0;
            }
        }
    }

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

void func_actor_800100_80163BF8(Task* arg0)
{
    arg0->state = 3;
}

void func_actor_800100_80163C04(GpActorWork* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    Task*      task;

    actor            = arg0->actor;
    d4               = actor->field_910;
    Gp_ActorSlots[1] = NULL;
    task             = actor->field_914;
    if (task != NULL) {
        taskKill(task);
    }
    task = actor->field_918;
    if (task != NULL) {
        taskKill(task);
    }
    task = actor->field_91C;
    if (task != NULL) {
        taskKill(task);
    }
    task = actor->field_920;
    if (task != NULL) {
        taskKill(task);
    }
    task = actor->field_924;
    if (task != NULL) {
        taskKill(task);
    }
    Gp_UnlinkObj((GpObj*)actor->field_AC);
    Gp_UnlinkObj((GpObj*)actor->field_CC);
    Gp_UnlinkObj((GpObj*)actor->field_EC);
    Gp_UnlinkObj((GpObj*)actor->field_10C);
    Gp_UnlinkObj(&d4->obj);
    taskKill((Task*)arg0);
}

INCLUDE_RODATA("actors/nonmatchings/actor_800100/actor_800100", ActorsShared801328ccTable);
