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
#include "overlay.h"

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

/// The 16.16 deltas `func_800E0C10` resolves for a contact record, followed
/// by the step's result: whether the X or Z delta was nonzero.
typedef struct ActorDeltaFlag {
    GpDeltaScratch delta;
    s32            moved;
} ActorDeltaFlag;
STATIC_ASSERT_SIZEOF(ActorDeltaFlag, 0x14);

/// A step resolved against the contact records: the 16.16 deltas
/// `func_800E0C10` resolves, their integer part (its XZ part capped in
/// length), that part's XZ length, and whether the X or Z delta was nonzero.
typedef struct ActorStepDelta {
    GpDeltaScratch delta;
    SVECTOR        step;
    s32            len;
    s32            moved;
} ActorStepDelta;
STATIC_ASSERT_SIZEOF(ActorStepDelta, 0x20);

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
    OverlayMat mat;
    VECTOR     scale;
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

/* Tables. */

/// One row of a per-room height clamp: when `field_0` / `field_2` match the
/// session's stage and area, the actor's height is clamped to [`lo`, `hi`].
typedef struct ActorHeightClamp {
    s16  field_0;
    s16  field_2;
    s16  lo;
    s16  hi;
    byte pad_8[8];
} ActorHeightClamp;
STATIC_ASSERT_SIZEOF(ActorHeightClamp, 0x10);

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

/// Work block of the enemy whose code both actor_105300 and actor_105400
/// carry, kept at `Task::work`: the animation context with its slots and
/// poses, the collision nodes and records, and the state the per-frame
/// handlers drive.
typedef struct Actor05300Work {
    GpAnimCtx  anim;
    GpAnimSlot slots[10];
    GpAnimPose poses[10];
    MATRIX     field_244;
    MATRIX     field_264;
    GpObj      node0;
    GpObj      node1;
    GpRec18    rec18[2];
    GpEffArg   field_2F4;
    MATRIX     field_2FC;
    s32        field_31C;
    u16        field_320;
    s16        field_322;
    u16        field_324;
    u16        field_326;
    u16        field_328;
    u16        field_32A;
    u16        field_32C;
    u16        field_32E;
    u16        field_330;
    s16        field_332;
    s16        field_334;
    s16        field_336;
    s16        field_338;
    s16        field_33A;
    s16        field_33C;
    s16        field_33E;
} Actor05300Work;
STATIC_ASSERT_SIZEOF(Actor05300Work, 0x340);

/// One row of that enemy's clip tables: a nonzero `field_0` ends the clip and
/// `field_2` is the row's scale.
typedef struct Actor05300Clip {
    s16 field_0;
    u16 field_2;
} Actor05300Clip;
STATIC_ASSERT_SIZEOF(Actor05300Clip, 0x4);

/// Part object that enemy's spawn allocates and keeps at the part task's
/// `Task::work`: a linked collision node with its single record, and the
/// record the part's death effect is spawned with.
typedef struct Actor05300Part {
    GpObj    obj;
    GpRec18  rec18[1];
    GpEffArg field_38; // record this part's death effect is spawned with
    s16      field_40;
    u16      field_42;
    s16      field_44;
    s16      field_46;
} Actor05300Part;
STATIC_ASSERT_SIZEOF(Actor05300Part, 0x48);

/// Scratch-pad block of that enemy's hit handler: the offset from the player
/// and the offset the hit effect is spawned at.
typedef struct Actor05300Scratch {
    VECTOR  delta;
    SVECTOR ofs;
} Actor05300Scratch;
STATIC_ASSERT_SIZEOF(Actor05300Scratch, 0x18);

/// A spawn position of that enemy, one per sub-state.
typedef struct Actor05300SpawnPos {
    s16 x;
    s16 y;
    s16 z;
} Actor05300SpawnPos;
STATIC_ASSERT_SIZEOF(Actor05300SpawnPos, 0x6);

/// One row of that enemy's per-area sound table: the two parameters its
/// sound event is queued with.
typedef struct Actor05300SndRow {
    s8 field_0;
    s8 pad_1;
    s8 field_2;
    s8 pad_3;
} Actor05300SndRow;
STATIC_ASSERT_SIZEOF(Actor05300SndRow, 0x4);

/// Work block of the enemy whose code both actor_350500 and actor_350700
/// carry, allocated zeroed at its full size and kept at `Task::work`. The two
/// matrices are published as the model's light and colour matrices, so the
/// actor draws with its own lighting.
typedef struct Actor350500Work {
    GpAnimCtx  anim;
    GpAnimSlot slots[0x13];  // the slot array handed to `func_800B3F84`
    byte       poses[0x130]; // pose buffer handed to `func_800B3F84`
    s8         field_43C;    // animation-tick enable
    s8         field_43D;    // current animation id
    s8         field_43E;    // current bank index
    s8         field_43F;    // animation id the approach step plays on arrival
    MATRIX     light;
    MATRIX     color;
    VECTOR3    target;    // world position the actor walks to
    byte       pad_48C[0x4];
    VECTOR3    step;      // per-frame world-space delta the accumulators take
    byte       pad_49C[0x4];
    s32        field_4A0; // 16.16 accumulators; only the high half reaches the coordinate
    s32        field_4A4;
    s32        field_4A8;
    byte       pad_4AC[0x4];
    SVECTOR    limit;     // per-axis stop threshold; 0x7FFF on all three disables it
    u16        field_4B8; // placement rotation
    u16        field_4BA; // placement yaw the final turn steers toward
    u16        field_4BC;
    byte       pad_4BE[0x2];
    u16        field_4C0; // selects the idle or the walk tick handler
    u16        field_4C2; // index into the walk-step table
    s8         field_4C4; // variant the two-case message handler latches
    s8         field_4C5; // frames until the model buffers are freed; -1 disables
    byte       pad_4C6[0x2];
} Actor350500Work;
STATIC_ASSERT_SIZEOF(Actor350500Work, 0x4C8);

