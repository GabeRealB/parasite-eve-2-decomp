#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actor.h"
#include "actors/actors_shared_80135990.h"
#include "actors/actors_shared_80135a60.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/rooms_shared_80182078.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// The actor's per-instance work block, reached through `Task::work`. `field_0`
/// is the display mode the tick dispatches on, `field_2` the mode dispatched
/// last and `field_4` set on the frame it changed; `field_174` is the motion
/// the slots play, `field_4A` the current animation id (low ten bits), and
/// `field_220` latches the last trigger id reported.
typedef struct Actor123200Work {
    /* 0x000 */ s16        field_0;
    /* 0x002 */ s16        field_2;
    /* 0x004 */ s16        field_4; // non-zero restarts the model (`func_actor_123200_80133820`)
    /* 0x006 */ u16        field_6; // frames since the restart branch last ran
    /* 0x008 */ s16        field_8;
    /* 0x00A */ byte       pad_A[0x2];
    /* 0x00C */ GpAnimCtx  anim;     // `func_800B3F84` arg0
    /* 0x020 */ GpAnimSlot slots[1]; // slots 1..5 continue past here, overlapping the fields below
    /* 0x048 */ byte       pad_48[0x2];
    /* 0x04A */ u16        field_4A; // low ten bits: animation id (`slots[1].field_2`)
    /* 0x04C */ byte       pad_4C[0xC];
    /* 0x058 */ u16        field_58;
    /* 0x05A */ byte       pad_5A[0xB6];
    /* 0x110 */ byte       poses[0x60]; // `func_800B3F84` arg3
    /* 0x170 */ s16        field_170;   // motion state `func_actor_123200_801332E0` switches on
    /* 0x172 */ s16        field_172;
    /* 0x174 */ s16        field_174;
    /* 0x176 */ u16        field_176;
    /* 0x178 */ s16        field_178;
    /* 0x17A */ s16        field_17A; // frames since the motion last restarted
    /* 0x17C */ s16        field_17C; // frames since then with `field_58` bit 1 set
    /* 0x17E */ s16        field_17E;
    /* 0x180 */ byte       pad_180[0x14];
    /* 0x194 */ u8         field_194;
    /* 0x195 */ u8         field_195;
    /* 0x196 */ u8         field_196;
    /* 0x197 */ byte       pad_197[0x1];
    /* 0x198 */ u16        field_198;
    /* 0x19A */ u16        field_19A;
    /* 0x19C */ byte       pad_19C[0xC];
    /// World X/Y/Z of the model's coordinate, narrowed to 16 bits as the spawn
    /// handler samples them through `Actor123200CoordPos`.
    /* 0x1A8 */ u16    field_1A8;
    /* 0x1AA */ u16    field_1AA;
    /* 0x1AC */ u16    field_1AC;
    /* 0x1AE */ byte   pad_1AE[0x2];
    /* 0x1B0 */ s16    field_1B0;
    /* 0x1B2 */ s16    field_1B2;
    /* 0x1B4 */ s16    field_1B4;
    /* 0x1B6 */ byte   pad_1B6[0x6];
    /* 0x1BC */ MATRIX field_1BC; // installed at `TmdObject.lightMtx` by `func_actor_123200_8013352C`
    /* 0x1DC */ MATRIX field_1DC; // installed at `TmdObject.colorMtx`
    /* 0x1FC */ byte   pad_1FC[0x20];
    /// Model scale `func_actor_123200_80133BA0` puts on `field_1BC` through
    /// `ScaleMatrix`; 0x1000 is 1.0 and skips the scale entirely. Picked from
    /// the top nibble of the enemy's `placeKey` by `func_actor_123200_80133EDC`.
    /* 0x21C */ s16  field_21C;
    /* 0x21E */ byte pad_21E[0x2];
    /* 0x220 */ u16  field_220;
    /* 0x222 */ byte pad_222[0xA];
} Actor123200Work;
STATIC_ASSERT_SIZEOF(Actor123200Work, 0x22C);

