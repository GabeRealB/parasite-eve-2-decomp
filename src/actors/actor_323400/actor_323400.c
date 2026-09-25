#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actor.h"
#include "actors/actors_shared_80164954.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Psy-Q `RotMatrixY`.
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// Whole-unit part of the last movement step `func_actor_323400_80162A2C`
/// applied, rounded away from zero when the step had a fraction.
extern SVECTOR D_actor_323400_80171218;

/// Per-state animation table `func_actor_323400_80163B58` reads when it
/// re-seeds the slots: 0x2D bytes per `field_82C`, indexed by `field_82E`.
extern s8 D_actor_323400_80170894[];

/// Animation source `func_800B3F84` is handed for both of the work block's
/// contexts.
extern u8 D_actor_323400_80171080[];

/// Message table published as `Task::msgTable` by the spawn handler.
extern void* D_actor_323400_801711D4;

/// Effect record the spawn handler fills: the model root's coordinate and
/// the two spawn arguments 0x100 and 2.
extern GpEffArg D_actor_323400_80171228;

/// Enemy pair source `GpEnemy::param` is pointed at by the spawn handler.
extern GpPairSrcE D_actor_323400_80164D5C;

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

void func_actor_323400_80163FC8(GpEnemy* enemy, Task* task);
void func_actor_323400_801641C4(GpEnemy* enemy, Task* task);
void func_actor_323400_801644C4(GpEnemy* enemy, Task* task);
void func_actor_323400_80164A78(Task* task);
void func_actor_323400_80164B98(GpEnemy* arg0, Task* arg1);
void func_actor_323400_80164BD0(GpEnemy* enemy, Task* task);
void func_actor_323400_80164C4C(GpEnemy* enemy, Task* task);

/// State handlers `func_actor_323400_801644C4` runs by `Actor323000Work::field_0`.
const GpEnemyTaskFuncTable4 D_actor_323400_80161E24 = {
    func_actor_323400_80164B98,
    func_actor_323400_80164BD0,
    func_actor_323400_801641C4,
    func_actor_323400_80164C4C,
};

/// Task states `func_actor_323400_80164CEC` runs by `Task::state`: the spawn
/// handler, the per-frame driver, then `Gp_DestroyEnemy`.
const GpEnemyTaskFuncTable3 D_actor_323400_80161E34 = {
    func_actor_323400_80163FC8,
    func_actor_323400_801644C4,
    Gp_DestroyEnemy,
};

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it. The tick uses it to turn the body and head parts.
void func_actor_323400_80161E8C(GpCoord* coord, s16 yaw)
{
    MATRIX*  rotation;
    GpCoord* out;

    SCRATCH_PUSH(MATRIX);
    rotation = SCRATCH_HEAD(MATRIX);
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    SCRATCH_POP(MATRIX);
}

/// Walks the first `count` contact records (stopping at a zero key) and keeps,
/// in a scratch block carved off `G_SCRATCH_HEAD`, the push that would move
/// `coord` out of the last record of kind 0x10000 or 0x30000, scaled down to
/// 0x100 units when longer. Returns whether any such record was found; returns
/// 0 at once when `gGameSession->viewReady` or `Mc_SaveData.field_5C1` is 1.
s32 func_actor_323400_80162198(GpCoord* coord, GpRec18* recs, s16 count)
{
    ActorRepelScratch* head;
    ActorRepelScratch* s;
    ActorRepelScratch* blk;
    SVECTOR*           offset;

    if (Mc_SaveData.field_5C1 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    coord->flg                      = 0;
    head                            = SCRATCH_HEAD(ActorRepelScratch);
    blk                             = head - 1;
    SCRATCH_HEAD(ActorRepelScratch) = blk;
    s                               = blk;
    Gp_UpdateCoord(coord);
    s->pos.vx  = coord->workm.t[0];
    s->pos.vy  = coord->workm.t[1];
    s->pos.vz  = coord->workm.t[2];
    s->last.vz = 0;
    s->last.vy = 0;
    s->last.vx = 0;
    s->hit     = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            actorCalcPush(&s->pos, &recs[s->i], &s->offset);
            s->last.vx = s->offset.vx;
            s->last.vz = s->offset.vz;
        }
    }
    s->len = SquareRoot0(s->offset.vx * s->offset.vx + s->offset.vy * s->offset.vy +
                         s->offset.vz * s->offset.vz);
    if (s->len > 0x100) {
        offset = &s->offset;
        VectorNormalSS(offset, offset);
        gte_lddp(0x100);
        gte_ldsv(offset);
        gte_gpf12();
        gte_stsv(offset);
    }
    coord->flg = 0;
    SCRATCH_POP(ActorRepelScratch);
    return s->hit;
}

