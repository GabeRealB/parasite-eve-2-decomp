#ifndef ACTORS_ACTOR_H
#define ACTORS_ACTOR_H

#include "common.h"
#include <psyq/libgte.h>
#include "gameplay/3FB8.h"

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

#endif /* ACTORS_ACTOR_H */