/// `GsCOORDINATE2.coord.t[]` seen as three unsigned halfwords, so
/// `func_actor_123200_8013352C` samples each world coordinate with `lhu`. The
/// same narrowing `GpCoordXZ` does for X and Z, extended to Y.
typedef struct Actor123200CoordPos {
    /* 0x00 */ byte pad_0[0x18];
    /* 0x18 */ u16  x;
    /* 0x1A */ byte pad_1A[2];
    /* 0x1C */ u16  y;
    /* 0x1E */ byte pad_1E[2];
    /* 0x20 */ u16  z;
    /* 0x22 */ byte pad_22[2];
} Actor123200CoordPos;
STATIC_ASSERT_SIZEOF(Actor123200CoordPos, 0x24);

/// Overlay-wide spawn record the spawn handler fills for the instance's own
/// coordinate, with the 0x100 / 1 argument pair. Each overlay that spawns this
/// way keeps one, and they differ only in the coordinate and the argument.
extern GpEffArg D_actor_123200_80137248;

/// Pair source the spawn handler installs at `GpEnemy::param`.
extern GpPairSrcE D_actor_123200_80134208;

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_123200_80137154[];

/// Message table the spawn handler publishes as `Task::msgTable`.
extern u8 D_actor_123200_80137214[];

/// Message packet handed to `func_actor_123200_80133EDC`. Its first three bytes
/// are copied into the work block, and its first four are then re-read as a
/// message type and a command.
typedef union Actor123200Msg {
    struct {
        u8 b0;
        u8 b1;
        u8 b2;
        u8 b3;
    } bytes;
    struct {
        u16 type;
        u16 cmd;
    } words;
} Actor123200Msg;
STATIC_ASSERT_SIZEOF(Actor123200Msg, 0x4);

/// While this is 1, the push-out helpers return without moving anything and
/// the forward step is skipped.
extern u8 D_80072729;

/// Integer part of the last movement step `func_actor_123200_801329F0`
/// applied.
extern SVECTOR D_actor_123200_80137240;

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

void func_actor_123200_80134178(GpEnemy* arg0, Task* arg1);

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it.
void func_actor_123200_80131E50(GsCOORDINATE2* coord, s16 yaw)
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

