#ifndef ACTORS_ACTOR_H
#define ACTORS_ACTOR_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "rooms/rooms_shared_80182078.h"

/*
 * Types and helpers that the actor overlays each carry a copy of.
 *
 * Every actor overlay is linked on its own, so code the actors share was
 * compiled into each of them. The layouts below are the ones several actors
 * repeat field for field; one declaration serves all of them.
 */

/* Matrix views. */

/// The leading rotation entries of a `MATRIX`, paired into words. Actors
/// reset a rotation to identity through it: five aligned word stores instead
/// of nine halfword ones, where the pairs whose halves are both 0x1000 or
/// both zero fold into one store.
typedef struct ActorMatWords {
    s32 m00_m01;
    s32 m02_m10;
    s32 m11_m12;
    s32 m20_m21;
    s16 m22;
} ActorMatWords;

/// A `MATRIX` that can also be written through `ActorMatWords`.
typedef union ActorMat {
    MATRIX        mat;
    ActorMatWords ident;
} ActorMat;
STATIC_ASSERT_SIZEOF(ActorMat, 0x20);

/* Scratch-pad blocks.
 *
 * These are carved off the top of the scratch pad through `G_SCRATCH_HEAD`
 * for the duration of one call and released before it returns. */

/// Working state of the push-out walk over an actor's contact records: the
/// coordinate's world translation, the push that moves it out of the latest
/// solid record (capped in length), that push's XZ part, the record cursor and
/// whether any solid record was met. `dist` receives the end marker at the
/// terminating record.
typedef struct ActorRepelScratch {
    byte    pad_0[0x20];
    SVECTOR offset;
    SVECTOR last;
    SVECTOR pos;
    s32     kind;
    u32     len;
    s16     dist[32];
    s16     i;
    byte    pad_82[4];
    s16     hit;
} ActorRepelScratch;
STATIC_ASSERT_SIZEOF(ActorRepelScratch, 0x88);

/// Working state of the steering walk that nudges a coordinate away from the
/// obstacles among its contact records. `angle` and `ok` hold the bearings of
/// up to eight obstacles and whether each survived the pairwise spread check,
/// `face` is the coordinate's own heading, `i`/`j` the loop cursors, and
/// `blocked` is set when any record is of the blocking kind.
typedef struct ActorAvoidScratch {
    MATRIX   m;
    SVECTOR  dir;
    SVECTOR3 eye;
    byte     pad_2E[0x2];
    s32      kind;
    s16      angle[8];
    s8       ok[8];
    s16      face;
    s16      diff;
    u8       i;
    u8       j;
    u8       count;
    u8       blocked;
} ActorAvoidScratch;
STATIC_ASSERT_SIZEOF(ActorAvoidScratch, 0x54);

/// The offset from one position to another, widened to words and staged on
/// the scratch pad just long enough to take its bearing with `ratan2`. The
/// steering walk's bearing helpers and the turn steps use it.
typedef struct ActorAvoidDelta {
    s32  vx;
    s32  vy;
    s32  vz;
    byte pad_C[0x4];
} ActorAvoidDelta;
STATIC_ASSERT_SIZEOF(ActorAvoidDelta, 0x10);

/// The 16.16 deltas `func_800E0C10` resolves for a contact record, followed
/// by the step's result: whether the X or Z delta was nonzero.
typedef struct ActorDeltaFlag {
    GpDeltaScratch delta;
    s32            moved;
} ActorDeltaFlag;
STATIC_ASSERT_SIZEOF(ActorDeltaFlag, 0x14);

/// Line-of-sight test between the player and an actor: each one's root
/// translation, raised to eye height, is put through the view rotation into
/// `out` and `from`, and `hit` is the collision query's answer.
typedef struct ActorSightScratch {
    SVECTOR out;
    SVECTOR from;
    SVECTOR local;
    s32     hit;
} ActorSightScratch;
STATIC_ASSERT_SIZEOF(ActorSightScratch, 0x1C);

/// A beam drawn between two parts of an actor's model: both parts' matrices
/// and positions in view space, the four corners of the quad widened around
/// them, and the projected corners and depth that become the primitive.
typedef struct ActorBeamScratch {
    MATRIX  firstMatrix;
    MATRIX  secondMatrix;
    SVECTOR first;
    SVECTOR second;
    SVECTOR corner0;
    SVECTOR corner1;
    SVECTOR corner2;
    SVECTOR corner3;
    s32     screen0;
    s32     screen1;
    s32     screen2;
    s32     screen3;
    s32     perspective;
    s32     flags;
    s32     depth;
} ActorBeamScratch;
STATIC_ASSERT_SIZEOF(ActorBeamScratch, 0x8C);

