#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actors_shared_80135990.h"
#include "actors/actors_shared_80135a60.h"
#include "rooms/rooms_shared_80182078.h"

/// The overlay's *other* work block, for the task `func_actor_341700_8016D130`
/// starts: that function calls `memCalloc(0x80, 0)` and stores the result in
/// the same `Task::work` slot, so the two blocks never coexist on one task.
///
/// `field_0` is the state index `func_actor_341700_8016CC9C` dispatches
/// through its three-entry handler table; `field_2` holds the previous value
/// and `field_4` the state-change flag. `func_actor_341700_8016CEB4` copies
/// the three leading bytes of an incoming command over `field_18` .. `field_1A`.
///
/// `light` / `color` are the matrices this block is allocated for:
/// `func_actor_341700_8016D130` stores their addresses into the model's
/// `TmdObject.lightMtx` / `field_20` light and colour matrix slots, so the
/// actor rasterises through its own work block rather than a separate
/// `MATRIX` allocation.
typedef struct Actor341700SubWork {
    /* 0x00 */ s16    field_0;
    /* 0x02 */ s16    field_2;
    /* 0x04 */ s16    field_4;
    /* 0x06 */ s16    field_6;
    /* 0x08 */ byte   pad_8[0x10];
    /* 0x18 */ u8     field_18;
    /* 0x19 */ u8     field_19;
    /* 0x1A */ u8     field_1A;
    /* 0x1B */ byte   pad_1B[0x1];
    /* 0x1C */ MATRIX light;
    /* 0x3C */ MATRIX color;
    /* 0x5C */ byte   pad_5C[0x24];
} Actor341700SubWork;
STATIC_ASSERT_SIZEOF(Actor341700SubWork, 0x80);

/// The three leading bytes of a command packet, as `func_actor_341700_8016CEB4`
/// copies them into the work block.
typedef struct Actor341700CmdBytes {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
} Actor341700CmdBytes;

/// A command packet an actor task receives. The same four bytes are read both
/// byte-wise (the copy above) and as halfwords — offset 0 is the opcode the
/// handler tests against 0x2704 and offset 2 the sub-command it switches on —
/// so the two views are modelled here rather than cast at the use sites.
typedef union Actor341700Cmd {
    /* 0x0 */ Actor341700CmdBytes bytes;
    /* 0x0 */ u16                 halfs[2];
} Actor341700Cmd;
STATIC_ASSERT_SIZEOF(Actor341700Cmd, 0x4);

/// 0x88-byte `G_SCRATCH_HEAD` block `func_actor_341700_8016AF70` takes while it
/// works out the push that moves a coordinate out of the contact records:
/// `pos` is the coordinate's world translation, `offset` the latest push
/// (scaled down to length 0x100 when longer), `last` its XZ copy, `i` the
/// record cursor and `hit` the result. `dist` gets 0x7FFE at the terminating
/// record.
typedef struct Actor341700RepelScratch {
    /* 0x00 */ byte    pad_0[0x20];
    /* 0x20 */ SVECTOR offset;
    /* 0x28 */ SVECTOR last;
    /* 0x30 */ SVECTOR pos;
    /* 0x38 */ s32     kind;
    /* 0x3C */ u32     len;
    /* 0x40 */ s16     dist[32];
    /* 0x80 */ s16     i;
    /* 0x82 */ byte    pad_82[4];
    /* 0x86 */ s16     hit;
} Actor341700RepelScratch;
STATIC_ASSERT_SIZEOF(Actor341700RepelScratch, 0x88);

/// 0x54-byte `G_SCRATCH_HEAD` block `func_actor_341700_8016B2B8` takes while it
/// steers a coordinate away from the contact records: `angle` / `ok` hold up
/// to eight obstacle bearings and whether each still counts, `dir` the facing
/// column and later each step, `eye` the coordinate's world position, `face`
/// its heading, `i` / `j` the loop cursors and `blocked` the result.
typedef struct Actor341700AvoidScratch {
    /* 0x00 */ MATRIX   m;
    /* 0x20 */ SVECTOR  dir;
    /* 0x28 */ SVECTOR3 eye;
    /* 0x2E */ byte     pad_2E[0x2];
    /* 0x30 */ s32      kind;
    /* 0x34 */ s16      angle[8];
    /* 0x44 */ s8       ok[8];
    /* 0x4C */ s16      face;
    /* 0x4E */ s16      diff;
    /* 0x50 */ u8       i;
    /* 0x51 */ u8       j;
    /* 0x52 */ u8       count;
    /* 0x53 */ u8       blocked;
} Actor341700AvoidScratch;
STATIC_ASSERT_SIZEOF(Actor341700AvoidScratch, 0x54);