/// Walks the first `count` records of `recs`, up to an empty key, and for
/// every kind 0x10000 or 0x30000 record computes the XZ push-out of the
/// coordinate's world position from it; the last such push is kept in the
/// scratch block, and its length is scaled down to 0x100 when longer. Returns
/// whether any record of those kinds was met. Does nothing, returning 0, while
/// `D_80072729` or the session's `viewReady` is 1.
s32 func_actor_123200_8013215C(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
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

/// Collects the bearings of up to eight kind 0x10000 / 0x30000 records among
/// the first `count` of `recs`, drops both bearings of every pair more than 0x400 apart, and
/// for each bearing left steps `coord` 10 units away from it, accumulating the
/// total XZ step in `pos`. Returns whether any kind 0x10000 record was met;
/// returns 0 at once when the session's `viewReady` or `D_80072729` is 1.
s32 func_actor_123200_801324A4(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
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

/// Steps `coord` by the movement the first `arg2` records of `movement`
/// resolve to, and latches the integer part of that delta into
/// `D_actor_123200_80137240`. Returns the "moved" flag: set when the X or Z
/// delta is nonzero; where a delta also has a fractional part, the coordinate
/// and the latched step are nudged one unit further away from zero.
s32 func_actor_123200_801329F0(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
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
        D_actor_123200_80137240.vx = ((ActorDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_123200_80137240.vy = s->delta.vy.w >> 16;
        D_actor_123200_80137240.vz = s->delta.vz.w >> 16;
        val                        = ((ActorDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_123200_80137240.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_123200_80137240.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_123200_80137240.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_123200_80137240.vz--;
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
s32 func_actor_123200_80132B94(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push)
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

/// Restarts animation slots 1-5 on the motion in `field_174` at the combined
/// rate `field_176 + field_178`, and records that motion as the one playing.
static __inline__ void Actor123200_ResetSlots(Actor123200Work* arg0)
{
    Actor123200Work* work = arg0;
    s32              i;

    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->field_176 + work->field_178;
        Gp_AnimResetSlot(&work->anim, i, work->field_174);
    }
    work->field_172 = work->field_174;
}

/// Advances animation slots 1-5 by one frame at the combined rate
/// `field_176 + field_178`.
static __inline__ void Actor123200_TickSlots(Task* task)
{
    Actor123200Work* work;
    s32              i;

    work = task->work;
    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->field_176 + work->field_178;
        Gp_AnimTickIndex(&work->anim, i);
    }
}

/// Drives the model's motion from the work block's `field_170`: 1 and 2
/// restart the slots on `field_174` and move to 3, clearing both frame
/// counters; 3 advances the slots one frame, counting it in `field_17A` and,
/// while `field_58` bit 1 is set, in `field_17C` too.
void func_actor_123200_801332E0(Task* task)
{
    Actor123200Work* work;

    work = task->work;
    if (work->field_170 == 1) {
        Actor123200_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 2) {
        Actor123200_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 3) {
        work->field_17A++;
        Actor123200_TickSlots(task);
        if (work->field_58 & 2) {
            work->field_17C++;
        }
    }
}

/// In motion states 2 and 3, reports 0x400C0001 the first time the animation id
/// in `field_4A` reaches one of that state's trigger ids (latched in
/// `field_220`); in state 5, 0x400C0005 while bit 1 of `field_58` is set.
/// Returns 0 otherwise.
s32 func_actor_123200_80133450(Actor123200Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->field_174) {
        case 2:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->field_220 == v) {
                goto same;
            }
            arg0->field_220 = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_220 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_220 = id;
            break;
        case 5:
            if (arg0->field_58 & 2) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

/// Normalises `dir` in place and scales it to 0x3E8/0x1000 of unit length on
/// the GTE. The pointer stays in one register across `VectorNormalSS` because
/// the GTE loads read it back afterwards.
static __inline__ void Actor123200_ScaleForward(SVECTOR* dir)
{
    VectorNormalSS(dir, dir);
    gte_lddp(0x3E8);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);
}

/// Spawn state of this enemy: allocates the work block, publishes it as
/// `Task::work`, reparents the model to `gGfxViewCoord`, seeds its animation
/// slots from `D_actor_123200_80137154` and hangs the enemy's display node off
/// part 2 of the model's coordinate array. The top nibble of the enemy's
/// `placeKey` biases the three timers in `field_176`, `field_198` and
/// `field_19A` -- up by the nibble when its low bit is set, down by half of it
/// otherwise.
void func_actor_123200_8013352C(GpEnemy* enemy, Task* task)
{
    SVECTOR          dir;
    Actor123200Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    u32              scale;
    u32              flag;

    obj        = (TmdObject*)task->extra;
    coord      = obj->coords;
    work       = memCalloc(sizeof(Actor123200Work), false);
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->msgTable = D_actor_123200_80137214;
    coord->sub     = &gGfxViewCoord;
    obj->flags     = 0;
    func_800B3F84(&work->anim, D_actor_123200_80137154, obj, work->poses, work->slots);

    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &((TmdObject*)task->extra)->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags    = 1;
    enemy->param         = &D_actor_123200_80134208;
    enemy->reactionFlags = 0;
    enemy->hpMax         = 0;
    enemy->hp            = 0;
    enemy->recs          = 0;

    work->field_174 = 1;
    work->field_170 = 2;
    work->field_176 = 0x10;
    work->field_178 = 0;
    func_actor_123200_801332E0(task);
    work->field_17E = 0;
    work->field_8   = 0;
    obj->lightMtx   = &work->field_1BC;
    obj->colorMtx   = &work->field_1DC;
    coord->flg      = 0;
    work->field_198 = 5;
    work->field_19A = 0x14;

    scale = (u16)(enemy->placeKey >> 12);
    flag  = scale & 1;
    if (flag == 1) {
        work->field_176 += enemy->placeKey >> 12;
        work->field_19A += enemy->placeKey >> 12;
        work->field_198 += enemy->placeKey >> 12;
    } else {
        work->field_176 -= scale >> 1;
        work->field_19A -= enemy->placeKey >> 13;
        work->field_198 -= enemy->placeKey >> 13;
    }

    work->field_1A8 = ((Actor123200CoordPos*)((TmdObject*)task->extra)->coords)->x;
    work->field_1AA = ((Actor123200CoordPos*)((TmdObject*)task->extra)->coords)->y;
    work->field_1AC = ((Actor123200CoordPos*)((TmdObject*)task->extra)->coords)->z;

    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, &dir);
    dir.vy = 0;
    Actor123200_ScaleForward(&dir);

    work->field_0                      = 0;
    work->field_2                      = -1;
    D_actor_123200_80137248.coord      = ((TmdObject*)task->extra)->coords;
    D_actor_123200_80137248.spawnArgLo = 0x100;
    D_actor_123200_80137248.spawnArgHi = 1;
    task->state++;
}

/// Steps `coord` 5/0x1000 of the way along its own forward axis (column 2 of
/// its rotation, normalised and GPF-scaled) and flags it for rebuild. The
/// direction vector lives in an `SVECTOR` carved off the scratch head and
/// handed straight back.
static __inline__ void Actor123200_StepForward(GsCOORDINATE2* coord)
{
    u8*      head;
    SVECTOR* dir;

    head       = (u8*)SCRATCH_SP;
    dir        = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_SP = (u32)dir;

    Gfx_MatrixCol2(&coord->coord, dir);
    VectorNormalSS(dir, dir);
    gte_lddp(5);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);

    coord->coord.t[0] += dir->vx;
    coord->coord.t[1] += dir->vy;
    coord->coord.t[2] += dir->vz;
    coord->flg         = 0;

    SCRATCH_SP = (u32)((u8*)SCRATCH_SP + sizeof(SVECTOR));
}

/// Display mode 1 handler (entry 1 of `D_actor_123200_80131E24`). On the frame
/// the mode is entered (`field_4` set) it re-arms the model -- clearing
/// `TmdObject.flags` and reinstating its buffers, rewriting the
/// 0x1B0/0x1B2/0x1B4 triple, restarting the motion on state 2 and the frame
/// counter `field_6`, and marking the enemy's lock-on node not lockable -- and
/// returns. Otherwise the frame counter runs, 0xC bytes are reserved off the
/// scratch head, and unless the game is frozen the model is stepped forward
/// along its facing; the reservation is released after the animation update
/// and the model's coordinate is flagged for rebuild.
void func_actor_123200_80133820(GpEnemy* enemy, Task* task)
{
    Actor123200Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    work = (Actor123200Work*)task->work;
    if (work->field_4 != 0) {
        obj               = (TmdObject*)task->extra;
        enemy->node.flags = 1;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_1B0 = 0x115D;
        work->field_1B2 = 1;
        work->field_1B4 = 0x12D5;
        work->field_174 = 2;
        work->field_170 = 2;
        func_actor_123200_801332E0(task);
        ((TmdObject*)task->extra)->coords->flg = 0;
        work->field_6                          = 0;
        return;
    }
    work->field_6++;
    SCRATCH_SP -= 0xC;
    coord       = ((TmdObject*)task->extra)->coords;
    if (D_80072729 != 1) {
        Actor123200_StepForward(coord);
    }
    func_actor_123200_801332E0(task);
    SCRATCH_SP                            += 0xC;
    ((TmdObject*)task->extra)->coords->flg = 0;
}

/// Unless the game is frozen, steps `coord` 5/0x1000 of the way along its own
/// forward axis through an `SVECTOR` carved off the scratch head, and flags it
/// for rebuild.
static __inline__ void Actor123200_MoveForward(GsCOORDINATE2* coord)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        SOFT_TOUCH_REG(vec);
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(5);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// Display mode 2 handler (entry 2 of `D_actor_123200_80131E24`): the same
/// re-arm on entry as `func_actor_123200_80133820`; on later frames it counts
/// the frame, steps the model along its facing unless the game is frozen, and
/// updates its animation, without the extra scratch reservation.
void func_actor_123200_801339F0(GpEnemy* enemy, Task* task)
{
    Actor123200Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    work = (Actor123200Work*)task->work;
    if (work->field_4 != 0) {
        obj               = (TmdObject*)task->extra;
        enemy->node.flags = 1;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_1B0 = 0x115D;
        work->field_1B2 = 1;
        work->field_1B4 = 0x12D5;
        work->field_174 = 2;
        work->field_170 = 2;
        func_actor_123200_801332E0(task);
        ((TmdObject*)task->extra)->coords->flg = 0;
        work->field_6                          = 0;
        return;
    }
    work->field_6++;
    coord = ((TmdObject*)task->extra)->coords;
    Actor123200_MoveForward(coord);
    func_actor_123200_801332E0(task);
    ((TmdObject*)task->extra)->coords->flg = 0;
}

/// The three display-mode handlers `func_actor_123200_80133BA0` picks between
/// by the work block's `field_0`, copied onto its stack before the call: 0 the
/// idle state, 1 and 2 the two stepping handlers.
const GpEnemyTaskFuncTable3 D_actor_123200_80131E24 = {
    {
        func_actor_123200_80134178,
        func_actor_123200_80133820,
        func_actor_123200_801339F0,
    },
};

/// Per-frame tick: flags the model's coordinate for rebuild, refreshes its
/// colour from the part matrix's translation, then scales that matrix from the
/// work block's `field_21C`. The render mode in `Gp_StateF0.field_4` runs next -- modes
/// 0 and 1 draw the ground quad while the display mode is non-zero, and 1 and 2
/// return without ticking. The rest re-records the display mode in `field_2`
/// (`field_4` restarting the model when it changed), dispatches the display
/// mode's handler from `D_actor_123200_80131E24`, and plays the sound that
/// handler reports, panned and depth-tagged from the model's coordinate. A
/// raised `gGameSession->viewReady` flags the coordinate for rebuild again.
void func_actor_123200_80133BA0(GpEnemy* enemy, Task* arg1)
{
    VECTOR                pos;
    GpEnemyTaskFuncTable3 table;
    Actor123200Work*      work;
    s32                   snd;
    s32                   pan;
    s32                   id;

    work                                   = (Actor123200Work*)arg1->work;
    table                                  = D_actor_123200_80131E24;
    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
    pos.vx = ((TmdObject*)arg1->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)arg1->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)arg1->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    if (work->field_21C != 0x1000) {
        pos.vx = pos.vy = pos.vz = work->field_21C;
        ScaleMatrix(&work->field_1BC, &pos);
    }
    switch (Gp_StateF0.field_4) {
        case 0:
            if (work->field_0 != 0) {
                ((TmdObject*)arg1->extra)->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)arg1->extra)->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            break;
        case 1:
            if (work->field_0 != 0) {
                ((TmdObject*)arg1->extra)->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)arg1->extra)->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            return;
        case 2:
            ((TmdObject*)arg1->extra)->flags = 0x80;
            return;
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    table.funcs[work->field_0](enemy, arg1);
    id = func_actor_123200_80133450(work);
    if (id != 0) {
        snd = id | ((enemy->placeKey >> 12) << 8);
        pan = (s8)Gp_GetObjPan(((TmdObject*)arg1->extra)->coords);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(((TmdObject*)arg1->extra)->coords));
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)arg1->extra)->coords->flg = 0;
    }
}

