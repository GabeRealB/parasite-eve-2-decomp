#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actor.h"
#include "actors/actors_shared_80164954.h"
#include "actors/actors_shared_80164af0.h"
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
#include "rooms/rooms_shared_80182078.h"

/// Payload of message 0x7DB in `D_actor_323000_801739D0`: `code` is the
/// sub-command the handler selects on (0x202 here) and `mode` its variation.
/// `func_actor_323000_80164A54` also reads the three leading bytes one at a
/// time, through `Actor323000MsgBytes`.
typedef struct Actor323000Msg {
    /* 0x0 */ u16 code;
    /* 0x2 */ u16 mode;
} Actor323000Msg;

/// Byte view of `Actor323000Msg`: `b0` and `b1` are the halves of `code` and
/// `b2` the low half of `mode`.
typedef struct Actor323000MsgBytes {
    /* 0x0 */ u8 b0;
    /* 0x1 */ u8 b1;
    /* 0x2 */ u8 b2;
} Actor323000MsgBytes;

/// Animation source `func_800B3F84` is handed for both of the work block's
/// contexts.
extern u8 D_actor_323000_8017387C[];

/// Effect record the spawn handler fills: the model root's coordinate and
/// the two spawn arguments 0x100 and 2.
extern GpEffArg D_actor_323000_80173A24;

/// Message table published as `Task::msgTable` by the spawn handler.
extern void* D_actor_323000_801739D0;

/// Enemy pair source `GpEnemy::param` is pointed at by the spawn handler.
extern GpPairSrcE D_actor_323000_80164D54;

/// Main-executable byte; while it is 1 the contact-record push and avoid
/// helpers return at once without touching the coordinate.
extern u8 D_80072729;

/// Whole-unit part of the last movement step `func_actor_323000_80162A2C`
/// applied, rounded away from zero when the step had a fraction.
extern SVECTOR D_actor_323000_80173A14;

/// Per-state animation table `func_actor_323000_80163A30` reads when it
/// re-seeds the slots: 0x2D bytes per `field_82C`, indexed by `field_82E`.
extern s8 D_actor_323000_80173090[];

/// Psy-Q `RotMatrixY`.
void func_8004BFF8(s16 angle, MATRIX* matrix);

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

void func_actor_323000_80163EA0(GpEnemy* enemy, Task* task);
void func_actor_323000_8016409C(GpEnemy* enemy, Task* task);
void func_actor_323000_8016420C(GpEnemy* enemy, Task* task);
void func_actor_323000_801645A4(GpEnemy* enemy, Task* task);
void func_actor_323000_80164B18(Task* task);
void func_actor_323000_80164C20(GpEnemy* arg0, Task* arg1);
void func_actor_323000_80164C58(GpEnemy* enemy, Task* task);

/// State handlers `func_actor_323000_801645A4` runs by `Actor323000Work::field_0`.
const GpEnemyTaskFuncTable4 D_actor_323000_80161E24 = {
    func_actor_323000_80164C20,
    func_actor_323000_8016409C,
    func_actor_323000_80164C58,
    func_actor_323000_8016420C,
};

/// Task states `func_actor_323000_80164CE4` runs by `Task::state`: the spawn
/// handler, the per-frame driver, then `Gp_DestroyEnemy`.
const GpEnemyTaskFuncTable3 D_actor_323000_80161E34 = {
    func_actor_323000_80163EA0,
    func_actor_323000_801645A4,
    Gp_DestroyEnemy,
};

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it. The tick uses it to turn the body and head parts.
void func_actor_323000_80161E8C(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}

