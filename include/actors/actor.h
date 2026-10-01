#ifndef INCLUDE_ACTORS_ACTOR_H
#define INCLUDE_ACTORS_ACTOR_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys_types.h"

#include "overlay.h"

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
    long    screen0;
    long    screen1;
    long    screen2;
    long    screen3;
    long    perspective;
    long    flags;
    s32     depth;
} ActorBeamScratch;
STATIC_ASSERT_SIZEOF(ActorBeamScratch, 0x8C);

/// The scratch-pad allocation pointer seen as the one member of a struct.
/// Reaching the pointer through a member rather than a bare word marks the
/// access as a structure access, which lets the scheduler order it against
/// the stores around it the way the original code was ordered. The functions
/// that use it do not compile the same through `SCRATCH_STACK_CURSOR`.
typedef struct ActorScratchStack {
    void* head;
} ActorScratchStack;
STATIC_ASSERT_SIZEOF(ActorScratchStack, 0x4);

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

/// The scratch-pad block of a facing check against the player: the offset to
/// the player, its squared length, the player's yaw and the bearing of the
/// contact, the turn and the yaw it aims for, and the reply the player's
/// contact message came back with.
typedef struct ActorFacingScratch {
    s16 vx, vy, vz, pad;
    u32 distanceSquared;
    s16 playerYaw;
    u16 contactYaw;
    s16 turnYaw, targetYaw;
    s16 messageResult;
    s16 pad16;
} ActorFacingScratch;
STATIC_ASSERT_SIZEOF(ActorFacingScratch, 0x18);

/// The scratch-pad block of a turn spread over several steps: `vec` is the
/// offset to the player and then the facing it leaves, `delta` the whole turn
/// to make, `steps` the steps it is spread over and `yaw` the heading after
/// this step.
typedef struct ActorTurnStepScratch {
    SVECTOR vec;
    s16     delta;
    s16     yaw;
    s16     steps;
    s16     pad;
} ActorTurnStepScratch;
STATIC_ASSERT_SIZEOF(ActorTurnStepScratch, 0x10);

/// A turn toward the player: the offset to the player, then the clamped turn
/// applied to the actor's root coordinate.
typedef struct ActorTurnScratch {
    SVECTOR delta;
    s16     angle;
    s16     pad_A;
} ActorTurnScratch;
STATIC_ASSERT_SIZEOF(ActorTurnScratch, 0xC);

/// The scratch-pad block of a turn toward the player that also weighs where
/// the player is facing: the offset to the player, the player's yaw, the yaw
/// from the player back to the actor, the wrapped turn toward the player and
/// the clamped turn applied. Aiming steps that only turn use `turn` alone.
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

/// A 0x38-byte scratch-pad frame around the 16.16 step `func_800E0C10` or
/// `func_800E0FEC` resolves into `delta`. The frame is taken whole so the
/// caller's own scratch, taken below the head, stays clear of it; nothing
/// else in it is read.
typedef struct ActorDeltaFrame38 {
    byte           pad_0[0x20];
    GpDeltaScratch delta;
    byte           pad_30[0x8];
} ActorDeltaFrame38;
STATIC_ASSERT_SIZEOF(ActorDeltaFrame38, 0x38);

/// The same frame at 0x48 bytes, for the steps that take the larger block.
typedef struct ActorDeltaFrame48 {
    byte           pad_0[0x20];
    GpDeltaScratch delta;
    byte           pad_30[0x18];
} ActorDeltaFrame48;
STATIC_ASSERT_SIZEOF(ActorDeltaFrame48, 0x48);

/// The scratch-pad frame of a wall contact: `delta` receives the
/// `func_800E0C10` push-back and is then reused for offsets, `normal` is the
/// normalised wall offset, and `result` the word `func_800E0C10` reports
/// through its last argument.
typedef struct ActorContactFrame {
    byte           pad_0[0x20];
    GpDeltaScratch delta;
    byte           pad_30[0x8];
    VECTOR         normal;
    s32            result;
} ActorContactFrame;
STATIC_ASSERT_SIZEOF(ActorContactFrame, 0x4C);

/// The scratch-pad frame of a push against the collision grid: `delta`
/// receives the `func_800E0C10` push-back and is then reused for each
/// record's offset, `normal` is that offset normalised, and `dir` the normal
/// brought into the grid's frame. `dx` and `dz` are the contact record's
/// normal, staged for the bearing some actors take from it.
typedef struct ActorPushFrame {
    byte           pad_0[0x20];
    GpDeltaScratch delta;
    VECTOR         normal;
    VECTOR         dir;
    s16            dx;
    byte           pad_52[0x2];
    s16            dz;
    byte           pad_56[0x2];
} ActorPushFrame;
STATIC_ASSERT_SIZEOF(ActorPushFrame, 0x58);