/// 0x10-byte block the bearing helpers of `func_actor_341700_8016B2B8` carve
/// below the scratch head: an obstacle's offset from the eye, widened to words.
typedef struct Actor341700AvoidDelta {
    /* 0x0 */ s32  vx;
    /* 0x4 */ s32  vy;
    /* 0x8 */ s32  vz;
    /* 0xC */ byte pad_C[0x4];
} Actor341700AvoidDelta;
STATIC_ASSERT_SIZEOF(Actor341700AvoidDelta, 0x10);

/// 0x14-byte `G_SCRATCH_HEAD` block `func_actor_341700_8016B804` gives
/// `func_800E0C10`: the `GpDeltaScratch` it fills plus the returned flag, set
/// when the X or Z delta is nonzero.
typedef struct Actor341700DeltaFlag {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ s32            field_10;
} Actor341700DeltaFlag;
STATIC_ASSERT_SIZEOF(Actor341700DeltaFlag, 0x14);

/// Whole-unit part of the last movement step `func_actor_341700_8016B804`
/// applied, rounded away from zero when the step had a fraction.
extern SVECTOR D_actor_341700_80176360;

/// Global render mode: 2 hides the model, 1 only colours it, 0 runs the
/// state handler as well.
extern u8 D_801153F4;

extern u32 Gp_LcgState;

/// Main-executable byte; while it is 1 the contact-record push and avoid
/// helpers return at once without touching the coordinate.
extern u8 D_80072729;

extern SVECTOR D_actor_341700_80175F7C[];
extern u8      D_actor_341700_801760FC[];
extern u8      D_actor_341700_80175F5C[]; // stored into `Task::msgTable` by func_actor_341700_8016D130

/// Psy-Q `RotMatrixY`.
void func_8004BFF8(s32 angle, MATRIX* matrix);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

void func_8017FCF4(GsCOORDINATE2* coord, SVECTOR* vec);

void func_actor_341700_8016D130(GpEnemy* arg0, Task* arg1);
void func_actor_341700_8016D2B8(GpEnemy* arg0, Task* arg1);
void func_actor_341700_8016D2E8(GpEnemy* arg0, Task* arg1);

/// Builds `joint`'s absolute rotation in `out`: its own rotation, then each
/// ancestor pre-multiplied in turn (renormalised after every step) up to but
/// not including `stop`. Returns whether the walk reached `stop` rather than
/// the end of the chain.
static __inline__ s32 Actor341700_AccumulateRotation(GsCOORDINATE2* joint, MATRIX* out, GsCOORDINATE2* stop)
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
static __inline__ GsCOORDINATE2* Actor341700_LocalizeRotation(GsCOORDINATE2* joint, MATRIX* rotation)
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
void func_actor_341700_8016AC64(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    Actor341700_AccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = Actor341700_LocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}

/// Push-out of `pos` from contact record `rec`: how far it sits inside the
/// record's radius (`depth`), along the direction from the record's centre,
/// carried into grid space. Only X and Z are written.
static __inline__ void Actor341700_CalcPush(SVECTOR* pos, GpRec18* rec, SVECTOR* out)
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