/// Walks the first `count` contact records (stopping at a zero key) and keeps,
/// in a scratch block carved off `G_SCRATCH_HEAD`, the push that would move
/// `coord` out of the last record of kind 0x10000 or 0x30000, scaled down to
/// 0x100 units when longer. Returns whether any such record was found; returns
/// 0 at once when `gGameSession->viewReady` or `D_80072729` is 1.
s32 func_actor_323000_80162198(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
{
    ActorRepelScratch* head;
    ActorRepelScratch* s;
    ActorRepelScratch* blk;
    SVECTOR*           offset;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    coord->flg                           = 0;
    head                                 = *(ActorRepelScratch**)G_SCRATCH_HEAD;
    blk                                  = head - 1;
    *(ActorRepelScratch**)G_SCRATCH_HEAD = blk;
    s                                    = blk;
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
    coord->flg                            = 0;
    *(ActorRepelScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}

/// Steers `coord` away from the obstacles among the first `count` contact
/// records: collects the bearing of up to eight records of kind 0x10000 or
/// 0x30000 (in the XZ plane, or XY when the facing column is near vertical),
/// discards any pair more than 0x400 apart, and for each remaining bearing
/// nudges both `coord`'s translation and `*pos` a short step away from it.
/// `*pos` accumulates the total nudge. Returns whether any record was of kind
/// 0x10000; returns 0 at once when `gGameSession->viewReady` or `D_80072729`
/// is 1.
s32 func_actor_323000_801624E0(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
{
    u8*                head;
    ActorAvoidScratch* s;
    s16                diff;
    s16                t;
    s32                mag;

    if (gGameSession->viewReady == 1 || D_80072729 == 1) {
        return 0;
    }

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(ActorAvoidScratch);
    s                     = (ActorAvoidScratch*)*(u8**)G_SCRATCH_HEAD;
    s->blocked            = 0;
    pos->vz               = 0;
    pos->vy               = 0;
    pos->vx               = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x34));
    VectorNormalSS((SVECTOR*)(head - 0x34), (SVECTOR*)(head - 0x34));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->eye.vx = *(u16*)&coord->workm.t[0];
    s->eye.vy = *(u16*)&coord->workm.t[1];
    s->eye.vz = *(u16*)&coord->workm.t[2];
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
            s->angle[s->count] = actorBearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = actorBearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
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

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + sizeof(ActorAvoidScratch);
    return s->blocked != 0;
}

/// Steps `coord` by the movement the first `arg2` `GpRec18` records of
/// `movement` resolve to, and keeps the whole-unit part of that step in
/// `D_actor_323000_80173A14`. Returns 1 when the X or Z step is nonzero; a
/// step with a fractional part moves the coordinate and the kept step one
/// unit further from zero.
s32 func_actor_323000_80162A2C(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
{
    void**          scratch;
    u8*             head;
    ActorDeltaFlag* s;
    register void*  p asm("v1");
    s32             val;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    p        = head - 0x14;
    s        = p;
    *scratch = p;
    s->moved = 0;
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((ActorDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_323000_80173A14.vx = ((ActorDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_323000_80173A14.vy = s->delta.vy.w >> 16;
        D_actor_323000_80173A14.vz = s->delta.vz.w >> 16;
        val                        = ((ActorDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_323000_80173A14.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_323000_80173A14.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_323000_80173A14.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_323000_80173A14.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->moved;
}

/// Pushes `coord` `push` units away from each obstacle among the first
/// `count` contact records (kind 0x10000 or 0x30000) whose bearing lies within
/// 0x400 of every other obstacle's. Bearings are taken in world space from the
/// frame's position, relative to the point one unit in front of it. Returns
/// whether any push was applied; returns 0 at once when
/// `gGameSession->viewReady` is 1.
s32 func_actor_323000_80162BD0(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push)
{
    void**                      scratch;
    void**                      tail;
    u8*                         head;
    RoomsShared80182078Scratch* st;
    u16                         vz;
    s16                         d;
    s16                         dz;
    s32                         t;
    s32                         hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp = head - sizeof(RoomsShared80182078Scratch);
        st  = (RoomsShared80182078Scratch*)tmp;
    }
    st->eye.vx = *(u16*)&coord->coord.t[0];
    st->eye.vy = *(u16*)&coord->coord.t[1];
    vz         = *(u16*)&coord->coord.t[2];
    *scratch   = st;
    st->eye.vz = vz;

    actorToWorld(coord->sub, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    actorToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = *(u16*)&recs[st->i].point.vx - *(u16*)&st->eye.vx;
            st->delta.vy     = *(u16*)&recs[st->i].point.vy - *(u16*)&st->eye.vy;
            dz               = *(u16*)&recs[st->i].point.vz - *(u16*)&st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = ratan2(st->delta.vx, dz);

            st->delta.vx     = *(u16*)&st->aim.vx - *(u16*)&st->eye.vx;
            st->delta.vy     = *(u16*)&st->aim.vy - *(u16*)&st->eye.vy;
            dz               = *(u16*)&st->aim.vz - *(u16*)&st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = *(u16*)&st->angle[st->i] - ratan2(st->delta.vx, dz);

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

    tail  = (void**)G_SCRATCH_HEAD;
    hit   = st->hit;
    *tail = (u8*)*tail + sizeof(RoomsShared80182078Scratch);
    return hit;
}

/// Tick of the animation slots while the blend context is live: slots 1..10
/// sample both contexts and write their pose mixed by `field_83C` (the blend
/// context gets the 0x1000 complement), each rate seeded from `field_832`
/// (three below it) and `field_83A`; slots 11..17 only tick the main context.
void func_actor_323000_8016331C(Task* task)
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

/// Effect and sound step of the tick: for the clip in `field_82E`, watches
/// the clip each relevant slot plays, and the first frame one reaches a
/// watched value spawns effect 0x60054 at the matching coordinate and returns
/// the `SndEvt_EnqueueType6` id to play (0 where only effects fire).
/// `field_848` remembers each slot's last clip so the step fires once; it is
/// cleared when none of the watched clips is playing.
s32 func_actor_323000_80163448(Task* task, Actor323000Work* work)
{
    SVECTOR vec;
    s32     reset;

    reset = 1;
    switch (work->field_82E) {
        case 0: {
            s32 clip = work->slots[9].curRec & 0x3FF;
            s32 old;

            if (clip == 0x58) {
                old = work->field_848[9];
                if (old != clip) {
                    work->field_848[9] = clip;
                    vec.vx             = -500;
                    vec.vz             = 200;
                    vec.vy             = 650;
                    Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80002220, &vec);
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
                        vec.vx             = -1000;
                        vec.vz             = 200;
                        vec.vy             = 650;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80002220, &vec);
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
                        vec.vy              = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80002220, &vec);
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
                        vec.vy              = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80002220, &vec);
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
                    Gp_SpawnEff(0x60054, ((TmdObject*)task->extra)->coords, 0x80004A00, &vec);
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
                    vec.vx             = -500;
                    vec.vz             = 200;
                    vec.vy             = 650;
                    Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80003200, &vec);
                    vec.vx = -1000;
                    vec.vz = 200;
                    vec.vy = 650;
                    Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80003200, &vec);
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
                        vec.vy             = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80004480, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80004480, &vec);
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
                        vec.vx             = -500;
                        vec.vz             = 200;
                        vec.vy             = 650;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80002200, &vec);
                        vec.vx = -1000;
                        vec.vz = 200;
                        vec.vy = 650;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80002240, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80003300, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80003340, &vec);
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
                        vec.vy             = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80002200, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80002300, &vec);
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
/// (1 from the per-state table `D_actor_323000_80173090`, 2 by resetting to
/// `field_82E`), seeds the blend context when `field_836` is 2, then ticks
/// the slots - blended through `func_actor_323000_8016331C` while `field_82A`
/// is set. It eases `field_844` toward `field_840` and spreads it over the
/// body joints 2-4, eases `field_842` toward `field_83E` for joint 10, and
/// plays the sound `func_actor_323000_80163448` returns, panned at the root.
void func_actor_323000_80163A30(Task* task)
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
            table     = (u32)D_actor_323000_80173090;
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
        func_actor_323000_8016331C(task);
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
        func_actor_323000_80161E8C(&((TmdObject*)task->extra)->coords[2], thirdAngle);
        ((TmdObject*)task->extra)->coords[2].flg = 0;
        func_actor_323000_80161E8C(&((TmdObject*)task->extra)->coords[3], thirdAngle);
        ((TmdObject*)task->extra)->coords[3].flg = 0;
        func_actor_323000_80161E8C(&((TmdObject*)task->extra)->coords[4], (s16)clampedAngle / 2);
        ((TmdObject*)task->extra)->coords[4].flg = 0;
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
    func_actor_323000_80161E8C(&((TmdObject*)task->extra)->coords[10], (s16)((s32)(u16)turnWork->field_842 * -1));
    ((TmdObject*)task->extra)->coords[10].flg = 0;
    sound                                     = func_actor_323000_80163448(task, work);
    if (sound != 0) {
        pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
    }
}

/// Spawn state: allocates the work block (destroying the enemy if that
/// fails), installs the exit callback, binds the model's light and colour
/// matrices to the block, sets up the enemy record and links its node,
/// initialises both animation contexts, seeds clip 1 and ticks once. It then
/// publishes the message table, parents the root to the view, takes its world
/// position as the actor colour, fills the effect record and advances the
/// task to the per-frame driver.
void func_actor_323000_80163EA0(GpEnemy* enemy, Task* task)
{
    SVECTOR          unused; // never referenced; only reserves the frame slot the ROM has
    VECTOR           pos;
    TmdObject*       obj;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    Actor323000Work* work;
    Actor323000Work* work2;
    Actor323000Work* mem;

    obj        = (TmdObject*)task->extra;
    coord      = obj->coords;
    mem        = (Actor323000Work*)memCalloc(0x934, 0);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback = func_actor_323000_80164B18;
    work2              = (Actor323000Work*)task->work;
    tmd                = (TmdObject*)task->extra;
    tmd->lightMtx      = &work2->light;
    tmd->colorMtx      = &work2->color;
    enemy->field_4     = &((TmdObject*)task->extra)->coords->coord;
    enemy->field_48    = 0;
    enemy->bodyPos.vx  = 0;
    enemy->bodyPos.vy  = 0;
    enemy->bodyPos.vz  = 0;
    enemy->coord       = &((TmdObject*)task->extra)->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags    = 1;
    enemy->param         = &D_actor_323000_80164D54;
    enemy->reactionFlags = 0;
    enemy->hp            = 0;
    enemy->recs          = 0;
    func_800B3F84(&work->anim, D_actor_323000_8017387C, obj, work->poses, work->slots);
    func_800B3F84(&work->blendAnim, D_actor_323000_8017387C, obj, work->blendPoses, work->blendSlots);
    work->field_828 = 2;
    work->field_82E = 1;
    work->field_82A = 0;
    work->field_844 = 0;
    work->field_840 = 0;
    work->field_834 = 0x10;
    work->field_832 = 0x10;
    func_actor_323000_80163A30(task);
    task->msgTable = &D_actor_323000_801739D0;
    coord->sub     = &gGfxViewCoord;
    coord->flg     = 0;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    D_actor_323000_80173A24.coord      = ((TmdObject*)task->extra)->coords;
    D_actor_323000_80173A24.spawnArgLo = 0x100;
    D_actor_323000_80173A24.spawnArgHi = 2;
    work->field_0                      = 0;
    task->state++;
}

/// State 1: on entry clears the enemy's link flag and the model's flags,
/// rebuilds its buffers and asks the tick to reset the slots. Each frame it
/// ticks; when slot 1 sets flag bit 0 during clip 0xF it moves on to clip
/// 0x10, and during clip 0xE it spawns effect 0x60054 at coordinate 7 with
/// spawn argument 0x80002300 while slot 1 plays clip 7 or 9, 0x80003400 for 8.
void func_actor_323000_8016409C(GpEnemy* enemy, Task* task)
{
    Actor323000Work* work;
    TmdObject*       obj;
    SVECTOR          sp10;

    work = (Actor323000Work*)task->work;
    if (work->field_4 != 0) {
        obj               = (TmdObject*)task->extra;
        enemy->node.flags = 0;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        func_actor_323000_80163A30(task);
        return;
    }
    func_actor_323000_80163A30(task);
    if (work->slots[1].flags & 1) {
        if (work->field_82E == 0xF) {
            work->field_828 = 2;
            work->field_82E = 0x10;
        }
        func_actor_323000_80163A30(task);
    }
    if (work->field_82E == 0xE) {
        if ((work->slots[1].curRec & 0x3FF) == 7 || (work->slots[1].curRec & 0x3FF) == 9) {
            sp10.vx = -0x3E8;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80002300, &sp10);
        }
        if ((work->slots[1].curRec & 0x3FF) == 8) {
            sp10.vx = -0x3E8;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80003400, &sp10);
        }
    }
}