/// Steers `coord` away from the obstacles among the first `count` contact
/// records: collects the bearing of up to eight records of kind 0x10000 or
/// 0x30000 (in the XZ plane, or XY when the facing column is near vertical),
/// discards any pair more than 0x400 apart, and for each remaining bearing
/// nudges both `coord`'s translation and `*pos` a short step away from it.
/// `*pos` accumulates the total nudge. Returns whether any record was of kind
/// 0x10000; returns 0 at once when `gGameSession->viewReady` or `Mc_SaveData.field_5C1`
/// is 1.
s32 func_actor_323400_801624E0(GpCoord* coord, GpRec18* recs, s16 count, SVECTOR* pos)
{
    u8*                  head;
    OverlayAvoidScratch* s;
    s16                  diff;
    s16                  t;
    s32                  mag;

    if (gGameSession->viewReady == 1 || Mc_SaveData.field_5C1 == 1) {
        return 0;
    }

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - sizeof(OverlayAvoidScratch);
    s                = (OverlayAvoidScratch*)SCRATCH_HEAD(u8);
    s->blocked       = 0;
    pos->vz          = 0;
    pos->vy          = 0;
    pos->vx          = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x34));
    VectorNormalSS((SVECTOR*)(head - 0x34), (SVECTOR*)(head - 0x34));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->eye.vx = (u16)coord->workm.t[0];
    s->eye.vy = (u16)coord->workm.t[1];
    s->eye.vz = (u16)coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        switch (s->kind) {
            case 0x10000:
                s->blocked = 1;
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = overlayBearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = overlayBearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            diff = (u16)s->angle[s->i] - (u16)s->angle[s->j];
            t    = diff;
            if (diff < 0) {
            wrapUp:
                if (t < -0x800) {
                    t += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (t > 0x800) {
                    t -= 0x1000;
                    goto wrapDown;
                }
            }
            mag     = t;
            s->diff = mag;
            SOFT_BARRIER();
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x401) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            Gfx_RotMatrixY(&s->m, diff, 1);
            Gfx_MatrixCol2(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    SCRATCH_POP_BYTES(sizeof(OverlayAvoidScratch));
    return s->blocked != 0;
}

/// Steps `coord` by the movement the first `arg2` `GpRec18` records of
/// `movement` resolve to, and keeps the whole-unit part of that step in
/// `D_actor_323400_80171218`. Returns 1 when the X or Z step is nonzero; a
/// step with a fractional part moves the coordinate and the kept step one
/// unit further from zero. Nothing in this package calls it.
s32 func_actor_323400_80162A2C(GpCoord* coord, GpRec18* movement, s16 arg2)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    register void*    p asm("v1");
    s32               val;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    p                              = head - 0x14;
    s                              = p;
    SCRATCH_HEAD_AT(scratch, void) = p;
    s->moved                       = 0;
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_323400_80171218.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_323400_80171218.vy = s->delta.vy.w >> 16;
        D_actor_323400_80171218.vz = s->delta.vz.w >> 16;
        val                        = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_323400_80171218.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_323400_80171218.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_323400_80171218.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_323400_80171218.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP_BYTES(0x14);
    return s->moved;
}

/// Pushes `coord` `push` units away from each obstacle among the first
/// `count` contact records (kind 0x10000 or 0x30000) whose bearing lies within
/// 0x400 of every other obstacle's. Bearings are taken in world space from the
/// frame's position, relative to the point one unit in front of it. Returns
/// whether any push was applied; returns 0 at once when
/// `gGameSession->viewReady` is 1.
s32 func_actor_323400_80162BD0(GpCoord* coord, GpRec18* recs, s16 count, s16 push)
{
    void**                  scratch;
    void**                  tail;
    u8*                     head;
    OverlayBisectorScratch* st;
    u16                     vz;
    s16                     d;
    s16                     dz;
    s32                     t;
    s32                     hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    scratch = SCRATCH_HEAD_ADDR;
    head    = SCRATCH_HEAD_AT(scratch, void);
    {
        register u8* tmp asm("v0");
        tmp = head - sizeof(OverlayBisectorScratch);
        st  = (OverlayBisectorScratch*)tmp;
    }
    st->eye.vx                     = (u16)coord->coord.t[0];
    st->eye.vy                     = (u16)coord->coord.t[1];
    vz                             = (u16)coord->coord.t[2];
    SCRATCH_HEAD_AT(scratch, void) = st;
    st->eye.vz                     = vz;

    overlayToWorld(coord->sub, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    overlayToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = (u16)recs[st->i].point.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)recs[st->i].point.vy - (u16)st->eye.vy;
            dz               = (u16)recs[st->i].point.vz - (u16)st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = ratan2(st->delta.vx, dz);

            st->delta.vx     = (u16)st->aim.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)st->aim.vy - (u16)st->eye.vy;
            dz               = (u16)st->aim.vz - (u16)st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = (u16)st->angle[st->i] - ratan2(st->delta.vx, dz);

            d = st->angle[st->i];
            if (st->angle[st->i] < 0) {
            wrapUp1:
                if (d < -0x800) {
                    d += 0x1000;
                    goto wrapUp1;
                }
            } else {
            wrapDown1:
                if (d > 0x800) {
                    d -= 0x1000;
                    goto wrapDown1;
                }
            }
            st->angle[st->i] = d;
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                d        = st->diff;
                if (st->diff < 0) {
                wrapUp2:
                    if (d < -0x800) {
                        d += 0x1000;
                        goto wrapUp2;
                    }
                } else {
                wrapDown2:
                    if (d > 0x800) {
                        d -= 0x1000;
                        goto wrapDown2;
                    }
                }
                t        = d;
                st->diff = t;
                SOFT_BARRIER();
                if (t < 0) {
                    t = -t;
                }
                if (t >= 0x401) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    tail = SCRATCH_HEAD_ADDR;
    hit  = st->hit;
    SCRATCH_POP_BYTES_AT(tail, sizeof(OverlayBisectorScratch));
    return hit;
}