/// The scratch-pad allocation pointer seen as the one member of a struct.
/// Reaching the pointer through a member rather than a bare word marks the
/// access as a structure access, which lets the scheduler order it against
/// the stores around it the way the original code was ordered.
typedef struct ActorScratchStack {
    u32 sp;
} ActorScratchStack;

/// Scaling a coordinate's rotation: an identity matrix scaled by `scale`,
/// then multiplied into the coordinate.
typedef struct ActorScaleScratch {
    ActorMat mat;
    VECTOR   scale;
} ActorScaleScratch;
STATIC_ASSERT_SIZEOF(ActorScaleScratch, 0x30);

/// Scaling a matrix uniformly with its translation: the scale vector handed
/// to `ScaleMatrix`, and the translation scaled on the GTE.
typedef struct ActorScaleMatrixScratch {
    VECTOR  scale;
    SVECTOR trans;
} ActorScaleMatrixScratch;
STATIC_ASSERT_SIZEOF(ActorScaleMatrixScratch, 0x18);

/// Turning an actor to face the player: the offset from the actor to the
/// player, flattened to the XZ plane, and the rotation built from its
/// bearing.
typedef struct ActorFaceScratch {
    VECTOR  delta;
    SVECTOR rot;
} ActorFaceScratch;
STATIC_ASSERT_SIZEOF(ActorFaceScratch, 0x18);

/// Rebuilding a coordinate's matrix as a uniform scale followed by a turn
/// about Y: the matrix being built, the scale vector and the yaw.
typedef struct ActorScaleRotScratch {
    MATRIX m;
    VECTOR scale;
    s16    angle;
    s16    pad_32;
} ActorScaleRotScratch;
STATIC_ASSERT_SIZEOF(ActorScaleRotScratch, 0x34);

/// An in-radius test on the XZ plane: the two offsets and the radius are each
/// squared in place, then compared as `dx + dz < r`.
typedef struct ActorRangeScratch {
    s32 dx;
    s32 dz;
    s32 r;
} ActorRangeScratch;
STATIC_ASSERT_SIZEOF(ActorRangeScratch, 0xC);

/// A turn toward the player: the offset to the player, then the clamped turn
/// applied to the actor's root coordinate.
typedef struct ActorTurnScratch {
    SVECTOR delta;
    s16     angle;
    s16     pad_A;
} ActorTurnScratch;
STATIC_ASSERT_SIZEOF(ActorTurnScratch, 0xC);

/// The same turn as `ActorTurnScratch`, with room the aiming step leaves
/// unused between the offset and the turn.
typedef struct ActorAimScratch {
    SVECTOR delta;
    s16     pad_8;
    s16     pad_A;
    s16     angle;
    s16     pad_E;
} ActorAimScratch;
STATIC_ASSERT_SIZEOF(ActorAimScratch, 0x10);

/// A turn toward the player that also weighs where the player is facing: the
/// offset to the player, the player's yaw, the yaw from the player back to
/// the actor, the wrapped turn toward the player and the clamped turn applied.
typedef struct ActorChaseScratch {
    SVECTOR delta;
    s16     playerYaw;
    s16     yaw;
    s16     turn;
    s16     angle;
} ActorChaseScratch;
STATIC_ASSERT_SIZEOF(ActorChaseScratch, 0x10);

/// Applying a hit the actor took: where it landed and its offset from the
/// model (later the knockback step), the offset to the player, the hit
/// record, the damage and distance derived from it, the critical roll and
/// the effect to spawn.
typedef struct ActorHitScratch {
    MATRIX  m;
    s32     dx;
    s32     dy;
    s32     dz;
    s32     pad_2C;
    SVECTOR dir;
    SVECTOR hitPos;
    s32     id;
    s32     damage;
    s32     dist;
    s16     yaw;
    s16     crit;
    s16     effect;
    s16     pad_52;
} ActorHitScratch;
STATIC_ASSERT_SIZEOF(ActorHitScratch, 0x54);

/// Pushing a coordinate out of the solid records of its contact table, with
/// the push taken from the record's direction rather than its centre: the
/// latest push (capped in length), the coordinate's translation, the record
/// cursor and whether any solid record was met. `dist` receives the end
/// marker at the terminating record.
typedef struct ActorPushScratch {
    SVECTOR offset;
    SVECTOR pos;
    s32     kind;
    s32     len;
    s16     i;
    s16     hit;
    s16     dist[12];
} ActorPushScratch;
STATIC_ASSERT_SIZEOF(ActorPushScratch, 0x34);