/// The enemy's three task states -- spawn, per-frame tick and teardown -- which
/// `func_actor_123200_801341A8` runs by `Task::state`.
const GpEnemyTaskFuncTable3 D_actor_123200_80131E30 = {
    {
        func_actor_123200_8013352C,
        func_actor_123200_80133BA0,
        Gp_DestroyEnemy,
    },
};

/// Message handler (id 0x7D5 in `D_actor_123200_80137214`). `arg2` selects the
/// mode: 0 hides the model (`TmdObject.flags` bit 0x80), 1 clears its flags and
/// so shows it, 2 sets bit 0x4, and 3 and 4 both clear the flags and then set
/// bit 0x4. Modes 0 and 1 reinstate the model's buffers through
/// `Tmd_AllocBuffers` and set the work block's display mode `field_0` to 1;
/// modes 2, 3 and 4 set it to 0. `arg1` is unused. Always returns 0.
s32 func_actor_123200_80133E30(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor123200Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor123200Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 1;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 1;
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
        case 4:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}

/// Message handler (id 0x7DB in `D_actor_123200_80137214`). Copies the
/// message's first three bytes into the work block and handles type 0xB02
/// commands: 1 selects display mode 2, at full scale when the top nibble of
/// the enemy's `placeKey` is 1 and at quarter scale otherwise; 2 selects mode 1
/// at full scale; 3 selects mode 0. Always returns 0.
s32 func_actor_123200_80133EDC(Task* task, s32 arg1, Actor123200Msg* msg)
{
    Actor123200Work* work;
    GpEnemy*         enemy;

    work            = (Actor123200Work*)task->work;
    enemy           = (GpEnemy*)task->spawnArg2;
    work->field_194 = msg->bytes.b0;
    work->field_195 = msg->bytes.b1;
    work->field_196 = msg->bytes.b2;
    if (msg->words.type == 0xB02) {
        switch ((s32)msg->words.cmd) {
            case 1:
                if ((enemy->placeKey >> 12) == 1) {
                    work->field_21C = 0x1000;
                } else {
                    work->field_21C = 0x400;
                }
                work->field_0 = 2;
                break;
            case 2:
                work->field_21C = 0x1000;
                work->field_0   = 1;
                break;
            case 3:
                work->field_0 = 0;
                break;
            case 0:
                break;
        }
    }
    return 0;
}