/// Tick of the animation slots while the blend context is live: slots 1..10
/// sample both contexts and write their pose mixed by `field_83C` (the blend
/// context gets the 0x1000 complement), each rate seeded from `field_832`
/// (three below it) and `field_83A`; slots 11..17 only tick the main context.
void func_actor_323400_8016331C(Task* task)
{
    GpAnimPose       pose;
    GpAnimPose       blendPose;
    GpAnimCtx*       anim;
    s16              weight;
    s16              i;
    Actor323000Work* work;

    work   = (Actor323000Work*)task->work;
    weight = work->field_83C;
    anim   = &work->anim;
    for (i = 1; i < 0x12; i++) {
        if (i < 0xB) {
            work->blendSlots[i].rate = work->field_83A;
            work->slots[i].rate      = (u8)(work->field_832 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].rate = (u8)(work->field_832 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

/// Per-frame effect dispatch keyed on `field_82E` and the record each animation
/// slot has reached. A recognised record is handled once: `field_848` remembers,
/// per slot, the record last handled, and meeting it again only clears `reset`.
/// A handled record spawns its effects while the room effect mode is 2 and
/// returns a request word; otherwise the result is 0, after wiping `field_848`
/// when no case claimed a record.
///
/// `steer` is a matching carrier (see `CSE_STEER`); it has no effect.
s32 func_actor_323400_80163448(Task* task, Actor323000Work* work)
{
    SVECTOR vec;
    s32     reset;
    s32     steer;
    reset = 1;
    switch (work->field_82E) {
        case 0: {
            s32 clip = work->slots[9].curRec & 0x3FF;
            s32 old;
            if (clip == 0x58) {
                old = work->field_848[9];
                if (old != clip) {
                    work->field_848[9] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80002220, &vec);
                    }
                    return 0x40010002;
                }
                work->field_848[9] = old;
                reset              = 0;
            }
        }
            {
                s32 clip = work->slots[7].curRec & 0x3FF;
                s32 old;
                if (clip == 0x3E) {
                    old = work->field_848[7];
                    if (old != clip) {
                        work->field_848[7] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x2BC;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80002220, &vec);
                        }
                        return 0x40010001;
                    }
                    work->field_848[7] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[14].curRec & 0x3FF;
                s32 old;
                if (clip == 0x84) {
                    old = work->field_848[14];
                    if (old != clip) {
                        work->field_848[14] = clip;
                        vec.vz              = 0;
                        vec.vx              = 0;
                        vec.vy              = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80002220, &vec);
                        }
                        return 0x40010002;
                    }
                    work->field_848[14] = old;
                    reset               = 0;
                }
            }
            {
                s32 clip = work->slots[17].curRec & 0x3FF;
                s32 old;
                if (clip == 0xA9) {
                    old = work->field_848[17];
                    if (old != clip) {
                        work->field_848[17] = clip;
                        vec.vz              = 0;
                        vec.vx              = 0;
                        vec.vy              = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80002220, &vec);
                        }
                        return 0x40010001;
                    }
                    work->field_848[17] = old;
                    reset               = 0;
                }
            }
            break;

        case 10: {
            s32 clip = work->slots[1].curRec & 0x3FF;
            s32 old;
            if (clip == 0x9) {
                old = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, task->extra.tmd->coords, 0x80004A00, &vec);
                    }
                    return 0x40010005;
                }
                work->field_848[1] = old;
                reset              = 0;
            }
        } break;

        case 3: {
            s32 clip = work->slots[1].curRec & 0x3FF;
            s32 old;
            if (clip == 0x4) {
                reset = 0;
                old   = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    return 0x40010004;
                }
                work->field_848[1] = old;
            }
        }
            {
                s32 clip = work->slots[1].curRec & 0x3FF;
                s32 old;
                if (clip == 0x8) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        return 0x40010003;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            break;

        case 6: {
            s32 clip = work->slots[1].curRec & 0x3FF;
            s32 old;
            if (clip == 0x6) {
                old = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80003200, &vec);
                    }
                    vec.vz = 0;
                    vec.vx = 0;
                    vec.vy = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80003200, &vec);
                    }
                    return 0;
                }
                work->field_848[1] = old;
                reset              = 0;
            }
        }
            {
                s32 clip = work->slots[1].curRec & 0x3FF;
                s32 old;
                if (clip == 0xB) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80004480, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80004480, &vec);
                        }
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[1].curRec & 0x3FF;
                s32 old;
                if (clip == 0xC) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x2BC;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80002200, &vec);
                        }
                        CSE_STEER(steer);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x2BC;
                        if (steer == 0 && Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80002240, &vec);
                        }
                        CSE_STEER(steer);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (steer == 0 && Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80003300, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80003340, &vec);
                        }
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[1].curRec & 0x3FF;
                s32 old;
                if (clip == 0xD) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80002200, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80002300, &vec);
                        }
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            break;
    }

    if (reset == 1) {
        Mem_Set(work->field_848, 0, 0x48);
    }
    return 0;
}

