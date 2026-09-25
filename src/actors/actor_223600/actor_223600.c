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
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/rooms_shared_80182078.h"

/// The actor's per-instance work block, allocated by the spawn state and
/// reached through `Task::work`. Only the fields the actor's code touches are
/// modelled: the state word and its change latch, the animation context and
/// slots, the motion fields `func_actor_223600_8014B2F4` drives, the bytes a
/// message handler copies out of its event packet, the spawn and target
/// positions, the model's light and colour matrices and the park latch.
typedef struct Actor223600Work {
    /* 0x000 */ s16        field_0; ///< state
    /* 0x002 */ s16        field_2; ///< state at the previous dispatch
    /* 0x004 */ s16        field_4; ///< set when `field_0` moved away from `field_2`
    /* 0x006 */ s16        field_6; ///< frames spent in the approach state
    /* 0x008 */ s16        field_8;
    /* 0x00A */ byte       pad_A[0x2];
    /* 0x00C */ GpAnimCtx  anim;     ///< `func_800B3F84` arg0
    /* 0x020 */ GpAnimSlot slots[1]; ///< slots 1.. continue past here, overlapping the fields below
    /* 0x048 */ byte       pad_48[0x2];
    /* 0x04A */ u16        field_4A; ///< low ten bits: current animation id
    /* 0x04C */ byte       pad_4C[0xC];
    /* 0x058 */ u16        field_58;
    /* 0x05A */ byte       pad_5A[0xB6];
    /* 0x110 */ byte       poses[0x60]; ///< `func_800B3F84` arg3
    /* 0x170 */ s16        field_170;
    /* 0x172 */ s16        field_172;
    /* 0x174 */ s16        field_174; ///< motion state
    /* 0x176 */ u16        field_176;
    /* 0x178 */ s16        field_178;
    /* 0x17A */ s16        field_17A; ///< frames since the motion last restarted
    /* 0x17C */ s16        field_17C; ///< frames since then with `field_58` bit 1 set
    /* 0x17E */ s16        field_17E;
    /* 0x180 */ u8         field_180;
    /* 0x181 */ u8         field_181;
    /* 0x182 */ u8         field_182;
    /* 0x183 */ byte       pad_183[0x1];
    /* 0x184 */ u16        field_184;
    /* 0x186 */ u16        field_186;
    /* 0x188 */ byte       pad_188[0xC];
    /// World X/Y/Z of the model's coordinate, narrowed to 16 bits as the spawn
    /// handler samples them through `Actor223600CoordPos`.
    /* 0x194 */ u16  field_194;
    /* 0x196 */ u16  field_196;
    /* 0x198 */ u16  field_198;
    /* 0x19A */ byte pad_19A[0x2];
    /// Target the approach state steers towards: world X in `field_19C` and
    /// world Z in `field_1A0`, both seeded from the spawn point.
    /* 0x19C */ u16    field_19C;
    /* 0x19E */ s16    field_19E;
    /* 0x1A0 */ u16    field_1A0;
    /* 0x1A2 */ byte   pad_1A2[0x6];
    /* 0x1A8 */ MATRIX field_1A8; ///< installed at `TmdObject.lightMtx`
    /* 0x1C8 */ MATRIX field_1C8; ///< installed at `TmdObject.colorMtx`
    /* 0x1E8 */ byte   pad_1E8[0x20];
    /* 0x208 */ u16    field_208; ///< animation id that last raised the reaction
    /* 0x20A */ byte   pad_20A[0x2];
    /* 0x20C */ s8     field_20C; ///< 1 while the model's coordinate is zeroed
    /* 0x20D */ byte   pad_20D[0x5];
    /// Per-frame height step the parked state adds to the model's world Y,
    /// seeded by the motion the tick enters and retuned as it advances.
    /* 0x212 */ s16 field_212;
} Actor223600Work;
STATIC_ASSERT_SIZEOF(Actor223600Work, 0x214);

/// `GsCOORDINATE2.coord.t[]` seen as three unsigned halfwords, so
/// `func_actor_223600_8014B540` samples each world coordinate with `lhu`.
typedef struct Actor223600CoordPos {
    /* 0x00 */ byte pad_0[0x18];
    /* 0x18 */ u16  x;
    /* 0x1A */ byte pad_1A[2];
    /* 0x1C */ u16  y;
    /* 0x1E */ byte pad_1E[2];
    /* 0x20 */ u16  z;
    /* 0x22 */ byte pad_22[2];
} Actor223600CoordPos;
STATIC_ASSERT_SIZEOF(Actor223600CoordPos, 0x24);

/// 0xC-byte scratch taken from `0x1F8003FC` by the approach state: the XZ
/// offset from the model to its target, and the yaw step derived from it.
typedef struct Actor223600Turn {
    /* 0x0 */ s16  dx;
    /* 0x2 */ s16  dy;
    /* 0x4 */ s16  dz;
    /* 0x6 */ byte pad_6[0x2];
    /* 0x8 */ s16  yaw;
    /* 0xA */ byte pad_A[0x2];
} Actor223600Turn;
STATIC_ASSERT_SIZEOF(Actor223600Turn, 0xC);