/// Message handler (id 0x7D4 in `D_actor_123200_80137214`). Places the model's
/// coordinate from `placement`: the three longs become the translation, the
/// X, Y and Z angles are applied in that order with `Gfx_RotMatrixX` / `Y` /
/// `Z`, and the coordinate is marked dirty. Always returns 1.
s32 func_actor_123200_80133F90(Task* task, s32 arg1, GpXformArg* placement)
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

/// Rebuilds `coord`'s rotation as a pure yaw -- its current heading, taken
/// with `ratan2` of `-m[2][0], m[2][2]` -- uniformly scaled by `scale`, working
/// in a 0x34-byte block borrowed from the scratchpad. Marks the coordinate
/// dirty.
void func_actor_123200_80134060(GsCOORDINATE2* coord, s16 scale)
{
    void**                scratch;
    void*                 head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (ActorScaleRotScratch*)((u8*)head - 0x34);
    *scratch = blk;

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
    *scratch             = (u8*)*scratch + 0x34;
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

/// Idle state of this enemy (entry 0 of `D_actor_123200_80131E24`). On the
/// frame the state is entered (`field_4` set) it sets the display node's flags
/// to 1 and the model's flags to 0x80; it does nothing on later frames.
void func_actor_123200_80134178(GpEnemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((Actor123200Work*)arg1->work)->field_4 != 0) {
        model            = (TmdObject*)arg1->extra;
        arg0->node.flags = 1;
        model->flags     = 0x80;
    }
}

/// Runs the handler of `D_actor_123200_80131E30` that `Task::state` selects --
/// spawn, per-frame tick or teardown -- on the enemy in `Task::spawnArg2`,
/// copying the table onto the stack before the call.
void func_actor_123200_801341A8(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_123200_80131E30;
    sp.funcs[task->state](task->spawnArg2, task);
}
