#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actor.h"
#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Movement-freeze flag: while it is 1 the collision push and the obstacle
/// steering leave the coordinate alone.
extern u8 D_80072729;

extern u8 D_actor_312200_80169F44[];
extern u8 D_actor_312200_80169F5C[];

/// Whole-unit part of the last step `func_actor_312200_801626C4` applied.
extern SVECTOR D_actor_312200_80169F88;

/// Dual-width view of the animation rate the seeding body
/// `func_actor_312200_80162FB4` copies into every slot's `GpAnimSlot.rate`:
/// the state handlers arm it as a halfword, the seeding body reads back only
/// its low byte.
typedef union Actor312200Rate {
    /* 0x0 */ u16 half;
    /* 0x0 */ u8  byte;
} Actor312200Rate;
STATIC_ASSERT_SIZEOF(Actor312200Rate, 0x2);

/// Private work block of the actor 312200 task, hanging off `Task::work`,
/// `memCalloc(sizeof(Actor312200Work), 0)` in the spawn handler.
///
/// Only the fields the matched code touches are named so far: `yaw` is the
/// heading `func_actor_312200_801635CC` reads back from the root coordinate.
///
/// `field_0` is the state word the 0x7DB handler raises and the per-tick
/// handler dispatches on,
/// `field_4` the live-actor flag every state handler tests on entry, `field_88C`
/// the work state, and `field_892` / `field_896` the two timers the state
/// handlers arm.
///
/// `field_8B4` / `field_8B6` / `field_8B8` are the record
/// `func_actor_312200_801636CC` leaves of the last 0x7DB command it saw: the
/// sender id's two bytes - stored as the bytes they are read as, not as the
/// halfword the handler tests - and then the action halfword.
///
/// `field_8BC` is the display node the spawn handler
/// `func_actor_312200_80163178` builds in place and hands to `Gp_LinkObj` - the
/// `GpObj` whose `ctx.recs` is a three-entry `GpRec18` table at 0x8DC.
/// `func_actor_312200_80163778` clears bit 0x8000 of that node's `flags`.
typedef struct Actor312200Work {
    /* 0x000 */ s16 field_0;
    /// Second halfword of the state word above, set to -1 by the spawn handler.
    /* 0x002 */ s16  field_2;
    /* 0x004 */ s16  field_4;
    /* 0x006 */ byte pad_6[0x2];
    /* 0x008 */ s16  yaw;
    /* 0x00A */ byte pad_A[0x6];
    /// Animation context the spawn body hands `func_800B3F84` first, with its
    /// 19 slots directly behind it: the pose buffer that function is handed
    /// fourth starts directly after the 19 slots.
    /* 0x010 */ GpAnimCtx  anim;
    /* 0x024 */ GpAnimSlot slots[0x13];
    /// The flag halfword the per-tick callback tests: it falls inside the slot
    /// array, being the second slot's `field_10`, because the animation state
    /// runs from 0x24 to the pose buffer.
    /* 0x31C */ byte poses[0x130];
    /// Second animation context, seeded when the 0x89A request word is 2. It
    /// lives inside the pose buffer the first context was handed, and the slots
    /// it resets are the first context's, so the two share their slot array.
    /* 0x44C */ GpAnimCtx       anim2;
    /* 0x460 */ byte            poses2[0x42C];
    /* 0x88C */ s16             field_88C;
    /* 0x88E */ byte            pad_88E[0x2];
    /* 0x890 */ s16             field_890;
    /* 0x892 */ u16             field_892;
    /* 0x894 */ u16             field_894;
    /* 0x896 */ Actor312200Rate field_896;
    /* 0x898 */ byte            pad_898[0x2];
    /// Request state of the second animation context, laid out like the first:
    /// 2 seeds every slot and settles on 3.
    /* 0x89A */ s16             field_89A;
    /* 0x89C */ s16             field_89C;
    /* 0x89E */ Actor312200Rate field_89E;
    /* 0x8A0 */ s16             field_8A0;
    /* 0x8A2 */ byte            pad_8A2[0x6];
    /* 0x8A8 */ s32             field_8A8;
    /// The two bytes the spawn handler arms next to the display node; they sit
    /// immediately before the 0x7DB record, so they are the actor's own copy of
    /// that state rather than part of a message. `field_8AD` is read back with
    /// `lb` by the tick handler, so it is signed like the record halfwords.
    /* 0x8AC */ u8   field_8AC;
    /* 0x8AD */ s8   field_8AD;
    /* 0x8AE */ byte pad_8AE[0x6];
    /* 0x8B4 */ s16  field_8B4;
    /* 0x8B6 */ s16  field_8B6;
    /* 0x8B8 */ s16  field_8B8;
    /* 0x8BA */ byte pad_8BA[0x2];
    /// Display node: `GpObj` at 0x8BC, its `GpRec18` table at 0x8DC.
    /* 0x8BC */ GpObj   field_8BC;
    /* 0x8DC */ GpRec18 recs[3];
    /* 0x924 */ byte    pad_924[0x20];
    /// The light / colour matrices the spawn handler stores into
    /// `TmdObject::lightMtx` / `field_20`, at the top of the block.
    /* 0x944 */ MATRIX light;
    /* 0x964 */ MATRIX color;
} Actor312200Work;
STATIC_ASSERT_SIZEOF(Actor312200Work, 0x984);