/// Per-frame animation tick. Seeds the slots when `field_828` asks for it
/// (1 from the per-state table `D_actor_323400_80170894`, 2 by resetting to
/// `field_82E`), seeds the blend context when `field_836` is 2, then ticks
/// the slots - blended through `func_actor_323400_8016331C` while `field_82A`
/// is set. It eases `field_844` toward `field_840` and spreads it over the
/// body joints 2-4, eases `field_842` toward `field_83E` for joint 10, and
/// plays the sound `func_actor_323400_80163448` returns, panned at the root.
void func_actor_323400_80163B58(Task* task)
{
    Actor323000Work* work;
    Actor323000Work* seekWork;
    Actor323000Work* resetWork;
    Actor323000Work* secondaryWork;
    Actor323000Work* tickWork;
    Actor323000Work* turnWork;
    u32              table;
    s16              state;
    s32              seekIndex;
    s32              seekSlotIndex;
    s32              animation;
    s32              index;
    s32              resetIndex;
    s32              resetSlotIndex;
    s32              secondaryIndex;
    s32              secondarySlotIndex;
    s32              tickIndex;
    s32              tickSlotIndex;
    s32              targetAngle;
    s32              currentAngle;
    s32              targetAngleBits;
    s32              currentAngleBits;
    s16              angle;
    s32              clampedAngle;
    s16              thirdAngle;
    s32              targetTurn;
    u16              originalTurn;
    s32              signedTurn;
    s16              currentTurn;
    s32              updatedTurn;
    u16              updatedTurnBits;
    s32              delta;
    s32              sound;
    s32              pan;

    work  = (Actor323000Work*)task->work;
    state = work->field_828;
    if (state == 1) {
        if (work->field_82C != work->field_82E) {
            seekWork = work;
            TOUCH_REG(seekWork);
            seekIndex = 1;
            table     = (u32)D_actor_323400_80170894;
            do {
                seekSlotIndex               = seekIndex;
                work->slots[seekIndex].rate = (u8)seekWork->field_832;
                animation                   = seekWork->field_82E;
                index                       = seekWork->field_82C * 0x2D;
                func_800B4114(&seekWork->anim, seekSlotIndex, animation, 0, (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x12);
            seekWork->field_82C = seekWork->field_82E;
        }
        work->field_828 = 3;
        work->field_830 = 0;
        Mem_Set(work->field_848, 0U, 0x48U);
    } else if (state == 2) {
        resetWork = work;
        TOUCH_REG(resetWork);
        resetIndex = 1;
        do {
            resetSlotIndex               = resetIndex;
            work->slots[resetIndex].rate = (u8)resetWork->field_832;
            Gp_AnimResetSlot(&resetWork->anim, resetSlotIndex, (s32)resetWork->field_82E);
            resetIndex += 1;
        } while (resetIndex < 0x12);
        resetWork->field_82C = resetWork->field_82E;
        work->field_828      = 3;
        work->field_830      = 0U;
        Mem_Set(work->field_848, 0U, 0x48U);
    }
    if (work->field_836 == 2) {
        secondaryWork            = (Actor323000Work*)task->work;
        secondaryIndex           = 1;
        secondaryWork->field_83A = 0x20;
        secondaryWork->field_83C = 0x800;
        do {
            secondarySlotIndex                        = secondaryIndex;
            secondaryWork->slots[secondaryIndex].rate = (u8)secondaryWork->field_83A;
            Gp_AnimResetSlot(&secondaryWork->blendAnim, secondarySlotIndex, (s32)secondaryWork->field_838);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x12);
        work->field_836 = 3;
    }
    work->field_830 = (u16)(work->field_830 + 1);
    if (work->field_82A == 0) {
        tickWork  = (Actor323000Work*)task->work;
        tickIndex = 1;
        do {
            tickSlotIndex                   = tickIndex;
            tickWork->slots[tickIndex].rate = (u8)tickWork->field_832;
            Gp_AnimTickIndex(&tickWork->anim, tickSlotIndex);
            tickIndex += 1;
        } while (tickIndex < 0x12);
    } else {
        func_actor_323400_8016331C(task);
        if (work->blendSlots[1].flags & 1) {
            work->field_82A = 0;
        }
    }
    targetAngle      = work->field_840;
    currentAngle     = work->field_844;
    targetAngleBits  = (u16)work->field_840;
    currentAngleBits = (u16)work->field_844;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x72) {
            work->field_844 = currentAngleBits + 0x71;
        } else {
            goto snap;
        }
    } else if ((currentAngle - targetAngle) >= 0x72) {
        work->field_844 = currentAngleBits - 0x71;
    } else {
    snap:
        work->field_844 = targetAngleBits;
    }
    angle        = work->field_844;
    clampedAngle = (u16)work->field_844;
    if (angle != 0) {
        if (angle >= 0x501) {
            clampedAngle = 0x500;
        }
        if (angle < -0x500) {
            clampedAngle = -0x500;
        }
        thirdAngle = (s16)clampedAngle / 3;
        func_actor_323400_80161E8C(&task->extra.tmd->coords[2], thirdAngle);
        task->extra.tmd->coords[2].flg = 0;
        func_actor_323400_80161E8C(&task->extra.tmd->coords[3], thirdAngle);
        task->extra.tmd->coords[3].flg = 0;
        func_actor_323400_80161E8C(&task->extra.tmd->coords[4], (s16)clampedAngle / 2);
        task->extra.tmd->coords[4].flg = 0;
    }
    turnWork     = (Actor323000Work*)task->work;
    targetTurn   = (u16)turnWork->field_83E;
    originalTurn = targetTurn;
    if ((s16)targetTurn >= 0x201) {
        targetTurn = 0x200;
    }
    if ((s16)originalTurn < -0x200) {
        targetTurn = -0x200;
    }
    signedTurn  = (s16)targetTurn;
    currentTurn = turnWork->field_842;
    if (currentTurn < signedTurn) {
        if ((signedTurn - currentTurn) >= 0xD) {
            turnWork->field_842 = (s16)((u16)turnWork->field_842 + 0xC);
        } else {
            turnWork->field_842 = (s16)targetTurn;
        }
    }
    updatedTurn     = turnWork->field_842;
    updatedTurnBits = (u16)turnWork->field_842;
    if ((s16)targetTurn < updatedTurn) {
        delta = updatedTurn - (s16)targetTurn;
        if (delta < 0) {
            delta = -delta;
        }
        if (delta >= 0xD) {
            turnWork->field_842 = (s16)(updatedTurnBits - 0xC);
        } else {
            turnWork->field_842 = (s16)targetTurn;
        }
    }
    func_actor_323400_80161E8C(&task->extra.tmd->coords[10], (s16)((s32)(u16)turnWork->field_842 * -1));
    task->extra.tmd->coords[10].flg = 0;
    sound                           = func_actor_323400_80163448(task, work);
    if (sound != 0) {
        pan = (s8)Gp_GetObjPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)gpGetObjDepth(task->extra.tmd->coords));
    }
}

