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

#endif /* ACTORS_ACTOR_H */