/// State 3: on entry flags the enemy's link node, clears the model's flags,
/// rebuilds its buffers and starts clip 0xE with the frame counter at 0. Each
/// frame it ticks and, on frames 29, 32 and 33, spawns effect 0x60054 at the
/// limb coordinates; frame 32 also plays a sound chosen by the enemy's
/// `placeKey`.
void func_actor_323000_8016420C(GpEnemy* enemy, Task* task)
{
    Actor323000Work* work;
    TmdObject*       obj;
    s32              id;
    s32              pan;
    SVECTOR          ofs2;
    SVECTOR          ofs;

    work = (Actor323000Work*)task->work;
    if (work->field_4 != 0) {
        obj               = (TmdObject*)task->extra;
        enemy->node.flags = 1;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_82E = 0xE;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        work->field_6   = 0;
        func_actor_323000_80163A30(task);
        return;
    }
    func_actor_323000_80163A30(task);
    switch (++work->field_6) {
        case 29: {
            SVECTOR* p = &ofs;
            p->vx      = -0x1F4;
            p->vz      = 0xC8;
            p->vy      = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80005600, p);
            p->vx = -0x3E8;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80005A00, p);
        } break;
        case 32: {
            SVECTOR* p = &ofs;
            p->vx      = -0x3E8;
            p->vz      = 0xC8;
            p->vy      = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80005A80, p);
            p->vx = -0x1F4;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80006800, p);
            id  = ((enemy->placeKey >> 12) << 8) | 0x4001000D;
            pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
            ofs2.vy = -0x258;
            ofs2.vx = 0;
            ofs2.vz = -0x384;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[1], 0x80005A00, &ofs2);
        } break;
        case 33: {
            SVECTOR* p = &ofs;
            p->vx      = -0x1F4;
            p->vz      = 0xC8;
            p->vy      = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80006800, p);
            p->vx = -0x3E8;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80006B00, p);
            p->vx = -0x3E8;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80004400, p);
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x258;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80003800, p);
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x258;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80004900, p);
            ofs2.vy = -0x2BC;
            ofs2.vx = 0;
            ofs2.vz = -0x258;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[1], 0x80005A00, &ofs2);
        } break;
    }
    ((TmdObject*)task->extra)->coords->flg = 0;
}