/// Work block of the enemy whose code both actor_160700 and actor_215100
/// carry, allocated zeroed at its full size and kept at `Task::work`: the
/// light and colour matrices its model draws with, the animation context and
/// slots, the animation state, and the yaw and remaining travel the placement
/// commands leave. `field_4F0` is the task of the enemy spawned alongside it
/// and `enemy` the enemy its own task belongs to.
typedef struct Actor160700Work {
    MATRIX     light;
    MATRIX     color;
    GpAnimCtx  anim;
    GpAnimSlot slots[0x14];
    byte       field_374;
    byte       pad_375[0x13F];
    s16        state;
    s16        appliedAnimId;
    s16        animId;
    s16        field_4BA;
    byte       pad_4BC[0x2A];
    u16        yaw;
    byte       pad_4E8[0x2];
    s16        travel;
    s16        animArg;
    byte       pad_4EE[0x2];
    Task*      field_4F0;
    GpEnemy*   enemy;
} Actor160700Work;
STATIC_ASSERT_SIZEOF(Actor160700Work, 0x4F8);

/* actor_402200 and actor_403900 carry the same enemy code. Function and data
 * names in these comments are actor_402200's; actor_403900 has the same
 * bodies at other addresses. */

/// One 0x10-byte entry of the box table `Actor402200Work::field_6B4`: the
/// entry's kind at `field_0` (0 a circle of radius `field_2` round
/// `field_4`, `field_6`; 1 a box, x from `field_8` to `field_C` and z from
/// `field_E` to `field_A`).
typedef struct Actor402200Region {
    s16 field_0;
    s16 field_2;
    s16 field_4;
    s16 field_6;
    s16 field_8;
    s16 field_A;
    s16 field_C;
    s16 field_E;
} Actor402200Region;
STATIC_ASSERT_SIZEOF(Actor402200Region, 0x10);

/// One 8-byte entry of the spawn's placement run `D_actor_402200_80153C78`,
/// terminated by a zero `field_0`: when the session's stage (`field_2`) and
/// room (`field_4`) match, `field_0` indexes the box tables and `field_6` is
/// the box count stored to `Actor402200Work::field_6FA`.
typedef struct Actor402200Spot {
    s16 field_0;
    s16 field_2;
    s16 field_4;
    u16 field_6;
} Actor402200Spot;
STATIC_ASSERT_SIZEOF(Actor402200Spot, 0x8);