/// Walks the first `count` contact records (stopping at a zero key) and keeps,
/// in a scratch block carved off `G_SCRATCH_HEAD`, the push that would move
/// `coord` out of the last record of kind 0x10000 or 0x30000, scaled down to
/// 0x100 units when longer. Returns whether any such record was found; returns
/// 0 at once when `gGameSession->viewReady` or `D_80072729` is 1.
s32 func_actor_341700_8016AF70(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
{
    Actor341700RepelScratch* head;
    Actor341700RepelScratch* s;
    Actor341700RepelScratch* blk;
    SVECTOR*                 offset;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    coord->flg                                 = 0;
    head                                       = *(Actor341700RepelScratch**)G_SCRATCH_HEAD;
    blk                                        = head - 1;
    *(Actor341700RepelScratch**)G_SCRATCH_HEAD = blk;
    s                                          = blk;
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
            Actor341700_CalcPush(&s->pos, &recs[s->i], &s->offset);
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
    coord->flg                                  = 0;
    *(Actor341700RepelScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}

/// Bearing of `p` from `eye` in the XZ plane, staged in a scratch block of its
/// own that is released before `ratan2` runs.
static __inline__ s16 Actor341700_BearingXZ(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                    head;
    Actor341700AvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (Actor341700AvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vz);
}

/// Bearing of `p` from `eye` in the XY plane; used when the facing column is
/// close to vertical.
static __inline__ s16 Actor341700_BearingXY(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                    head;
    Actor341700AvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (Actor341700AvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vy);
}

/// Steers `coord` away from the obstacles among the first `count` contact
/// records: collects the bearing of up to eight records of kind 0x10000 or
/// 0x30000 (in the XZ plane, or XY when the facing column is near vertical),
/// discards any pair more than 0x400 apart, and for each remaining bearing
/// nudges both `coord`'s translation and `*pos` a short step away from it.
/// `*pos` accumulates the total nudge. Returns whether any record was of kind
/// 0x10000; returns 0 at once when `gGameSession->viewReady` or `D_80072729`
/// is 1.
s32 func_actor_341700_8016B2B8(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
{
    u8*                      head;
    Actor341700AvoidScratch* s;
    s16                      diff;
    s16                      t;
    s32                      mag;

    if (gGameSession->viewReady == 1 || D_80072729 == 1) {
        return 0;
    }

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(Actor341700AvoidScratch);
    s                     = (Actor341700AvoidScratch*)*(u8**)G_SCRATCH_HEAD;
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
            s->angle[s->count] = Actor341700_BearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = Actor341700_BearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
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

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + sizeof(Actor341700AvoidScratch);
    return s->blocked != 0;
}

/// Steps `coord` by the movement the first `arg2` `GpRec18` records of
/// `movement` resolve to, and keeps the whole-unit part of that step in
/// `D_actor_341700_80176360`. Returns 1 when the X or Z step is nonzero; a
/// step with a fractional part moves the coordinate and the kept step one
/// unit further from zero.
s32 func_actor_341700_8016B804(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
{
    void**                scratch;
    u8*                   head;
    Actor341700DeltaFlag* s;
    register void*        p asm("v1");
    s32                   val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((Actor341700DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_341700_80176360.vx = ((Actor341700DeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_341700_80176360.vy = s->delta.vy.w >> 16;
        D_actor_341700_80176360.vz = s->delta.vz.w >> 16;
        val                        = ((Actor341700DeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_341700_80176360.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_341700_80176360.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_341700_80176360.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_341700_80176360.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}

/// Carries `v` from the local frame `coord` up the `GsCOORDINATE2::sub` parent
/// chain into world space, using a 0x20 scratch block from `G_SCRATCH_HEAD`.
static __inline__ void Actor341700_ToWorld(GsCOORDINATE2* coord, SVECTOR* v)
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

/// The same walk as `Actor341700_ToWorld`, spelled without its register
/// bindings; each caller site needs its own form to match.
static __inline__ void Actor341700_ToWorld2(GsCOORDINATE2* coord, SVECTOR* v)
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
s32 func_actor_341700_8016B9A8(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push)
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

    Actor341700_ToWorld(coord->sub, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    Actor341700_ToWorld2(coord, &st->aim);

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

void func_actor_341700_8016C0F4(GpEnemy* arg0, Task* arg1)
{
    SVECTOR             vec;
    Actor341700SubWork* work = (Actor341700SubWork*)arg1->work;
    s16                 i;
    s16                 r;
    u32                 t;
    SVECTOR*            table;
    u8*                 p;
    u32                 rnd;
    u32                 index;
    SVECTOR*            vecPtr;
    u8*                 indices;

    if (work->field_4 != 0) {
        work->field_6                                 = 0;
        ((TmdObject*)arg1->extra)->coords->coord.t[1] = 0x708;
    }
    work->field_6++;
    if (((TmdObject*)arg1->extra)->coords->coord.t[1] > 200) {
        if (work->field_6 >= 0xA6) {
            if (work->field_6 % 4 < 2) {
                ((TmdObject*)arg1->extra)->coords->coord.t[1] += 90;
            } else {
                ((TmdObject*)arg1->extra)->coords->coord.t[1] -= 100;
            }
            if (work->field_6 % 6 < 3) {
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[1].coord, 12, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[2].coord, 24, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[3].coord, -24, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[4].coord, 12, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[5].coord, -6, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[6].coord, 18, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[8].coord, -6, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[9].coord, -3, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[10].coord, -27, 0);
            } else {
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[1].coord, -12, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[2].coord, -24, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[3].coord, 24, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[4].coord, -12, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[5].coord, 6, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[6].coord, -18, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[8].coord, 6, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[9].coord, 3, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[10].coord, 27, 0);
            }
        } else if (work->field_6 >= 0x79) {
            if (work->field_6 % 4 < 2) {
                ((TmdObject*)arg1->extra)->coords->coord.t[1] += 50;
            } else {
                ((TmdObject*)arg1->extra)->coords->coord.t[1] -= 58;
            }
            if (work->field_6 % 6 < 3) {
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[1].coord, 8, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[2].coord, 16, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[3].coord, -16, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[4].coord, 8, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[5].coord, -4, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[6].coord, 12, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[8].coord, -4, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[9].coord, -2, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[10].coord, -18, 0);
            } else {
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[1].coord, -8, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[2].coord, -16, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[3].coord, 16, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[4].coord, -8, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[5].coord, 4, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[6].coord, -12, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[8].coord, 4, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[9].coord, 2, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[10].coord, 18, 0);
            }
        } else if (work->field_6 >= 11 && work->field_6 < 18) {
            if (work->field_6 % 4 < 2) {
                ((TmdObject*)arg1->extra)->coords->coord.t[1] += 30;
            } else {
                ((TmdObject*)arg1->extra)->coords->coord.t[1] -= 33;
            }
            if (work->field_6 % 6 < 3) {
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[1].coord, 4, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[2].coord, 8, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[3].coord, -8, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[4].coord, 4, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[5].coord, -2, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[6].coord, 6, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[8].coord, -2, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[9].coord, -1, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[10].coord, -9, 0);
            } else {
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[1].coord, -4, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[2].coord, -8, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[3].coord, 8, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[4].coord, -4, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[5].coord, 2, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[6].coord, -6, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[8].coord, 2, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[9].coord, 1, 0);
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[10].coord, 9, 0);
            }
            switch ((work->field_6 - 11) % 8) {
                case 0:
                    vec = D_actor_341700_80175F7C[35];
                    func_8017FCF4(((TmdObject*)arg1->extra)->coords, &vec);
                    break;
                case 1:
                    vec = D_actor_341700_80175F7C[8];
                    func_8017FCF4(((TmdObject*)arg1->extra)->coords, &vec);
                    break;
                case 2:
                    vec = D_actor_341700_80175F7C[25];
                    func_8017FCF4(((TmdObject*)arg1->extra)->coords, &vec);
                    break;
                case 4:
                    vec = D_actor_341700_80175F7C[24];
                    func_8017FCF4(((TmdObject*)arg1->extra)->coords, &vec);
                    break;
                case 5:
                    vec = D_actor_341700_80175F7C[26];
                    func_8017FCF4(((TmdObject*)arg1->extra)->coords, &vec);
                    break;
            }
        }
    } else {
        if (work->field_6 % 4 < 2) {
            ((TmdObject*)arg1->extra)->coords->coord.t[1] += 100;
        } else {
            ((TmdObject*)arg1->extra)->coords->coord.t[1] -= 100;
        }
        if (work->field_6 % 6 < 3) {
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[1].coord, 12, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[2].coord, 24, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[3].coord, -24, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[4].coord, 12, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[5].coord, -6, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[6].coord, 18, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[8].coord, -6, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[9].coord, -3, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[10].coord, -27, 0);
        } else {
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[1].coord, -12, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[2].coord, -24, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[3].coord, 24, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[4].coord, -12, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[5].coord, 6, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[6].coord, -18, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[8].coord, 6, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[9].coord, 3, 0);
            Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords[10].coord, 27, 0);
        }
    }
    ((TmdObject*)arg1->extra)->coords[0].flg  = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg  = 0;
    ((TmdObject*)arg1->extra)->coords[2].flg  = 0;
    ((TmdObject*)arg1->extra)->coords[3].flg  = 0;
    ((TmdObject*)arg1->extra)->coords[4].flg  = 0;
    ((TmdObject*)arg1->extra)->coords[5].flg  = 0;
    ((TmdObject*)arg1->extra)->coords[6].flg  = 0;
    ((TmdObject*)arg1->extra)->coords[7].flg  = 0;
    ((TmdObject*)arg1->extra)->coords[8].flg  = 0;
    ((TmdObject*)arg1->extra)->coords[9].flg  = 0;
    ((TmdObject*)arg1->extra)->coords[10].flg = 0;
    i                                         = 0;
    indices                                   = D_actor_341700_801760FC;
    table                                     = D_actor_341700_80175F7C;
    vecPtr                                    = &vec;
    for (; i < 4; i++) {
        if (work->field_6 > 240) {
            return;
        }
        if (work->field_6 < 120) {
            return;
        }
        p = (u8*)((i + (work->field_6 - 120) * 4) + (u32)indices);
        if (*p == 0) {
            return;
        }
        rnd   = Gp_LcgState * 5 + 0x71357911;
        t     = (rnd >> 16) & 0xFF;
        index = *p;
        SOFT_TOUCH_REG_USE(t, index);
        r           = t;
        Gp_LcgState = rnd;
        TOUCH_REG(r);
        vec = table[index];
        if (r & 1) {
            r = -t;
        }
        vec.vx += r / 2;
        vec.vy -= r;
        vec.vz += r;
        func_8017FCF4(((TmdObject*)arg1->extra)->coords, vecPtr);
    }
}

/// Three state handlers, indexed by `Actor341700SubWork::field_0`; copied onto
/// the stack before dispatch.
const GpEnemyTaskFuncTable3 D_actor_341700_80162058 = { {
    func_actor_341700_8016D2B8,
    func_actor_341700_8016D2E8,
    func_actor_341700_8016C0F4,
} };

/// Per-frame callback of the `func_actor_341700_8016D130` task. It colours the
/// model from the world position of its *second* attach coordinate and then,
/// unless `D_801153F4` hides the model, runs the handler `Actor341700SubWork::
/// field_0` names.
///
/// `case 0` is folded into `default` on purpose. The two bodies are the same,
/// so the case list keeps three nodes and GCC's tree tests `case 1` at the
/// root; dropping the case makes `case 2` the root and the emitted branches
/// come out with the wrong polarity and a stray low-bound test.
void func_actor_341700_8016CC9C(GpEnemy* arg0, Task* arg1)
{
    VECTOR                block;
    Actor341700SubWork*   work = (Actor341700SubWork*)arg1->work;
    GpEnemyTaskFuncTable3 sp   = D_actor_341700_80162058;

    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
    block.vx = ((TmdObject*)arg1->extra)->coords[1].workm.t[0];
    block.vy = ((TmdObject*)arg1->extra)->coords[1].workm.t[1];
    block.vz = ((TmdObject*)arg1->extra)->coords[1].workm.t[2];
    Gp_UpdateActorColor(arg0, &block, 0, 0);
    switch (D_801153F4) {
        case 2:
            ((TmdObject*)arg1->extra)->flags |= 0x80;
            return;
        case 1:
            return;
        case 0:
        default:
            if (work->field_2 != work->field_0) {
                work->field_4 = 1;
            } else {
                work->field_4 = 0;
            }
            work->field_2 = work->field_0;
            sp.funcs[work->field_0](arg0, arg1);
            if (gGameSession->viewReady != 0) {
                ((TmdObject*)arg1->extra)->coords->flg = 0;
            }
            return;
    }
}

/// Task-state handlers of the `func_actor_341700_8016D130` task: set-up, the
/// per-frame callback, teardown. `func_actor_341700_8016D32C` dispatches them
/// on `Task::state`.
const GpEnemyTaskFuncTable3 D_actor_341700_80162064 = { {
    func_actor_341700_8016D130,
    func_actor_341700_8016CC9C,
    Gp_DestroyEnemy,
} };

s32 func_actor_341700_8016CE28(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj = task->extra;

    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            break;
        case 2:
            obj->flags |= 4;
            break;
        case 3:
            obj->flags = 4;
            break;
    }
    return 0;
}

