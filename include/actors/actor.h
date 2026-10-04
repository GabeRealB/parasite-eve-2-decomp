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

/// Scratch-stack block of an actor's push out of its world contacts.
///
/// A push reserves one block, has its contact records resolved into `delta`,
/// moves the actor's coordinate frame by the whole units of that correction
/// and releases the block before returning. X and Z then take one more unit
/// away from zero wherever the correction leaves a fraction, rounding the
/// applied step outward. Nothing clears the block when it is reserved;
/// `moved` is set explicitly and `delta` is the resolver's to write. The
/// pushes that report `moved` read it after the release, while the bytes are
/// still intact.
typedef struct {
    WorldCollisionDelta delta; // Correction resolved from the contact records, in signed 16.16 units
    s32                 moved; // 1 when the X or Z correction is nonzero, so the push displaced the actor horizontally; 0 otherwise
} ActorContactPushScratch;
STATIC_ASSERT_SIZEOF(ActorContactPushScratch, 0x14);

/// A step resolved against the contact records: the 16.16 deltas
/// `func_800E0C10` resolves, their integer part (its XZ part capped in
/// length), that part's XZ length, and whether the X or Z delta was nonzero.
typedef struct ActorStepDelta {
    WorldCollisionDelta delta;
    SVECTOR             step;
    s32                 len;
    s32                 moved;
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

/// The scratch-stack block of a model rescale, which multiplies a per-axis
/// scale into a coordinate's rotation.
///
/// One block serves one rescale: it is reserved, an identity rotation is
/// written into it and scaled, and it is released once that has been
/// multiplied into the coordinate. The multiply changes the coordinate's
/// rotation only. A routine that rescales every frame first sets the
/// coordinate from an unscaled matrix, so the scale does not compound.
typedef struct {
    GfxMatrix matrix; // Identity rotation, written word-wise, then scaled in place; its translation is never set or read
    VECTOR    scale;  // Factor for each axis, 4096 = 1.0
} ActorScaleScratch;
STATIC_ASSERT_SIZEOF(ActorScaleScratch, 0x30);

/// Scaling a matrix uniformly with its translation: the scale vector handed
/// to `ScaleMatrix`, and the translation scaled on the GTE.
typedef struct ActorScaleMatrixScratch {
    VECTOR  scale;
    SVECTOR trans;
} ActorScaleMatrixScratch;
STATIC_ASSERT_SIZEOF(ActorScaleMatrixScratch, 0x18);

/// The scratch-stack block of an enemy routine that aims at a target or turns
/// the model: the ground offset to the target, and the short vector the
/// routine hands on by address.
///
/// A routine reserves one block on entry whether it needs one member or both.
/// The target is the player or a waypoint of the enemy's own; its bearing is
/// `ratan2(delta.vx, delta.vz)`, taken from the low 16 bits of each component,
/// and its distance the square root of their squares. Nothing carries over
/// from one call to the next.
typedef struct {
    VECTOR  delta; // Target position minus the actor's, world units, with `vy` set to zero; `pad` is never written
    SVECTOR rot;   // Euler angles a coordinate's rotation is rebuilt from, 4096 per turn; routines that spawn an effect instead borrow it for the effect's offset from its parent coordinate
} ActorFaceScratch;
STATIC_ASSERT_SIZEOF(ActorFaceScratch, 0x18);

/// The scratch-stack block of a yaw rebuild, which replaces a coordinate's
/// rotation with a turn about Y through the heading it already has, scaled
/// along each axis.
///
/// One block serves one rebuild: it is reserved, the rotation is built and
/// scaled in it, and it is released once the nine rotation terms have been
/// copied to the coordinate. Whatever pitch and roll the coordinate had are
/// discarded, and its translation is left alone.
typedef struct {
    MATRIX rotation; // Turn about Y by `yaw`, then scaled in place; its translation is never set or read
    VECTOR scale;    // Factor for each axis, 4096 = 1.0
    s16    yaw;      // Heading read from the coordinate's Z axis before the rebuild, 4096 units per turn
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
    byte                pad_0[0x20];
    WorldCollisionDelta delta;
    byte                pad_30[0x8];
} ActorDeltaFrame38;
STATIC_ASSERT_SIZEOF(ActorDeltaFrame38, 0x38);

/// The same frame at 0x48 bytes, for the steps that take the larger block.
typedef struct ActorDeltaFrame48 {
    byte                pad_0[0x20];
    WorldCollisionDelta delta;
    byte                pad_30[0x18];
} ActorDeltaFrame48;
STATIC_ASSERT_SIZEOF(ActorDeltaFrame48, 0x48);

/// The scratch-pad frame of a wall contact: `delta` receives the
/// `func_800E0C10` push-back and is then reused for offsets, `normal` is the
/// normalised wall offset, and `result` the word `func_800E0C10` reports
/// through its last argument.
typedef struct ActorContactFrame {
    byte                pad_0[0x20];
    WorldCollisionDelta delta;
    byte                pad_30[0x8];
    VECTOR              normal;
    s32                 result;
} ActorContactFrame;
STATIC_ASSERT_SIZEOF(ActorContactFrame, 0x4C);

/// The scratch-pad frame of a push against the collision grid: `delta`
/// receives the `func_800E0C10` push-back and is then reused for each
/// record's offset, `normal` is that offset normalised, and `dir` the normal
/// brought into the grid's frame. `dx` and `dz` are the contact record's
/// normal, staged for the bearing some actors take from it.
typedef struct ActorPushFrame {
    byte                pad_0[0x20];
    WorldCollisionDelta delta;
    VECTOR              normal;
    VECTOR              dir;
    s16                 dx;
    byte                pad_52[0x2];
    s16                 dz;
    byte                pad_56[0x2];
} ActorPushFrame;
STATIC_ASSERT_SIZEOF(ActorPushFrame, 0x58);

/// The scratch-pad frame of a contact walk: `delta` receives the move
/// `func_800E0C10` resolves and is then reused for each contact's offset,
/// `normal` is that offset normalised, and `dir` the normal carried into the
/// collision grid's frame, along which the deepest contact pushes.
typedef struct ActorWallPushFrame {
    WorldCollisionDelta delta;
    VECTOR              normal;
    VECTOR              dir;
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

/// One end of the two-point beat a patrolling enemy walks back and forth.
///
/// An enemy's work block keeps a pair of these and a selector for the one it
/// is walking toward; coming within reach of that one switches it to the
/// other. Setup seeds the pair with where the enemy was placed and a point a
/// fixed step ahead of that along its facing, and an enemy that roams
/// replaces either end later. Both coordinates are the root coordinate's
/// translation in its parent's space, narrowed to 16 bits. No height is kept:
/// the walk is planar.
typedef struct {
    s16 x; // X translation of the point
    s16 z; // Z translation of the point
} ActorPatrolPoint;
STATIC_ASSERT_SIZEOF(ActorPatrolPoint, 4);

/// Animation playback storage for a model of twenty-one parts.
///
/// An enemy's work block embeds one of these for each playback it runs over
/// its model: the animation context, a slot for every part and an encoded-pose
/// entry for every slot. Setup binds `anim` to the two arrays and to the
/// model's part coordinates, so the rig has to stay live, and stay where it
/// is, for as long as the context is used. An owner that mixes two motions
/// keeps a second rig bound to the same model.
///
/// A slot's index is its model part's, and slot `i` uses pose entry `i`. Each
/// entry reserves `ANIMATION_POSE_BUFFER_BYTES` and holds the slot's encoding
/// at its start: `AnimationPackedPose` (12 bytes) or `AnimationPackedRotation`
/// (4 bytes). Playback stores no capacity, so a slot or pose index has to stay
/// below `ARRAY_SIZE(slots)`. Owners start and tick slots 1 to 20; slot 0, the
/// root part's, keeps its place in both arrays and is never started.
typedef struct {
    AnimationContext anim;                                   // Context bound to `slots`, `poses` and the model's part coordinates
    AnimationSlot    slots[21];                              // Playback state of the model part at the same index
    u8               poses[21][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose of the slot at the same index
} ActorAnimRig21;
STATIC_ASSERT_SIZEOF(ActorAnimRig21, 0x4AC);

/// Caller-owned playback storage for twenty slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 20 entries.
/// Slot 0 keeps its position in both arrays even where an owner drives only
/// slots 1 to 19.
typedef struct {
    AnimationContext anim;                                   // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[20];                              // Playback slot for one driven index
    u8               poses[20][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
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

/// Caller-owned playback storage for eighteen slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 18 entries.
/// Slot 0 keeps its position in both arrays even where an owner drives only
/// slots 1 to 17.
typedef struct {
    AnimationContext anim;                                   // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[18];                              // Playback slot for one driven index
    u8               poses[18][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig18;
STATIC_ASSERT_SIZEOF(ActorAnimRig18, 0x404);

/// Caller-owned playback storage for fifteen slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 15 entries.
/// Slot 0 keeps its position in both arrays even where an owner drives only
/// slots 1 to 14.
typedef struct {
    AnimationContext anim;                                   // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[15];                              // Playback slot for one driven index
    u8               poses[15][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig15;
STATIC_ASSERT_SIZEOF(ActorAnimRig15, 0x35C);

/// Caller-owned playback storage for six slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 6 entries.
/// Slot 0 keeps its position in both arrays even where an owner drives only
/// slots 1 to 5.
typedef struct {
    AnimationContext anim;                                  // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[6];                              // Playback slot for one driven index
    u8               poses[6][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig6;
STATIC_ASSERT_SIZEOF(ActorAnimRig6, 0x164);

/// Caller-owned playback storage for seven slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 7 entries.
/// Slot 0 keeps its position in both arrays even where an owner drives only
/// slots 1 to 6.
typedef struct {
    AnimationContext anim;                                  // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[7];                              // Playback slot for one driven index
    u8               poses[7][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig7;
STATIC_ASSERT_SIZEOF(ActorAnimRig7, 0x19C);

/// Caller-owned playback storage for eight slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 8 entries.
/// Slot 0 keeps its position in both arrays even where an owner drives only
/// slots 1 to 7, and an owner that drives fewer slots uses the leading
/// entries and leaves the rest untouched.
typedef struct {
    AnimationContext anim;                                  // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[8];                              // Playback slot for one driven index
    u8               poses[8][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig8;
STATIC_ASSERT_SIZEOF(ActorAnimRig8, 0x1D4);

/// Caller-owned playback storage for four slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 4 entries.
/// Slot 0 keeps its position in both arrays even where an owner drives only
/// slots 1 to 3.
typedef struct {
    AnimationContext anim;                                  // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[4];                              // Playback slot for one driven index
    u8               poses[4][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig4;
STATIC_ASSERT_SIZEOF(ActorAnimRig4, 0xF4);

/// Caller-owned playback storage for five slots.
///
/// The context borrows the model's part coordinates and is bound to this
/// rig's slots and encoded-pose buffer. Both arrays stay live while playback
/// uses them. Each slot has one pose entry of `ANIMATION_POSE_BUFFER_BYTES`.
/// The entry holds that slot's encoding at its start: `AnimationPackedPose`
/// (12 bytes) or `AnimationPackedRotation` (4 bytes). Playback stores no
/// capacity, so a slot or pose index has to stay within these 5 entries.
/// Slot 0 keeps its position in both arrays even where an owner drives only
/// slots 1 to 4.
typedef struct {
    AnimationContext anim;                                  // Context bound to `slots`, `poses` and the model coordinates
    AnimationSlot    slots[5];                              // Playback slot for one driven index
    u8               poses[5][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose for the slot at the same index
} ActorAnimRig5;
STATIC_ASSERT_SIZEOF(ActorAnimRig5, 0x12C);

/// Steps of an enemy's animation, kept in `ActorEnemyState::state`.
///
/// A play request leaves one of the two reseeds pending. The enemy's next
/// update performs it and moves on to `ACTOR_ENEMY_ANIM_TICK`, where it stays
/// until the next request. A block still zeroed has no step, and its update
/// leaves the rig alone.
enum {
    ACTOR_ENEMY_ANIM_BLEND = 1, // Reseed the slots from `animId`, blending from the pose they hold
    ACTOR_ENEMY_ANIM_RESET = 2, // Reseed the slots from `animId` at its start, with no blend
    ACTOR_ENEMY_ANIM_TICK  = 3, // Tick the slots each frame
};

/// The animation request and walk an animated enemy keeps right after its rig.
///
/// A play request stores the clip in `animId` and the reseed to perform in
/// `state`; the update that performs it copies the clip to `appliedAnimId`.
/// Clip ids index the package's own animation table. An enemy that walks
/// keeps the heading it last gave its root coordinate and the frames its walk
/// has left; one that stays where it is placed leaves both zero. No enemy
/// reads or writes the bytes of `pad_A` or `pad_34`, and the block is
/// allocated zeroed.
typedef struct {
    s16  state;         // Step of the animation (0 none, else `ACTOR_ENEMY_ANIM_BLEND`, `_RESET` or `_TICK`)
    s16  appliedAnimId; // Clip the slots were last seeded with; recorded, never read
    s16  animId;        // Clip the last play request selected
    s16  field_6;       // Cleared by each play request and raised once by a view figure's spawn; never read, role unproven
    s16  cueRecord;     // Animation record the enemy last cued a sound for, so a record held for several frames cues once
    byte pad_A[0x28];
    s16  yaw;           // Heading last given the root coordinate, 4096 to a turn
    byte pad_34[0x2];
    s16  travel;        // Frames of forward movement the walk has left
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

/// `ActorModelState::animId` and `ActorModelState::bank` before the first play
/// request: no clip applied, no bank bound.
///
/// Bank indices and clip ids index tables, so it matches none of them: a block
/// seeded with it binds its bank and applies its clip on the first request.
#define ACTOR_MODEL_STATE_NONE (-1)

/// What an actor's model is playing and the matrices it is lit with, kept
/// right after the model's rig in the actor's work block.
///
/// A play request rebinds the rig when its bank differs from `bank` and
/// records the clip it seeds the slots with in `animId`; the handlers that
/// skip a repeated request do so by comparing against `animId`. The work block
/// is allocated zeroed, and the actor's spawn then sets both ids to
/// `ACTOR_MODEL_STATE_NONE`. The model object borrows `light` and `color` for
/// as long as the work block lives.
typedef struct {
    s8     ticking;    // Set once a clip has been applied, never cleared: the slots are ticked each frame and a request may blend from their pose
    s8     animId;     // Clip the slots were last seeded with, within `bank`
    s8     bank;       // Index, in the package's animation bank table, of the bank the rig is bound to
    s8     nextAnimId; // Clip, in bank 0, a walk changes to as it ends; unused by an actor that does not walk
    MATRIX light;      // Light-direction matrix lent to the model object
    MATRIX color;      // Light-colour matrix lent to the model object
} ActorModelState;
STATIC_ASSERT_SIZEOF(ActorModelState, 0x44);

/// Which of its two handlers a scripted walker's tick runs, kept in
/// `ActorWalkState::motion`.
enum {
    ACTOR_WALK_MOTION_IDLE    = 0, // No walk in progress: the package's idle handler runs
    ACTOR_WALK_MOTION_WALKING = 1, // A walk in progress: the handler runs the step `motionStep` selects
};

/// `ActorWalkState::lastDistance` before a walk's first arrival check: larger
/// than any distance the check measures, so that check only records.
#define ACTOR_WALK_DISTANCE_NONE 0x7FFF

/// The walk a scripted walker keeps after its model state: an actor the room
/// script places and sends from point to point.
///
/// A walk request copies the destination's position and rotation into
/// `target` and `targetRot` and sets `motion`; the package's sequence of
/// steps then carries the walk out, and the step that ends it returns
/// `motion` to idle. The steps and what each does are the package's own.
///
/// A walker that moves under `velocity` adds it into `carry` each frame, moves
/// the root coordinate by the integer halves and keeps the fractions, so a
/// velocity below one unit a frame still moves the actor. One whose sequence
/// stops on arrival measures its X and Z distance to `target` each frame and
/// has arrived once neither is smaller than `lastDistance`, which reaching
/// and passing the target both cause.
///
/// No walker's own code reads or writes the fourth words of the two SDK
/// vectors or `pad_2C`, and the block is allocated zeroed.
typedef struct {
    VECTOR  target;       // Destination of the walk, a position the root coordinate's translation is to reach
    VECTOR  velocity;     // Displacement added each frame, in signed 16.16 units; zero while standing
    Fixed16 carry[3];     // X, Y and Z displacement not yet applied; only the fractions survive a frame
    byte    pad_2C[0x4];
    SVECTOR lastDistance; // Absolute X and Z distance to `target` at the last arrival check, `ACTOR_WALK_DISTANCE_NONE` before the first; Y is only seeded
    SVECTOR targetRot;    // Rotation of the destination, 4096 to a turn; the closing turn steers the yaw to `vy`, the other angles are not read
    s16     motion;       // Handler the tick runs (0 `ACTOR_WALK_MOTION_IDLE`, 1 `ACTOR_WALK_MOTION_WALKING`)
    s16     motionStep;   // Step of the package's walk sequence in progress, counted from 0
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

/// Work block of the animated actor whose code actor_110300 and actor_110800
/// both carry, reached through a global the spawn publishes: the rig at the
/// front and the animation state after it. `st.cueRecord` is the animation
/// record slot 19 or 16 last cued a sound for, which only actor_110800 uses.
typedef struct Actor110300Work {
    ActorAnimRig20  rig;
    ActorEnemyState st;
    byte            pad_4AC[0xB0];
} Actor110300Work;
STATIC_ASSERT_SIZEOF(Actor110300Work, 0x55C);

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

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0] = (u16)(head - 1)->rotation.m[0][0];
    coord->coord.m[0][1] = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2] = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0] = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1] = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2] = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0] = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1] = (u16)blk->rotation.m[2][1];
    m22                  = (u16)blk->rotation.m[2][2];
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
        gfxReadMatrixZAxis(&coord->coord, vec);
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
            gfxReadMatrixZAxis(&coord->coord, vec);
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
        gfxReadMatrixZAxis(&coord->coord, vec);
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

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vz = 1;
    blk->scale.vy = 1;
    blk->scale.vx = 1;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0] = (u16)blk->rotation.m[0][0];
    coord->coord.m[0][1] = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2] = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0] = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1] = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2] = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0] = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1] = (u16)blk->rotation.m[2][1];
    coord->coord.m[2][2] = (u16)blk->rotation.m[2][2];
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

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vx = scale;
    blk->scale.vy = scaleY;
    blk->scale.vz = scale;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0] = (u16)(head - 1)->rotation.m[0][0];
    coord->coord.m[0][1] = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2] = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0] = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1] = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2] = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0] = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1] = (u16)blk->rotation.m[2][1];
    m22                  = (u16)blk->rotation.m[2][2];
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
            gfxReadMatrixZAxis(&coord->coord, vec);
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
static __inline__ AreaVariant* actorGetCurrentAreaRec(void)
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
    AreaVariant*   layout;
    AreaPlacement* place;
    s32            idx;

    idx                      = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    layout                   = actorGetCurrentAreaRec();
    place                    = gpAreaPlaceAt(layout->placements, idx);
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
    AreaVariant*     layout;
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
    layout                   = Gp_GetNestedAreaRec(&key);
    place                    = gpAreaPlaceAt(layout->placements, idx);
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
    scratch->radius                           = radius;
    scratch->dx                              *= scratch->dx;
    scratch->dz                              *= scratch->dz;
    scratch->radius                          *= scratch->radius;
    SCRATCH_STACK_RELEASE_BLOCK(OverlayRangeScratch);
    return scratch->dx + scratch->dz >= scratch->radius;
}

/// Tells the player task that `ctx` touched it, packing the pair with `mode`.
static __inline__ s32 actorPlayerContactMessage(Enemy* ctx, s32 mode)
{
    Task* player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    return taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(ctx, mode), 0);
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