/// Per-instance work block the overlay's setup `func_actor_402200_80137444`
/// allocates with `memCalloc(0x71C)` and parks in the 0x1C slot below (the
/// task's `Task::work`, which is not a `TaskIdMap` here).
///
/// `field_6E2` is the ground-shadow shade the ground-quad body
/// `func_actor_402200_8013806C` hands to `Gp_DrawEffGroundQuad`, which draws
/// nothing for a negative value: the body turns the calloc'd zero into -1 the
/// first time it runs, so an actor that never raises the shade casts no
/// shadow.
///
/// `field_6F4` is the actor's phase, written and read as a signed halfword:
/// the frame handler clears it on entry, `func_actor_402200_801381E0` raises
/// it to 1 while the remaining-enemy count is positive, and the handler
/// branches on 0 / 1 thereafter.
///
/// `field_6EA` is a pending tint request: `func_actor_402200_80137FB0` reads it
/// once a frame and, while it is 1 or 2, hands the display object the matching
/// translate (0, 0, 0x400 or full 0xFFF) and clears it, so each request is
/// consumed on the frame it is raised.
///
/// `field_718` arms a one-shot vocal cue and `field_71A` is its frame counter.
/// While the flag is clear the body does nothing; once it is set the counter
/// runs up, plays the actor's cue at 0x14, and at 0x5F asks the scene for
/// message 0x3ED - clearing the flag and sending 0x3F1 instead if the scene
/// refuses it.
/// `field_3C` is the animation slot the cue body `func_actor_402200_80135BE0`
/// hands to `Gp_AnimGetRec`: the second of the 0x28-byte slots the actor work
/// blocks lay out from 0x14, the same one the other actor overlays' cue bodies
/// play from. `field_6CA` latches the record's two cue bits (`0x30`) for the
/// next frame, and `field_712` is the running entry index into the overlay's
/// cue-id table `D_actor_402200_80138420` - zero disarms the body, and while it
/// is set the two adjacent words `[field_712 * 2 - 1]` and `[field_712 * 2]`
/// are the cue ids it plays.
typedef struct Actor402200Work {
    byte       pad_0[0x14];
    GpAnimSlot field_14;
    GpAnimSlot field_3C;
    byte       pad_64[0x2A8];
    byte       field_30C[0x130];
    MATRIX     field_43C;
    MATRIX     field_45C;
    byte       field_47C[8];
    void*      field_484;
    GpRec18*   field_488;
    s16        field_48C;
    s16        field_48E;
    /// Halfword the attack sequences park alongside the timers: state 0 stores
    /// -0xA7 when `field_6D2` is clear and 0x109 when it is set. The branch
    /// sequence `func_actor_402200_80135630` stores the same pair, so it is the
    /// same slot set's vertical placement.
    s16  field_490;
    byte pad_492[2];
    /// Hit descriptor the flinch handler `func_actor_402200_80131F54` and the
    /// hurt states `func_actor_402200_80133AEC` / `func_actor_402200_80134194`
    /// store on the frame a hit lands: the damage amount `field_716` with the
    /// tag bits 0x30000 OR'd in.
    s32 field_494;
    /// Halfword the attack sequences arm to 0x15E next to `field_490`.
    s16 field_498;
    /// Hit-pending flags, raised together with `field_494`: bit 0x8000 is the
    /// flag the hit handler clears when it consumes the descriptor. The frame
    /// handler `func_actor_402200_80137A1C` makes the enemy lockable only while
    /// it is set, and `func_actor_402200_80131F54` clears `field_6C6` as it
    /// raises it.
    u16      field_49A;
    GpRec18  field_49C[3];
    byte     field_4E4[8];
    void*    field_4EC;
    GpRec18* field_4F0;
    s16      field_4F4;
    s16      field_4F6;
    s16      field_4F8;
    byte     pad_4FA[2];
    s32      field_4FC;
    s16      field_500;
    /// Flag word the attack sequences raise: bit 0x4000 is set by state 0 of
    /// both `func_actor_402200_80135630` and `func_actor_402200_80135A24`,
    /// alongside clearing bit 0x4000 of the 0x502 word below.
    u16      field_502;
    GpRec18  field_504[4];
    byte     field_564[8];
    void*    field_56C;
    GpRec18* field_570;
    s16      field_574;
    s16      field_576;
    s16      field_578;
    byte     pad_57A[2];
    s32      field_57C;
    s16      field_580;
    u16      field_582;
    GpRec18  field_584;
    byte     field_59C[8];
    void*    field_5A4;
    void*    field_5A8;
    s16      field_5AC;
    s16      field_5AE;
    s16      field_5B0;
    byte     pad_5B2[2];
    s32      field_5B4;
    s16      field_5B8;
    u16      field_5BA;
    byte     field_5BC[8];
    void*    field_5C4;
    GpRec18* field_5C8;
    s16      field_5CC;
    s16      field_5CE;
    s16      field_5D0;
    byte     pad_5D2[2];
    s32      field_5D4;
    s16      field_5D8;
    u16      field_5DA;
    s16      field_5DC;
    s16      field_5DE;
    s16      field_5E0;
    byte     pad_5E2[2];
    s16      field_5E4;
    s16      field_5E6;
    s16      field_5E8;
    byte     pad_5EA[2];
    s16      field_5EC;
    s16      field_5EE;
    GpRec18* field_5F0;
    /// Head of the actor's first `GpRec18` table; `func_actor_402200_801329A4`
    /// branches on its `key` before clearing it.
    GpRec18  field_5F4;
    byte     field_60C[8];
    void*    field_614;
    void*    field_618;
    s16      field_61C;
    s16      field_61E;
    s16      field_620;
    byte     pad_622[2];
    s32      field_624;
    s16      field_628;
    u16      field_62A;
    s16      field_62C;
    s16      field_62E;
    s16      field_630;
    byte     pad_632[2];
    s16      field_634;
    s16      field_636;
    s16      field_638;
    byte     pad_63A[2];
    s16      field_63C;
    s16      field_63E;
    GpRec18* field_640;
    /// Head of a second `GpRec18` table, cleared by state 5 of
    /// `func_actor_402200_801329A4`.
    GpRec18 field_644;
    /// `func_800FDB18` argument record for the hit spark: the fourth part's
    /// coordinate, 0x500, 2.
    GpEffArg field_65C;
    s32      field_664;
    s32      field_668;
    s32      field_66C;
    byte     pad_670[4];
    /// Copy of the root coordinate's matrix `func_actor_402200_80134968`
    /// takes when its fade-out finishes, with `scale` reset to 0x1000 beside
    /// it.
    MATRIX field_674;
    /// Per-axis scale applied to the root's rotation.
    VECTOR scale;
    s32    field_6A4;
    s32    field_6A8;
    s32    field_6AC;
    byte   pad_6B0[4];
    /// Box table the scans `func_actor_402200_80132D78` and
    /// `func_actor_402200_80132688` walk, `field_6FA` entries of 0x10 bytes
    /// each.
    Actor402200Region* field_6B4;
    s32                field_6B8;
    /// Sound event id the sequence body `func_actor_402200_8013539C` queues: the
    /// overlay's cue word `D_actor_402200_80138468` with the `GpEnemy` work id's
    /// high nibble in bits 8-11, the same construction the cue body
    /// `func_actor_402200_80135BE0` uses on `D_actor_402200_80138420`. Stored
    /// back to the block and re-read from there as the first argument of
    /// `SndEvt_EnqueueType6`.
    s32 field_6BC;
    /// Animation id the frame code reseeds slots 1..0x12 with; the reseed body
    /// `func_actor_402200_80137EEC` also indexes the blend table
    /// `D_actor_402200_801383AC` with it.
    s16 field_6C0;
    /// Animation id the slots were last reseeded with, so the reseed runs once
    /// per change rather than every frame.
    s16 field_6C2;
    /// Frames the current animation has been ticking; the reseed clears it and
    /// the tick path walks it up by one a frame.
    s16 field_6C4;
    /// Flinch countdown: `func_actor_402200_80131F54` arms it from
    /// `Gp_GetIdParam2` when a hit lands and ticks it down a frame at a time,
    /// raising `field_494`/`field_49A` on the frame it runs out. While it is
    /// non-zero a hit is already being flinched, so the sequence bodies arm
    /// the pair immediately only when it is zero.
    s16 field_6C6;
    /// Cleared on the frame the sequence body `func_actor_402200_8013539C`
    /// reseeds the animation.
    s16 field_6C8;
    u16 field_6CA;
    /// Set to 4 when the sequence restarts in mode 2, cleared otherwise.
    s16 field_6CC;
    /// State `func_actor_402200_8013539C` advances: 0 reseeds the animation at
    /// `field_6C0` and arms the cue, 1 waits for `field_6C4` to reach 0x37 and
    /// then drops the state back to 0 so the reseed runs again.
    /// The attack sequence `func_actor_402200_801354B0` runs the same shape
    /// over three states: its state 0 picks between slot sets 9 and 0xA on
    /// `field_6D2` and parks the state on the matching one, and states 1 / 2
    /// each wait out their own `field_6C4` threshold (0x50 and 0x3B) before
    /// dropping back to 0.
    s16 field_6CE;
    s16 field_6D0;
    /// Which-side flag the target body `func_actor_402200_80131F54` raises from
    /// a dot product of the offset to the actor it is tracking: 1 when the
    /// product comes out zero, 0 otherwise. The sequence bodies branch on it -
    /// `func_actor_402200_801354B0` picks between slot sets 9 and 0xA, and
    /// `func_actor_402200_80135630` / `func_actor_402200_80135A24` between 0xD
    /// and the set at `field_6C0`.
    s16 field_6D2;
    /// Countdown `func_actor_402200_801347F4` rolls from the `Gp_LcgState` LCG
    /// (0x4B..0x6A) when it reseeds the animation, and ticks down a frame at a
    /// time until it runs out and the cue fires.
    u16 field_6D4;
    s16 field_6D6;
    s16 field_6D8;
    /// Timer pair the reseed arms alongside `field_6DE`.
    s16 field_6DA;
    s16 field_6DC;
    /// Third timer the reseed arms; written last of the three.
    s16 field_6DE;
    /// Fourth timer `func_actor_402200_801347F4` clears alongside the trio
    /// above when its countdown runs out.
    s16 field_6E0;
    s16 field_6E2;
    s16 field_6E4;
    s16 field_6E6;
    s16 field_6E8;
    s16 field_6EA;
    /// Sequence mode `func_actor_402200_8013539C` tests: the reseed arms the
    /// cue unless it is already 1, and a restart that finds it 1 flips it to 2.
    s16 field_6EC;
    s16 field_6EE;
    /// Latch the attack sequences park the slot set in: state 0 stores 1 or 2
    /// next to `field_6C0`, and state 1 reads it back to pick the frame count
    /// it waits for (0x2C for the 0x11 animation, 0x19 otherwise).
    s16 field_6F0;
    /// Pair `func_actor_402200_80135A24` parks at 2 while it runs, cleared when
    /// its countdown runs out.
    s16 field_6F2;
    s16 field_6F4;
    s16 field_6F6;
    s16 field_6F8;
    /// Entry count of the box table at `field_6B4`, read as a signed halfword;
    /// a non-positive count disarms the scan.
    s16 field_6FA;
    /// Screen x / y and quartered depth of the two points
    /// `func_actor_402200_80135D5C` projects.
    s16 field_6FC[2];
    s16 field_700[2];
    s16 field_704[2];
    /// Index of the box the scan last reported a hit on.
    s16 field_708;
    s16 field_70A;
    s16 field_70C;
    s16 field_70E;
    s16 field_710;
    s16 field_712;
    /// Second per-state latch, read and written as a signed halfword: the
    /// attack sequences raise it to 1 in state 0 and state 1 bumps it to 2 on
    /// the frame it still equals the state.
    s16 field_714;
    /// Damage amount the hit handlers OR into `field_494`; read as a signed
    /// halfword on the frame the hit lands.
    s16 field_716;
    s16 field_718;
    s16 field_71A;
} Actor402200Work;
STATIC_ASSERT_SIZEOF(Actor402200Work, 0x71C);

