#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// Dual-width view of the animation rate in the work block. The message
/// handler `func_actor_210600_8014B770` arms it as one halfword, while the
/// seeding body copies the low byte into every slot's `GpAnimSlot.rate`.
typedef union Actor210600Rate {
    /* 0x0 */ u16 half;
    /* 0x0 */ u8  byte;
} Actor210600Rate;
STATIC_ASSERT_SIZEOF(Actor210600Rate, 0x2);

/// The actor's work block. The spawn body allocates it zeroed with
/// `memCalloc(0x8D8, false)` and keeps it in `Task::work`, which an enemy
/// actor uses for its own state rather than a `TaskIdMap`. It holds the
/// animation context and slots at the front, the animation request state, and
/// the light / colour matrices the task's `TmdObject` is pointed at.
typedef struct Actor210600Work {
    /// Animation context the spawn body starts through `func_800B3F84`, with
    /// its 19 slots directly behind it and the pose buffer after them.
    /* 0x000 */ ActorAnimRig19 rig;
    /* 0x43C */ byte           pad_43C[0x440];
    /// Animation request state. `field_87C` is the step the seeding body
    /// `func_actor_210600_8014B2C0` dispatches on -- 1 seeks every slot to
    /// `field_882`, 2 resets them, and both settle on 3 and clear the frame
    /// counter at `field_884`, which the running step then counts in.
    /// `field_882` is the requested clip, `field_880` the clip the previous
    /// request latched (the row `D_actor_210600_8015A498` is indexed with);
    /// `field_88A` steps 2 to 3 on the first update that sees it at 2.
    /* 0x87C */ s16             field_87C;
    /* 0x87E */ byte            pad_87E[0x2];
    /* 0x880 */ s16             field_880;
    /* 0x882 */ u16             field_882;
    /* 0x884 */ u16             field_884;
    /* 0x886 */ Actor210600Rate field_886;
    /* 0x888 */ byte            pad_888[0x2];
    /* 0x88A */ s16             field_88A;
    /* 0x88C */ byte            pad_88C[0x4];
    /* 0x890 */ s16             field_890;
    /* 0x892 */ byte            pad_892[0x4];
    /// Clip id (low 10 bits of `curRec`) slot 0 held on the last update, kept
    /// so the once-per-clip effect is not respawned while the clip is held.
    /* 0x896 */ s16 field_896;
    /// The light / colour matrices the spawn body points the task's
    /// `TmdObject::lightMtx` / `colorMtx` at.
    /* 0x898 */ MATRIX light;
    /* 0x8B8 */ MATRIX color;
} Actor210600Work;
STATIC_ASSERT_SIZEOF(Actor210600Work, 0x8D8);

/// Step table the seeding body `func_actor_210600_8014B2C0` walks: one 5-byte
/// row per clip the previous request latched in `Actor210600Work::field_880`,
/// addressed by the requested clip in `field_882`. The byte it reads is handed
/// to `func_800B4114` as the request's fifth argument.
extern s8 D_actor_210600_8015A498[][5];

/// Stack record the state dispatcher copies the state table into before the
/// indirect call. Only `table` is written; the dispatcher's frame is larger
/// than the table alone, which the two trailing words account for.
typedef struct Actor210600DispatchCtx {
    /* 0x00 */ GpEnemyTaskFuncTable3 table;
    /* 0x0C */ s32                   field_C;
    /* 0x10 */ s32                   field_10;
} Actor210600DispatchCtx;
STATIC_ASSERT_SIZEOF(Actor210600DispatchCtx, 0x14);

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// Animation source the spawn body starts the work block's animation context
/// from.
extern u8 D_actor_210600_8015A4B4[];

/// Message table the spawn body publishes as `Task::msgTable`.
extern u8 D_actor_210600_8015A4CC[];

/// Integer part of the last movement step `func_actor_210600_8014A9D0`
/// applied.
extern SVECTOR D_actor_210600_8015D310;

/// Integer part of the last movement step `func_actor_210600_8014C638`
/// applied.
extern SVECTOR D_actor_210600_8015D318;

/// Declared here with a signed `arg2`: the callee's own definition takes it
/// as `u16`, but callers pass a sign-extended animation id, and a `u16`
/// prototype in scope would add a zero-extension the original calls do not
/// have.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it.
void func_actor_210600_80149E30(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    SCRATCH_PUSH(MATRIX);
    rotation = SCRATCH_HEAD(MATRIX);
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    SCRATCH_POP(MATRIX);
}

