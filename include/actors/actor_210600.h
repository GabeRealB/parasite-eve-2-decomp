#ifndef ACTOR_210600_H
#define ACTOR_210600_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

/// Dual-width view of the animation rate at 0x886. The message handler
/// `func_actor_210600_8014B770` arms the pair as one halfword, while the
/// seeding body copies the low byte into every slot's `GpAnimSlot.rate`,
/// the rate `Gp_AnimResetSlot` otherwise writes 0x10 into; the same
/// `word` / `half` overlap `ActorsShared80168d3cFlags` describes.
typedef union Actor210600Rate {
    /* 0x0 */ u16 half;
    /* 0x0 */ u8  byte;
} Actor210600Rate;
STATIC_ASSERT_SIZEOF(Actor210600Rate, 0x2);

/// Per-actor state block for the `actor_210600` overlay. `func_actor_210600_8014B8C8`
/// is the overlay's allocator: it calls `memCalloc(0x8D8, 0)` and stores the
/// result in `Task::work` (0x1C), which an enemy actor reuses for its own work
/// block, so it is *not* a `TaskIdMap` here. The same function hands
/// `work + 0x898` and `work + 0x8B8` to the task's own 0x1C / 0x20 slots. The
/// size below is the allocation, not a guess; only the fields this overlay's
/// matched bodies touch are named.
typedef struct Actor210600Work {
    /// Animation context the spawn body hands `func_800B3F84` as `anim`, with
    /// its 19 slots (`0x28` each) directly behind it: the pose buffer that
    /// function is handed as `poses` starts at 0x30C, exactly `0x14 + 19 *
    /// 0x28`, the same pack `Actor206100Work` and `Actor151000Work` carry.
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x13];
    /* 0x30C */ byte       field_30C[0x570];
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
    /// Clip id of slot 0 the update body last saw, masked to 12 bits and kept
    /// so the once-per-clip effect is not respawned while it is held; the same
    /// slot `Actor110600AnimWork::field_8AC` remembers its cue in.
    /* 0x896 */ s16 field_896;
    /// The light / colour matrices the spawn body stores into
    /// `TmdObject::lightMtx` / `field_20`. They sit at the top of the block
    /// rather than at its head, so the overlay's actor keeps its animation
    /// state in the first half of the allocation.
    /* 0x898 */ MATRIX light;
    /* 0x8B8 */ MATRIX color;
} Actor210600Work;
STATIC_ASSERT_SIZEOF(Actor210600Work, 0x8D8);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor143900Msg` and `Actor560800Msg`.
typedef struct Actor210600Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor210600Msg;
STATIC_ASSERT_SIZEOF(Actor210600Msg, 0x4);

/// State table the overlay dispatches through, indexed by `Task::state`. The
/// rodata object `D_actor_210600_80149E24` is exactly its 3 words: create
/// (0x8014B8C8), update (0x8014B434) and `Gp_DestroyEnemy`. Both handlers take
/// the task's 0x20 spawn argument first, like `Actor100300StateFuncTable3`.
typedef struct Actor210600StateFuncTable3 {
    void (*funcs[3])(void* spawnArg2, Task* task);
} Actor210600StateFuncTable3;
STATIC_ASSERT_SIZEOF(Actor210600StateFuncTable3, 0xC);

extern Actor210600StateFuncTable3 D_actor_210600_80149E24;

/// Step table the seeding body `func_actor_210600_8014B2C0` walks: one 5-byte
/// row per clip the previous request latched in `Actor210600Work::field_880`,
/// addressed by the requested clip in `field_882`. The byte it reads is handed
/// to `func_800B4114` as the request's fifth argument.
extern s8 D_actor_210600_8015A498[][5];

/// Stack copy `func_actor_210600_8014BA3C` makes before the indirect call.
/// The copy itself moves only the 3 words of `D_actor_210600_80149E24`, but
/// the dispatcher's frame is 0x30 with `$ra` at 0x28, which needs 17-24 bytes
/// of locals - so the copy target is this larger record and not the table
/// type. Only `table` is written here; the trailing fields are unread, and
/// `actor_521100`'s dispatcher builds the same 20-byte table-plus-context
/// shape.
typedef struct Actor210600DispatchCtx {
    /* 0x00 */ Actor210600StateFuncTable3 table;
    /* 0x0C */ s32                        field_C;
    /* 0x10 */ s32                        field_10;
} Actor210600DispatchCtx;
STATIC_ASSERT_SIZEOF(Actor210600DispatchCtx, 0x14);

/// Payload of message 0x7D4: the position and orientation the actor is placed
/// at. The three longs become the model root's translation and the three
/// shorts its X / Y / Z Euler angles.
typedef struct Actor210600Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor210600Placement;

/// 0x34-byte block borrowed from `G_SCRATCH_HEAD` while the model root's
/// rotation is rebuilt: the rotation matrix, the uniform scale handed to
/// `ScaleMatrix`, and the yaw it was rebuilt from.
typedef struct Actor210600Scratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ s16    pad_32;
} Actor210600Scratch;
STATIC_ASSERT_SIZEOF(Actor210600Scratch, 0x34);

/// 0x88-byte block the repel helpers take from `G_SCRATCH_HEAD`: `pos` is the
/// coordinate's world translation, `offset` the latest push-out (scaled down
/// to length 0x100 at the end), `last` its XZ copy, `i` the record cursor and
/// `hit` the return value; `dist` takes 0x7FFE at the record that ends the
/// table.
typedef struct Actor210600RepelScratch {
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
} Actor210600RepelScratch;
STATIC_ASSERT_SIZEOF(Actor210600RepelScratch, 0x88);

/// 0x54-byte block the avoid helpers take from `G_SCRATCH_HEAD` while they
/// steer a coordinate away from the contact records: `angle` / `ok` hold up to
/// eight obstacle bearings and whether each still counts, `dir` the facing
/// column and later each step, `eye` the coordinate's world position, `face`
/// its heading, `i` / `j` the loop cursors and `blocked` the result.
typedef struct Actor210600AvoidScratch {
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
} Actor210600AvoidScratch;
STATIC_ASSERT_SIZEOF(Actor210600AvoidScratch, 0x54);

/// 0x10-byte block the bearing helpers carve below the scratch head: an
/// obstacle's offset from the eye, widened to words.
typedef struct Actor210600AvoidDelta {
    /* 0x0 */ s32  vx;
    /* 0x4 */ s32  vy;
    /* 0x8 */ s32  vz;
    /* 0xC */ byte pad_C[0x4];
} Actor210600AvoidDelta;
STATIC_ASSERT_SIZEOF(Actor210600AvoidDelta, 0x10);

/// 0x14-byte block the movement-step helpers take from `G_SCRATCH_HEAD`: the
/// delta `func_800E0C10` resolves from the movement records, plus the "moved"
/// flag the helper returns.
typedef struct Actor210600DeltaFlag {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ s32            field_10;
} Actor210600DeltaFlag;
STATIC_ASSERT_SIZEOF(Actor210600DeltaFlag, 0x14);

/// 0x20-byte block the world-space walk takes from `G_SCRATCH_HEAD`: `coord`
/// is the frame the walk currently stands on (it climbs the
/// `GsCOORDINATE2::sub` chain until NULL), `vec` the vector being carried up,
/// `out` the GTE result fed back into `vec` each step, and `flag` the GTE flag
/// register.
typedef struct Actor210600Walk {
    /* 0x00 */ GsCOORDINATE2* coord;
    /* 0x04 */ SVECTOR        vec;
    /* 0x0C */ s32            out[3];
    /* 0x18 */ s32            pad_18;
    /* 0x1C */ s32            flag;
} Actor210600Walk;
STATIC_ASSERT_SIZEOF(Actor210600Walk, 0x20);

/// 0xE4-byte block the push helpers take from `G_SCRATCH_HEAD` while they
/// nudge a coordinate away from the obstacles in a `GpRec18` table. `m` is
/// the working matrix, `eye` the frame's world position and `aim` the world
/// point one unit (0x1000) in front of it; `delta` is the difference fed to
/// `ratan2` and later the scaled push. `kind` is a record's key high half,
/// `angle[]` each record's bearing relative to the facing (0x7FFE marks the
/// end of the records, 0x7FFF a record that does not count), `i` / `j` the
/// loop counters, `diff` the wrapped difference between two bearings and
/// `hit` the return value.
typedef struct Actor210600PushScratch {
    /* 0x00 */ MATRIX  m;
    /* 0x20 */ byte    pad_20[0x80];
    /* 0xA0 */ SVECTOR delta;
    /* 0xA8 */ SVECTOR eye;
    /* 0xB0 */ SVECTOR aim;
    /* 0xB8 */ s32     kind;
    /* 0xBC */ s16     angle[0x10];
    /* 0xDC */ s16     i;
    /* 0xDE */ s16     j;
    /* 0xE0 */ s16     diff;
    /* 0xE2 */ s16     hit;
} Actor210600PushScratch;
STATIC_ASSERT_SIZEOF(Actor210600PushScratch, 0xE4);

/// While this is 1, the repel and avoid helpers return without moving
/// anything.
extern u8 D_80072729;

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// Builds `joint`'s absolute rotation in `out`: its own rotation, then each
/// ancestor pre-multiplied in turn (renormalised after every step) up to but
/// not including `stop`. Returns whether the walk reached `stop` rather than
/// the end of the chain.
static __inline__ s32 Actor210600_AccumulateRotation(GsCOORDINATE2* joint, MATRIX* out, GsCOORDINATE2* stop)
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
static __inline__ GsCOORDINATE2* Actor210600_LocalizeRotation(GsCOORDINATE2* joint, MATRIX* rotation)
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

/// XZ push-out of `pos` from one obstacle record: the record's radius minus
/// the horizontal distance to its point, floored at zero, applied along the
/// direction from the point to `pos` taken into grid space.
static __inline__ void Actor210600_CalcPush(SVECTOR* pos, GpRec18* rec, SVECTOR* out)
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

/// Bearing of `p` from `eye` in the XZ plane.
static __inline__ s16 Actor210600_BearingXZ(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                    head;
    Actor210600AvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (Actor210600AvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vz);
}

/// Bearing of `p` from `eye` in the XY plane, used when the coordinate's
/// facing is close to vertical.
static __inline__ s16 Actor210600_BearingXY(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                    head;
    Actor210600AvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (Actor210600AvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vy);
}

/// Carries `v` from the local frame `coord` up the `GsCOORDINATE2::sub` parent
/// chain into world space, using an `Actor210600Walk` block from
/// `G_SCRATCH_HEAD`.
static __inline__ void Actor210600_ToWorld(GsCOORDINATE2* coord, SVECTOR* v)
{
    Actor210600Walk* blk;

    {
        register GsCOORDINATE2* parent asm("v0");
        parent                                                                              = coord;
        ((Actor210600Walk*)((u8*)*(void**)G_SCRATCH_HEAD - sizeof(Actor210600Walk)))->coord = parent;
    }
    {
        register u8* tmp asm("v0");
        tmp = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(Actor210600Walk);
        blk = (Actor210600Walk*)tmp;
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

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Actor210600Walk);
}

/// The same walk as `Actor210600_ToWorld`, spelled without its register
/// bindings; each caller site needs its own form to match.
static __inline__ void Actor210600_ToWorld2(GsCOORDINATE2* coord, SVECTOR* v)
{
    Actor210600Walk* blk;

    blk         = (Actor210600Walk*)((u8*)*(void**)G_SCRATCH_HEAD - sizeof(Actor210600Walk));
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

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Actor210600Walk);
}

/// Spawn body: allocates the actor's 0x8D8-byte `Actor210600Work`, hands it to
/// the task, and seeds the enemy object, the model's root coordinate and the
/// animation context from the `TmdObject` in `Task::extra`. `enemy` is the
/// `GpEnemy` the spawner left in the task's 0x20 spawn-argument slot.
void func_actor_210600_8014B8C8(GpEnemy* enemy, Task* task);

s32 func_actor_210600_8014B770(Task* task, s32 msgId, Actor210600Msg* msg);

/// Display-object mode handler: 0 hides the object, 1 shows it, 2 and any
/// other value set bit 0x4, with modes 0 and 1 reinstating the object's
/// buffers and modes 0 and 2 arming `Actor210600Work::field_890`. `arg1` is
/// unused; it exists because the dispatch passes three arguments.
s32 func_actor_210600_8014B5F4(Task* task, s32 arg1, s32 arg2);

#endif // ACTOR_210600_H