/// 0x18-byte block `func_actor_402200_80132E34` takes from `G_SCRATCH_HEAD`
/// to place the actor relative to the player: `in` is the offset rotated
/// through the player's root coordinate into `out`.
typedef struct Actor402200OffsetScratch {
    VECTOR  out;
    SVECTOR in;
} Actor402200OffsetScratch;
STATIC_ASSERT_SIZEOF(Actor402200OffsetScratch, 0x18);

/// 0x48-byte block `func_actor_402200_80135D5C` takes from `G_SCRATCH_HEAD`
/// to aim the actor: `m` is the root's world matrix brought local to the
/// fourth part, `out` the GTE's rotated offset, and `pts` the two world points
/// (root-based aim point, fourth-part offset) projected through `GsWSMATRIX`
/// into `sxy` and the quartered screen z `otz`.
typedef struct Actor402200AimScratch {
    MATRIX  m;
    VECTOR  out;
    SVECTOR pts[2];
    s32     sxy;
    s32     otz;
} Actor402200AimScratch;
STATIC_ASSERT_SIZEOF(Actor402200AimScratch, 0x48);

/// One 4-byte entry of `D_actor_402200_801383D8`: the first entry whose
/// `frame` is not below the animation frame `Actor402200Work::field_6C4`
/// supplies `value` for `field_6C8`.
typedef struct Actor402200FrameStep {
    s16 frame;
    u16 value;
} Actor402200FrameStep;
STATIC_ASSERT_SIZEOF(Actor402200FrameStep, 4);

/// 0x30-byte block `func_actor_402200_80131F54` takes from `G_SCRATCH_HEAD`:
/// `delta` receives the `func_800E0C10` push-back and is then reused for the
/// offset to the player, and `ofs` is the spark offset handed to
/// `func_800FDB18`.
typedef struct Actor402200HitScratch {
    GpDeltaScratch delta;
    byte           pad_10[0x10];
    SVECTOR        ofs;
    byte           pad_28[8];
} Actor402200HitScratch;
STATIC_ASSERT_SIZEOF(Actor402200HitScratch, 0x30);

/* actor_323000 and actor_323400 carry the same enemy code. Function names in
 * these comments are actor_323000's. */