/// Step table the seeding body `func_actor_312200_80162FB4` walks: one 5-byte
/// row per clip the previous request latched in `Actor312200Work::field_890`,
/// addressed by the requested clip in `field_892`. The byte it reads is handed
/// to `func_800B4114` as the request's fifth argument.
extern s8 D_actor_312200_80169F28[][5];

void func_actor_312200_80163778(Task* task);
void func_actor_312200_801637CC(Task* task);

/// Walks the first `count` contact records (stopping at a zero key) and keeps,
/// in a scratch block carved off `G_SCRATCH_HEAD`, the push that would move
/// `coord` out of the last record of kind 0x10000 or 0x30000, scaled down to
/// 0x100 units when longer. Returns whether any such record was found; returns
/// 0 at once when `gGameSession->viewReady` or `D_80072729` is 1.
s32 func_actor_312200_80161E30(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
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
s32 func_actor_312200_80162178(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
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
/// `D_actor_312200_80169F88`. Returns 1 when the X or Z step is nonzero; a
/// step with a fractional part moves the coordinate and the kept step one
/// unit further from zero.
s32 func_actor_312200_801626C4(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
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
        D_actor_312200_80169F88.vx = ((ActorDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_312200_80169F88.vy = s->delta.vy.w >> 16;
        D_actor_312200_80169F88.vz = s->delta.vz.w >> 16;
        val                        = ((ActorDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_312200_80169F88.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_312200_80169F88.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_312200_80169F88.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_312200_80169F88.vz--;
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
s32 func_actor_312200_80162868(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push)
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

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp = head - sizeof(OverlayBisectorScratch);
        st  = (OverlayBisectorScratch*)tmp;
    }
    st->eye.vx = *(u16*)&coord->coord.t[0];
    st->eye.vy = *(u16*)&coord->coord.t[1];
    vz         = *(u16*)&coord->coord.t[2];
    *scratch   = st;
    st->eye.vz = vz;

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

    tail  = (void**)G_SCRATCH_HEAD;
    hit   = st->hit;
    *tail = (u8*)*tail + sizeof(OverlayBisectorScratch);
    return hit;
}

/// Animation seeding body, run once per tick: request state 1 seeks every slot
/// of the first context to `field_892` through the step table, state 2 resets
/// them, and both settle on 3 and clear the frame counter at `field_894`.
/// Request state 2 on the second context resets its slots at rate 0x30, then
/// the tail counts a frame and ticks every slot of the first context.
void func_actor_312200_80162FB4(Task* task)
{
    Actor312200Work* work;
    Actor312200Work* start;
    Actor312200Work* reset;
    Actor312200Work* second;
    Actor312200Work* tick;
    s32              i;
    s32              j;
    s32              k;
    s32              m;

    work = (Actor312200Work*)task->work;
    if (work->field_88C == 1) {
        start = (Actor312200Work*)task->work;
        for (i = 1; i < 0x13; i++) {
            start->slots[i].rate = start->field_896.byte;
            func_800B4114(&start->anim, i, (s16)start->field_892, 0,
                          D_actor_312200_80169F28[start->field_890][(s16)start->field_892]);
        }
        start->field_890 = start->field_892;
        goto advance;
    }
    if (work->field_88C == 2) {
        reset = (Actor312200Work*)task->work;
        for (j = 1; j < 0x13; j++) {
            reset->slots[j].rate = reset->field_896.byte;
            Gp_AnimResetSlot(&reset->anim, j, (s16)reset->field_892);
        }
        reset->field_890 = reset->field_892;
    advance:
        work->field_88C = 3;
        work->field_894 = 0;
        work->field_8A8 = 0;
    }
    if (work->field_89A == 2) {
        second                 = (Actor312200Work*)task->work;
        second->field_89E.half = 0x30;
        second->field_8A0      = 0x500;
        for (k = 1; k < 0x13; k++) {
            second->slots[k].rate = second->field_89E.byte;
            Gp_AnimResetSlot(&second->anim2, k, (s16)second->field_89C);
        }
        work->field_89A = 3;
    }
    work->field_894++;
    tick = (Actor312200Work*)task->work;
    for (m = 1; m < 0x13; m++) {
        tick->slots[m].rate = tick->field_896.byte;
        Gp_AnimTickIndex(&tick->anim, m);
    }
}

/// Spawn handler, the first entry of the actor's state table: allocates the
/// 0x984-byte `Actor312200Work` into `Task::work` (tearing the enemy down if
/// that fails) and seeds the enemy object, the model's root coordinate and the
/// animation context from the `TmdObject` in `Task::extra`. The model's light
/// and colour matrices are pointed into the work block, the enemy takes the
/// root coordinate's matrix as `field_4` and the model's third part coordinate
/// as `coord`, and the display node built in place at `field_8BC` gets the
/// block's three-entry `GpRec18` table and the model's fourth part coordinate.
/// The model coordinate is parented to `gGfxViewCoord` and rebuilt once before
/// its translation is propagated over the three part coordinates
/// (`func_800D7A9C`, start 0, count 3).
void func_actor_312200_80163178(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    TmdObject*       tmd;
    Actor312200Work* mem;
    Actor312200Work* work;
    GpObj*           node;

    obj        = (TmdObject*)task->extra;
    coord      = obj->coords;
    mem        = (Actor312200Work*)memCalloc(sizeof(Actor312200Work), 0);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    tmd               = (TmdObject*)task->extra;
    tmd->lightMtx     = &work->light;
    tmd->colorMtx     = &work->color;
    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &((TmdObject*)task->extra)->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags    = 1;
    enemy->field_4D      = 0;
    enemy->reactionFlags = 0;
    enemy->field_4D      = 0;
    func_800B3F84(&work->anim, D_actor_312200_80169F44, obj, work->poses, work->slots);
    work->field_88C      = 2;
    work->field_892      = 1;
    work->field_896.half = 0x10;
    func_actor_312200_80162FB4(task);
    node           = &work->field_8BC;
    node->coord    = &((TmdObject*)task->extra)->coords[3];
    node->ctx.recs = work->recs;
    node->pos.vx   = 0;
    node->pos.vy   = 0;
    node->pos.vz   = 0;
    node->key      = 0x3000A;
    node->radius   = 0x180;
    node->flags    = 1;
    Gp_LinkObj(2, node);
    node->flags |= 0x8000;
    Gp_InitRec18Table(node->ctx.recs, 3, 0);
    task->msgTable  = D_actor_312200_80169F5C;
    work->field_8AC = 0;
    work->field_8AD = 1;
    coord->sub      = &gGfxViewCoord;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C((TmdObject*)task->extra, &vec, 0, 3);
    work->field_0 = 0;
    work->field_2 = -1;
    task->state++;
}

/// Per-tick handler and state dispatcher, called with the task second. The
/// handler table is built in place - `func_actor_312200_80163778` at index 0,
/// the tick handler at index 1 - and `field_0` selects from it, unless the global
/// `Gp_StateF0.field_4` holds the actor. `field_4` records whether the state moved
/// before it is re-latched into `field_2`. The tail clears the display node's
/// `GpRec18` record while occupied, re-propagates the root coordinate's
/// translation over the model's three part coordinates while `field_8AD` is
/// set, and then refreshes `field_8AD` from that coordinate's `flg` - so the
/// propagation runs on the frame after the coordinate is dirtied. That same
/// coordinate is rebuilt (`flg` dropped) and the 0x51030008 loop queued while
/// the room is live, from view 0x10 with the 0x7DB action `field_8B8` at 1.
///
/// The `SOFT_BARRIER` is a matching aid, not the original's: without it the
/// scheduler pulls the handler table's first `lui` in front of the `work`
/// load. The trailing `vec` is the original's own - three dead stores, but the
/// frame and the rest of the schedule are built around them.
void func_actor_312200_80163370(GpEnemy* enemy, Task* task)
{
    TmdObject*       obj;
    Actor312200Work* work;
    VECTOR           vec;
    void             (*states[2])(Task*);
    s32              pan;

    work = (Actor312200Work*)task->work;
    SOFT_BARRIER();
    states[0] = func_actor_312200_80163778;
    states[1] = func_actor_312200_801637CC;
    if (Gp_StateF0.field_4 == 0) {
        if (work->field_2 != work->field_0) {
            work->field_4 = 1;
        } else {
            work->field_4 = 0;
        }
        work->field_2 = work->field_0;
        states[work->field_0](task);
        if (work->recs[0].key != 0) {
            Gp_ClearRec18Occupied(work->recs);
        }
        if (work->field_8AD != 0) {
            obj = (TmdObject*)task->extra;
            func_800D7A9C(obj, (VECTOR*)obj->coords->workm.t, 0, 3);
        }
        if (gGameSession->viewReady != 0) {
            ((TmdObject*)task->extra)->coords->flg = 0;
            if ((Gp_GetViewIndex() == 0x10) && (work->field_8B8 == 1)) {
                pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
                SndEvt_EnqueueType6(0x51030008, pan,
                                    (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
            }
        }
        if (((TmdObject*)task->extra)->coords->flg == 0) {
            work->field_8AD = 1;
        } else {
            work->field_8AD = 0;
        }
        vec.vz = 0;
        vec.vy = 0;
        vec.vx = 0;
    }
}

/// Id 0x7D5 command handler, listed first in `D_actor_312200_80169F5C`. `arg2`
/// is the mode: 0 sets the model's `TmdObject::flags` to exactly 0x80, 1 clears
/// them, 2 raises bit 0x4, and 3 clears them and then raises bit 0x4. Modes 0
/// and 1 re-run `Tmd_AllocBuffers` on the model, and every mode except 1 resets
/// the work block's `field_0` state word. `arg1` is unused.
s32 func_actor_312200_80163510(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor312200Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor312200Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
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

/// Id 0x7D4 placement opcode: the three longs of `placement->pos` are copied
/// onto the actor's root coordinate, the Euler angles are applied X / Y / Z,
/// and the resulting heading is read back out of the matrix Z-axis with
/// `ratan2` and cached in `Actor312200Work::yaw`.
s32 func_actor_312200_801635CC(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2*   coord;
    s32              mx;
    s32              mz;
    Actor312200Work* work;

    work                                          = (Actor312200Work*)task->work;
    ((TmdObject*)task->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->coords->flg = 0;
    coord                                  = ((TmdObject*)task->extra)->coords;
    mx                                     = coord->coord.m[2][0];
    mz                                     = coord->coord.m[2][2];
    work->yaw                              = ratan2(-mx, mz);
    return 1;
}

/// Id 0x7DB command handler. The payload is always recorded in the work block,
/// and a message from sender 0x301 additionally selects the work state: action
/// 1 takes state 2, actions 2, 3 and 4 take state 1, and the action itself is
/// latched in the 0x892 timer. Either way the actor's `field_0` state word is
/// raised to 1.
s32 func_actor_312200_801636CC(Task* task, s32 msgId, GpCmdArg* msg)
{
    Actor312200Work* work;
    s32              action;

    work            = (Actor312200Work*)task->work;
    work->field_8B4 = msg->from.loc.stage;
    work->field_8B6 = msg->from.loc.area;
    work->field_8B8 = msg->command;

    if (msg->from.key == 0x301) {
        action = msg->command;
        switch (action) {
            case 1:
                work->field_892 = action;
                work->field_88C = 2;
                break;

            case 2:
                work->field_892 = action;
                work->field_88C = 1;
                break;

            case 3:
                work->field_892 = action;
                work->field_88C = 1;
                break;

            case 4:
                work->field_892 = action;
                work->field_88C = 1;
                break;
        }
    }

    work->field_0 = 1;
    return 1;
}

/// On a live actor, sets the enemy's link-node flags to 1, raises the model's
/// 0x80 bit (which takes it out of `Tmd_DrawActiveNodes`), clears
/// `GpEnemy::field_4D` and drops bit 0x8000 of the `field_8BC` node's flags.
void func_actor_312200_80163778(Task* task)
{
    Actor312200Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;

    work = (Actor312200Work*)task->work;
    if (work->field_4 != 0) {
        obj                    = (TmdObject*)task->extra;
        enemy                  = (GpEnemy*)task->spawnArg2;
        enemy->node.flags      = 1;
        obj->flags            |= 0x80;
        enemy->field_4D        = 0;
        work->field_8BC.flags &= 0x7FFF;
    }
}

/// Per-tick state callback. A live actor (`field_4`) re-enters work state 2
/// with the 0x896 timer armed at 0x10; once the 0x892 timer has counted those
/// 0x10 ticks and the task's flag bit 0 is set, the state drops to 1 and the
/// timer to 4. Either way the tick ends in the actor's anim/particle update.
void func_actor_312200_801637CC(Task* task)
{
    Actor312200Work* work;

    work = (Actor312200Work*)task->work;
    if (work->field_4 != 0) {
        work->field_88C      = 2;
        work->field_896.half = 0x10;
        func_actor_312200_80162FB4(task);
    }
    if ((s16)work->field_892 == 0x10 && (work->slots[1].flags & 1)) {
        work->field_892 = 4;
        work->field_88C = 1;
    }
    func_actor_312200_80162FB4(task);
}

/// The actor's three state handlers, dispatched by
/// `func_actor_312200_80163854`: spawn, per-frame tick and teardown.
const GpEnemyTaskFuncTable3 D_actor_312200_80161E24 = {
    func_actor_312200_80163178,
    func_actor_312200_80163370,
    Gp_DestroyEnemy,
};

/// Runs the handler `Task::state` selects from `D_actor_312200_80161E24`,
/// passing the spawn argument and the task. The table is copied onto the stack
/// before the call.
void func_actor_312200_80163854(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_312200_80161E24;
    sp.funcs[task->state](task->spawnArg2, task);
}