/// Effect record the spawn handler fills with the instance's own coordinate
/// and the 0x100 / 1 argument pair.
extern GpEffArg D_actor_223600_80150B5C;

/// Pair source the spawn handler installs at `GpEnemy::param`.
extern GpPairSrcE D_actor_223600_8014CFCC;

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_223600_801509C0[];

/// Message table the spawn handler publishes as `Task::msgTable`.
extern u8 D_actor_223600_80150B28[];

/// Event packet handed to this actor's message handlers. Its first three bytes
/// are copied into the work block, and its first four are then re-read as two
/// little-endian `u16` words: a command word and a sub-command.
typedef union Actor223600Event {
    /* 0x0 */ u8  bytes[4];
    /* 0x0 */ u16 words[2];
} Actor223600Event;
STATIC_ASSERT_SIZEOF(Actor223600Event, 0x4);

/// While this is 1, the push-out helpers return without moving anything and
/// the forward step is skipped.
extern u8 D_80072729;

/// Integer part of the last movement step `func_actor_223600_8014AA04`
/// applied.
extern SVECTOR D_actor_223600_80150B54;

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

void func_actor_223600_8014CF3C(GpEnemy* arg0, Task* arg1);

/// Builds `joint`'s absolute rotation in `out`: its own rotation, then each
/// ancestor pre-multiplied in turn (renormalised after every step) up to but
/// not including `stop`. Returns whether the walk reached `stop` rather than
/// the end of the chain.
static __inline__ s32 Actor223600_AccumulateRotation(GsCOORDINATE2* joint, MATRIX* out, GsCOORDINATE2* stop)
{
    MATRIX         matrix;
    GsCOORDINATE2* coord;

    coord = joint->sub;
    *out  = joint->coord;
    while (1) {
        if (coord == NULL) {
            return 0;
        }
        if (coord == stop) {
            return 1;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(out);
        MatrixNormal(out, &matrix);
        *out  = matrix;
        coord = coord->sub;
    }
}

/// Turns the world-space rotation in `rotation` back into one relative to
/// `joint`'s parent: accumulates the chain above the parent up to the view
/// coordinate, transposes it and pre-multiplies. Nothing is done when the
/// parent is the view coordinate itself. Returns `joint`; the caller stores
/// through the returned pointer, which the matched code needs.
static __inline__ GsCOORDINATE2* Actor223600_LocalizeRotation(GsCOORDINATE2* joint, MATRIX* rotation)
{
    MATRIX         matrix;
    MATRIX         normal;
    MATRIX         transposed;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* view;

    coord = joint->sub;
    if (coord != &gGfxViewCoord) {
        view   = &gGfxViewCoord;
        matrix = coord->coord;
        while (1) {
            coord = coord->sub;
            if (coord == NULL) {
                break;
            }
            if (coord == view) {
                __asm__ volatile(
                    "lhu $12, 0(%0);"
                    "lhu $13, 6(%0);"
                    "lhu $14, 12(%0);"
                    "sh $12, 0(%1);"
                    "sh $13, 2(%1);"
                    "sh $14, 4(%1);"
                    "lhu $12, 2(%0);"
                    "lhu $13, 8(%0);"
                    "lhu $14, 14(%0);"
                    "sh $12, 6(%1);"
                    "sh $13, 8(%1);"
                    "sh $14, 10(%1);"
                    "lhu $12, 4(%0);"
                    "lhu $13, 10(%0);"
                    "lhu $14, 16(%0);"
                    "sh $12, 12(%1);"
                    "sh $13, 14(%1);"
                    "sh $14, 16(%1);"
                    : : "r"(&matrix), "r"(&transposed) : "$12", "$13", "$14", "memory");
                gte_SetRotMatrix(&transposed);
                MulRotMatrix(rotation);
                break;
            }
            gte_SetRotMatrix(&coord->coord);
            MulRotMatrix(&matrix);
            MatrixNormal(&matrix, &normal);
            matrix = normal;
        }
    }
    return joint;
}

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it.
void func_actor_223600_80149E64(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    Actor223600_AccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = Actor223600_LocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}

/// XZ push-out of `pos` from one obstacle record: the record's radius minus
/// the horizontal distance to its point, floored at zero, applied along the
/// direction from the point to `pos` taken into grid space.
static __inline__ void Actor223600_CalcPush(SVECTOR* pos, GpRec18* rec, SVECTOR* out)
{
    VECTOR d;
    VECTOR n;
    s32    t;
    s32    pen;

    d.vx = pos->vx - rec->point.vx;
    d.vy = 0;
    d.vz = pos->vz - rec->point.vz;
    pen  = SquareRoot0(d.vx * d.vx + d.vz * d.vz);
    pen  = rec->depth - pen;
    if (pen <= 0) {
        t = 0;
    } else {
        t = pen;
    }
    pen  = t;
    d.vx = pos->vx - rec->point.vx;
    d.vy = pos->vy - rec->point.vy;
    d.vz = pos->vz - rec->point.vz;
    VectorNormal(&d, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &n, &d);
    out->vx = (pen * d.vx) >> 12;
    out->vy = 0;
    out->vz = (pen * d.vz) >> 12;
}

/// Walks the first `count` records of `recs`, up to an empty key, and for
/// every kind 0x10000 or 0x30000 record computes the XZ push-out of the
/// coordinate's world position from it; the last such push is kept in the
/// scratch block, and its length is scaled down to 0x100 when longer. Returns
/// whether any record of those kinds was met. Does nothing, returning 0, while
/// `D_80072729` or the session's `viewReady` is 1.
s32 func_actor_223600_8014A170(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
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
            Actor223600_CalcPush(&s->pos, &recs[s->i], &s->offset);
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

/// Bearing of `p` from `eye` in the XZ plane.
static __inline__ s16 Actor223600_BearingXZ(SVECTOR3* p, SVECTOR3* eye)
{
    u8*              head;
    ActorAvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (ActorAvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vz);
}

/// Bearing of `p` from `eye` in the XY plane, used when the coordinate's
/// facing is close to vertical.
static __inline__ s16 Actor223600_BearingXY(SVECTOR3* p, SVECTOR3* eye)
{
    u8*              head;
    ActorAvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (ActorAvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vy);
}

/// Collects the bearings of up to eight kind 0x10000 / 0x30000 records among
/// the first `count` of `recs`, drops both bearings of every pair more than 0x400 apart, and
/// for each bearing left steps `coord` 10 units away from it, accumulating the
/// total XZ step in `pos`. Returns whether any kind 0x10000 record was met;
/// returns 0 at once when the session's `viewReady` or `D_80072729` is 1.
s32 func_actor_223600_8014A4B8(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
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
            s->angle[s->count] = Actor223600_BearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = Actor223600_BearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
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
/// `D_actor_223600_80150B54`. Returns the "moved" flag: set when the X or Z
/// delta is nonzero; where a delta also has a fractional part, the coordinate
/// and the latched step are nudged one unit further away from zero.
s32 func_actor_223600_8014AA04(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
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
        D_actor_223600_80150B54.vx = ((ActorDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_223600_80150B54.vy = s->delta.vy.w >> 16;
        D_actor_223600_80150B54.vz = s->delta.vz.w >> 16;
        val                        = ((ActorDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_223600_80150B54.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_223600_80150B54.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_223600_80150B54.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_223600_80150B54.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->moved;
}

/// Carries `v` from the local frame `coord` up the `GsCOORDINATE2::sub` parent
/// chain into world space, using a 0x20 scratch block from `G_SCRATCH_HEAD`.
static __inline__ void Actor223600_ToWorld(GsCOORDINATE2* coord, SVECTOR* v)
{
    RoomsShared80182078Walk* blk;

    {
        register GsCOORDINATE2* parent asm("v0");
        parent                                                                                              = coord;
        ((RoomsShared80182078Walk*)((u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomsShared80182078Walk)))->coord = parent;
    }
    {
        register u8* tmp asm("v0");
        tmp = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomsShared80182078Walk);
        blk = (RoomsShared80182078Walk*)tmp;
    }
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    *(void**)G_SCRATCH_HEAD = blk;
    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr();
        gte_stlvnl(blk->out);
        gte_stflg(&blk->flag);
        blk->vec.vx = *(u16*)&blk->out[0];
        blk->vec.vy = *(u16*)&blk->out[1];
        blk->vec.vz = *(u16*)&blk->out[2];
        blk->coord  = blk->coord->sub;
    }
    v->vx = blk->vec.vx;
    v->vy = blk->vec.vy;
    v->vz = blk->vec.vz;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomsShared80182078Walk);
}

/// The same walk as `Actor223600_ToWorld`, spelled without its register
/// bindings; each caller site needs its own form to match.
static __inline__ void Actor223600_ToWorld2(GsCOORDINATE2* coord, SVECTOR* v)
{
    RoomsShared80182078Walk* blk;

    blk         = (RoomsShared80182078Walk*)((u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomsShared80182078Walk));
    blk->coord  = coord;
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    *(void**)G_SCRATCH_HEAD = blk;
    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr();
        gte_stlvnl(blk->out);
        gte_stflg(&blk->flag);
        blk->vec.vx = *(u16*)&blk->out[0];
        blk->vec.vy = *(u16*)&blk->out[1];
        blk->vec.vz = *(u16*)&blk->out[2];
        blk->coord  = blk->coord->sub;
    }
    v->vx = blk->vec.vx;
    v->vy = blk->vec.vy;
    v->vz = blk->vec.vz;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomsShared80182078Walk);
}

/// Pushes `coord` `push` units away from each obstacle among the first
/// `count` contact records (kind 0x10000 or 0x30000) whose bearing lies within
/// 0x400 of every other obstacle's. Bearings are taken in world space from the
/// frame's position, relative to the point one unit in front of it. Returns
/// whether any push was applied; returns 0 at once when
/// `gGameSession->viewReady` is 1.
s32 func_actor_223600_8014ABA8(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push)
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

    Actor223600_ToWorld(coord->sub, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    Actor223600_ToWorld2(coord, &st->aim);

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
static __inline__ void Actor223600_ResetSlots(Actor223600Work* arg0)
{
    Actor223600Work* work = arg0;
    s32              i;

    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->field_176 + work->field_178;
        Gp_AnimResetSlot(&work->anim, i, work->field_174);
    }
    work->field_172 = work->field_174;
}

/// Advances animation slots 1-5 by one frame at the combined rate
/// `field_176 + field_178`.
static __inline__ void Actor223600_TickSlots(Task* task)
{
    Actor223600Work* work;
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
///
/// Defined old-style, so later calls in this file are not checked against a
/// prototype: the spawn handler passes the enemy's HP as a second argument the
/// function never reads.
void func_actor_223600_8014B2F4(task)
    Task* task;
{
    Actor223600Work* work;

    work = task->work;
    if (work->field_170 == 1) {
        Actor223600_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 2) {
        Actor223600_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 3) {
        work->field_17A++;
        Actor223600_TickSlots(task);
        if (work->field_58 & 2) {
            work->field_17C++;
        }
    }
}

/// In motion states 2 and 3, reports 0x400C0001 the first time the animation id
/// in `field_4A` reaches one of that state's trigger ids (latched in
/// `field_208`); in state 5, 0x400C0005 while bit 2 of `field_58` is set.
/// Returns 0 otherwise.
s32 func_actor_223600_8014B464(Actor223600Work* arg0)
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
            if (arg0->field_208 == v) {
                goto same;
            }
            arg0->field_208 = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_208 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_208 = id;
            break;
        case 5:
            if (arg0->field_58 & 2) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

/// Normalises `dir` in place and scales it to `amount`/0x1000 of unit length on
/// the GTE. The pointer stays in one register across `VectorNormalSS` because
/// the GTE loads read it back afterwards.
static __inline__ void Actor223600_ScaleForward(SVECTOR* dir, s16 amount)
{
    VectorNormalSS(dir, dir);
    gte_lddp(amount);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);
}

/// Steps the model `amount` units along its facing -- the coordinate matrix's z
/// column, normalised and GTE-scaled in a scratch-pad vector -- and invalidates
/// the coordinate. Skipped entirely while `D_80072729` is 1.
static __inline__ void Actor223600_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        Actor223600_ScaleForward(vec, amount);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// Folds a yaw difference back into +/-0x800, a twelfth of a turn either way.
static __inline__ s16 Actor223600_NormalizeYaw(s16 input)
{
    s16 value = input;
    if (input < 0) {
        while (1) {
            if (value >= -0x800) {
                break;
            }
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800) {
                break;
            }
            value -= 0x1000;
        }
    }
    return value;
}

/// Spawn state of this enemy: allocates the 0x214 work block, publishes it as
/// `Task::work`, reparents the model to `gGfxViewCoord`, seeds its animation
/// slots from `D_actor_223600_801509C0` and hangs the enemy's display node off
/// part 2 of the model's coordinate array. HP and max HP both come from
/// `D_actor_223600_8014CFCC`, which also picks the opening motion through
/// `func_actor_223600_8014B2F4`. The context's top `field_8` nibble biases the
/// three timers in `field_176`, `field_184` and `field_186` -- up by the nibble
/// when its low bit is set, down by half of it otherwise. The model's world
/// position is sampled into `field_194`..`field_198` and its facing is
/// normalised and scaled on the GTE, and the instance is published as the
/// overlay's anchor `D_actor_223600_80150B5C`.
void func_actor_223600_8014B540(GpEnemy* enemy, Task* task)
{
    SVECTOR          dir;
    Actor223600Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    u32              scale;
    u32              flag;
    s32              hp;

    obj        = (TmdObject*)task->extra;
    coord      = obj->coords;
    work       = memCalloc(sizeof(Actor223600Work), false);
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    coord->sub     = &gGfxViewCoord;
    task->msgTable = D_actor_223600_80150B28;
    obj->flags     = 0;
    func_800B3F84(&work->anim, D_actor_223600_801509C0, obj, work->poses, work->slots);

    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &((TmdObject*)task->extra)->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags    = 1;
    enemy->hpMax         = 1;
    enemy->hp            = 1;
    enemy->reactionFlags = 0;
    hp                   = D_actor_223600_8014CFCC.hpMax;
    enemy->param         = &D_actor_223600_8014CFCC;
    enemy->recs          = 0;
    enemy->hpMax         = hp;
    enemy->hp            = hp;

    work->field_170 = 2;
    work->field_174 = 1;
    work->field_176 = 0x10;
    work->field_178 = 0;
    func_actor_223600_8014B2F4(task, hp);
    work->field_17E = 0;
    work->field_8   = 0;
    obj->lightMtx   = &work->field_1A8;
    obj->colorMtx   = &work->field_1C8;
    coord->flg      = 0;
    work->field_184 = 5;
    work->field_186 = 0x14;

    scale = (u16)(enemy->placeKey >> 12);
    flag  = scale & 1;
    if (flag == 1) {
        work->field_176 += enemy->placeKey >> 12;
        work->field_186 += enemy->placeKey >> 12;
        work->field_184 += enemy->placeKey >> 12;
    } else {
        work->field_176 -= scale >> 1;
        work->field_186 -= enemy->placeKey >> 13;
        work->field_184 -= enemy->placeKey >> 13;
    }

    work->field_194 = ((Actor223600CoordPos*)((TmdObject*)task->extra)->coords)->x;
    work->field_196 = ((Actor223600CoordPos*)((TmdObject*)task->extra)->coords)->y;
    work->field_198 = ((Actor223600CoordPos*)((TmdObject*)task->extra)->coords)->z;

    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, &dir);
    dir.vy = 0;
    Actor223600_ScaleForward(&dir, 0x3E8);

    work->field_0 = 0;
    work->field_2 = -1;

    D_actor_223600_80150B5C.coord      = ((TmdObject*)task->extra)->coords;
    D_actor_223600_80150B5C.spawnArgLo = 0x100;
    D_actor_223600_80150B5C.spawnArgHi = 1;
    task->state++;
}