/// The 0x934-byte work block the spawn handler allocates and hangs behind
/// `Task::work`. Only the fields the handlers touch are known: `field_4` is
/// the state-change flag every state handler tests, and `field_828` onwards
/// are the animation-state slots the state handlers seed and the tick keeps.
///
/// It holds two animation contexts, each a `GpAnimCtx`, its 18-slot array
/// and the 0x120-byte pose buffer `func_800B3F84` is handed as its arg3: the
/// main one at 0x1C and the blend one at 0x420. The light / colour matrices
/// the spawn handler binds to `TmdObject::lightMtx` / `colorMtx` sit after
/// the animation state.
typedef struct Actor323000Work {
    /// Animation state, the index `func_actor_323000_801645A4` dispatches on;
    /// `func_actor_323000_80164A54` picks it from a message, and the message
    /// 0x7D3 handler `func_actor_323000_80164AF0` restarts it at 1.
    s16 field_0;
    /// State `func_actor_323000_801645A4` ran last frame; `field_4` is set
    /// when `field_0` differs from it.
    s16 field_2;
    s16 field_4;
    /// Frame counter `func_actor_323000_8016420C` advances to time its
    /// effects; zeroed when that state or `func_actor_323000_80164C58`
    /// starts.
    s16  field_6;
    byte pad_8[0xE];
    /// Yaw of the root coordinate as the placement handler
    /// `func_actor_323000_80164954` leaves it, read back from the matrix.
    s16        field_16;
    byte       pad_18[4];
    GpAnimCtx  anim;
    GpAnimSlot slots[18];
    /// Pose buffer `func_800B3F84` takes as its arg3, `GpAnimCtx.poses`.
    byte       poses[0x120];
    GpAnimCtx  blendAnim;
    GpAnimSlot blendSlots[18];
    byte       blendPoses[0x120];
    byte       pad_824[4];
    /// Animation-state slots the handlers seed and the tick keeps: the seed
    /// mode the tick acts on (1 re-seeds from the per-state table, 2 resets
    /// the slots, 3 runs), whether the blend context is live, the clip the
    /// slots were last seeded with and the one to seed next, and the slot
    /// rate.
    s16 field_828;
    s16 field_82A;
    s16 field_82C;
    s16 field_82E;
    u16 field_830;
    s16 field_832;
    s16 field_834;
    s16 field_836;
    s16 field_838;
    s16 field_83A;
    s16 field_83C;
    s16 field_83E;
    s16 field_840;
    s16 field_842;
    /// Turn angle the tick eases toward `field_840` and splits over the body
    /// joints; cleared by the spawn handler.
    s16  field_844;
    byte pad_846[2];
    /// Clip id each slot was last seen playing by `func_actor_323000_80163448`,
    /// indexed like `slots`; zeroed (18 entries) when no watched clip plays.
    s32  field_848[18];
    byte pad_890[4];
    /// Light / colour matrices `func_actor_323000_80163EA0` binds to the
    /// model.
    MATRIX light;
    MATRIX color;
    byte   pad_8D4[0x48];
    /// Three bytes `func_actor_323000_80164A54` takes from a message payload
    /// one at a time; nothing else in this overlay reads them.
    u8   field_91C;
    u8   field_91D;
    u8   field_91E;
    byte pad_91F[0x15];
} Actor323000Work;
STATIC_ASSERT_SIZEOF(Actor323000Work, 0x934);

/// 0x1C-byte block `func_actor_323000_801645A4` pushes on `G_SCRATCH_HEAD`:
/// the model root's world position for `Gp_UpdateActorColor`, and the local
/// point walked up the coordinate chain into view space.
typedef struct Actor323000TickScratch {
    VECTOR  pos;
    SVECTOR local;
    s32     pad_18;
} Actor323000TickScratch;
STATIC_ASSERT_SIZEOF(Actor323000TickScratch, 0x1C);

/// Work block of the animated actor whose code actor_110300 and actor_110800
/// both carry, reached through a global the spawn publishes: the animation
/// context at the front, its slots and pose records, and the step and
/// animation-id state. actor_110800 also cues sounds by frame, which is the
/// only use of `field_47C`.
typedef struct Actor110300Work {
    GpAnimCtx  anim;
    GpAnimSlot slots[0x14]; // the slot array `func_800B3F84` is handed
    byte       aux[0x140];  // `GpAnimCtx.poses`, one 0x10-byte record per slot
    s16        field_474;   // actor step: 1 and 2 select the body to run, which then advances it to 3
    s16        field_476;   // copy of `animId`, kept for change detection
    u16        animId;      // animation id the slots are seeded with
    u16        field_47A;   // incremented by the step-0 handler, cleared by the animation-start handler
    s16        field_47C;   // frame slot 19 or 16 last cued a sound for (actor_110800 only), kept for change detection
    byte       pad_47E[0xDE];
} Actor110300Work;
STATIC_ASSERT_SIZEOF(Actor110300Work, 0x55C);

/* actor_403200 and actor_444000 carry variants of the same boss code; their
 * main work blocks still differ in the declared signedness of the state
 * index, but the helper tasks' blocks and the tables are the same. Function
 * and data names in these comments are actor_403200's. */

/// Ten-set view of one `Gp_PlayerAnimBlkTbl` entry: an array of animation-set
/// pointers. The launch state reads `sets[7]` and the grab's hold state
/// `sets[9]`.
typedef struct Actor403200AnimTable {
    GpAnimSet* sets[10];
} Actor403200AnimTable;
STATIC_ASSERT_SIZEOF(Actor403200AnimTable, 0x28);

/// Scratch coordinate with word access to its identity rotation matrix.
typedef union Actor403200DropCoord {
    GsCOORDINATE2 c;
    struct {
        s32 flg;
        s32 m00_m01;
        s32 m02_m10;
        s32 m11_m12;
        s32 m20_m21;
        s16 m22;
    } ident;
} Actor403200DropCoord;
STATIC_ASSERT_SIZEOF(Actor403200DropCoord, 0x50);

/// One of the nine back-to-back collision groups in `Actor403200Work` at
/// 0x7F4. `obj` is the `GpObj` the gameplay collision list carries and `recs`
/// is the `GpRec18` table it fills in for that part, which is why the stride is
/// 0x98. `obj.field_8` is the part's own coordinate -- what the hit handler
/// spawns the hit effect on. The same shape as `Actor403200HitGroup`.
typedef struct Actor403200HitGroup {
    GpObj   obj;
    GpRec18 recs[5];
} Actor403200HitGroup;
STATIC_ASSERT_SIZEOF(Actor403200HitGroup, 0x98);

/// 0x30-byte scratchpad frame the group-0 hit handler
/// `func_actor_403200_80139A60` carves off `SCRATCH_HEAD` for the one hit it
/// takes this frame. `pos` is the contact point copied out of the `GpRec18`;
/// `delta` is the player-relative offset whose length is `dist`, the range
/// `Gp_ComputeDamage` scales `damage` by. `rot` doubles as `Gp_SpawnEff`'s
/// rotation argument and, afterwards, as the workspace for the contact point
/// relative to the part's world translation, which `angle` is the yaw of.
typedef struct Actor403200HitScratch {
    VECTOR3 delta;
    byte    pad_C[0x4];
    SVECTOR rot;
    SVECTOR pos;
    s32     id;     // attack id of the hit that landed, 0 for none
    u32     damage; // HP taken off the enemy
    s32     dist;   // distance from the player, in world units
    s16     angle;  // yaw of the contact point, wrapped to +/-0x800
    byte    pad_2E[0x2];
} Actor403200HitScratch;
STATIC_ASSERT_SIZEOF(Actor403200HitScratch, 0x30);