/// The scratch-pad frame of a contact walk: `delta` receives the move
/// `func_800E0C10` resolves and is then reused for each contact's offset,
/// `normal` is that offset normalised, and `dir` the normal carried into the
/// collision grid's frame, along which the deepest contact pushes.
typedef struct ActorWallPushFrame {
    GpDeltaScratch delta;
    VECTOR         normal;
    VECTOR         dir;
} ActorWallPushFrame;
STATIC_ASSERT_SIZEOF(ActorWallPushFrame, 0x30);

/// The scratch-pad block of an attack that pulls the player in: the
/// animation argument sent with message 0x3F4, the placement sent with
/// message 0x3E9, and the offset to the player with its normalised direction.
typedef struct ActorAttackScratch {
    AnimationPlayRequest anim;
    ActorTransform       place;
    VECTOR               delta;
    SVECTOR              dir;
} ActorAttackScratch;
STATIC_ASSERT_SIZEOF(ActorAttackScratch, 0x44);

/// The scratch-pad block of a point placed relative to a coordinate: `offset`
/// in the coordinate's frame, and `result` the world position it is rotated
/// and moved to.
typedef struct ActorOffsetScratch {
    SVECTOR offset;
    VECTOR  result;
} ActorOffsetScratch;
STATIC_ASSERT_SIZEOF(ActorOffsetScratch, 0x18);

/// The scratch-pad block of a hit the actor takes: `d` is the position of the
/// first contact record of the attacking kind, `pos` its offset from the
/// model's origin, `id` the attack, `dmg` the damage worked out from it and
/// `angle` the hit's yaw relative to the model's facing.
typedef struct ActorHitTakenScratch {
    SVECTOR d;
    SVECTOR pos;
    s32     id;
    u16     dmg;
    s16     angle;
} ActorHitTakenScratch;
STATIC_ASSERT_SIZEOF(ActorHitTakenScratch, 0x18);

/// The scratch-pad block of a bearing measured in a coordinate's own frame:
/// `delta` is the other point's offset from the coordinate, rotated in place
/// into that frame through `frame`, the transpose of the coordinate's world
/// matrix.
typedef struct ActorBearingScratch {
    SVECTOR delta;
    byte    pad_8[0x18];
    MATRIX  frame;
} ActorBearingScratch;
STATIC_ASSERT_SIZEOF(ActorBearingScratch, 0x40);

/// The scratch-pad block of a head turned to aim at the player: `view` is the
/// head coordinate in view space, `delta` the player's offset from it, and
/// `local` that offset rotated into the body's frame and clamped.
typedef struct ActorAimScratch {
    MATRIX view;
    VECTOR delta;
    VECTOR local;
} ActorAimScratch;
STATIC_ASSERT_SIZEOF(ActorAimScratch, 0x40);

/// Work block of a model that draws with its own lighting: the light and
/// colour matrices its `TmdObject::lightMtx` / `colorMtx` point at, and the
/// task that spawned it, which the spawn routine reparents to it.
typedef struct ActorLitWork {
    MATRIX light;
    MATRIX color;
    Task*  field_40;
} ActorLitWork;
STATIC_ASSERT_SIZEOF(ActorLitWork, 0x44);

/// One entry of a zone table: a rectangle on the floor from (`x`, `z`)
/// spanning `w` along X and `h` along Z, and the id a lookup returns for a
/// point inside it. A table ends at an entry whose `id` is -1.
typedef struct ActorZone {
    s16 x;
    s16 z;
    s16 w;
    s16 h;
    s16 id;
} ActorZone;
STATIC_ASSERT_SIZEOF(ActorZone, 0xA);

/// The state of the effect actor_403600's `func_actor_403600_80138C9C` and
/// `func_actor_403600_801353D0` drive, which actor_361100 also runs through
/// those two functions: two tables of 0x20 halfwords, the ramp words and
/// halfwords after them, the coordinate the effect hangs off, and two more
/// words.
typedef struct ActorEffectState {
    s16      field_0[0x20];
    s16      field_40[0x20];
    s32      field_80;
    s32      field_84;
    s32      field_88;
    s16      field_8C;
    s16      field_8E;
    GfxCoord field_90;
    s32      field_E0;
    s32      field_E4;
} ActorEffectState;
STATIC_ASSERT_SIZEOF(ActorEffectState, 0xE8);

/// A row of a small table the spawn argument's low nibble selects; its four
/// halfwords are copied into the work block when the enemy is set up, and
/// the tail is not copied.
typedef struct ActorSpawnParamRow {
    s16  field_0;
    s16  field_2;
    s16  field_4;
    s16  field_6;
    byte pad_8[0x4];
} ActorSpawnParamRow;
STATIC_ASSERT_SIZEOF(ActorSpawnParamRow, 0xC);

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