/// Spawn state: allocates the work block (destroying the enemy if that
/// fails), installs the exit callback, binds the model's light and colour
/// matrices to the block, sets up the enemy record and links its node,
/// initialises both animation contexts, seeds clip 1 and ticks once. It then
/// publishes the message table, parents the root to the view, takes its world
/// position as the actor colour, fills the effect record and advances the
/// task to the per-frame driver.
void func_actor_323400_80163FC8(GpEnemy* enemy, Task* task)
{
    SVECTOR          unused; // never referenced; only reserves the frame slot the ROM has
    VECTOR           pos;
    TmdObject*       obj;
    TmdObject*       tmd;
    GpCoord*         coord;
    Actor323000Work* work;
    Actor323000Work* work2;
    Actor323000Work* mem;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = (Actor323000Work*)memCalloc(0x934, 0);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback = func_actor_323400_80164A78;
    work2              = (Actor323000Work*)task->work;
    tmd                = task->extra.tmd;
    tmd->lightMtx      = &work2->light;
    tmd->colorMtx      = &work2->color;
    enemy->field_4     = &task->extra.tmd->coords->coord;
    enemy->field_48    = 0;
    enemy->bodyPos.vx  = 0;
    enemy->bodyPos.vy  = 0;
    enemy->bodyPos.vz  = 0;
    enemy->coord       = &task->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.b.flags = 1;
    enemy->param              = &D_actor_323400_80164D5C;
    enemy->reactionFlags      = 0;
    enemy->hp                 = 0;
    enemy->recs               = 0;
    func_800B3F84(&work->anim, D_actor_323400_80171080, obj, work->poses, work->slots);
    func_800B3F84(&work->blendAnim, D_actor_323400_80171080, obj, work->blendPoses, work->blendSlots);
    work->field_828 = 2;
    work->field_82E = 1;
    work->field_82A = 0;
    work->field_844 = 0;
    work->field_840 = 0;
    work->field_834 = 0x10;
    work->field_832 = 0x10;
    func_actor_323400_80163B58(task);
    task->msgTable = &D_actor_323400_801711D4;
    coord->sub     = &gGfxViewCoord;
    coord->flg     = 0;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    D_actor_323400_80171228.coord      = task->extra.tmd->coords;
    D_actor_323400_80171228.spawnArgLo = 0x100;
    D_actor_323400_80171228.spawnArgHi = 2;
    work->field_0                      = 0;
    task->state++;
}