/// Approach state of this enemy. On the frame it is entered (`field_4` set) it
/// allocates the model's draw buffers, seeds the target position in
/// `field_19C`/`field_1A0`, writes the starting world position for this
/// context's top `field_8` nibble -- two spawn points, a third leaving the
/// coordinate alone -- faces the model down +Z and restarts its motion. On
/// every later frame it counts the frame in `field_6`, turns the model by up to
/// 0x10 towards the target (the clamped yaw kept in the scratch block) and
/// walks it 5 units forward.
void func_actor_223600_8014B840(GpEnemy* enemy, Task* task)
{
    Actor223600Work* work;
    Actor223600Turn* head;
    Actor223600Turn* turn;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    u32              mode;

    work = (Actor223600Work*)task->work;
    if (work->field_4 != 0) {
        obj               = (TmdObject*)task->extra;
        enemy->node.flags = 1;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_19C = 0x115D;
        work->field_19E = 1;
        work->field_1A0 = 0x12D5;

        mode = enemy->placeKey >> 12;
        switch (mode) {
            case 0:
                ((TmdObject*)task->extra)->coords->coord.t[0] = 0xA8C;
                ((TmdObject*)task->extra)->coords->coord.t[1] = 1;
                ((TmdObject*)task->extra)->coords->coord.t[2] = 0xA28;
                break;
            case 1:
                ((TmdObject*)task->extra)->coords->coord.t[0] = 0x384;
                ((TmdObject*)task->extra)->coords->coord.t[1] = mode;
                ((TmdObject*)task->extra)->coords->coord.t[2] = 0x960;
                break;
        }
        Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, 0, 1);
        work->field_174 = 2;
        work->field_170 = 2;
        func_actor_223600_8014B2F4(task);
        ((TmdObject*)task->extra)->coords->flg = 0;
        work->field_6                          = 0;
        return;
    }

    work->field_6++;
    head                               = *(Actor223600Turn**)G_SCRATCH_HEAD;
    head[-1].dx                        = work->field_19C - ((Actor223600CoordPos*)((TmdObject*)task->extra)->coords)->x;
    *(Actor223600Turn**)G_SCRATCH_HEAD = head - 1;
    turn                               = head - 1;
    turn->dy                           = 0;
    turn->dz                           = work->field_1A0 - ((Actor223600CoordPos*)((TmdObject*)task->extra)->coords)->z;

    coord     = ((TmdObject*)task->extra)->coords;
    turn->yaw = Actor223600_NormalizeYaw(ratan2(head[-1].dx, turn->dz) -
                                         ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    if (turn->yaw > 0x10) {
        turn->yaw = 0x10;
    }
    if (turn->yaw < -0x10) {
        turn->yaw = -0x10;
    }
    turn->yaw += ratan2(-((TmdObject*)task->extra)->coords->coord.m[2][0],
                        ((TmdObject*)task->extra)->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, turn->yaw, 1);
    Actor223600_MoveForward(((TmdObject*)task->extra)->coords, 5);
    func_actor_223600_8014B2F4(task);
    *(Actor223600Turn**)G_SCRATCH_HEAD    += 1;
    ((TmdObject*)task->extra)->coords->flg = 0;
}