/// The `0x2704` command handler, reached through the task's `Task::msgTable`
/// table (`D_actor_341700_80175F5C`): the three leading bytes of `cmd` are
/// copied over `Actor341700SubWork::field_18` .. `field_1A` and the second
/// halfword, when the opcode matches, picks the state the work block moves to.
///
/// `case 2` is folded into `default` on purpose. The two bodies are the same,
/// so the case list keeps three nodes and GCC's decision tree balances around
/// `case 1`; dropping `case 2` makes `case 0` the root and the emitted branches
/// come out in a different order.
s32 func_actor_341700_8016CEB4(Task* task, s32 arg1, Actor341700Cmd* cmd)
{
    Actor341700SubWork* work = (Actor341700SubWork*)task->work;

    work->field_18 = cmd->bytes.field_0;
    work->field_19 = cmd->bytes.field_1;
    work->field_1A = cmd->bytes.field_2;

    if (cmd->halfs[0] == 0x2704) {
        switch (cmd->halfs[1]) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 1:
                ((TmdObject*)task->extra)->coords->flg = 0;
                work->field_0                          = 2;
                break;
            case 2:
            default:
                work->field_0 = 0;
                task->state   = 1;
                break;
        }
    }
    return 1;
}

/// Seeds the task's `TmdObject` root coordinate from `placement`: the three
/// longs become the translation, then pitch / yaw / roll are applied with
/// `Gfx_RotMatrixX` / `Y` / `Z`, re-fetching the coordinate for every field,
/// and the coordinate is marked dirty. Always returns 1.
s32 func_actor_341700_8016CF48(Task* task, s32 arg1, ActorShared80135990Placement* placement)
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