/// 0xC-byte scratchpad frame `func_actor_403200_8013EF6C` carves off
/// `SCRATCH_SP` for the escort-spawn tick: `delta` is the player-relative
/// offset the tick yaws the host by, and `i` is the escort slot the loop and
/// the 0x7DB message both index `Actor403200Work::field_EE8` with.
typedef struct Actor403200SpawnScratch {
    SVECTOR delta;
    byte    pad_8[0x2];
    s16     i; // escort slot, 0 or 1
} Actor403200SpawnScratch;
STATIC_ASSERT_SIZEOF(Actor403200SpawnScratch, 0xC);

/// Scratchpad frame the hit-effect spawner `func_actor_403200_80134044` carves
/// off `SCRATCH_SP` to hand `func_800FDB18` an effect rotation together with
/// the `GpEffArg` naming the coordinate the effect hangs off.
typedef struct Actor403200EffScratch {
    SVECTOR  rot; // effect rotation, chosen from the attack's param 0
    GpEffArg eff; // coordinate, 0x500, 3
} Actor403200EffScratch;
STATIC_ASSERT_SIZEOF(Actor403200EffScratch, 0x10);

/// Work block of the enemies spawned through `D_actor_403200_80131E90`,
/// `D_actor_403200_80131E9C` and `D_actor_403200_80131F04`: their spawn states
/// allocate it with `memCalloc(0x1C0, 0)` and park it in the task's
/// `Task::work` slot, so the size below is the allocation, not a guess.
///
/// The spawn states drop the model onto the view coordinate and hang one or two
/// `GpObj` display nodes off it. `rec0` is the table the first node carries,
/// `rec1` the second's; the two matrices are handed out through the task's
/// `TmdObject::lightMtx` / `colorMtx`. `field_1AA` is a ninth of the model's
/// height and `field_1AC` the step counter, both re-read by the states that
/// follow the spawn.
typedef struct Actor403200GrabWork {
    /// Horizontal gap to the player, a fifteenth of which the later states add
    /// to the model each step; only `vx` and `vz` are filled in here.
    VECTOR3 vel;
    byte    pad_C[0x54];
    /// The work block's own coordinate, parented to the view coordinate and
    /// kept tracking the model's world position so the ground marker under it
    /// can be drawn from `coord.workm.t`.
    GsCOORDINATE2 coord;
    /// The two display nodes, linked with `prio` 3 and 2.
    GpObj obj0;
    GpObj obj1;
    /// Their collision-record tables.
    GpRec18 rec0;
    GpRec18 rec1;
    byte    pad_120[0x30];
    /// The colour and light matrices: `field_1C` of the task's `TmdObject` is
    /// handed `lightMtx` and `field_20` `colorMtx`.
    MATRIX colorMtx;
    MATRIX lightMtx;
    byte   pad_190[0x4];
    /// Message 0x3FF payload the hold states send the player, by address.
    GpAnimArg anim;
    /// Armed to 1 by the spawn state `func_actor_403200_8013509C` once the
    /// model has been stood up on its escort's part 1; the states that follow
    /// re-arm the step counter and the first display node on the tick they see
    /// it set. Same slot and role as `Actor403200GrabWork::field_1A8`.
    s16  field_1A8;
    s16  field_1AA;
    s16  field_1AC;
    byte pad_1AE[0x2];
    /// Radius of the ground marker, in eighths once shifted down.
    u16 field_1B0;
    /// Set while the player animation this enemy sent is installed, so only
    /// the state that set it sends the cancel.
    s16 field_1B2;
    /// The state the dispatcher last ran, so it can spot a change.
    s16  field_1B4;
    byte pad_1B6[0xA];
} Actor403200GrabWork;
STATIC_ASSERT_SIZEOF(Actor403200GrabWork, 0x1C0);

/// Work block of the enemy dispatched through `D_actor_403200_80131F14`, the one
/// that rises out of view and slams back down onto the floor. Its spawn state
/// allocates it with `memCalloc(0x1C0, 0)` and parks it in the task's
/// `Task::work` slot, so the size is the allocation.
///
/// `target` is the landing point the spawn state picks; `coord` is the
/// coordinate its shadow marker is drawn at, kept on the floor directly under
/// the model and refreshed every step; `obj` is its collision node, whose
/// `radius` is the marker size, carrying the one-entry `rec` table. `timer` is
/// the step counter of the current state.
typedef struct Actor403200DropWork {
    VECTOR3       target;
    byte          pad_C[0x4];
    GsCOORDINATE2 coord;
    byte          pad_60[0x50];
    GpObj         obj;
    byte          pad_D0[0x20];
    GpRec18       rec;
    byte          pad_108[0x88];
    /// The effect the spawn state starts, reparented onto the task so it dies
    /// with it; the landing state tells it to finish.
    GpEffWork* eff;
    byte       pad_194[0x16];
    s16        field_1AA;
    u16        timer;
    /// Per-step bias of the rise and fall, rolled off the LCG.
    s16  field_1AE;
    byte pad_1B0[0x10];
} Actor403200DropWork;
STATIC_ASSERT_SIZEOF(Actor403200DropWork, 0x1C0);

/// Work block of the spinner enemy dispatched through `D_actor_403200_80131F28`:
/// its spawn state allocates it with `memCalloc(0xA0, 0)` and parks it in the
/// task's `Task::work` slot. `spin` counts down while the model only yaws in
/// place and is also the phase that yaw follows; `field_98` is the homing
/// speed and radius and `field_96` the step count that accelerates it.
typedef struct Actor403200SpinnerWork {
    byte   pad_0[0x50];
    MATRIX colorMtx;
    MATRIX lightMtx;
    /// Set when the dispatcher sees the state change, cleared when it has not.
    s16  field_90;
    byte pad_92[0x2];
    /// The state the dispatcher last ran, so it can spot the change.
    s16  field_94;
    s16  field_96;
    s16  field_98;
    byte pad_9A[0x2];
    u8   spin;
    byte pad_9D[0x3];
} Actor403200SpinnerWork;
STATIC_ASSERT_SIZEOF(Actor403200SpinnerWork, 0xA0);