/// A model coordinate's translation carried into view space.
typedef struct ActorViewScratch {
    byte    pad_0[0x10];
    SVECTOR pos;
} ActorViewScratch;
STATIC_ASSERT_SIZEOF(ActorViewScratch, 0x18);

/// Projecting a point, usually an actor's origin through one of its
/// coordinates, to find its ordering-table depth. The results follow in the
/// order `rtps` writes them: screen position, depth cue, flags and depth.
typedef struct ActorProjectScratch {
    SVECTOR vec;
    s32     sxy;
    s32     dp;
    s32     flag;
    s32     otz;
} ActorProjectScratch;
STATIC_ASSERT_SIZEOF(ActorProjectScratch, 0x18);

/// A flat quad on the ground under an actor: its four corners in world space
/// and their projected screen positions.
typedef struct ActorGroundScratch {
    SVECTOR vec[4];
    DVECTOR sxy0;
    DVECTOR sxy1;
    DVECTOR sxy2;
    DVECTOR sxy3;
} ActorGroundScratch;
STATIC_ASSERT_SIZEOF(ActorGroundScratch, 0x30);

/// Four points projected together, with the first one's screen position and
/// the depth the primitive drawn from them is sorted at.
typedef struct ActorQuadScratch {
    SVECTOR v[4];
    s32     sxy;
    s32     otz;
} ActorQuadScratch;
STATIC_ASSERT_SIZEOF(ActorQuadScratch, 0x28);

/// A spark sprite drawn around one projected point: the depth it is sorted
/// at, the corner offset worked out for each pair of corners, the projection
/// flags, the point and its screen position.
typedef struct ActorSparkScratch {
    s32     otz;
    s32     dx;
    s32     dy;
    s32     flag;
    SVECTOR vec;
    s16     sx;
    s16     sy;
} ActorSparkScratch;
STATIC_ASSERT_SIZEOF(ActorSparkScratch, 0x1C);

/// Picking the next point to walk to relative to the player: the offset to
/// the player, turned and scaled into a step toward the new point, the
/// target, the matrix the turn is built in, and the turn with the yaws it is
/// worked out from.
typedef struct ActorMoveScratch {
    SVECTOR vec;
    SVECTOR target;
    MATRIX  matrix;
    s16     delta;
    s16     original;
    s16     yaw;
    s16     playerYaw;
} ActorMoveScratch;
STATIC_ASSERT_SIZEOF(ActorMoveScratch, 0x38);

/* Work blocks of actors that carry the same code. */

/// Status flags of `Actor341700Work`, read through two widths: every guard
/// tests bit 0 as a halfword and then bits 0x102 as a word.
typedef union Actor341700Flags {
    u32 word;
    u16 half;
} Actor341700Flags;
STATIC_ASSERT_SIZEOF(Actor341700Flags, 0x4);