/// Walks the first `count` records of `recs`, up to an empty key, and for
/// every kind 0x10000 or 0x30000 record computes the XZ push-out of the
/// coordinate's world position from it; the last such push is kept in the
/// scratch block, and its length is scaled down to 0x100 when longer. Returns
/// whether any record of those kinds was met. Does nothing, returning 0, while
/// `Mc_SaveData.field_5C1` or the session's `viewReady` is 1.
s32 func_actor_210600_8014A13C(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
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

/// Collects the bearings of up to eight kind 0x10000 / 0x30000 records among
/// the first `count` of `recs`, taken in the XZ plane unless the coordinate's
/// facing is near vertical. Any two bearings more than a quarter turn (0x400)
/// apart cancel each other; each bearing left steps `coord` 10 units away from
/// it, the total XZ step accumulating in `pos`. Returns whether a kind 0x10000
/// record was among them; returns 0 at once while the session's `viewReady`
/// or `Mc_SaveData.field_5C1` is 1.
s32 func_actor_210600_8014A484(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
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

/// Steps `coord` by the movement the first `arg2` records of `movement`
/// resolve to, and latches the integer part of that delta into
/// `D_actor_210600_8015D310`. Returns the "moved" flag: set when the X or Z
/// delta is nonzero; where a delta also has a fractional part, the coordinate
/// and the latched step are nudged one unit further away from zero.
s32 func_actor_210600_8014A9D0(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
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
        D_actor_210600_8015D310.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_210600_8015D310.vy = s->delta.vy.w >> 16;
        D_actor_210600_8015D310.vz = s->delta.vz.w >> 16;
        val                        = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_210600_8015D310.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_210600_8015D310.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_210600_8015D310.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_210600_8015D310.vz--;
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
s32 func_actor_210600_8014AB74(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push)
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
    st->eye.vx                     = *(u16*)&coord->coord.t[0];
    st->eye.vy                     = *(u16*)&coord->coord.t[1];
    vz                             = *(u16*)&coord->coord.t[2];
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

    tail = SCRATCH_HEAD_ADDR;
    hit  = st->hit;
    SCRATCH_POP_BYTES_AT(tail, sizeof(OverlayBisectorScratch));
    return hit;
}

/// Animation request handler: step 1 of the work block's `field_87C` seeks
/// every slot 1..18 to the clip in `field_882` through `func_800B4114`,
/// passing `field_886`'s rate byte into the slot and the step `field_880`'s row
/// of `D_actor_210600_8015A498` as the request's fifth argument, then latches
/// the clip into `field_880`; step 2 does the same through
/// `Gp_AnimResetSlot`. Both settle on step 3 and clear the frame counter at
/// `field_884`, which is counted from here on while every slot is ticked.
void func_actor_210600_8014B2C0(Task* task)
{
    Actor210600Work* work;
    Actor210600Work* start;
    Actor210600Work* reset;
    Actor210600Work* tick;
    s32              i;
    s32              j;
    s32              k;

    work = (Actor210600Work*)task->work;
    if (work->field_87C == 1) {
        start = (Actor210600Work*)task->work;
        for (i = 1; i < 0x13; i++) {
            start->rig.slots[i].rate = start->field_886.byte;
            func_800B4114(&start->rig.anim, i, (s16)start->field_882, 0,
                          D_actor_210600_8015A498[start->field_880][(s16)start->field_882]);
        }
        start->field_880 = start->field_882;
        goto advance;
    }
    if (work->field_87C == 2) {
        reset = (Actor210600Work*)task->work;
        for (j = 1; j < 0x13; j++) {
            reset->rig.slots[j].rate = reset->field_886.byte;
            Gp_AnimResetSlot(&reset->rig.anim, j, (s16)reset->field_882);
        }
        reset->field_880 = reset->field_882;
    advance:
        work->field_87C = 3;
        work->field_884 = 0;
    }
    if (work->field_88A == 2) {
        work->field_88A = 3;
    }
    work->field_884++;
    tick = (Actor210600Work*)task->work;
    for (k = 1; k < 0x13; k++) {
        tick->rig.slots[k].rate = tick->field_886.byte;
        Gp_AnimTickIndex(&tick->rig.anim, k);
    }
}

/// Rebuilds the model's root part rotation around the yaw it already faces and
/// rescales it uniformly through a 0x34-byte block borrowed from
/// `G_SCRATCH_HEAD`, which is handed back once the rotation has been copied
/// onto the coordinate. The same code as `func_actor_210600_8014B7B0`,
/// expanded in place where the update body calls it.
static __inline__ void Actor210600_ScaleRotation(Task* task, s16 scale)
{
    ActorScaleRotScratch* blk;
    GsCOORDINATE2*        coord;
    u8*                   head;
    s16                   ang;
    u16                   m22;

    head                               = SCRATCH_HEAD(u8);
    coord                              = ((TmdObject*)task->extra)->coords;
    blk                                = (ActorScaleRotScratch*)(head - 0x34);
    SCRATCH_HEAD(ActorScaleRotScratch) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((ActorScaleRotScratch*)(head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
    SCRATCH_POP_BYTES(0x34);
}

/// Update state of the actor. While `Actor210600Work::field_890` is clear it
/// runs the animation pass, rebuilds the model root's rotation around its yaw
/// at 0.75 scale, and when animation slot 1 holds clip 7 while slot 0 did not
/// on the previous update, spawns the effect `Gp_GetIdParam1(0x1001)` on the
/// model's second part. `enemy` is unused.
void func_actor_210600_8014B434(GpEnemy* enemy, Task* task)
{
    Actor210600Work* work;
    SVECTOR          vec;
    GpEffArg         eff;
    s32              id;

    work = (Actor210600Work*)task->work;
    if (work->field_890 == 0) {
        func_actor_210600_8014B2C0(task);
        Actor210600_ScaleRotation(task, 0xC00);

        id = work->rig.slots[1].curRec & 0x3FF;
        if (id == 7 && work->field_896 != id) {
            memset(&vec, 0, 8);
            eff.coord      = ((TmdObject*)task->extra)->coords;
            eff.spawnArgLo = 0x100;
            eff.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, ((TmdObject*)task->extra)->coords + 1, &vec, &eff);
        }
        work->field_896 = work->rig.slots[0].curRec & 0x3FF;
    }
}

/// Message 0x7D5 handler, listed in `D_actor_210600_8015A4CC`: `arg2` selects
/// the display mode. 0 hides the model (`TmdObject::flags` = 0x80) and 1 shows
/// it (flags cleared), both reallocating its buffers through
/// `Tmd_AllocBuffers`; 2 adds bit 0x4 to the flags and any other value sets
/// them to 0x4 alone. Modes 0 and 2 set `Actor210600Work::field_890`, which
/// stops the update state, and the other two clear it. `arg1` is unused.
s32 func_actor_210600_8014B5F4(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor210600Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor210600Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_890 = 1;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_890 = 0;
            break;
        case 2:
            obj->flags     |= 4;
            work->field_890 = 1;
            break;
        default:
            obj->flags      = 4;
            work->field_890 = 0;
            break;
    }
    return 0;
}

/// Message 0x7D4 handler, listed in `D_actor_210600_8015A4CC`: places the
/// model root at `placement`. The three longs become the coordinate's
/// translation, the X, Y and Z angles are then applied in that order through
/// `Gfx_RotMatrixX` / `Y` / `Z`, and the coordinate is marked dirty. `msgId`
/// is unused; the handler always reports the message handled.
s32 func_actor_210600_8014B6A0(Task* task, s32 msgId, GpXformArg* placement)
{
    ((TmdObject*)task->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->coords->flg = 0;
    return 1;
}

/// Message 0x7DB handler, listed in `D_actor_210600_8015A4CC`. When the payload
/// comes from sender 0x401 with selector 1, it requests clip 1 through the
/// reset step at rate 0x10 and clears `Actor210600Work::field_890` so the
/// update state runs. Always reports the message handled.
s32 func_actor_210600_8014B770(Task* task, s32 msgId, GpCmdArg* msg)
{
    Actor210600Work* work;
    u16              selector;

    work = (Actor210600Work*)task->work;
    if (msg->from.key == 0x401) {
        selector = msg->command;
        if (selector == 1) {
            work->field_886.half = 0x10;
            work->field_882      = selector;
            work->field_890      = 0;
            work->field_87C      = 2;
        }
    }
    return 1;
}

/// Rebuilds `coord`'s rotation as a pure Y rotation by the yaw it currently
/// faces (`ratan2` of `-m[2][0], m[2][2]`), uniformly scaled by `scale`,
/// through a 0x34-byte block borrowed from `G_SCRATCH_HEAD` and handed back
/// once the matrix is copied. Marks the coordinate dirty. Nothing in the
/// overlay calls it: the update body carries the same code inline.
void func_actor_210600_8014B7B0(GsCOORDINATE2* coord, s16 scale)
{
    void**                scratch;
    void*                 head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    blk                            = (ActorScaleRotScratch*)((u8*)head - 0x34);
    SCRATCH_HEAD_AT(scratch, void) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((ActorScaleRotScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    SCRATCH_POP_BYTES_AT(scratch, 0x34);
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

/// Spawn state of the actor: allocates its `Actor210600Work`, destroying the
/// enemy if that fails, and points the task's `TmdObject` at the block's
/// light / colour matrices. The enemy takes the model root's matrix and its
/// third part coordinate, with its body offset zeroed, and is linked in. The
/// animation context is started from `D_actor_210600_8015A4B4` and reset to
/// clip 1, the message table is installed, and the model root is parented to
/// `gGfxViewCoord` and rebuilt once before its world position is handed to
/// `func_800D7A9C`. Advances the task to the next state.
void func_actor_210600_8014B8C8(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    Actor210600Work* work;
    Actor210600Work* mem;
    TmdObject*       tmd;

    obj        = task->extra;
    coord      = obj->coords;
    mem        = (Actor210600Work*)memCalloc(0x8D8, false);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    tmd               = task->extra;
    tmd->lightMtx     = &work->light;
    tmd->colorMtx     = &work->color;
    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &((TmdObject*)task->extra)->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.b.flags = 1;
    enemy->field_4D           = 0;
    enemy->reactionFlags      = 0;
    enemy->field_4D           = 0;
    func_800B3F84(&work->rig.anim, D_actor_210600_8015A4B4, obj, work->rig.poses, work->rig.slots);
    work->field_87C = 2;
    work->field_882 = 1;
    func_actor_210600_8014B2C0(task);
    task->msgTable = D_actor_210600_8015A4CC;
    coord->sub     = &gGfxViewCoord;
    coord->flg     = 0;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C((TmdObject*)task->extra, &vec, 0, 3);
    task->state++;
}

/// The actor's three task states - spawn, update and teardown - which
/// `func_actor_210600_8014BA3C` runs by `Task::state`.
const GpEnemyTaskFuncTable3 D_actor_210600_80149E24 = {
    {
        func_actor_210600_8014B8C8,
        func_actor_210600_8014B434,
        Gp_DestroyEnemy,
    },
};

/// State dispatcher: copies the state table onto the stack and calls the entry
/// `Task::state` selects with the task's enemy and the task itself.
void func_actor_210600_8014BA3C(Task* arg0)
{
    Actor210600DispatchCtx sp;

    sp.table = D_actor_210600_80149E24;
    sp.table.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// A second copy of `func_actor_210600_80149E30`; the package carries both.
void func_actor_210600_8014BA98(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    SCRATCH_PUSH(MATRIX);
    rotation = SCRATCH_HEAD(MATRIX);
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    SCRATCH_POP(MATRIX);
}

/// A second copy of `func_actor_210600_8014A13C`; the package carries both.
s32 func_actor_210600_8014BDA4(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
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

/// A second copy of `func_actor_210600_8014A484`; the package carries both.
s32 func_actor_210600_8014C0EC(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
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

/// A second copy of `func_actor_210600_8014A9D0`, latching its step into
/// `D_actor_210600_8015D318` instead.
s32 func_actor_210600_8014C638(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
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
        D_actor_210600_8015D318.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_210600_8015D318.vy = s->delta.vy.w >> 16;
        D_actor_210600_8015D318.vz = s->delta.vz.w >> 16;
        val                        = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_210600_8015D318.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_210600_8015D318.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_210600_8015D318.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_210600_8015D318.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP_BYTES(0x14);
    return s->moved;
}

/// A second copy of `func_actor_210600_8014AB74`; the package carries both.
s32 func_actor_210600_8014C7DC(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push)
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
    st->eye.vx                     = *(u16*)&coord->coord.t[0];
    st->eye.vy                     = *(u16*)&coord->coord.t[1];
    vz                             = *(u16*)&coord->coord.t[2];
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

    tail = SCRATCH_HEAD_ADDR;
    hit  = st->hit;
    SCRATCH_POP_BYTES_AT(tail, sizeof(OverlayBisectorScratch));
    return hit;
}