/* actor_05600, actor_05700 and actor_02300 carry the same enemy code, and
 * actor_02000 shares its hit handling. Function names in these comments are
 * actor_05600's or actor_05700's. */

/// Work block of that enemy, allocated at its full size and kept at
/// `Task::work`: the animation context with its nineteen slots and pose
/// records, the light and colour matrices, the body objects with their
/// collision records, and the animation and state halfwords the handlers
/// drive.
typedef struct Actor105600Work {
    GpAnimCtx  anim;
    GpAnimSlot slots[19];
    byte       poses[0x130];
    MATRIX     field_43C;
    MATRIX     field_45C;
    /// First body object, handed to `Gp_UnlinkObj` by the teardown of
    /// `Actor05700_Fn01A58`; its `pos.vz` is the pose the state-0
    /// branch of `Actor05700_Fn01318` parks (-0xA7 or 0x109) and its
    /// `radius` the frame count parked alongside it.
    GpObj        field_47C;
    GpActorD4Rec field_49C;
    GpRec18      field_4B4[1];
    /// Second body object; `pos.vz` is the pose the state-0 branch parks
    /// (0x15E) and `flags` the bits whose 0x4000 it raises.
    GpObj   field_4CC;
    GpRec18 field_4EC[5];
    /// Third body object; `flags` is the field whose bit 0x4000 the state-0
    /// branch clears.
    GpObj   field_564;
    GpRec18 field_584[4];
    /// Fourth body object: `key` is the object `Gp_PackPair` hands it when
    /// `field_698` first reaches the animation's 0x1C mark and `flags` the
    /// bits whose 0x8000 is raised with it and dropped at the 0x28 mark
    /// (`Actor05700_Fn023AC`).
    GpObj   field_5E4;
    GpRec18 field_604[1];
    /// Fifth body object, unlinked with the others by `Actor05700_Fn01A58`.
    GpObj        field_61C;
    GpActorD4Rec field_63C;
    GpRec18      field_654[1];
    TaskDesc*    field_66C;
    GpEffArg     field_670;
    s32          field_678;
    s32          field_67C;
    s32          field_680;
    byte         pad_684[4];
    /// Tilt angles decayed toward zero by `Actor05700_Fn016D0`.
    SVECTOR           field_688;
    struct GpEffWork* field_690;
    /// Animation index selected by the state machine; 4 is the "handover"
    /// clip of `Actor05700_Fn04CC0`'s state 0.
    s16 field_694;
    /// Animation the playing clip was started from; when it differs from
    /// `field_694` the frame counter is reset and the slots reseeded.
    s16 field_696;
    s16 field_698; ///< current frame of the playing clip
    s16 field_69A;
    s16 field_69C; ///< dwell counter, cleared on state 0 entry
    s16 field_69E; ///< dwell counter, cleared on state 0 entry
    u16 field_6A0; ///< sound flags; bit 5/4 gate the two cues
                   /// Current yaw, walked toward `field_6A4` by
                   /// `Actor05700_Fn01544`, using `field_69E` as the per-frame step.
    s16 field_6A2;
    s16 field_6A4; ///< yaw the actor wants to face
    s16 field_6A6; ///< parked animation for the state-F0 path
    s16 field_6A8; ///< state-machine step
    s16 field_6AA; ///< animation the state-0 branch picks
    s16 field_6AC;
    s16 field_6AE; ///< state-0 frame budget
    s16 field_6B0;
    s16 field_6B2; ///< non-zero forces the state-F0 path
    s16 field_6B4; ///< cleared once the tilt has settled
    s16 field_6B6;
    /// State-0 branch selector: 1 picks the short dwell and animation 1,
    /// 2 the long dwell and animation 2.
    s16  field_6B8;
    s16  field_6BA;
    s16  field_6BC;
    s16  field_6BE;
    s16  field_6C0;
    s16  field_6C2;
    s16  field_6C4;
    byte pad_6C6[4];
    /// Body variant select: `Actor05700_Fn01A58` drops the fifth body
    /// object for the two values 0x38 / 0x39 and hands the halfword to
    /// `Gp_ReleaseStateF0Add`.
    s16 field_6CA;
    s16 field_6CC;
    s16 field_6CE;
    s16 field_6D0;
    /// Spawn state driven by `Actor05700_Fn05310`: 0 clears the
    /// coordinate, 1 fires the effect burst and sound cue, 2 is idle.
    s16 field_6D2;
    /// Latched on state-0 entry, cleared when the frame budget runs out.
    s16 field_6D4;
    s16 field_6D6; ///< animation index, used as a table row
    s16 field_6D8;
    s16 field_6DA; ///< state-0 frame budget, drained by `field_69C`
    s16 field_6DC;
    /// State-1 step gate: 1 while the state-0 exit is still to be seen, 2
    /// once it has been.
    s16 field_6DE;
    /// State-1 branch selector: zero picks the short dwell and animation 2,
    /// non-zero the long dwell and animation 0x14.
    s16  field_6E0;
    byte pad_6E2[2];
} Actor105600Work;
STATIC_ASSERT_SIZEOF(Actor105600Work, 0x6E4);