/// Work block of the enemy whose code both actor_341700 and actor_342400
/// carry. Each allocates it zeroed at its full size and keeps it at
/// `Task::work`. `field_420` / `field_422` are the state and sub-state indices
/// the handler tables walk and `field_412` the per-state frame counter;
/// `field_414` .. `field_41C` are the animation request.
typedef struct Actor341700Work {
    MATRIX    savedRootMtx; // root matrix saved at death, rescaled each frame while the model shrinks
    MATRIX    colorMtx;     // the model's `TmdObject::colorMtx`
    MATRIX    lightMtx;     // the model's `TmdObject::lightMtx`
    VECTOR    field_60;     // position the root snaps back to when blocked
    SVECTOR   field_70;     // origin of slot 4 entry 0's coords[3], carried into view space
    s16       field_78;     // pitch, fed to RotMatrixX
    s16       field_7A;     // heading fed to rsin / rcos
    s16       field_7C;     // roll, fed to RotMatrixZ
    byte      pad_7E[0x2];
    u16       field_80;     // spawn position: root coord.t[0]
    u16       field_82;     // root coord.t[1], after lifting it by 0x3C
    u16       field_84;     // root coord.t[2]
    byte      pad_86[0x2];
    s16       field_88;     // x of the offset to the nearer player actor
    s16       field_8A;     // y of that offset
    s16       field_8C;     // z of that offset
    byte      pad_8E[0x2];
    u16       field_90;     // root coord.t[0], snapshotted with the view-space origin
    u16       field_92;     // root coord.t[1]
    u16       field_94;     // root coord.t[2]
    byte      pad_96[0x2];
    SVECTOR   field_98;     // translation of coords[6] relative to the view
    GpAnimCtx anim;
    /// First of the nine `GpAnimSlot`s handed to `func_800B3F84`; the second
    /// overlaps `flags_EC`, so only the first is spelled out.
    GpAnimSlot       slot_B4;
    byte             pad_DC[0x10];
    Actor341700Flags flags_EC;
    byte             pad_F0[0x12C];
    byte             field_21C[0x90]; // `func_800B3F84`'s arg3 buffer
    GpObj            obj_2AC;
    GpObj            obj_2CC;
    GpRec18          rec_2EC[8];
    GpObj            obj_3AC;
    GpRec18          rec_3CC[2]; // records of `obj_3AC`
    GpEffArg         eff_3FC;    // `func_800FDB18`'s arg3; field_0 is the model's second coord part
    byte             pad_404[0x8];
    s16              field_40C;  // heading the root is moved along
    s16              field_40E;  // hit cooldown: `Gp_GetIdParam2` of the last hit, counted down each frame
    s16              field_410;
    u16              field_412;  // per-state frame counter
    s16              field_414;  // animation request kind
    s16              field_416;  // animation id last applied to the slots
    s16              field_418;  // animation id
    u16              field_41A;  // frames since the animation was applied
    s16              field_41C;  // animation speed / step scale
    s16              field_41E;  // 1 lets `field_448` jump the state machine
    u16              field_420;  // state index
    u16              field_422;  // sub-state index
    s16              field_424;  // yaw added to model parts 3..5, a third each; eased toward zero each frame
    s16              field_426;
    s16              field_428;
    s16              field_42A;
    s16              field_42C; // frames spent turning toward field_444; 16 enters state 3
    byte             pad_42E[0x2];
    u16              field_430; // Y scale while the model shrinks after death
    s16              field_432; // 1 re-derives the spawn position
    s16              field_434; // pitch latched when a sway ends, then eased back to zero
    s16              field_436; // turn step applied to the heading
    s16              field_438;
    s16              field_43A; // distance to the nearer player actor
    byte             pad_43C[0x2];
    s16              field_43E; // counted down each frame while blocked
    s16              field_440; // picks animation 5 (zero) or 6 after animation 8
    s16              field_442; // frame phase driving the pitch sway
    u16              field_444; // heading to the nearer player actor relative to field_7A, masked to 0xFFF
    s16              field_446; // randomised hold in frames
    s16              field_448; // pending state request; 4 moves the task to state 4 once the enemy is dead
    s16              field_44A;
    u16              field_44C; // message 0x2C00's halfword, when its low nibble is 1..5
    u8               field_44E; // set while the enemy carries status flag 4/8
    u8               field_44F; // 1 runs the post-sub-state step
    byte             pad_450[0x1];
    u8               field_451;
    byte             pad_452[0x2];
} Actor341700Work;
STATIC_ASSERT_SIZEOF(Actor341700Work, 0x454);

/// Work block of the enemy whose code both actor_05500 and actor_02600 carry,
/// kept at `Task::work`. Most of it is still unnamed.
typedef struct Actor105500Work {
    GpObj     obj;
    GpRec18   rec;
    s16       field_38;
    s16       field_3A;
    byte      pad_3C[0x118];
    byte      field_154[0x80];
    MATRIX    field_1D4;
    MATRIX    field_1F4;
    GpObj     field_214;
    GpRec18   field_234[4];
    GpObj     field_294;
    GpRec18   field_2B4[2];
    GpObj     field_2E4;
    GpRec18   field_304[1];
    GpObj     field_31C;
    GpRec18   field_33C[1];
    GpEffArg  field_354;
    VECTOR3   field_35C;
    byte      pad_368[4];
    TaskDesc* field_36C;
    MATRIX    field_370;
    s16       field_390;
    s16       field_392;
    s16       field_394;
    u16       field_396;
    s16       field_398;
    s16       field_39A;
    s16       field_39C;
    s16       field_39E;
    s16       field_3A0;
    s16       field_3A2;
    s16       field_3A4;
    s16       field_3A6;
    s16       field_3A8;
    s16       field_3AA;
    u16       field_3AC;
    byte      pad_3AE[2];
    s16       field_3B0;
    s16       field_3B2;
    s16       field_3B4;
    s16       field_3B6;
    byte      pad_3B8[2];
    s16       field_3BA;
    s16       field_3BC;
    s16       field_3BE;
    s16       field_3C0;
    s16       field_3C2;
    s16       field_3C4;
    s16       field_3C6;
    s16       field_3C8;
    s16       field_3CA;
    s16       field_3CC;
    s16       field_3CE;
    s16       field_3D0;
    s16       field_3D2;
} Actor105500Work;
STATIC_ASSERT_SIZEOF(Actor105500Work, 0x3D4);