/// The animation rig of a twenty-part model: the context `func_800B3F84`
/// builds, and the playback slots and pose buffer that context points at. The
/// slots and the poses are the owner's storage, one of each per model part;
/// each pose record is in the encoding its slot's `AnimationSlot.poseEncoding` names,
/// so the buffer is kept as raw records.
typedef struct ActorAnimRig20 {
    AnimationContext anim;
    AnimationSlot    slots[0x14];
    byte             poses[0x14][ANIMATION_POSE_BUFFER_BYTES];
} ActorAnimRig20;
STATIC_ASSERT_SIZEOF(ActorAnimRig20, 0x474);

/// Caller-owned playback storage for nineteen slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 19 entries.
typedef struct {
    AnimationContext anim;                                   // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[19];                              // Playback slot for one driven index
    u8               poses[19][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig19;
STATIC_ASSERT_SIZEOF(ActorAnimRig19, 0x43C);

/// The animation and walk state an animated enemy keeps right after its rig.
/// `state` is the animation step: 1 and 2 reseed the slots from `animId`, 1
/// blending into it and 2 outright, and both advance to 3, which ticks the
/// slots. `appliedAnimId` is the id the slots were last seeded with. `yaw` is
/// the heading last given the root coordinate, and `travel` counts down the
/// steps left in a walk. `field_6`, `field_8` and `field_A` are each enemy's
/// own, and nothing the enemies share reads the bytes between them and `yaw`.
typedef struct ActorEnemyState {
    s16  state;
    s16  appliedAnimId;
    s16  animId;
    s16  field_6;
    s16  field_8;
    s16  field_A;
    byte pad_C[0x26];
    s16  yaw;
    byte pad_34[0x2];
    s16  travel;
} ActorEnemyState;
STATIC_ASSERT_SIZEOF(ActorEnemyState, 0x38);

/// One step of an animation script a cutscene controller walks, feeding the
/// player's animation: `hold` is how many frames to hold the step, 0 waiting
/// for the player to report the clip done, and `animId` the next animation,
/// sent to the player as message 0x3F4; a negative `animId` ends the script.
typedef struct ActorAnimStep {
    u16 hold;
    s16 animId;
} ActorAnimStep;
STATIC_ASSERT_SIZEOF(ActorAnimStep, 0x4);

/// One entry of a sprite frame table: the texture-page coordinates of the
/// frame, each in the low byte of its halfword.
typedef struct ActorSpriteUv {
    u8 u;
    u8 pad_1;
    u8 v;
    u8 pad_3;
} ActorSpriteUv;
STATIC_ASSERT_SIZEOF(ActorSpriteUv, 0x4);

/// The animation state and lighting a scripted actor keeps right after its
/// rig. `ticking` enables the animation tick, `animId` and `bank` are the
/// animation now playing and the bank it comes from, and `nextAnimId` is the
/// animation a later step starts. `light` and `color` are the matrices the
/// model is lit with.
typedef struct ActorModelState {
    s8     ticking;
    s8     animId;
    s8     bank;
    s8     nextAnimId;
    MATRIX light;
    MATRIX color;
} ActorModelState;
STATIC_ASSERT_SIZEOF(ActorModelState, 0x44);

/// The walk a scripted walker keeps after its model state: an actor the room
/// script places and walks from point to point. A walk heads for `target`:
/// each frame `step` is added into the 16.16 accumulators `acc`, whose high
/// halves move the root coordinate and whose low halves carry over, until the
/// remaining distance falls below `limit` on every axis, 0x7FFF disabling the
/// check; an actor that walks without one leaves `limit` unused. `rotX`, `rotY` and `rotZ` are the placement rotation, and `rotY` the
/// yaw the final turn steers toward. `motion` selects the handler the tick
/// runs, and `motionStep` the step of the walk sequence that handler is on.
typedef struct ActorWalkState {
    VECTOR3 target;
    byte    pad_C[0x4];
    VECTOR3 step;
    byte    pad_1C[0x4];
    Fixed16 acc[3];
    byte    pad_2C[0x4];
    SVECTOR limit;
    u16     rotX;
    u16     rotY;
    u16     rotZ;
    byte    pad_3E[0x2];
    s16     motion;
    s16     motionStep;
} ActorWalkState;
STATIC_ASSERT_SIZEOF(ActorWalkState, 0x44);

/// Work block of the scripted walker whose code both actor_350500 and
/// actor_350700 carry for a nineteen-part model, allocated zeroed at its full
/// size and kept at `Task::work`. `field_4C4` is the variant the
/// two-case message handler latches, and `freeCountdown` the frames until the
/// model buffers are freed, -1 disabling the countdown.
typedef struct Actor350500Work {
    ActorAnimRig19  rig;
    ActorModelState model;
    ActorWalkState  walk;
    s8              field_4C4;
    s8              freeCountdown;
    byte            pad_4C6[0x2];
} Actor350500Work;
STATIC_ASSERT_SIZEOF(Actor350500Work, 0x4C8);

/// Work block of the scripted walker whose code actor_135600 and the parent
/// actor of actor_350700 carry, allocated zeroed at its full size and kept at
/// `Task::work`: a twenty-part rig and the walk state, the three child tasks
/// the spawn routine starts, whose models the visibility command drives
/// alongside the walker's, and `freeCountdown`, the frames until the model
/// buffers are freed, -1 disabling the countdown.
typedef struct Actor135600Work {
    ActorAnimRig20  rig;
    ActorModelState model;
    ActorWalkState  walk;
    Task*           child0;
    Task*           child1;
    Task*           child2;
    s32             freeCountdown;
} Actor135600Work;
STATIC_ASSERT_SIZEOF(Actor135600Work, 0x50C);

/// Work block of the enemy whose code actor_160600, actor_160700,
/// actor_215100 and the first variant of actor_460200 carry, allocated zeroed
/// at its full size and kept at `Task::work`: the light and colour matrices
/// its model draws with, its rig and animation state, and `animArg`, the
/// argument the blended reseed passes on. `effects` nonzero turns on the
/// per-frame effect spawns of the actors that have them. `pairTask` is the
/// task whose model the visibility command drives alongside the actor's own,
/// which only the actors that spawn a partner store, and `enemy` the enemy
/// the actor's own task belongs to.
typedef struct Actor160600Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig20  rig;
    ActorEnemyState st;
    s16             animArg;
    s16             effects;
    Task*           pairTask;
    Enemy*          enemy;
} Actor160600Work;
STATIC_ASSERT_SIZEOF(Actor160600Work, 0x4F8);

/// Work block of the enemy whose code actor_161500 and the paired variant of
/// actor_460200 carry, allocated zeroed at its full size and kept at
/// `Task::work`. It is laid out as `Actor160600Work` up to `animArg`, then
/// carries a head turn: `turnWeight` is the weight, 0 to 0x1000, of the
/// per-frame turn toward the player, ramped up while `turnUp` is 1 and down
/// otherwise. `pairTask` is the task of the partner enemy the spawn routine
/// may start, which its own task is reparented under, and `enemy` the enemy
/// the actor's own task belongs to.
typedef struct Actor161500Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig20  rig;
    ActorEnemyState st;
    s16             animArg;
    s16             turnUp;
    s16             turnWeight;
    byte            pad_4F2[0x2];
    Task*           pairTask;
    Enemy*          enemy;
} Actor161500Work;
STATIC_ASSERT_SIZEOF(Actor161500Work, 0x4FC);