/// Per-frame driver of the live actor: brings the model root's coordinate up
/// to date, takes its world position as the actor colour, flags a state change
/// in `field_4`, and runs the state handler `field_0` selects from a stack copy
/// of `D_actor_323000_80161E24`. Afterwards it walks the origin of the model's
/// third part coordinate up to `gGfxViewCoord` and stores it as the enemy's
/// local position, parented to the view.
void func_actor_323000_801645A4(GpEnemy* enemy, Task* task)
{
    Actor323000Work*        work;
    GpEnemyTaskFuncTable4   sp;
    Actor323000TickScratch* scratch;
    u8*                     head;
    GsCOORDINATE2*          walker;
    SVECTOR*                pos;

    work = (Actor323000Work*)task->work;
    gameGetPtrSlot(3);
    sp                                     = D_actor_323000_80161E24;
    ((TmdObject*)task->extra)->coords->flg = 0;
    head                                   = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD                  = head - 0x1C;
    scratch                                = (Actor323000TickScratch*)(head - 0x1C);
    Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
    scratch->pos.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    scratch->pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    scratch->pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
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

        walker   = &((TmdObject*)task->extra)->coords[2];
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
    enemy->bodyPos.vx      = scratch->local.vx;
    enemy->bodyPos.vy      = scratch->local.vy;
    enemy->bodyPos.vz      = scratch->local.vz;
    enemy->coord           = &gGfxViewCoord;
    *(u8**)G_SCRATCH_HEAD += 0x1C;
}

