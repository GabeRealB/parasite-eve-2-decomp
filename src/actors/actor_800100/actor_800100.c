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
    coord = ((TmdObject*)task->extra)->field_8;
    base  = &D_8011505C;
    light = &base->coord;
    slot  = (GpCoordTail*)light;
    if ((((GpActorWork*)Game_GetPtrSlot(10))->extra->field_C & 0x80) != 0) {
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

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80162264);

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

    beam  = (Actor800100Beam*)task->idMap;
    work  = task->spawnArg2;
    fade  = Gp_State1C->field_4;
    coord = ((TmdObject*)task->extra)->field_8;
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
            beam = Mem_Calloc(sizeof(Actor800100Beam), 0);
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
            task->idMap        = (TaskIdMap*)beam;
            beam->obj.field_8  = coord;
            beam->obj.field_C  = beam->rec;
            beam->obj.field_18 = 0x21C9E;
            beam->obj.field_1C = (s16)(u16)work->field_24 >> 1;
            Gp_LcgState        = ang1;
            beam->obj.flags    = 1;
            Gp_LinkObj(1, &beam->obj);
            beam->rec[0].field_0 = 2;
            beam->obj.flags     |= 0x8000;
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
            if (Gp_CountRec18Hi(beam->obj.field_C, 0x30000) != 0) {
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

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80162A14);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80162E90);

void func_actor_800100_801631C8(Task* arg0)
{
    GpObj* temp_a0;
    void*  temp_s1;

    temp_a0 = arg0->idMap;
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
        Task_Kill(task);
    }
    task = actor->field_918;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = actor->field_91C;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = actor->field_920;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = actor->field_924;
    if (task != NULL) {
        Task_Kill(task);
    }
    Gp_UnlinkObj((GpObj*)actor->field_AC);
    Gp_UnlinkObj((GpObj*)actor->field_CC);
    Gp_UnlinkObj((GpObj*)actor->field_EC);
    Gp_UnlinkObj((GpObj*)actor->field_10C);
    Gp_UnlinkObj((GpObj*)d4->field_68);
    Task_Kill((Task*)arg0);
}

INCLUDE_RODATA("actors/nonmatchings/actor_800100/actor_800100", ActorsShared801328ccTable);