/// Work block of the animated actor whose code actor_110300 and actor_110800
/// both carry, reached through a global the spawn publishes: the rig at the
/// front and the animation state after it. `st.field_6` is raised by the
/// spawn routine and cleared when an animation starts, and `st.field_8` is
/// the frame slot 19 or 16 last cued a sound for, kept for change detection,
/// which only actor_110800 uses.
typedef struct Actor110300Work {
    ActorAnimRig20  rig;
    ActorEnemyState st;
    byte            pad_4AC[0xB0];
} Actor110300Work;
STATIC_ASSERT_SIZEOF(Actor110300Work, 0x55C);

/// Work block of the animated enemy whose code actor_461800 and actor_143900's
/// second variant both carry, allocated zeroed at its full size and kept both
/// at `Task::work` and in a global the other handlers reach it through: the
/// model's light and colour matrices, its rig and animation state, the frames
/// of turning left while animation 3 plays, and the two helper tasks the spawn
/// routine starts and the exit callback kills.
typedef struct Actor461800Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig20  rig;
    ActorEnemyState st;
    s16             turnFrames;
    byte            pad_4EE[0x2];
    Task*           helper1;
    Task*           helper2;
} Actor461800Work;
STATIC_ASSERT_SIZEOF(Actor461800Work, 0x4F8);