/// State 2 of `D_actor_323400_80161E24`. On entry it flags the enemy's link
/// node, shows the model (clears its flags) and rebuilds its buffers, resets
/// the slots to clip 0xD and zeroes the frame counter `field_6` before the
/// tick. Otherwise it advances `field_6` and, on frames 9, 10, 12 and 13,
/// spawns effect 0x60054 at the matching model part while the room's effect
/// mode is 2 (0x2BC up at parts 9 and 7, 0x258 up at 14 and 17); frame 10
/// also plays a placed sound, and frame 13 always spawns one more at part 1.
/// The tick then runs and the root coordinate is marked for rebuilding.
void func_actor_323400_801641C4(GpEnemy* enemy, Task* task)
{
    Actor323000Work* work;
    TmdObject*       obj;
    s32              id;
    s32              pan;
    SVECTOR          ofs2;
    SVECTOR          ofs;

    work = (Actor323000Work*)task->work;
    if (work->field_4 != 0) {
        obj                       = task->extra.tmd;
        enemy->node.state.b.flags = 1;
        obj->flags                = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_82E = 0xD;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        work->field_6   = 0;
        func_actor_323400_80163B58(task);
        return;
    }
    switch (++work->field_6) {
        case 9: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80002400, p);
            }
            break;
        }
        case 10: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80002400, p);
            }
            id  = ((enemy->placeKey >> 12) << 8) | 0x4001000E;
            pan = (s8)Gp_GetObjPan(task->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(task->extra.tmd->coords));
            break;
        }
        case 12: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x258;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80003600, p);
            }
            break;
        }
        case 13: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x258;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80004500, p);
            }
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80002480, p);
            }
            ofs2.vy = 0x3E8;
            ofs2.vx = 0;
            ofs2.vz = -0x12C;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[1], 0x80005900, &ofs2);
            break;
        }
    }
    func_actor_323400_80163B58(task);
    task->extra.tmd->coords->flg = 0;
}