/// Handler for message 0x7DF: does nothing.
void func_actor_323000_8016483C(void)
{
}

/// Handler for message 0x7D5: sets the model's display flags for the mode in
/// `arg2` and picks the state that follows. 0 sets flag 0x80, rebuilds the
/// buffers and restarts state 0; 1 clears the flags, rebuilds and starts
/// state 2; 2 raises flag 4 over the current flags and 3 replaces them with
/// it, both restarting state 0.
s32 func_actor_323000_80164844(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor323000Work* work;

    obj  = (TmdObject*)task->extra;
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
s32 func_actor_323000_80164904(Task* task)
{
    s32 ret;
    u16 flags;
    s32 mask2;
    s32 mask80;

    if (((GpEnemy*)task->spawnArg2)->hp > 0) {
        return 1;
    }

    flags   = ((TmdObject*)task->extra)->flags;
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
s32 func_actor_323000_80164954(Task* task, s32 arg1, GpPlaceArg* placement)
{
    Actor323000Work* work;

    work                                          = (Actor323000Work*)task->work;
    ((TmdObject*)task->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, placement->rot.vy, 1);
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, placement->rot.vx, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->coords->flg = 0;
    work->field_16                         = ratan2(-((TmdObject*)task->extra)->coords->coord.m[2][0],
                                                    ((TmdObject*)task->extra)->coords->coord.m[2][2]);
    return 1;
}

/// Handler for message 0x7DB: copies the payload's three leading bytes into
/// the work block and, when `code` is 0x202, selects the state from `mode`:
/// 1 starts state 2, 0 and 2 state 0, and 3 state 3. Other codes only store
/// the bytes.
s32 func_actor_323000_80164A54(Task* task, s32 arg1, Actor323000Msg* msg, s32 arg3)
{
    Actor323000Work*     work;
    Actor323000MsgBytes* bytes;

    work  = (Actor323000Work*)task->work;
    bytes = (Actor323000MsgBytes*)msg;

    work->field_91C = bytes->b0;
    work->field_91D = bytes->b1;
    work->field_91E = bytes->b2;

    if (msg->code == 0x202) {
        switch (msg->mode) {
            case 1:
                work->field_0 = 2;
                break;
            case 0:
            case 2:
                work->field_0 = 0;
                break;
            case 3:
                work->field_0 = msg->mode;
                break;
        }
    }
    return 0;
}

/// Handler for message 0x7D3: latches the requested animation id into
/// `field_82E` and restarts the state machine at state 1.
s32 func_actor_323000_80164AF0(Task* task, s32 arg1, GpAnimArg* msg, s32 arg3)
{
    Actor323000Work* work = (Actor323000Work*)task->work;

    work->field_82E = msg->field_4;
    work->field_0   = 1;
    work->field_2   = -1;
    return 0;
}

/// `Task::exitCallback` the spawn handler installs: destroys the enemy the
/// task carries.
void func_actor_323000_80164B18(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Spawns effect 0x60054 at coordinate `arg1` with the offset that limb
/// uses; the spawn argument is `arg2` with bit 31 set. Coordinates the
/// switch does not list use whatever the offset holds. Nothing in this
/// package calls it.
void func_actor_323000_80164B40(Task* task, s16 arg1, s16 arg2)
{
    SVECTOR    sp10;
    TmdObject* obj;

    switch (arg1) {
        case 0:
        case 1:
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0;
            break;
        case 9:
            sp10.vx = -0x1F4;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            break;
        case 7:
            sp10.vx = -0x3E8;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            break;
        case 14:
        case 17:
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x258;
            break;
    }

    obj = (TmdObject*)task->extra;
    Gp_SpawnEff(0x60054, &obj->coords[arg1], arg2 | 0x80000000, &sp10);
}

/// State 0 of `D_actor_323000_80161E24`: when the work block's `field_4` flag
/// is set, flags the enemy's link node and raises bit 0x80 of the model's
/// flags. `obj` gets its own local: the fused form ranks the `Task::extra`
/// load with the store and transposes it.
void func_actor_323000_80164C20(GpEnemy* arg0, Task* arg1)
{
    Actor323000Work* work;
    TmdObject*       obj;

    work = (Actor323000Work*)arg1->work;
    if (work->field_4 != 0) {
        obj              = (TmdObject*)arg1->extra;
        arg0->node.flags = 1;
        obj->flags      |= 0x80;
    }
}

/// State 2: on entry flags the enemy's link node, clears the model's flags,
/// rebuilds its buffers and starts clip 0xD with the frame counter at 0; the
/// tick runs every frame.
void func_actor_323000_80164C58(GpEnemy* enemy, Task* task)
{
    Actor323000Work* work;
    TmdObject*       obj;
    SVECTOR          unused; // never referenced; only reserves the frame slot the ROM has

    work = (Actor323000Work*)task->work;
    if (work->field_4 != 0) {
        obj               = (TmdObject*)task->extra;
        enemy->node.flags = 1;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_82E = 0xD;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        work->field_6   = 0;
        func_actor_323000_80163A30(task);
    } else {
        func_actor_323000_80163A30(task);
    }
}

/// Task body of the actor's descriptor: runs the handler for the task's
/// state from a stack copy of `D_actor_323000_80161E34` - the spawn handler,
/// the per-frame driver, then `Gp_DestroyEnemy`.
void func_actor_323000_80164CE4(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_323000_80161E34;
    sp.funcs[task->state](task->spawnArg2, task);
}