/// Work block of the animated enemy whose code actor_151000, actor_535700 and
/// actor_461800's second variant carry, allocated zeroed at its full size and
/// kept both at `Task::work` and in a global the other handlers reach it
/// through: the model's light and colour matrices, its nineteen-part rig and
/// animation state, and the frames of turning left while animation 3 plays.
/// `stepRec` is the last animation record the footstep check saw, so each
/// footstep fires once, and `footsteps` is the flag the message handler sets
/// that makes the per-frame step play them.
typedef struct Actor151000Work {
    MATRIX                 light;
    MATRIX                 color;
    ActorAnimRig19         rig;
    ActorEnemyState        st;
    s16                    turnFrames;
    byte                   pad_4B6[0x2];
    const AnimationRecord* stepRec;
    u8                     footsteps;
    byte                   pad_4BD[0x3];
} Actor151000Work;
STATIC_ASSERT_SIZEOF(Actor151000Work, 0x4C0);

/// Work block of the animated enemy whose code actor_260500 and the first
/// actor of actor_451100 carry, allocated zeroed at its full size and kept
/// both at `Task::work` and in a global the message handlers reach it
/// through: the model's light and colour matrices, its nineteen-part rig and
/// animation state, and the frames of turning left while animation 3 plays,
/// which message 0x7DB arms.
typedef struct Actor260500Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig19  rig;
    ActorEnemyState st;
    s16             turnFrames;
    byte            pad_4B6[0x2];
} Actor260500Work;
STATIC_ASSERT_SIZEOF(Actor260500Work, 0x4B8);

/// Work block of the animated enemy whose code actor_150400, actor_535700's
/// second enemy, actor_450800's spawned enemy and actor_451100's second actor
/// carry, allocated zeroed at its full size and kept at `Task::work`: the
/// matrices the enemy's model and its sub-model are lit with, its
/// nineteen-part rig and animation state, and `animArg`, the argument the
/// blended reseed passes on. `pairTask` is the task of the partner model the
/// spawn routine starts and reparents the enemy's own task under, whose model
/// the visibility command drives alongside the enemy's, and `enemy` the enemy
/// the block belongs to.
typedef struct Actor150400Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig19  rig;
    ActorEnemyState st;
    s16             animArg;
    byte            pad_4B6[0x2];
    Task*           pairTask;
    Enemy*          enemy;
} Actor150400Work;
STATIC_ASSERT_SIZEOF(Actor150400Work, 0x4C0);

/// Bearing of `other` from `self`, measured in `self`'s own frame and folded
/// into -0x800..0x800. The offset between the two world positions is written
/// to `blk->delta` and rotated there through `blk->frame`, the transpose of
/// `self`'s world matrix; the caller owns `blk` and may reuse it afterwards.
static __inline__ s32 actorBearingInFrame(ActorBearingScratch* blk, GfxCoord* self, GfxCoord* other)
{
    s32 angle;

    blk->delta.vx = other->workm.t[0] - self->workm.t[0];
    blk->delta.vy = other->workm.t[1] - self->workm.t[1];
    blk->delta.vz = other->workm.t[2] - self->workm.t[2];
    TransposeMatrix(&self->workm, &blk->frame);
    gfxRotateSv(&blk->frame, &blk->delta);
    angle = ratan2(blk->delta.vx, blk->delta.vz);
    if (angle >= 0x801) {
        angle -= 0x1000;
    } else if (angle < -0x800) {
        angle += 0x1000;
    }
    return angle;
}

/// The push that moves `pos` out of the contact record `rec`: how deep `pos`
/// sits inside the record's radius, along the direction from the record's
/// centre carried into grid space. Only X and Z are written.
static __inline__ void actorCalcPush(SVECTOR* pos, WorldCollisionContact* rec, SVECTOR* out)
{
    VECTOR d;
    VECTOR n;
    s32    t;
    s32    pen;

    d.vx = pos->vx - rec->point.vx;
    d.vy = 0;
    d.vz = pos->vz - rec->point.vz;
    pen  = SquareRoot0(d.vx * d.vx + d.vz * d.vz);
    pen  = rec->distance - pen;
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
    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &n, &d);
    out->vx = (pen * d.vx) >> 12;
    out->vy = 0;
    out->vz = (pen * d.vz) >> 12;
}

/// Builds `joint`'s absolute rotation in `out`: its own rotation with each
/// ancestor pre-multiplied in turn, renormalised after every step, up to but
/// not including `stop`. Returns whether the walk reached `stop` rather than
/// the end of the chain.
static __inline__ s32 actorAccumulateRotation(GfxCoord* joint, MATRIX* out, GfxCoord* stop)
{
    MATRIX    matrix;
    GfxCoord* coord;

    coord = joint->parent;
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
        coord = coord->parent;
    }
}