/// Parked state of this enemy. On the frame it is entered (`field_4` set) it
/// allocates the model's draw buffers and drops the model at the spawn point
/// the context's top `field_8` nibble selects -- two of them just face the
/// model and hand it to motion state 2, while the third steps it forward and
/// spins up motion state 0xE, waiting out the restart until `field_58` bit 0
/// comes back. Every later frame it runs one step of the motion the work
/// block's `field_174` names: 0xE raises the model by `field_212` a frame,
/// stepping it forward while the frame counter is inside the walk window, and
/// hands over to 0xF once the model's world Y goes positive; 0xF walks the
/// coordinate along its own axes on the GTE and swings part 1 through the
/// flourish, in a longer form for the nibble-0 context than for the others;
/// state 2 only widens `field_176`. The scratch block comes off
/// `G_SCRATCH_HEAD` under three names -- `head`, whose negative index the
/// world-X step reads, `vec`, which the column and normalise calls take, and
/// `gte`, which the GTE round trip reads back -- and the two `G_SCRATCH_HEAD`
/// pointers are the carve and the release, each materialised where it is used.
void func_actor_223600_8014BBF4(GpEnemy* enemy, Task* task)
{
    Actor223600Work*  work;
    Actor223600Turn** push;
    Actor223600Turn** pop;
    Actor223600Turn*  head;
    SVECTOR*          vec;
    SVECTOR*          gte;
    TmdObject*        obj;
    s32               mode;
    s32               state;
    s16               frame;

    work = (Actor223600Work*)task->work;
    if (work->field_4 != 0) {
        obj               = (TmdObject*)task->extra;
        enemy->node.flags = 1;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        mode = enemy->placeKey >> 12;
        switch (mode) {
            case 0:
                ((TmdObject*)task->extra)->coords->coord.t[0] = 0xA1E;
                ((TmdObject*)task->extra)->coords->coord.t[1] = -0x384;
                ((TmdObject*)task->extra)->coords->coord.t[2] = 0x1590;
                Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, 0x7D0, 1);
                work->field_6   = -0xA;
                work->field_212 = 0xB4;
                work->field_174 = 2;
                work->field_170 = 2;
                break;
            case 1:
                ((TmdObject*)task->extra)->coords->coord.t[0] = 0x12C;
                ((TmdObject*)task->extra)->coords->coord.t[1] = -0x4C4;
                ((TmdObject*)task->extra)->coords->coord.t[2] = 0x1194;
                Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, 0x3E8, 1);
                work->field_212 = 0xBE;
                work->field_6   = 0;
                work->field_174 = 2;
                work->field_170 = 2;
                break;
            case 2:
                ((TmdObject*)task->extra)->coords->coord.t[0] = 0x104A;
                ((TmdObject*)task->extra)->coords->coord.t[1] = -0x384;
                ((TmdObject*)task->extra)->coords->coord.t[2] = 0xFE6;
                Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, -0x400, 1);
                Actor223600_MoveForward(((TmdObject*)task->extra)->coords, 0x15E);
                work->field_6   = 0x3C;
                work->field_212 = 0x50;
                work->field_176 = 0x40;
                work->field_174 = 0xE;
                work->field_170 = mode;
                do {
                    func_actor_223600_8014B2F4(task);
                } while ((work->field_58 & 1) == 0);
                work->field_176 = 0x10;
                work->field_212 = 0x46;
                break;
        }
        func_actor_223600_8014B2F4(task);
        ((TmdObject*)task->extra)->coords->flg = 0;
        return;
    }

    if (work->field_6 == 0) {
        state = work->field_174;
        if (state == 2) {
            work->field_174 = 0xE;
            work->field_170 = state;
        }
    }
    func_actor_223600_8014B2F4(task);

    push  = (Actor223600Turn**)G_SCRATCH_HEAD;
    head  = *push;
    vec   = (SVECTOR*)(head - 1);
    gte   = (SVECTOR*)(head - 1);
    *push = head - 1;

    switch (work->field_174) {
        case 0xE:
            work->field_176 = 0x10;
            if ((enemy->placeKey >> 12) != 2) {
                if (work->field_6 < 0x32) {
                    if (work->field_6 >= 0x28) {
                        Actor223600_MoveForward(((TmdObject*)task->extra)->coords, 0x16);
                    } else if (work->field_6 >= 0x23) {
                        Actor223600_MoveForward(((TmdObject*)task->extra)->coords, 0xA);
                    } else if (work->field_6 >= 0x13) {
                        Actor223600_MoveForward(((TmdObject*)task->extra)->coords, 0xA);
                    }
                }
            }
            frame = (u16)work->field_6;
            if (frame >= 0x28) {
                if ((u16)(frame % 5) < 2) {
                    ((TmdObject*)task->extra)->coords->coord.t[1] += work->field_212 - (frame - 0x28) / 4;
                } else {
                    ((TmdObject*)task->extra)->coords->coord.t[1] += work->field_212 + (frame - 0x28) / 2;
                }
                if (((TmdObject*)task->extra)->coords->coord.t[1] >= 2) {
                    ((TmdObject*)task->extra)->coords->coord.t[1] = 1;
                }
            }
            if (((TmdObject*)task->extra)->coords->coord.t[1] > 0) {
                work->field_174 = 0xF;
                work->field_170 = 2;
                work->field_6   = 0;
            }
            break;
        case 0xF:
            work->field_176 = 0x10;
            if (work->field_6 < 0xB) {
                Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, vec);
                VectorNormalSS(vec, vec);
                gte_lddp(0x23);
                gte_ldsv(gte);
                gte_gpf12();
                gte_stsv(gte);
                ((TmdObject*)task->extra)->coords->coord.t[0] += head[-1].dx;
                ((TmdObject*)task->extra)->coords->coord.t[1] += vec->vy;
                ((TmdObject*)task->extra)->coords->coord.t[2] += vec->vz;
            }
            if ((u32)((u16)work->field_6 - 5) < 9) {
                Gfx_MatrixCol1(&((TmdObject*)task->extra)->coords->coord, vec);
                VectorNormalSS(vec, vec);
                gte_lddp(-0x14);
                gte_ldsv(gte);
                gte_gpf12();
                gte_stsv(gte);
                ((TmdObject*)task->extra)->coords->coord.t[0] += head[-1].dx;
                ((TmdObject*)task->extra)->coords->coord.t[1] += vec->vy;
                ((TmdObject*)task->extra)->coords->coord.t[2] += vec->vz;
                Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[1].coord,
                               -((work->field_6 - 4) * 0xCC), 0);
            }
            if ((enemy->placeKey >> 12) == 0) {
                if ((u32)((u16)work->field_6 - 0xE) < 0x17) {
                    Gfx_MatrixCol1(&((TmdObject*)task->extra)->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(0xB);
                    gte_ldsv(gte);
                    gte_gpf12();
                    gte_stsv(gte);
                    ((TmdObject*)task->extra)->coords->coord.t[0] += head[-1].dx;
                    ((TmdObject*)task->extra)->coords->coord.t[1] += vec->vy;
                    ((TmdObject*)task->extra)->coords->coord.t[2] += vec->vz;
                    Gfx_MatrixCol0(&((TmdObject*)task->extra)->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(-0x1D);
                    gte_ldsv(gte);
                    gte_gpf12();
                    gte_stsv(gte);
                    switch ((s16)((u16)work->field_6 - 0xF)) {
                        case 0:
                        case 1:
                        case 3:
                        case 4:
                        case 5:
                        case 7:
                        case 9:
                            ((TmdObject*)task->extra)->coords->coord.t[0] -= gte->vx;
                            ((TmdObject*)task->extra)->coords->coord.t[1] -= gte->vy;
                            ((TmdObject*)task->extra)->coords->coord.t[2] -= gte->vz;
                            Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[1].coord, -0x800, 0);
                            Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords[1].coord,
                                           (work->field_6 - 0xD) * 0x55 - 0x6E, 0);
                            break;
                        default:
                            ((TmdObject*)task->extra)->coords->coord.t[0] += gte->vx;
                            ((TmdObject*)task->extra)->coords->coord.t[1] += gte->vy;
                            ((TmdObject*)task->extra)->coords->coord.t[2] += gte->vz;
                            Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[1].coord, -0x800, 0);
                            Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords[1].coord,
                                           (work->field_6 - 0xD) * 0x55, 0);
                            break;
                    }
                }
                if (work->field_6 >= 0x25) {
                    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[1].coord, -0x800, 0);
                    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords[1].coord, 0x800, 0);
                }
            } else {
                if ((u32)((u16)work->field_6 - 0xE) < 0x10) {
                    Gfx_MatrixCol1(&((TmdObject*)task->extra)->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(0xB);
                    gte_ldsv(gte);
                    gte_gpf12();
                    gte_stsv(gte);
                    ((TmdObject*)task->extra)->coords->coord.t[0] += head[-1].dx;
                    ((TmdObject*)task->extra)->coords->coord.t[1] += vec->vy;
                    ((TmdObject*)task->extra)->coords->coord.t[2] += vec->vz;
                    Gfx_MatrixCol0(&((TmdObject*)task->extra)->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(-0x1D);
                    gte_ldsv(gte);
                    gte_gpf12();
                    gte_stsv(gte);
                    ((TmdObject*)task->extra)->coords->coord.t[0] += head[-1].dx;
                    ((TmdObject*)task->extra)->coords->coord.t[1] += vec->vy;
                    ((TmdObject*)task->extra)->coords->coord.t[2] += vec->vz;
                    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[1].coord, -0x800, 0);
                    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords[1].coord,
                                   (work->field_6 - 0xD) * 0x78, 0);
                }
                if (work->field_6 >= 0x1E) {
                    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[1].coord, -0x800, 0);
                    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords[1].coord, 0x800, 0);
                }
            }
            break;
        case 2:
            work->field_176 = 0x20;
            break;
    }
    ((TmdObject*)task->extra)->coords->flg = 0;
    ((TmdObject*)task->extra)->coords->flg = 0;
    pop                                    = (Actor223600Turn**)G_SCRATCH_HEAD;
    *pop                                  += 1;
    work->field_6++;
}