/// The animation context and eight slots at the front of the same work block,
/// the view the animation setup is handed.
typedef struct Actor105500Anim {
    GpAnimCtx  context;
    GpAnimSlot slots[8];
} Actor105500Anim;
STATIC_ASSERT_SIZEOF(Actor105500Anim, 0x154);

/// One texture-page coordinate of the enemy's sprite table.
typedef struct Actor105500Uv {
    u8 u;
    u8 pad_1;
    u8 v;
    u8 pad_3;
} Actor105500Uv;

/// Scratch-pad block for projecting one end of the enemy's line primitives:
/// the point, its screen position and the depth the line is sorted at.
typedef struct Actor105500LineScratch {
    s32     unused[4];
    SVECTOR position;
    s32     screen;
    s32     depth;
} Actor105500LineScratch;
STATIC_ASSERT_SIZEOF(Actor105500LineScratch, 0x20);

/// Scratch-pad block of that enemy's push-back: the deltas the collision walk
/// resolves, their normal and its image in grid space, and the rotation the
/// actor is re-aimed with.
typedef struct Actor105500HitScratch {
    GpDeltaScratch delta;
    VECTOR         normal;
    VECTOR         local;
    SVECTOR        rot;
} Actor105500HitScratch;
STATIC_ASSERT_SIZEOF(Actor105500HitScratch, 0x38);

/* Contexts. */

/// Ramp context of the screen-wave task. Whoever spawns the task seeds the
/// span and the scale and passes the block as the spawn argument; a later
/// message writes the ramp state, and the task advances the frame, derives
/// the wave amplitude as `frame * scale / span` and reads the tint. The field
/// names are still the generic ones because rooms that carry the same task
/// reach the block through them.
typedef struct ActorWaveCtx {
    s16 field_0; // span: frames the ramp takes to reach full scale
    s16 field_2; // scale: amplitude at the top of the ramp
    s16 field_4; // ramp state: 0 up, 1 down, 2 finished
    s16 field_6; // current ramp frame
    u8  field_8; // nonzero: tint the mesh with field_9..field_B
    u8  field_9;
    u8  field_A;
    u8  field_B;
} ActorWaveCtx;
STATIC_ASSERT_SIZEOF(ActorWaveCtx, 0xC);

/* Task work blocks. */

/// Work block of the full-screen fade tasks, allocated eight bytes at a time
/// and kept at `Task::work`: the three colour channels the fade overlay is
/// drawn with, stepped toward black or clear by the task's rate each frame.
/// The channels are signed, since a fade-in ends when a channel goes
/// negative. The leading halfword is never touched.
typedef struct ActorFadeWork {
    byte pad_0[0x2];
    s16  r;
    s16  g;
    s16  b;
} ActorFadeWork;
STATIC_ASSERT_SIZEOF(ActorFadeWork, 0x8);

/* Helpers.
 *
 * Inline bodies each actor compiled from its own copy of the same source.
 * They stay inline: the callers' code was generated with the body expanded in
 * place, which a call would not reproduce. */

/// Nonzero while movement is frozen; the stepping helpers do nothing then.
extern u8 D_80072729;

/// Bearing of `p` from `eye` on the XZ plane. The offset is staged on the
/// scratch pad at full width and released before `ratan2` runs.
static __inline__ s16 actorBearingXZ(SVECTOR3* p, SVECTOR3* eye)
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

/// Bearing of `p` from `eye` on the XY plane, the form the steering walk uses
/// while the coordinate's facing column is close to vertical.
static __inline__ s16 actorBearingXY(SVECTOR3* p, SVECTOR3* eye)
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

/// The push that moves `pos` out of the contact record `rec`: how deep `pos`
/// sits inside the record's radius, along the direction from the record's
/// centre carried into grid space. Only X and Z are written.
static __inline__ void actorCalcPush(SVECTOR* pos, GpRec18* rec, SVECTOR* out)
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

/// Builds `joint`'s absolute rotation in `out`: its own rotation with each
/// ancestor pre-multiplied in turn, renormalised after every step, up to but
/// not including `stop`. Returns whether the walk reached `stop` rather than
/// the end of the chain.
static __inline__ s32 actorAccumulateRotation(GsCOORDINATE2* joint, MATRIX* out, GsCOORDINATE2* stop)
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

/// Turns the world-space `rotation` into one relative to `joint`'s parent:
/// accumulates the chain above the parent up to the view coordinate,
/// transposes it and pre-multiplies. Nothing happens when the parent is the
/// view coordinate. Returns `joint`, which callers store through.
static __inline__ GsCOORDINATE2* actorLocalizeRotation(GsCOORDINATE2* joint, MATRIX* rotation)
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