/// Rebuilds `coord`'s Y rotation from its current yaw (`ratan2` of
/// `-m[2][0], m[2][2]`), uniformly scaled by `scale`, through a 0x34-byte
/// block borrowed from the scratchpad. Marks the coordinate dirty.
void func_actor_341700_8016D018(GsCOORDINATE2* coord, s16 scale)
{
    void**                      scratch;
    void*                       head;
    ActorShared80135a60Scratch* blk;
    s16                         ang;
    u16                         m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (ActorShared80135a60Scratch*)((u8*)head - 0x34);
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((ActorShared80135a60Scratch*)((u8*)head - 0x34))->m.m[0][0];
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

void func_actor_341700_8016D130(GpEnemy* arg0, Task* arg1)
{
    Actor341700SubWork* work;
    TaskIdMap*          idMap;
    TmdObject*          model;
    GsCOORDINATE2*      coord;
    VECTOR              block;

    model      = (TmdObject*)arg1->extra;
    coord      = model->coords;
    idMap      = memCalloc(0x80U, false);
    work       = (Actor341700SubWork*)idMap;
    arg1->work = idMap;
    if (idMap == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    coord->sub                                    = &gGfxViewCoord;
    ((TmdObject*)arg1->extra)->coords->coord.t[1] = 0;
    ((TmdObject*)arg1->extra)->coords->coord.t[0] = 0x1388;
    ((TmdObject*)arg1->extra)->coords->coord.t[2] = -0x1770;
    ((TmdObject*)arg1->extra)->coords->flg        = 0;
    arg1->msgTable                                = D_actor_341700_80175F5C;
    arg0->field_4                                 = &coord->coord;
    arg0->field_48                                = 0;
    arg0->bodyPos.vx                              = 0;
    arg0->bodyPos.vy                              = 0;
    arg0->bodyPos.vz                              = 0;
    arg0->coord                                   = &((TmdObject*)arg1->extra)->coords[2];
    arg0->node.flags                              = 1;
    arg0->reactionFlags                           = 0;
    arg0->hpMax                                   = 0;
    arg0->hp                                      = 0;
    model->lightMtx                               = &work->light;
    model->colorMtx                               = &work->color;
    coord->flg                                    = 0;
    Gp_UpdateCoord(coord);
    block.vx = coord->workm.t[0];
    block.vy = coord->workm.t[1];
    block.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &block, 0, 0);
    work->field_2 = -1;
    work->field_0 = 1;
    arg1->state  += 1;
}

void func_actor_341700_8016D2B8(GpEnemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((Actor341700SubWork*)arg1->work)->field_4 != 0) {
        model            = (TmdObject*)arg1->extra;
        arg0->node.flags = 1;
        model->flags     = 0x84;
    }
}

void func_actor_341700_8016D2E8(GpEnemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((Actor341700SubWork*)arg1->work)->field_4 != 0) {
        model            = (TmdObject*)arg1->extra;
        arg0->node.flags = 1;
        model->flags     = 0;
        Tmd_AllocBuffers(model);
    }
}

/// Runs the controlled enemy's current state handler from
/// `D_actor_341700_80162064` - spawn/setup, per-frame tick or teardown -
/// copying the table onto the stack before the call.
void func_actor_341700_8016D32C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_341700_80162064;
    sp.funcs[task->state](task->spawnArg2, task);
}