/// 0xF0-byte body block `Actor05600_Fn031B0` parks at `Task::work`.
/// The two leading matrices are the light/colour pair published on the model
/// root's `TmdObject`; the three `GpObj` bodies collide against `rec60`
/// (shared by the first two) and, through the `GpActorD4Rec` between them,
/// `recD0`. `field_EE` mirrors the placement table's variant flag.
typedef struct Actor105600FxWork {
    MATRIX       colorMtx;
    MATRIX       lightMtx;
    GpObj        obj40;
    GpRec18      rec60[1];
    GpObj        obj78;
    GpObj        obj98;
    GpActorD4Rec d4rec;
    GpRec18      recD0[1];
    s16          field_E8;
    s16          field_EA;
    /// Teardown step: 0 unlinks the three bodies, 1 counts `field_E8` up to
    /// the frame the task is destroyed on.
    s16 field_EC;
    s16 field_EE;
} Actor105600FxWork;
STATIC_ASSERT_SIZEOF(Actor105600FxWork, 0xF0);

/// 0x14-byte placement descriptor in the overlay's `.data`, handed to
/// `Gp_PackPair` as the source of the body objects' `GpObj.key`.
/// `field_E` is the variant flag `Actor05600_Fn031B0` latches into its
/// work block: it is 1 when the actor is placed normally, and anything else
/// puts the body in the other pose.
typedef struct Actor105600PlaceSrc {
    GpU16Pair pair;
    u16       field_4;
    u16       field_6;
    u16       field_8;
    u16       field_A;
    u16       field_C;
    u16       field_E;
    u16       field_10;
    u16       field_12;
} Actor105600PlaceSrc;
STATIC_ASSERT_SIZEOF(Actor105600PlaceSrc, 0x14);

/// 0x40-byte scratch carved off `G_SCRATCH_HEAD` by `Actor05600_Fn02548`:
/// the converted matrix, the `gte_rtv0` output and the two vectors fed through
/// it (`rot` and `vec` are also the pair handed to `Actor05600_Fn02950`).
typedef struct Actor105600AimScratch {
    MATRIX  mtx;
    VECTOR  pos;
    SVECTOR rot;
    SVECTOR vec;
} Actor105600AimScratch;
STATIC_ASSERT_SIZEOF(Actor105600AimScratch, 0x40);

/// 0x48-byte scratch carved off `G_SCRATCH_HEAD` by `Actor05600_Fn02950`:
/// the beam is walked in eight steps from `vec` to `rot`, each step projected
/// into `cur` (packed screen xy) and `curZ` (OTZ). `xs`/`ys` hold the two
/// projected ends followed by the four offset corners the ribbon polygons are
/// cut from.
typedef struct Actor105600BeamScratch {
    VECTOR  vec;
    SVECTOR pt;
    SVECTOR step;
    s32     prev;
    s32     cur;
    s32     prevZ;
    s32     curZ;
    s16     xs[6];
    s16     ys[6];
} Actor105600BeamScratch;
STATIC_ASSERT_SIZEOF(Actor105600BeamScratch, 0x48);

/// 0x38-byte scratch carved off `G_SCRATCH_HEAD` by
/// `Actor05600_Fn031B0`. `rot` first holds the local offset the root
/// coordinate is translated by (through `gte_rtv0` into `pos`), then the
/// placement angles `RotMatrix` turns into `mtx` for the three `rtir` column
/// transforms that overwrite the root coordinate's matrix.
typedef struct Actor105600PlaceScratch {
    SVECTOR rot;
    VECTOR  pos;
    MATRIX  mtx;
} Actor105600PlaceScratch;
STATIC_ASSERT_SIZEOF(Actor105600PlaceScratch, 0x38);

/// Scratch-pad block of that enemy's push-back and hit: the deltas the
/// collision walk resolves, their normal, the push, and the effect offset and
/// target.
typedef struct Actor105600HitScratch {
    GpDeltaScratch delta;
    VECTOR         normal;
    VECTOR         push;
    SVECTOR        effOfs;
    SVECTOR        target;
} Actor105600HitScratch;
STATIC_ASSERT_SIZEOF(Actor105600HitScratch, 0x40);

/// Work block of the animated enemy whose code actor_461800 and actor_143900's
/// second variant both carry, allocated zeroed at its full size and kept both
/// at `Task::work` and in a global the other handlers reach it through: the
/// model's light and colour matrices, the animation context with its slots
/// and poses, the animation state, the yaw seeding the root coordinate, and
/// the two helper tasks the exit callback kills.
typedef struct Actor461800Work {
    MATRIX     light; // model light matrix (`TmdObject::lightMtx`)
    MATRIX     color; // model colour matrix (`TmdObject::colorMtx`)
    GpAnimCtx  anim;
    GpAnimSlot slots[0x14];
    byte       pose[0x140];
    s16        field_4B4; // reset mode the play-animation handler selects (1 or 2)
    s16        field_4B6; // copy of `field_4B8`, kept for change detection
    s16        field_4B8; // animation id the slots are seeded with
    s16        field_4BA; // cleared by the play-animation handler before the reseed
    byte       pad_4BC[0x2A];
    s16        yaw;       // yaw seeding the root coordinate
    byte       pad_4E8[0x2];
    s16        field_4EA; // distance to the target over the step count
    s16        field_4EC; // reset argument handed to `func_800B4114`
    byte       pad_4EE[0x2];
    Task*      field_4F0; // first helper task the spawn starts
    Task*      field_4F4; // second helper task
} Actor461800Work;
STATIC_ASSERT_SIZEOF(Actor461800Work, 0x4F8);

/* Helpers.
 *
 * Inline bodies each actor compiled from its own copy of the same source.
 * They stay inline: the callers' code was generated with the body expanded in
 * place, which a call would not reproduce. */

/// Nonzero while movement is frozen; the stepping helpers do nothing then.
extern u8 D_80072729;

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
    OverlayRangeScratch* head;
    OverlayRangeScratch* scratch;
    head                                    = *(OverlayRangeScratch**)G_SCRATCH_HEAD;
    scratch                                 = head - 1;
    *(OverlayRangeScratch**)G_SCRATCH_HEAD  = scratch;
    scratch->dx                             = pos->vx;
    scratch->dz                             = pos->vz;
    scratch->r                              = radius;
    scratch->dx                            *= scratch->dx;
    scratch->dz                            *= scratch->dz;
    scratch->r                             *= scratch->r;
    *(OverlayRangeScratch**)G_SCRATCH_HEAD += 1;
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