/// Carries `v` from the frame of `coord` up the parent chain into world
/// space, walking in a block taken from the scratch pad.
static __inline__ void actorToWorld(GsCOORDINATE2* coord, SVECTOR* v)
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

/// The walk of `actorToWorld` without its register bindings. Callers use
/// whichever of the two spellings their code was compiled from.
static __inline__ void actorToWorld2(GsCOORDINATE2* coord, SVECTOR* v)
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

/// Carries `out` from the frame of `p` up the parent chain to the view
/// coordinate, leaving it in view space. `out` is written only when the walk
/// reaches the view coordinate.
static __inline__ void actorTransformToView(GsCOORDINATE2* p, SVECTOR* out)
{
    SVECTOR        sv;
    VECTOR         vec;
    s32            flag;
    SVECTOR*       svp   = &sv;
    GsCOORDINATE2* view  = &gGfxViewCoord;
    VECTOR*        vecp  = &vec;
    s32*           flagp = &flag;
    sv.vx                = out->vx;
    sv.vy                = out->vy;
    sv.vz                = out->vz;
loop:
    if (p->sub != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            gte_rtv0tr();
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            sv.vx = vec.vx;
            sv.vy = vec.vy;
            sv.vz = vec.vz;
            p     = p->sub;
            goto loop;
        }
        out->vx = sv.vx;
        out->vy = sv.vy;
        out->vz = sv.vz;
    }
}

/// Wraps an angle difference into [-0x800, 0x800].
static __inline__ s16 actorNormalizeYaw(s16 input)
{
    s16 value = input;
    if (input < 0) {
        while (1) {
            if (value >= -0x800)
                break;
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800)
                break;
            value -= 0x1000;
        }
    }
    return value;
}

/// The offset from `coord` to the translation of `config`'s coordinate.
static __inline__ void actorConfigPositionDelta(PlayerStatus* config, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = config->coordMtx->t[0] - coord->coord.t[0];
    pos->vy = config->coordMtx->t[1] - coord->coord.t[1];
    pos->vz = config->coordMtx->t[2] - coord->coord.t[2];
}