/// Per-frame driver of the live actor: brings the model root's coordinate up
/// to date, takes its world position as the actor colour, flags a state change
/// in `field_4`, and runs the state handler `field_0` selects from a stack copy
/// of `D_actor_323400_80161E24`. Afterwards it walks the origin of the model's
/// third part coordinate up to `gGfxViewCoord` and stores it as the enemy's
/// local position, parented to the view.
void func_actor_323400_801644C4(GpEnemy* enemy, Task* task)
{
    Actor323000Work*        work;
    GpEnemyTaskFuncTable4   sp;
    Actor323000TickScratch* scratch;
    u8*                     head;
    GpCoord*                walker;
    SVECTOR*                pos;

    work = (Actor323000Work*)task->work;
    gameGetPtrSlot(3);
    sp                           = D_actor_323400_80161E24;
    task->extra.tmd->coords->flg = 0;
    head                         = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8)             = head - 0x1C;
    scratch                      = (Actor323000TickScratch*)(head - 0x1C);
    Gp_UpdateCoord(task->extra.tmd->coords);
    scratch->pos.vx = task->extra.tmd->coords->workm.t[0];
    scratch->pos.vy = task->extra.tmd->coords->workm.t[1];
    scratch->pos.vz = task->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &scratch->pos, 0, 0);
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    sp.funcs[work->field_0](enemy, task);
    scratch->local.vx = 0;
    scratch->local.vy = 0;
    scratch->local.vz = 0;
    {
        SVECTOR  local;
        VECTOR   result;
        s32      flag;
        SVECTOR* localp = &local;

        walker   = &task->extra.tmd->coords[2];
        pos      = &scratch->local;
        local.vx = scratch->local.vx;
        local.vy = pos->vy;
        local.vz = pos->vz;
        while (1) {
            if (walker->sub == NULL)
                break;
            if (walker != &gGfxViewCoord) {
                gte_SetTransMatrix(&walker->coord);
                gte_SetRotMatrix(&walker->coord);
                gte_ldv0(localp);
                gte_rtv0tr();
                gte_stlvnl(&result);
                gte_stflg(&flag);
                local.vx = result.vx;
                local.vy = result.vy;
                local.vz = result.vz;
                walker   = walker->sub;
                continue;
            }
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            break;
        }
    }
    enemy->bodyPos.vx = scratch->local.vx;
    enemy->bodyPos.vy = scratch->local.vy;
    enemy->bodyPos.vz = scratch->local.vz;
    enemy->coord      = &gGfxViewCoord;
    SCRATCH_POP_BYTES(0x1C);
}

void func_actor_323400_8016475C(void)
{
}

/// Handler for message 0x7D5: sets the model's display flags for the mode in
/// `arg2` and picks the state that follows. 0 hides the model (flag 0x80
/// alone), rebuilds the buffers and restarts state 0; 1 clears the flags,
/// showing it, rebuilds and starts state 2; 2 raises flag 4 over the current
/// flags and 3 replaces them with it, both restarting state 0.
s32 func_actor_323400_80164764(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor323000Work* work;

    obj  = task->extra.tmd;
    work = (Actor323000Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 2;
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}

/// Handler for message 0x7D6: returns 1 while the enemy still has hit points,
/// and otherwise 1 only when the model has neither flag 0x80 nor flag 2 set.
s32 func_actor_323400_80164824(Task* task)
{
    s32 ret;
    u16 flags;
    s32 mask2;
    s32 mask80;

    if (((GpEnemy*)task->spawnArg2)->hp > 0) {
        return 1;
    }

    flags   = task->extra.tmd->flags;
    mask80  = flags;
    mask80 &= 0x80;
    mask2   = flags & 2;
    if (mask80 != 0) {
        return 0;
    }

    ret = 0;
    if (mask2 == 0) {
        ret = 1;
        SOFT_BARRIER();
    }
    return ret;
}

/// Handler for message 0x7D4: writes `placement` onto the actor's root
/// coordinate (translation, then yaw / pitch / roll), then reads the resulting
/// yaw back out of the matrix's third row and caches it in `field_16`.
///
/// The `TmdObject` is re-read from `Task::extra` for every access because the
/// stores and the `Gfx_RotMatrix*` calls in between may alias it.
s32 func_actor_323400_80164874(Task* task, s32 arg1, GpXformArg* placement)
{
    Actor323000Work* work;

    work                                = (Actor323000Work*)task->work;
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 1);
    Gfx_RotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, 0);
    Gfx_RotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, 0);
    task->extra.tmd->coords->flg = 0;
    work->field_16               = ratan2(-task->extra.tmd->coords->coord.m[2][0],
                                          task->extra.tmd->coords->coord.m[2][2]);
    return 1;
}

/// Handler for message 0x7DB: copies the payload's three leading bytes into
/// the work block and, when its `code` is 0x1602, picks the state from `mode`:
/// 1 moves the root coordinate to (0x4330, 1, 0xA8C), marks it for rebuilding
/// and starts state 2; 0 and 2 restart state 0; any other mode only stores the
/// bytes.
s32 func_actor_323400_80164974(Task* task, s32 arg1, GpCmdArg* msg, s32 arg3)
{
    Actor323000Work* work;
    u16              mode;

    work = (Actor323000Work*)task->work;

    work->field_91C = msg->from.loc.stage;
    work->field_91D = msg->from.loc.area;
    work->field_91E = (u8)msg->command;

    if (msg->from.key == 0x1602) {
        mode = msg->command;
        switch (mode) {
            case 1:
                task->extra.tmd->coords->coord.t[0] = 0x4330;
                task->extra.tmd->coords->coord.t[1] = mode;
                task->extra.tmd->coords->coord.t[2] = 0xA8C;
                task->extra.tmd->coords->flg        = 0;
                work->field_0                       = 2;
                break;
            case 0:
            case 2:
                work->field_0 = 0;
                break;
        }
    }
    return 0;
}

