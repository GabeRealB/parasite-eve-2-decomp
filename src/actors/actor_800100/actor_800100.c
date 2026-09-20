#include "common.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/gfx.h"
#include "main/mem.h"
#include "main/mc.h"
#include "main/wipsys.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actor_800100.h"

/// Per-frame flare task of the actor: while the player model is visible
/// (`field_C & 0x80` clear) and the room is not fading out
/// (`Gp_State1C->eventState < 2`) it claims room-light slot 3 as the flare's
/// coordinate. State 0 hangs that coordinate off the actor's own at the fixed
/// offset and zeroes its `age`; state 1 then dispatches on `spawnArg1`:
///
/// - 1 draws the flare at the coordinate's `workm.t` every frame and re-aims
///   the light at a random angle in `0x400..0xB00`, arming the flare width in
///   `scale`.
/// - 2 widens that flare by 0x40 a frame up to 0x180, spawns effect `0x60181`
///   as a child of this task, and re-claims the light with a much wider
///   (`0x400` / `0x4000`) falloff and a `0x800..0xF00` angle.
/// - 3 and 4 switch back to sub-state 1 and 0, and 5 releases the pool block.
///
/// While `Gp_State1C->eventState` is non-zero the two drawing sub-states wind
/// `age` back down instead of advancing.
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
    if ((((GpActorWork*)gameGetPtrSlot(10))->extra->flags & 0x80) != 0) {
        return;
    }
    if (Gp_State1C->eventState >= 2) {
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            rot               = (GpMtxWords*)&coord->coord;
            coord->sub        = work->parent;
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
                    if (Gp_State1C->eventState != 0) {
                        work->age--;
                        func_actor_800100_80162264(
                            (VECTOR3*)&coord->workm.t, work->age, 0x80);
                        break;
                    }
                    func_actor_800100_80162264(
                        (VECTOR3*)&coord->workm.t, work->age, 0x80);
                    base->field_0  = 4;
                    slot->field_58 = 0x80;
                    slot->field_5C = 0x400;
                    ang            = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState    = ang;
                    slot->field_50 = ((ang >> 16) & 0x700) + 0x400;
                    slot->field_52 = (u16)slot->field_50 >> 1;
                    slot->field_54 = slot->field_50 >> 2;
                    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
                    light->flg  = 0;
                    work->scale = 0x40;
                    break;
                case 2:
                    if (Gp_State1C->eventState != 0) {
                        work->age--;
                        break;
                    }
                    if (work->scale < 0x180) {
                        work->scale = (u16)work->scale + 0x40;
                    }
                    eff = Gp_SpawnEff(0x60181, coord, work->scale, NULL);
                    if (eff != NULL) {
                        Task_Reparent(task, eff->task);
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
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
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
/// running it winds `work->age` (the animation frame, halved for the
/// draw) forward, and while one is (`Gp_State1C->eventState` non-zero) it just
/// redraws at the coordinate. `eventState >= 4` tears the task down.
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
    fade  = Gp_State1C->eventState;
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
                                   ((s16)(u16)work->age >> 1) + 1, work->scale,
                                   work->angle);
        return;
    }
    work->age = (u16)work->age + 1;
    switch (task->state) {
        case 0:
            beam = memCalloc(sizeof(Actor800100Beam), 0);
            if (beam == NULL) {
                work->age = 0;
                return;
            }
            task->exitCallback = func_actor_800100_801631C8;
            work->move.vx      = 0;
            ang0               = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState        = ang0;
            work->move.vy      = (u16)task->spawnArg1 - ((ang0 >> 16) & 0x3F);
            work->move.vz      = 0;
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&work->move);
            gte_rtv0_real();
            gte_stsv(&work->move);
            work->scale        = (u16)task->spawnArg1 + 0x180;
            ang1               = Gp_LcgState * 5 + 0x71357911;
            work->angle        = (ang1 >> 16) & 0xFFF;
            task->state        = 1;
            task->work         = (TaskIdMap*)beam;
            beam->obj.coord    = coord;
            beam->obj.ctx.recs = beam->rec;
            beam->obj.key      = 0x21C9E;
            beam->obj.radius   = (s16)(u16)work->scale >> 1;
            Gp_LcgState        = ang1;
            beam->obj.flags    = 1;
            Gp_LinkObj(1, &beam->obj);
            beam->rec[0].flags = 2;
            beam->obj.flags   |= 0x8000;
            /* fallthrough */
        case 1:
            work->scale        = (u16)work->scale + 0x10;
            work->move.vy      = (u16)work->move.vy + 8;
            before.vx          = coord->workm.t[0];
            before.vy          = coord->workm.t[1];
            before.vz          = coord->workm.t[2];
            coord->coord.t[0] += work->move.vx;
            coord->coord.t[1] += work->move.vy;
            coord->coord.t[2] += work->move.vz;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            after.vx = coord->workm.t[0];
            after.vy = coord->workm.t[1];
            after.vz = coord->workm.t[2];
            func_actor_800100_80162A14((VECTOR3*)coord->workm.t,
                                       ((s16)(u16)work->age >> 1) + 1, work->scale,
                                       work->angle);
            ang2        = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = ang2;
            if ((u16)((ang2 >> 16) % 3) == 0 && Gp_State1C->groundTrace != 0 &&
                Gp_TraceGroundCoord(coord, &ground) == 1) {
                func_actor_800100_80162E90((VECTOR3*)ground.workm.t,
                                           (s16)((work->scale * 2) / 3));
            }
            if (Gp_CountRec18Hi(beam->obj.ctx.recs, 0x30000) != 0) {
                Gp_UnlinkObj(&beam->obj);
                Gp_ReleaseState1CMem(work, task);
                return;
            }
            if (func_800DE7CC(&after, &before, NULL, NULL) == 1) {
                Gp_UnlinkObj(&beam->obj);
                task->state   = 2;
                work->move.vx = (u32)rcos(work->angle) >> 8;
                work->move.vy = (u32)rsin(work->angle) >> 8;
                ang3          = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState   = ang3;
                work->move.vz = (u32)rsin((ang3 >> 16) & 0xFFF) >> 8;
                return;
            }
            if (work->age >= 0x15) {
                Gp_UnlinkObj(&beam->obj);
                Gp_ReleaseState1CMem(work, task);
                return;
            }
            Gp_ClearRec18Occupied(beam->rec);
            return;
        case 2:
            work->scale        = (u16)work->scale + 0x40;
            coord->coord.t[0] += work->move.vx;
            coord->coord.t[1] += work->move.vy;
            coord->coord.t[2] += work->move.vz;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            func_actor_800100_80162A14((VECTOR3*)coord->workm.t,
                                       ((s16)(u16)work->age >> 1) + 1, work->scale,
                                       work->angle);
            if (work->age >= 0x15) {
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
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
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
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
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

void func_actor_800100_80163214(GpActorWork* arg0)
{
    GameActor*     actor;
    TmdObject*     extra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* next;
    GsCOORDINATE2* third;
    McSaveData*    save;
    GpRec18*       recs;
    GpObj*         obj;
    GpActorD4*     d4;
    GpEffWork*     eff;
    Task*          task;
    SVECTOR3*      scratch;
    s32            idx;
    s32            packed;
    u8             saved;
    void*          head;

    actor                   = arg0->actor;
    head                    = *(void**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = head - 8;
    scratch                 = (SVECTOR3*)(head - 8);
    extra                   = arg0->extra;
    coord                   = extra->coords;
    arg0->state++;
    arg0->field_24   = &D_actor_800100_80167130;
    arg0->field_18   = func_actor_800100_80163C04;
    actor->field_938 = 0x14;
    Gp_ActorSlots[1] = arg0;
    coord->sub       = &gGfxViewCoord;
    coord->flg       = 0;
    extra->flags     = 0;
    RotMatrix((SVECTOR*)&actor->field_50, &coord->coord);
    func_8010BFCC(arg0);
    actor->field_985 = 0x10;
    Gp_AnimResetChildSlots(arg0, actor->field_93C);
    Gp_AnimTickChildSlots(arg0);
    recs            = actor->field_17C;
    obj             = (GpObj*)actor->field_AC;
    actor->field_10 = coord->coord.t[0];
    actor->field_14 = coord->coord.t[1];
    actor->field_18 = coord->coord.t[2];
    obj->ctx.dir    = (GpObjDirRec*)actor->field_88;
    obj->coord      = coord;
    actor->field_90 = (s32)recs;
    save            = &Mc_SaveData;
    obj->pos.vy     = -0x12C;
    obj->pos.vx     = 0;
    obj->pos.vz     = 0;
    packed          = 0x10000;
    {
        s32 temp;

        temp        = save->characterId;
        obj->radius = 0x12C;
        obj->flags  = 4;
        obj->key    = temp | packed | 0x80;
        Gp_LinkObj(0, obj);
    }
    Gp_InitRec18Table((GpRec18*)actor->field_90, 0x12, 0);
    obj->flags     |= 0xC200;
    next            = arg0->extra->coords + 4;
    obj             = (GpObj*)actor->field_CC;
    obj->ctx.dir    = (GpObjDirRec*)actor->field_94;
    obj->coord      = next;
    actor->field_9C = (s32)recs;
    obj->pos.vx     = 0;
    obj->pos.vy     = 0x64;
    obj->pos.vz     = 0;
    {
        s32 f = 0x14;
        s32 temp;

        temp        = save->characterId;
        obj->radius = 0xDC;
        obj->flags  = f;
        obj->key    = temp | packed | 0x80;
        Gp_LinkObj(0, obj);
    }
    obj->flags     |= 0x8000;
    obj             = (GpObj*)actor->field_EC;
    third           = arg0->extra->coords;
    obj->ctx.dir    = (GpObjDirRec*)actor->field_A0;
    obj->coord      = third + 1;
    actor->field_A8 = (s32)recs;
    obj->pos.vx     = 0;
    obj->pos.vy     = 0x52;
    obj->pos.vz     = 0;
    {
        s32 temp;

        temp        = save->characterId;
        obj->radius = 0xDC;
        obj->flags  = 4;
        obj->key    = temp | packed | 0x80;
        Gp_LinkObj(0, obj);
    }
    obj->flags            |= 0xC000;
    actor->field_984       = 7;
    saved                  = Player_Status.field_26;
    Player_Status.field_26 = save->companionVariant;
    actor->field_920       = func_80104258(arg0, 0, 5, 1);
    actor->field_924       = func_80104258(arg0, 1, 5, 1);
    Player_Status.field_26 = saved;
    if (actor->field_924 != NULL) {
        task             = func_80104364((GpActorWork*)actor->field_924, save->companionType + 1, save->companionVariant, 0);
        actor->field_91C = task;
        if (task != NULL) {
            d4  = actor->field_910;
            idx = D_actor_800100_80167218[save->companionVariant];
            Gp_AttachActorObj(arg0, idx, D_actor_800100_80167224[save->companionVariant]);
            actor->field_124 |= 0x80;
            d4->actionCount   = D_actor_800100_80167230[save->companionVariant];
            if ((u8)save->companionVariant == 4) {
                eff = Gp_SpawnEff(0x80060180, ((TmdObject*)actor->field_91C->extra)->coords, idx, 0);
                if (eff != NULL) {
                    actor->field_914 = eff->task;
                    Task_Reparent((Task*)arg0, eff->task);
                    func_80106350(arg0, idx, 0);
                }
            }
        }
    }
    scratch->vx = 0;
    scratch->vy = -0x200;
    scratch->vz = 0;
    Gp_BindActorD4(arg0, scratch, 0x1000);
    func_8010BF7C(arg0, 0x3C, 0x7F);
    *(u32*)G_SCRATCH_HEAD += 8;
}

void func_actor_800100_801635F4(GpActorWork* arg0)
{
    Actor800100ShadowScratch* scratch;
    void**                    scratchHead;
    u8*                       head;
    GameActor*                actor;
    TmdObject*                work;
    TmdObject*                extra;
    GsCOORDINATE2*            coord;
    GsCOORDINATE2*            ground;
    GpActorD4*                d4;
    Task*                     task;
    GpObj*                    objs[2];
    s32                       dy;
    s32                       i;
    s8                        bits;

    scratchHead  = (void**)G_SCRATCH_HEAD;
    head         = *scratchHead;
    extra        = arg0->extra;
    *scratchHead = head - 0x18;
    work         = extra;
    scratch      = (Actor800100ShadowScratch*)(head - 0x18);
    coord        = work->coords;
    actor        = arg0->actor;
    d4           = actor->field_910;

    if (actor->field_954 != 2 &&
        (dy = coord->coord.t[1], dy = dy - actor->field_14, dy = ABS(dy), dy >= 0x200)) {
        coord->coord.t[0] = actor->field_10;
        coord->coord.t[1] = actor->field_14;
        coord->coord.t[2] = actor->field_18;
    } else {
        actor->field_10 = coord->coord.t[0];
        actor->field_14 = coord->coord.t[1];
        actor->field_18 = coord->coord.t[2];
        if (actor->field_984 & 1) {
            actor->field_992 = func_801011D0(coord, actor->field_90, 0x12, &actor->field_930);
        } else {
            actor->field_992 = 0;
        }
    }

    task = actor->field_91C;
    if (task != NULL) {
        *(GsCOORDINATE2*)actor->field_3D4 =
            *(GsCOORDINATE2*)((TmdObject*)task->extra)->coords;
        Gfx_RotMatrixX(&((GsCOORDINATE2*)actor->field_3D4)->workm, -0x400, 0);
    }

    d4->coord = *(GsCOORDINATE2*)arg0->extra->coords;
    Gfx_RotMatrixY(&d4->coord.workm, d4->scanAngle, 0);

    objs[0] = (GpObj*)actor->field_AC;
    objs[1] = (GpObj*)actor->field_EC;
    for (i = 0; i < 2; i++) {
        bits = actor->field_983;
        if ((bits >> i) & 1) {
            actor->field_984 |= 1 << i;
            objs[i]->flags   |= 0x4000;
        } else if (bits & (8 << i)) {
            actor->field_984 &= ~(1 << i);
            objs[i]->flags   &= ~0x4000;
        }
    }
    actor->field_983 = 0;

    if (D_80115768 == 0 && D_801153F4 == 0) {
        func_actor_800100_80165528(arg0);
    }
    func_actor_800100_80163A58(arg0);

    Gp_ClearRec18Occupied(actor->field_17C);
    Gp_ClearRec18Occupied(&actor->field_910->contact);
    if (actor->field_91C != NULL) {
        Gp_ClearRec18Occupied(actor->field_32C);
    }
    if (actor->field_984 & 1) {
        coord->coord.t[1] += 8;
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);

    if ((s8)actor->field_986 != 0) {
        scratch->vx = (u16)actor->field_30.vx;
        scratch->vy = (u16)actor->field_30.vy;
        scratch->vz = (u16)actor->field_30.vz;
    } else {
        scratch->vx = (u16)coord->workm.m[0][2] *
                      (s8)((volatile Actor800100DirByte*)actor)->field_973;
        scratch->vy = (u16)coord->workm.m[1][2] *
                      (s8)((volatile Actor800100DirByte*)actor)->field_973;
        scratch->vz = (u16)coord->workm.m[2][2] *
                      (s8)((volatile Actor800100DirByte*)actor)->field_973;
    }
    ((SVECTOR*)actor->field_88)->vx = scratch->vx;
    ((SVECTOR*)actor->field_88)->vy = scratch->vy;
    ((SVECTOR*)actor->field_88)->vz = scratch->vz;
    ((SVECTOR*)actor->field_94)->vx = scratch->vx;
    ((SVECTOR*)actor->field_94)->vy = scratch->vy;
    ((SVECTOR*)actor->field_94)->vz = scratch->vz;
    ((SVECTOR*)actor->field_A0)->vx = scratch->vx;
    ((SVECTOR*)actor->field_A0)->vy = scratch->vy;
    ((SVECTOR*)actor->field_A0)->vz = scratch->vz;

    if (!(work->flags & 0x80)) {
        ground      = ((TmdObject*)arg0->extra)->coords + 1;
        ground->flg = 0;
        Gp_UpdateCoord(ground);
        if (func_800EA1A8((VECTOR3*)ground->workm.t, (VECTOR3*)scratch) != 0) {
            Gp_DrawEffGroundQuad((VECTOR3*)scratch, 0x200, Gp_State1C->groundShade);
        }
    }
    *(u8**)G_SCRATCH_HEAD += 0x18;
}

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