/// The turn that would face `actor` toward `config`'s coordinate: the bearing
/// of the offset, written to `pos`, less the actor's own heading, wrapped.
static __inline__ s16 actorPositionYaw(Task* actor, SVECTOR* pos, PlayerStatus* config)
{
    GsCOORDINATE2* coord;
    s32            angle;
    actorConfigPositionDelta(config, ((TmdObject*)actor->extra)->coords, pos);
    coord = ((TmdObject*)actor->extra)->coords;
    angle = ratan2(pos->vx, pos->vz);
    return actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// Rebuilds `coord`'s rotation as a turn about Y by its current heading,
/// uniformly scaled by `scale`.
static __inline__ void actorRescaleYaw(GsCOORDINATE2* coord, s16 scale)
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

/// Steps `coord` `amount` units along its local Z axis unless movement is
/// frozen, staging the direction on the scratch pad.
static __inline__ void actorMoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
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

/// `actorMoveForward` that also skips the step when `amount` is zero, though
/// it still takes and releases its scratch vector.
static __inline__ void actorMoveForwardNonzero(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        gteVec                     = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12();
            gte_stsv(gteVec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// Whether the XZ offset `d` reaches at least `r` from its origin.
static __inline__ s32 actorOutOfRange(SVECTOR* d, s16 r)
{
    u8*                head;
    ActorRangeScratch* blk;
    s32                ret;

    head                                    = *(u8**)G_SCRATCH_HEAD;
    ((ActorRangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                     = (ActorRangeScratch*)(head - 0xC);
    blk->dz                                 = d->vz;
    blk->r                                  = r;
    ((ActorRangeScratch*)(head - 0xC))->dx *= ((ActorRangeScratch*)(head - 0xC))->dx;
    *(ActorRangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                *= blk->dz;
    blk->r                                 *= blk->r;
    *(u8**)G_SCRATCH_HEAD                   = head;
    ret                                     = ((ActorRangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// `actorMoveForward` applied to the root coordinate of `task`'s model.
static __inline__ void actorMoveModelForward(Task* task, s16 amount)
{
    GsCOORDINATE2* coord;
    SVECTOR*       head;
    SVECTOR*       vec;

    coord = ((TmdObject*)task->extra)->coords;
    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
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

/// Rebuilds `coord`'s rotation as a turn about Y by its current heading at
/// unit scale.
static __inline__ void actorResetYaw(GsCOORDINATE2* coord)
{
    void**                scratch;
    void*                 head;
    ActorScaleRotScratch* blk;
    s16                   ang;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (ActorScaleRotScratch*)((u8*)head - 0x34);
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = 1;
    blk->scale.vy = 1;
    blk->scale.vx = 1;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&blk->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    coord->coord.m[2][2] = *(u16*)&blk->m.m[2][2];
    coord->flg           = 0;
    *scratch             = (u8*)*scratch + 0x34;
}

/// `actorRescaleYaw` with a separate scale on Y.
static __inline__ void actorRescaleYawY(GsCOORDINATE2* coord, s32 scale, s16 scaleY)
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
    blk->scale.vx = scale;
    blk->scale.vy = scaleY;
    blk->scale.vz = scale;
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

/// `actorMoveForwardNonzero` spelled with the GTE reading the scratch vector
/// under a single name.
static __inline__ void actorStepForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(vec);
            gte_gpf12();
            gte_stsv(vec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// The first of the leading twelve contact records whose kind is 0x20000:
/// copies its point to `pos` and returns its key, or returns 0 when none is
/// found before the table ends.
static __inline__ s32 actorFindHit(SVECTOR* pos, GpRec18* records)
{
    s16 i;

    for (i = 0; i < 12; i++) {
        if (!records[i].key)
            break;
        if ((records[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key;
        }
    }
    return 0;
}

/// Gives a freshly spawned effect model the texture page and palette of the
/// enemy's placement in the current area, and reprocesses its stream when it
/// already has one.
static __inline__ void actorTintEffect(GpEffWork* eff, GpEnemy* enemy)
{
    GpAreaKey    key;
    GpAreaKey*   sessionKey;
    GpAreaKey*   keyPtr;
    u8           areaByte0;
    GpAreaRec*   rec;
    GpAreaPlace* entry;
    TmdObject*   model;
    s32          idx;
    u32          raw;

    if (eff != NULL) {
        sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
        raw        = enemy->placeKey;
        model      = (TmdObject*)eff->task->extra;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        areaByte0  = gGameSession->at4.loc.view;
        idx        = raw >> 12;
        SOFT_BARRIER();
        keyPtr = &key;
        TOUCH_REG(keyPtr);
        key.view = areaByte0;
        Gp_SyncAreaKeyIndex(keyPtr);
        rec          = Gp_GetNestedAreaRec(&key);
        entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = entry->tpage;
        model->clut  = entry->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
}

/// The offset from `coord` to the translation of `m`.
static __inline__ void actorMatrixPositionDelta(MATRIX* m, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = m->t[0] - coord->coord.t[0];
    pos->vy = m->t[1] - coord->coord.t[1];
    pos->vz = m->t[2] - coord->coord.t[2];
}

/// `actorPositionYaw` toward the translation of `m`.
static __inline__ s16 actorMatrixPositionYaw(Task* actor, SVECTOR* pos, MATRIX* m)
{
    GsCOORDINATE2* coord;
    s32            angle;

    actorMatrixPositionDelta(m, ((TmdObject*)actor->extra)->coords, pos);
    coord = ((TmdObject*)actor->extra)->coords;
    angle = ratan2(pos->vx, pos->vz);
    return actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// The turn from `coord`'s heading to the bearing of the offset (`x`, `z`),
/// wrapped.
static __inline__ s16 actorYawTo(GsCOORDINATE2* coord, s16 x, s16 z)
{
    s32 angle;

    angle = ratan2(x, z);
    return actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// Combines a movement step with a push along the same axis: the push when
/// there is no step, the step when the two disagree in sign, otherwise the
/// larger in magnitude.
static __inline__ s16 actorPickStep(s16 step, s16 push)
{
    if (step == 0) {
        return push;
    }
    if ((step > 0 && push < 0) || (step < 0 && push > 0)) {
        return step;
    }
    if (step > 0) {
        if (push < step) {
            return step;
        }
        return push;
    }
    if (push < step) {
        return push;
    }
    return step;
}

/// Whether the XZ offset `pos` reaches at least `radius`, worked in a
/// scratch block pushed and popped around the test.
static __inline__ s32 actorOutsideRadius(SVECTOR* pos, s16 radius)
{
    ActorRangeScratch* head;
    ActorRangeScratch* scratch;
    head                                  = *(ActorRangeScratch**)G_SCRATCH_HEAD;
    scratch                               = head - 1;
    *(ActorRangeScratch**)G_SCRATCH_HEAD  = scratch;
    scratch->dx                           = pos->vx;
    scratch->dz                           = pos->vz;
    scratch->r                            = radius;
    scratch->dx                          *= scratch->dx;
    scratch->dz                          *= scratch->dz;
    scratch->r                           *= scratch->r;
    *(ActorRangeScratch**)G_SCRATCH_HEAD += 1;
    return scratch->dx + scratch->dz >= scratch->r;
}

/// Tells the player task that `ctx` touched it, packing the pair with `mode`.
static __inline__ s32 actorPlayerContactMessage(GpEnemy* ctx, s32 mode)
{
    Task* player = gameGetPtrSlot(3);
    return Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair(ctx, mode), 0);
}

/// Relights `enemy` for the world position of `coord`.
static __inline__ void actorUpdateColor(GpEnemy* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block         = (VECTOR*)(*(u8**)0x1F8003FC - 0x10);
    block->vx             = coord->workm.t[0];
    block->vy             = coord->workm.t[1];
    block->vz             = coord->workm.t[2];
    *(VECTOR**)0x1F8003FC = block;
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)0x1F8003FC += 0x10;
}

/// Relights the enemy of `arg0` for the world position of its model's second
/// part.
static __inline__ void actorUpdateModelColor(Task* arg0)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)arg0->extra)->coords[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(arg0->spawnArg2, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// `actorTransformToView` spelled as a `for` loop with an early return.
static __inline__ void actorLocalToView(GsCOORDINATE2* coord, SVECTOR* out)
{
    SVECTOR acc;
    VECTOR  v;
    s32     flag;

    acc.vx = out->vx;
    acc.vy = out->vy;
    acc.vz = out->vz;

    for (;;) {
        if (coord->sub == NULL) {
            return;
        }
        if (coord != &gGfxViewCoord) {
            gte_SetTransMatrix(&coord->coord);
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&acc);
            gte_rt();
            gte_stlvnl(&v);
            gte_stflg(&flag);
            acc.vx = v.vx;
            acc.vy = v.vy;
            acc.vz = v.vz;
            coord  = coord->sub;
        } else {
            out->vx = acc.vx;
            out->vy = acc.vy;
            out->vz = acc.vz;
            return;
        }
    }
}

/// `actorAccumulateRotation` stopping at the view coordinate, without the
/// result.
static __inline__ void actorAccumulateToView(GsCOORDINATE2* coord, MATRIX* mat)
{
    MATRIX         m;
    GsCOORDINATE2* cur;

    cur  = coord->sub;
    *mat = coord->coord;
    while (1) {
        if (cur == NULL) {
            return;
        }
        if (cur == &gGfxViewCoord) {
            return;
        }
        gte_SetRotMatrix(&cur->coord);
        MulRotMatrix(mat);
        MatrixNormal(mat, &m);
        *mat = m;
        cur  = cur->sub;
    }
}

/// Sets up a collision object on `coord` with its record table, position and
/// radius, links it at priority `prio`, and initialises the table as `kind`.
static __inline__ void actorLinkWorkObj(GsCOORDINATE2* coord, GpObj* obj, GpRec18* rec,
                                        SVECTOR* pos, s16 field1C, s32 prio, s32 kind)
{
    obj->coord    = coord;
    obj->ctx.recs = rec;
    obj->pos.vx   = pos->vx;
    obj->pos.vy   = pos->vy;
    obj->pos.vz   = pos->vz;
    obj->radius   = field1C;
    obj->flags    = 1;
    Gp_LinkObj(prio, obj);
    Gp_InitRec18Table(obj->ctx.recs, kind, 0);
}

/// Whether the XZ offset `gap` reaches at least 1000.
static __inline__ s32 actorOutOfReach(SVECTOR* gap)
{
    VECTOR3* v;

    v                          = (VECTOR3*)(*(u8**)G_SCRATCH_HEAD - sizeof(VECTOR3));
    *(VECTOR3**)G_SCRATCH_HEAD = v;
    v->vx                      = gap->vx;
    v->vy                      = gap->vz;
    v->vz                      = 1000;
    v->vx                      = v->vx * v->vx;
    v->vy                      = v->vy * v->vy;
    v->vz                      = v->vz * v->vz;
    *(u8**)G_SCRATCH_HEAD      = *(u8**)G_SCRATCH_HEAD + sizeof(VECTOR3);

    return v->vx + v->vy >= v->vz;
}

/// The scratch-pad allocation pointer.
static __inline__ u8* actorGetScratchHead(void)
{
    return *(u8**)G_SCRATCH_HEAD;
}

/// Moves the scratch-pad allocation pointer to `head`.
static __inline__ void actorSetScratchHead(void* head)
{
    *(void**)G_SCRATCH_HEAD = head;
}

/// Wraps an angle into [-0x800, 0x800], spelled with backward jumps.
static __inline__ s16 actorWrapAngle(s16 angle)
{
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    return angle;
}

#endif /* ACTORS_ACTOR_H */