/// Handler for message 0x7D3: latches the requested animation id into
/// `field_82E` and restarts the state machine at state 1.
s32 func_actor_323400_80164A50(Task* task, s32 arg1, GpAnimArg* msg, s32 arg3)
{
    Actor323000Work* work = (Actor323000Work*)task->work;

    work->field_82E = msg->field_4;
    work->field_0   = 1;
    work->field_2   = -1;
    return 0;
}

/// `Task::exitCallback` the spawn handler installs: destroys the enemy the
/// task carries.
void func_actor_323400_80164A78(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Spawns effect 0x60054 at coordinate `arg1` of the actor's model while the
/// room's effect set is live, with the offset that limb uses (none at the
/// root and part 1, 0x2BC up at parts 9 and 7, 0x258 up at 14 and 17); the
/// spawn argument is `arg2` with bit 31 set. Other coordinates spawn nothing.
/// Nothing in this package calls it.
void func_actor_323400_80164AA0(Task* task, s16 arg1, s16 arg2)
{
    SVECTOR sp10;
    s32     spawn;

    switch (arg1) {
        case 0:
        case 1:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0;
            break;
        case 9:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x2BC;
            break;
        case 7:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x2BC;
            break;
        case 14:
        case 17:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x258;
            break;
        default:
            spawn = 0;
            break;
    }

    if (Gp_State1C->roomEffectMode == 2 && spawn == 1) {
        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[arg1], arg2 | 0x80000000, &sp10);
    }
}

/// State 0 of `D_actor_323400_80161E24`: when the work block's `field_4` flag
/// is set, flags the enemy's link node and hides the model (raises flag
/// 0x80). `obj` gets its own local: the fused form ranks the `Task::extra`
/// load with the store and transposes it.
void func_actor_323400_80164B98(GpEnemy* arg0, Task* arg1)
{
    Actor323000Work* work;
    TmdObject*       obj;

    work = (Actor323000Work*)arg1->work;
    if (work->field_4 != 0) {
        obj                      = arg1->extra.tmd;
        arg0->node.state.b.flags = 1;
        obj->flags              |= 0x80;
    }
}

/// State 1 of `D_actor_323400_80161E24`: on entry clears the enemy's link-node
/// flags, shows the model (clears its flags), rebuilds its buffers and resets
/// the slots to the current clip `field_82E` with both turn targets zeroed.
/// The tick runs every frame.
void func_actor_323400_80164BD0(GpEnemy* enemy, Task* task)
{
    Actor323000Work* work;
    TmdObject*       obj;

    work = (Actor323000Work*)task->work;
    if (work->field_4 != 0) {
        obj                       = task->extra.tmd;
        enemy->node.state.b.flags = 0;
        obj->flags                = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        func_actor_323400_80163B58(task);
    } else {
        func_actor_323400_80163B58(task);
    }
}

/// State 3 of `D_actor_323400_80161E24`: on entry flags the enemy's link node,
/// shows the model (clears its flags), rebuilds its buffers and re-seeds the
/// slots with clip 2 from the per-state table, with both turn targets zeroed.
/// On later frames the tick runs and the root coordinate is marked for
/// rebuilding.
void func_actor_323400_80164C4C(GpEnemy* enemy, Task* task)
{
    Actor323000Work* work;
    TmdObject*       obj;

    work = (Actor323000Work*)task->work;
    if (work->field_4 != 0) {
        obj                       = task->extra.tmd;
        enemy->node.state.b.flags = 1;
        obj->flags                = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_82E = 2;
        work->field_828 = 1;
        work->field_83E = 0;
        work->field_840 = 0;
        func_actor_323400_80163B58(task);
    } else {
        func_actor_323400_80163B58(task);
        task->extra.tmd->coords->flg = 0;
    }
}

/// Task body of the actor's descriptor: runs the handler for the task's
/// state from a stack copy of `D_actor_323400_80161E34` - the spawn handler,
/// the per-frame driver, then `Gp_DestroyEnemy`.
void func_actor_323400_80164CEC(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_323400_80161E34;
    sp.funcs[task->state](task->spawnArg2, task);
}