/// The three state handlers the tick below picks between by the work block's
/// state word, copied onto the stack before the call. The copy is a three-word
/// block move out of the unit's `.rodata`, which is why the table is a rodata
/// object rather than a local initialiser.
const GpEnemyTaskFuncTable3 D_actor_223600_80149E4C = {
    {
        func_actor_223600_8014CF3C,
        func_actor_223600_8014B840,
        func_actor_223600_8014BBF4,
    },
};

/// Per-frame tick of this enemy, entry 1 of `D_actor_223600_80149E58`. The
/// game mode word selects a one-shot arm first: mode 0 clears the model's
/// `field_C` when the work block's state is nonzero and then carries on, mode 1
/// does the same and returns, and mode 2 forces `field_C` to 0x80 and returns.
/// The common path records the state change in `field_4` and the dispatched
/// state in `field_2`, runs the state handler from `D_actor_223600_80149E4C`,
/// turns the animation latch `func_actor_223600_8014B464` raises into a
/// `SndEvt_EnqueueType6` cue -- the top nibble of the enemy's `placeKey` in
/// bits 8-11, with the model's pan and depth -- and finally re-parks the model
/// through `func_800D7A9C` while `field_20C` is set, latching `field_20C` once
/// the session's `viewReady` or a dirty coordinate arrives.
void func_actor_223600_8014CA00(GpEnemy* enemy, Task* task)
{
    Actor223600Work*      work;
    GpEnemyTaskFuncTable3 fns;
    s32                   reaction;
    s32                   cue;
    s32                   pan;

    work = (Actor223600Work*)task->work;
    fns  = D_actor_223600_80149E4C;

    switch (Gp_StateF0.field_4) {
        case 0:
            if (work->field_0 != 0) {
                ((TmdObject*)task->extra)->flags = 0;
            }
            break;
        case 1:
            if (work->field_0 != 0) {
                ((TmdObject*)task->extra)->flags = 0;
            }
            return;
        case 2:
            ((TmdObject*)task->extra)->flags = 0x80;
            return;
    }

    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    fns.funcs[work->field_0](enemy, task);

    reaction = func_actor_223600_8014B464(work);
    if (reaction != 0) {
        cue = reaction | (((u16)enemy->placeKey >> 12) << 8);
        pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(
            cue, pan,
            (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
    }
    if (work->field_20C != 0) {
        func_800D7A9C((TmdObject*)task->extra,
                      (VECTOR*)((TmdObject*)task->extra)->coords->workm.t, 0, 3);
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)task->extra)->coords->flg = 0;
    }
    if (((TmdObject*)task->extra)->coords->flg == 0) {
        work->field_20C = 1;
        return;
    }
    work->field_20C = 0;
}