/// Turns the world-space `rotation` into one relative to `joint`'s parent:
/// accumulates the chain above the parent up to the view coordinate,
/// transposes it and pre-multiplies. Nothing happens when the parent is the
/// view coordinate. Returns `joint`, which callers store through.
static __inline__ GfxCoord* actorLocalizeRotation(GfxCoord* joint, MATRIX* rotation)
{
    MATRIX    matrix;
    MATRIX    normal;
    MATRIX    transposed;
    GfxCoord* coord;
    GfxCoord* view;

    coord = joint->parent;
    if (coord != &gGfxViewCoord) {
        view   = &gGfxViewCoord;
        matrix = coord->coord;
        while (1) {
            coord = coord->parent;
            if (coord == NULL) {
                break;
            }
            if (coord == view) {
                gte_TransposeMatrix(&matrix, &transposed);
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
static __inline__ void actorTransformToView(GfxCoord* p, SVECTOR* out)
{
    SVECTOR   sv;
    VECTOR    vec;
    s32       flag;
    SVECTOR*  svp   = &sv;
    GfxCoord* view  = &gGfxViewCoord;
    VECTOR*   vecp  = &vec;
    s32*      flagp = &flag;
    sv.vx           = out->vx;
    sv.vy           = out->vy;
    sv.vz           = out->vz;
loop:
    if (p->parent != NULL) {
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
            p     = p->parent;
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
static __inline__ void actorConfigPositionDelta(PlayerStatus* config, GfxCoord* coord, SVECTOR* pos)
{
    pos->vx = config->coordMtx->t[0] - coord->coord.t[0];
    pos->vy = config->coordMtx->t[1] - coord->coord.t[1];
    pos->vz = config->coordMtx->t[2] - coord->coord.t[2];
}

/// The turn that would face `actor` toward `config`'s coordinate: the bearing
/// of the offset, written to `pos`, less the actor's own heading, wrapped.
static __inline__ s16 actorPositionYaw(Task* actor, SVECTOR* pos, PlayerStatus* config)
{
    GfxCoord* coord;
    s32       angle;
    actorConfigPositionDelta(config, actor->extra.tmd->coords, pos);
    coord = actor->extra.tmd->coords;
    angle = ratan2(pos->vx, pos->vz);
    return actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// Rebuilds `coord`'s rotation as a turn about Y by its current heading,
/// uniformly scaled by `scale`.
static __inline__ void actorRescaleYaw(GfxCoord* coord, s16 scale)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    gfxRotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = (u16)(head - 1)->m.m[0][0];
    coord->coord.m[0][1] = (u16)blk->m.m[0][1];
    coord->coord.m[0][2] = (u16)blk->m.m[0][2];
    coord->coord.m[1][0] = (u16)blk->m.m[1][0];
    coord->coord.m[1][1] = (u16)blk->m.m[1][1];
    coord->coord.m[1][2] = (u16)blk->m.m[1][2];
    coord->coord.m[2][0] = (u16)blk->m.m[2][0];
    coord->coord.m[2][1] = (u16)blk->m.m[2][1];
    m22                  = (u16)blk->m.m[2][2];
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
}

/// Steps `coord` `amount` units along its local Z axis unless movement is
/// frozen, staging the direction on the scratch pad.
static __inline__ void actorMoveForward(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]  += head[-1].vx;
        coord->coord.t[1]  += vec->vy;
        coord->coord.t[2]  += vec->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// `actorMoveForward` that also skips the step when `amount` is zero, though
/// it still takes and releases its scratch vector.
static __inline__ void actorMoveForwardNonzero(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        gteVec                        = vec;
        if (amount != 0) {
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12();
            gte_stsv(gteVec);
            coord->coord.t[0]  += head[-1].vx;
            coord->coord.t[1]  += vec->vy;
            coord->coord.t[2]  += vec->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// `actorMoveForward` applied to the root coordinate of `task`'s model.
static __inline__ void actorMoveModelForward(Task* task, s16 amount)
{
    GfxCoord* coord;
    SVECTOR*  head;
    SVECTOR*  vec;

    coord = task->extra.tmd->coords;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]  += head[-1].vx;
        coord->coord.t[1]  += vec->vy;
        coord->coord.t[2]  += vec->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// Rebuilds `coord`'s rotation as a turn about Y by its current heading at
/// unit scale.
static __inline__ void actorResetYaw(GfxCoord* coord)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    gfxRotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = 1;
    blk->scale.vy = 1;
    blk->scale.vx = 1;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = (u16)blk->m.m[0][0];
    coord->coord.m[0][1] = (u16)blk->m.m[0][1];
    coord->coord.m[0][2] = (u16)blk->m.m[0][2];
    coord->coord.m[1][0] = (u16)blk->m.m[1][0];
    coord->coord.m[1][1] = (u16)blk->m.m[1][1];
    coord->coord.m[1][2] = (u16)blk->m.m[1][2];
    coord->coord.m[2][0] = (u16)blk->m.m[2][0];
    coord->coord.m[2][1] = (u16)blk->m.m[2][1];
    coord->coord.m[2][2] = (u16)blk->m.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
}

/// `actorRescaleYaw` with a separate scale on Y.
static __inline__ void actorRescaleYawY(GfxCoord* coord, s32 scale, s16 scaleY)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    gfxRotMatrixY(&blk->m, ang, 1);
    blk->scale.vx = scale;
    blk->scale.vy = scaleY;
    blk->scale.vz = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = (u16)(head - 1)->m.m[0][0];
    coord->coord.m[0][1] = (u16)blk->m.m[0][1];
    coord->coord.m[0][2] = (u16)blk->m.m[0][2];
    coord->coord.m[1][0] = (u16)blk->m.m[1][0];
    coord->coord.m[1][1] = (u16)blk->m.m[1][1];
    coord->coord.m[1][2] = (u16)blk->m.m[1][2];
    coord->coord.m[2][0] = (u16)blk->m.m[2][0];
    coord->coord.m[2][1] = (u16)blk->m.m[2][1];
    m22                  = (u16)blk->m.m[2][2];
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
}

/// `actorMoveForwardNonzero` spelled with the GTE reading the scratch vector
/// under a single name.
static __inline__ void actorStepForward(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(vec);
            gte_gpf12();
            gte_stsv(vec);
            coord->coord.t[0]  += head[-1].vx;
            coord->coord.t[1]  += vec->vy;
            coord->coord.t[2]  += vec->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// The first of the leading twelve contact records whose kind is 0x20000:
/// copies its point to `pos` and returns its key, or returns 0 when none is
/// found before the table ends.
static __inline__ s32 actorFindHit(SVECTOR* pos, WorldCollisionContact* records)
{
    s16 i;

    for (i = 0; i < 12; i++) {
        if (!records[i].key.value)
            break;
        if ((records[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key.value;
        }
    }
    return 0;
}

/// Looks up the current area's placement record from the session location.
static __inline__ GpAreaVariant* actorGetCurrentAreaRec(void)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;

    sessionKey = &gGameSession->location.loc;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    key.view   = sessionKey->view;
    areaSyncLocationVariant(&key);
    return Gp_GetNestedAreaRec(&key);
}

/// Gives `model` the texture page and palette of the enemy's placement in the
/// current area, and reprocesses its stream when it already has one.
static __inline__ void actorTintModel(TmdObject* model, Enemy* enemy)
{
    GpAreaVariant* rec;
    AreaPlacement* place;
    s32            idx;

    idx                      = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    rec                      = actorGetCurrentAreaRec();
    place                    = gpAreaPlaceAt(rec->field_0, idx);
    model->texturePageOffset = place->texturePageOffset;
    model->clutRowOffset     = place->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

/// `actorTintModel` for the model carried by the spawned task `spawned`.
static __inline__ void actorTintTask(Task* spawned, Enemy* enemy)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    GpAreaVariant*   rec;
    AreaPlacement*   place;
    TmdObject*       model;
    s32              idx;

    sessionKey = &gGameSession->location.loc;
    idx        = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    model      = spawned->extra.tmd;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    key.view   = sessionKey->view;
    areaSyncLocationVariant(&key);
    rec                      = Gp_GetNestedAreaRec(&key);
    place                    = gpAreaPlaceAt(rec->field_0, idx);
    model->texturePageOffset = place->texturePageOffset;
    model->clutRowOffset     = place->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

/// `actorTintModel` for a freshly spawned effect, when the spawn succeeded.
static __inline__ void actorTintEffect(EffectWork* eff, Enemy* enemy)
{
    if (eff != NULL) {
        actorTintModel(eff->task->extra.tmd, enemy);
    }
}

/// The offset from `coord` to the translation of `m`.
static __inline__ void actorMatrixPositionDelta(MATRIX* m, GfxCoord* coord, SVECTOR* pos)
{
    pos->vx = m->t[0] - coord->coord.t[0];
    pos->vy = m->t[1] - coord->coord.t[1];
    pos->vz = m->t[2] - coord->coord.t[2];
}

/// `actorPositionYaw` toward the translation of `m`.
static __inline__ s16 actorMatrixPositionYaw(Task* actor, SVECTOR* pos, MATRIX* m)
{
    GfxCoord* coord;
    s32       angle;

    actorMatrixPositionDelta(m, actor->extra.tmd->coords, pos);
    coord = actor->extra.tmd->coords;
    angle = ratan2(pos->vx, pos->vz);
    return actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// The turn from `coord`'s heading to the bearing of the offset (`x`, `z`),
/// wrapped.
static __inline__ s16 actorYawTo(GfxCoord* coord, s16 x, s16 z)
{
    s32 angle;

    angle = ratan2(x, z);
    return actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// The turn from `coord`'s heading to the bearing of the offset `dir`,
/// wrapped.
static __inline__ s16 actorViewYaw(GfxCoord* coord, SVECTOR* dir)
{
    s32 angle;

    angle = ratan2(dir->vx, dir->vz);
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
    head                                      = SCRATCH_STACK_CURSOR(OverlayRangeScratch);
    scratch                                   = head - 1;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = scratch;
    scratch->dx                               = pos->vx;
    scratch->dz                               = pos->vz;
    scratch->r                                = radius;
    scratch->dx                              *= scratch->dx;
    scratch->dz                              *= scratch->dz;
    scratch->r                               *= scratch->r;
    SCRATCH_STACK_RELEASE_BLOCK(OverlayRangeScratch);
    return scratch->dx + scratch->dz >= scratch->r;
}

/// Tells the player task that `ctx` touched it, packing the pair with `mode`.
static __inline__ s32 actorPlayerContactMessage(Enemy* ctx, s32 mode)
{
    Task* player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    return taskMessageDispatch(player, 0x3F9, Gp_PackObjPair(ctx, mode), 0);
}

/// Relights `enemy` for the world position of `coord`.
static __inline__ void actorUpdateColor(Enemy* enemy, GfxCoord* coord)
{
    VECTOR* block                = (VECTOR*)(SCRATCH_STACK_CURSOR(u8) - 0x10);
    block->vx                    = coord->workm.t[0];
    block->vy                    = coord->workm.t[1];
    block->vz                    = coord->workm.t[2];
    SCRATCH_STACK_CURSOR(VECTOR) = block;
    Gp_UpdateActorColor(enemy, block, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Relights the enemy of `arg0` for the world position of its model's second
/// part.
static __inline__ void actorUpdateModelColor(Task* arg0)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;

    coord                          = &arg0->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// `actorTransformToView` spelled as a `for` loop with an early return.
static __inline__ void actorLocalToView(GfxCoord* coord, SVECTOR* out)
{
    SVECTOR acc;
    VECTOR  v;
    s32     flag;

    acc.vx = out->vx;
    acc.vy = out->vy;
    acc.vz = out->vz;

    for (;;) {
        if (coord->parent == NULL) {
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
            coord  = coord->parent;
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
static __inline__ void actorAccumulateToView(GfxCoord* coord, MATRIX* mat)
{
    MATRIX    m;
    GfxCoord* cur;

    cur  = coord->parent;
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
        cur  = cur->parent;
    }
}

/// Sets up a collision object on `coord` with its record table, position and
/// radius, links it at priority `prio`, and initialises the table as `kind`.
static __inline__ void actorLinkWorkObj(GfxCoord* coord, WorldCollisionBody* obj, WorldCollisionContact* rec,
                                        SVECTOR* pos, s16 field1C, s32 prio, s32 kind)
{
    obj->coord            = coord;
    obj->context.contacts = rec;
    obj->pos.vx           = pos->vx;
    obj->pos.vy           = pos->vy;
    obj->pos.vz           = pos->vz;
    obj->radius           = field1C;
    obj->flags            = 1;
    Gp_LinkObj(prio, obj);
    Gp_InitRec18Table(obj->context.contacts, kind, 0);
}

/// Whether the XZ offset `gap` reaches at least 1000.
static __inline__ s32 actorOutOfReach(SVECTOR* gap)
{
    VECTOR3* v;

    v                             = (VECTOR3*)(SCRATCH_STACK_CURSOR(u8) - sizeof(VECTOR3));
    SCRATCH_STACK_CURSOR(VECTOR3) = v;
    v->vx                         = gap->vx;
    v->vy                         = gap->vz;
    v->vz                         = 1000;
    v->vx                         = v->vx * v->vx;
    v->vy                         = v->vy * v->vy;
    v->vz                         = v->vz * v->vz;
    SCRATCH_STACK_RELEASE_BYTES(sizeof(VECTOR3));

    return v->vx + v->vy >= v->vz;
}

/// The scratch-pad allocation pointer. These two stay inline functions rather
/// than `SCRATCH_STACK_CURSOR` at their call sites: inside an inlined body the
/// pointer's constant address folds into each access, which the callers'
/// code depends on.
static __inline__ u8* actorGetScratchHead(void)
{
    return SCRATCH_STACK_CURSOR(u8);
}

/// Moves the scratch-pad allocation pointer to `head`.
static __inline__ void actorSetScratchHead(void* head)
{
    SCRATCH_STACK_CURSOR(void) = head;
}

/// Wraps an angle into [-0x800, 0x800]; see `overlayWrapAngle`.
static __inline__ s16 actorWrapAngle(s16 angle)
{
    return overlayWrapAngle(angle);
}

#endif // INCLUDE_ACTORS_ACTOR_H