/// The enemy's three task states -- spawn, per-frame tick and teardown -- which
/// `func_actor_223600_8014CF6C` runs by `Task::state`.
const GpEnemyTaskFuncTable3 D_actor_223600_80149E58 = {
    {
        func_actor_223600_8014B540,
        func_actor_223600_8014CA00,
        Gp_DestroyEnemy,
    },
};

/// Message handler (id 0x7D5 in `D_actor_223600_80150B28`). Drives the model's
/// `field_C` flag word and the work block's state word from `arg2`: 0 sets 0x80
/// and rewrites the buffers, 1 clears it and rewrites the buffers, 2 sets bit
/// 2, and 3 clears then sets bit 2. Only case 1 keeps `arg2` as the state.
s32 func_actor_223600_8014CC04(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj  = task->extra;
    Actor223600Work* work = (Actor223600Work*)task->work;

    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 1;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = arg2;
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

/// Message handler (id 0x7DB in `D_actor_223600_80150B28`). Copies the first
/// three bytes of the event packet into the work block, then, for command word
/// 0x302, drives the work block's state word from the packet's sub-command: 1
/// selects 2, 2 and 9 select 0, and 0 is a no-op.
s32 func_actor_223600_8014CCD4(Task* task, s32 arg1, Actor223600Event* event)
{
    Actor223600Work* work;

    work            = (Actor223600Work*)task->work;
    work->field_180 = event->bytes[0];
    work->field_181 = event->bytes[1];
    work->field_182 = event->bytes[2];
    if (event->words[0] == 0x302) {
        switch (event->words[1]) {
            case 9:
                work->field_0 = 0;
                break;
            case 1:
                work->field_0 = 2;
                break;
            case 2:
                work->field_0 = 0;
                break;
            case 0:
                break;
        }
    }
    return 0;
}

/// Message handler (id 0x7D4 in `D_actor_223600_80150B28`). Places the model's
/// coordinate from `placement`: the three longs become the translation, the
/// X, Y and Z angles are applied in that order with `Gfx_RotMatrixX` / `Y` /
/// `Z`, and the coordinate is marked dirty. Always returns 1.
s32 func_actor_223600_8014CD54(Task* task, s32 arg1, ActorShared80135990Placement* placement)
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
void func_actor_223600_8014CE24(GsCOORDINATE2* coord, s16 scale)
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

/// Idle state of this enemy (entry 0 of `D_actor_223600_80149E4C`). On the
/// frame the state is entered (`field_4` set) it sets the display node's flags
/// to 1 and the model's flags to 0x80; it does nothing on later frames.
void func_actor_223600_8014CF3C(GpEnemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((Actor223600Work*)arg1->work)->field_4 != 0) {
        model            = (TmdObject*)arg1->extra;
        arg0->node.flags = 1;
        model->flags     = 0x80;
    }
}

/// Runs the handler of `D_actor_223600_80149E58` that `Task::state` selects --
/// spawn, per-frame tick or teardown -- on the enemy in `Task::spawnArg2`,
/// copying the table onto the stack before the call.
void func_actor_223600_8014CF6C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_223600_80149E58;
    sp.funcs[task->state](task->spawnArg2, task);
}
