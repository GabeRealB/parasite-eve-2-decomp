/* The Desert Chaser, one enemy built three ways; a package selects its build
 * with DESERT_CHASER_BUILD before including this header:
 *
 * - DESERT_CHASER_CUTSCENE: actor_323000, the Main Street introduction where
 *   the chaser leaps from a roof (the fight that follows uses the regular
 *   build); actor_323400 in the Dryfield breezeway carries the same build,
 *   its scene not yet confirmed. Its spawn sets 0 HP and
 *   WORLD_TARGET_NOT_LOCKABLE, and message 2005 toggles its display flags.
 * - DESERT_CHASER_REGULAR: actor_00100 (packages actor_400100 and actor_407500).
 * - DESERT_CHASER_WATER_TOWER: actor_421600, the dryfield_water_tower build.
 *
 * All three drive an 18-slot rig. The animation driver seeds its slots from a
 * per-transition start-frame table and cross-fades a second animation context
 * into slots 1-10; it eases a torso twist spread over joints 2-4 and a head
 * turn on joint 10, and plays the sound its per-package cue step returns.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_DESERT_CHASER_H
#define SRC_SHARED_DESERT_CHASER_H

#include "types.h"

#include "actors/actor.h"

#include "main/areas.h"
#include "main/task_types.h"

#define DESERT_CHASER_CUTSCENE    1
#define DESERT_CHASER_REGULAR     2
#define DESERT_CHASER_WATER_TOWER 3
#ifndef DESERT_CHASER_BUILD
#error "define DESERT_CHASER_BUILD (DESERT_CHASER_CUTSCENE, _REGULAR or _WATER_TOWER) before including desert_chaser.h"
#endif

/// Clips per row of the clip start-frame table, and the slot flag that ends
/// the blend context's cross-fade.
#if DESERT_CHASER_BUILD == DESERT_CHASER_CUTSCENE
#define DESERT_CHASER_CLIP_COUNT 0x2D
#define DESERT_CHASER_BLEND_DONE ANIMATION_SLOT_REACHED_BOUNDARY
#else
#define DESERT_CHASER_CLIP_COUNT 0x19
#define DESERT_CHASER_BLEND_DONE ANIMATION_SLOT_SETTLED
#endif

/// What the animation tick does differently per build: the regular build's
/// cue step returns a bare sound id, so the tick adds the placement index; the
/// cutscene and Water Tower builds reset the blend rate and turn defaults when
/// the blend context starts; the armed builds tip joint 4 forward in state
/// 0x26 while no clip is queued.
#define DESERT_CHASER_CUE_NEEDS_PLACE  (DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR)
#define DESERT_CHASER_BLEND_RATE_RESET (DESERT_CHASER_BUILD != DESERT_CHASER_REGULAR)
#define DESERT_CHASER_STATE26_TILT     (DESERT_CHASER_BUILD != DESERT_CHASER_CUTSCENE)

/// The task states `desertChaserTask` runs; the regular build has a fourth.
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
typedef EnemyTaskFuncTable4 DesertChaserTaskStates;
#else
typedef EnemyTaskFuncTable3 DesertChaserTaskStates;
#endif

#if DESERT_CHASER_BUILD == DESERT_CHASER_CUTSCENE
/// Allocation holding the cutscene build's contact push step.
///
/// `step` is the vector `ActorContact_GetScratchPosition` hands the contact
/// routines, which the armed builds allocate as a bare `SVECTOR`. In both
/// cutscene packages eight zero bytes separate it from the effect record that
/// follows. No access to them is recovered, so whether they are trailing
/// fields of this object or a separate unreferenced variable is unproven; they
/// stay in this allocation only to keep the data after it at its address.
typedef struct {
    SVECTOR step;         // Whole-unit correction the last contact push applied; X and Z step one further unit when the 16.16 correction had a fraction
    u8      unknown_8[8]; // Zero in the image; no access established and role unproven
} DesertChaserContactPushStepStorage;
STATIC_ASSERT_SIZEOF(DesertChaserContactPushStepStorage, 16);
#endif

#if DESERT_CHASER_BUILD != DESERT_CHASER_CUTSCENE
/* The armed builds. DESERT_CHASER_RUN_SEQUENCE is the Water Tower run: one
 * more state ahead of the turn states, hits that only reply to the player
 * while the chaser lives and rumble the pad, and the hit effect offset built on
 * the stack. The regular build instead keeps the effect offset in the work
 * block, checks the Mine region before backing off, and tells the scene when
 * it starts its lunge. */
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#define DESERT_CHASER_CONTACTS         5
#define DESERT_CHASER_RUN_SEQUENCE     0
#define DESERT_CHASER_STATE_TURN_RIGHT 8
#define DESERT_CHASER_STATE_TURN_LEFT  9
#define DESERT_CHASER_CLIP_STAGGER     0x10
#define DESERT_CHASER_CLIP_COLLAPSE    0x13
#define DESERT_CHASER_CLIP_STUNNED     0x15
#define DESERT_CHASER_CLIP_TURN_STEP   0x11
#define DESERT_CHASER_CLIP_TURN_PROBE  0x12
#define DESERT_CHASER_SLOT_RATE(work)  ((work)->field_834) /* the chaser's own speed */
/* seeing the player raises the alert and starts the chase */
#define DESERT_CHASER_NOTICE(work) (Gp_ArmStateF0(1), (work)->field_0 = 0x26)
/* how near the player has to be before a steering chaser closes in */
#define DESERT_CHASER_CLOSE_IN 2000
#else
#define DESERT_CHASER_CONTACTS         12
#define DESERT_CHASER_RUN_SEQUENCE     1
#define DESERT_CHASER_STATE_TURN_RIGHT 9
#define DESERT_CHASER_STATE_TURN_LEFT  10
#define DESERT_CHASER_CLIP_STAGGER     0x13 /* three more clips before these */
#define DESERT_CHASER_CLIP_COLLAPSE    0x16
#define DESERT_CHASER_CLIP_STUNNED     0x18
#define DESERT_CHASER_CLIP_TURN_STEP   0x14
#define DESERT_CHASER_CLIP_TURN_PROBE  0x15
#define DESERT_CHASER_SLOT_RATE(work)  0x10 /* every runner alike */
#define DESERT_CHASER_NOTICE(work)     ((work)->field_0 = 0x1C)
#define DESERT_CHASER_CLOSE_IN         1500
#endif

/// One of the armed chaser's collision spheres, with the contact table it
/// records into.
///
/// The work block holds three, each riding a model coordinate. The first sits
/// on part 2, the coordinate the enemy record's body sits at, with radius
/// 0x19C; the second on part 10, 0x100 behind its origin, with radius 0x100.
/// Those two take the pair tests, which the task switches off in a few of its
/// states, so their tables hold the hits the chaser receives and the bodies
/// it steers around; the first table is also lent to the enemy record as its
/// contact records. The third rides the model root, 0x11C above it with
/// radius 0x12C, and takes the room-grid test; its contacts push the root
/// horizontally back out of the room's geometry. One Water Tower state turns
/// the grid test on for the first as well and pushes the root with both
/// tables.
typedef struct {
    WorldCollisionBody    body;                             // Sphere linked into the world's body list; `context.contacts` names `contacts`
    WorldCollisionContact contacts[DESERT_CHASER_CONTACTS]; // Contacts `body` records, initialized whole so the last entry ends the table; occupied entries are reset at the end of every frame
} DesertChaserSphereBody;

/// The armed chaser's wall probe: a capsule body, the segment it carries and
/// the contact table that segment records into.
///
/// The segment lies along the model root's forward axis, 0x180 above the root
/// and 0x12C in radius, and only the room-grid pass tests it, so every contact
/// it records is a wall. Each movement state places the far end for the way it
/// is about to travel -- ahead for a walk or a lunge, behind for a leap back --
/// and shortens its step on the frames the probe is touching the grid.
typedef struct {
    WorldCollisionBody    body;                             // Capsule linked into the world's body list, riding the model root; never enabled for pair tests
    WorldCollisionCapsule shape;                            // Its segment in root space: `ends[0]` above the root, `ends[1]` the far end the movement states move along Z
    WorldCollisionContact contacts[DESERT_CHASER_CONTACTS]; // Contacts `shape` records; occupied entries are reset at the end of every frame
} DesertChaserCapsuleBody;

/// The last actor command the chaser was sent, with a frame counter in the
/// byte above it.
///
/// The `ACTOR_COMMAND_MESSAGE_APPLY` handler copies every command outside the
/// stage 9/area 1 namespace here before acting on it, keeping only the low
/// byte of its command word. The Water Tower build's states later branch on
/// that byte, or on the whole command at once through `word`; the regular build
/// stores it and never reads it back.
typedef union {
    s32 word;           // The four bytes together; compare `word & DESERT_CHASER_COMMAND_MASK` with a `DESERT_CHASER_COMMAND`
    struct {
        u8 stage;       // Stage tag of the command's namespace
        u8 area;        // Area tag of the command's namespace
        u8 command;     // Low byte of the receiver-specific command
        u8 catchFrames; // Water Tower build: frames since the caught player was last sent an animation, timing each step of the catch; the regular build leaves it unwritten
    } fields;
} DesertChaserLastCommand;
STATIC_ASSERT_SIZEOF(DesertChaserLastCommand, 0x4);

/// The bits of `DesertChaserLastCommand::word` that hold the command, leaving
/// out the frame counter above them.
#define DESERT_CHASER_COMMAND_MASK 0xFFFFFF
/// The value those bits hold for command `command` of the stage/area
/// namespace: stage in bits 0-7, area in bits 8-15, command in bits 16-23.
#define DESERT_CHASER_COMMAND(stage, area, command) ((stage) | ((area) << 8) | ((command) << 16))
/// Command 1 of the Dryfield Water Tower, which the room broadcasts as its
/// timed mechanism step starts. While it is the last command received, a chaser
/// that has landed its strike or finished aiming goes to state 5 instead of
/// following the player.
#define DESERT_CHASER_COMMAND_WATER_TOWER_1 DESERT_CHASER_COMMAND(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 1)

#endif

/// The work block the spawn handler allocates and hangs behind `Task::work`:
/// 0x934 bytes in the cutscene build, 0xC30 in the regular one and 0xEB0 in
/// the Water Tower one. The three share the head up to 0x890 -- the state
/// words, the route points, both animation contexts with their 18 slots and
/// pose buffers, and the animation-state words the handlers seed and the tick
/// keeps -- and differ after it.
typedef struct DesertChaserWork {
    /// Animation state, the index the per-frame tick dispatches on.
    s16 field_0;
    /// State the tick ran last frame; `field_4` is set when `field_0`
    /// differs from it.
    s16 field_2;
    s16 field_4;
    /// Frame counter the state handlers time their effects with.
    s16 field_6;
    /// Retry counter of the Water Tower build's contact walk.
    s16  field_8;
    byte pad_A[2];
    /// The point the actor was placed at and one a fixed step ahead of it;
    /// `field_14` picks the one it walks toward.
    ActorPatrolPoint field_C[2];
    s16              field_14;
    /// Yaw of the root coordinate as the placement handler leaves it, read
    /// back from the matrix.
    s16              field_16;
    byte             pad_18[4];
    AnimationContext anim;
    AnimationSlot    slots[18];
    /// Encoded pose storage borrowed as `animationInitContext`'s `poseBuffer`.
    byte             poses[0x120];
    AnimationContext blendAnim;
    AnimationSlot    blendSlots[18];
    byte             blendPoses[0x120];
    byte             pad_824[4];
    /// Animation-state slots the handlers seed and the tick keeps: the seed
    /// mode the tick acts on (1 re-seeds from the per-state table, 2 resets
    /// the slots, 3 runs), whether the blend context is live, the clip the
    /// slots were last seeded with and the one to seed next, and the slot
    /// rate.
    u16 field_828;
    s16 field_82A;
    s16 field_82C;
    s16 field_82E;
    u16 field_830;
    u16 field_832;
    u16 field_834;
    s16 field_836;
    s16 field_838;
    u16 field_83A;
    s16 field_83C;
    u16 field_83E;
    s16 field_840;
    s16 field_842;
    /// Turn angle the tick eases toward `field_840` and splits over the body
    /// joints; cleared by the spawn handler.
    s16  field_844;
    byte pad_846[2];
    /// Clip id each slot was last seen playing, indexed like `slots`; zeroed
    /// (18 entries) when no watched clip plays.
    s32 field_848[18];
#if DESERT_CHASER_BUILD == DESERT_CHASER_CUTSCENE
    byte pad_890[4];
    /// Light / colour matrices the spawn handler binds to the model.
    MATRIX light;
    MATRIX color;
    byte   pad_8D4[0x48];
    /// Three bytes the message handler takes from a payload one at a time;
    /// nothing else reads them.
    u8   field_91C;
    u8   field_91D;
    u8   field_91E;
    byte pad_91F[0x15];
#elif DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    /// Argument record the hit effect fills for `func_800FDB18`.
    EffectSpawnArg field_890;
    SVECTOR        field_898;
    /// Hit position the hit effect hands to `func_800FDB18` as its rotation.
    SVECTOR hitPos;
    SVECTOR field_8A8;
    SVECTOR field_8B0;
    byte    pad_8B8[8];
    /// Player position and rotation, sent together as message 0x3E9.
    ActorTransform playerPlacement;
    /// The push sent to the player with `GAME_ACTOR_MESSAGE_MOVE_BY`.
    GameActorMoveBy playerMove;
    /// Reply buffer for message 0x3F8; `queryMode` selects query mode 8.
    byte replyBuf[0x14];
    s32  queryMode;
    /// Command the actor broadcasts to the scene's actors.
    ActorCommand            broadcast;
    DesertChaserSphereBody  objs[3];
    DesertChaserCapsuleBody capsuleBody;
    /// Light / colour matrices the spawn handler binds to the model.
    MATRIX light;
    MATRIX color;
    byte   pad_BC0[0x20];
    /// Frames before another hit registers, seeded from the hit's parameter.
    s16 hitCooldown;
    /// Damage accumulated since the counter was last cleared.
    u16  damageTotal;
    u16  hitFlag;
    byte pad_BE6[0xA];
    /// Offset from the actor to the player.
    SVECTOR playerDelta;
    /// Animation-set table the caught player plays from, the front or the
    /// rear one. With `params` it lies where an `AnimationPlayRequest` keeps
    /// its table and playback words, and the pair is sent as one.
    AnimationSet** animCommand;
    s32            params[4];
    /// Last actor command received; this build stores it and never reads it.
    DesertChaserLastCommand actorId;
    byte                    pad_C10[8];
    /// One-shot "already reported" latch.
    s16  reported;
    s16  distance;
    byte pad_C1C[2];
    /// One pose row latched from the pose table, and the yaw one step behind.
    s16  poseVy;
    u16  poseVx;
    s16  poseVz;
    u16  poseYaw;
    s16  poseYawPrev;
    s16  field_C28;
    s16  field_C2A;
    byte pad_C2C[4];
#elif DESERT_CHASER_BUILD == DESERT_CHASER_WATER_TOWER
    /// Argument record the hit effect fills for `func_800FDB18`.
    EffectSpawnArg field_890;
    /// Hit position the hit effect hands to `func_800FDB18` as its rotation.
    SVECTOR hitPos;
    s8      field_8A0;
    byte    pad_8A1[3];
    /// The push sent to the player with `GAME_ACTOR_MESSAGE_MOVE_BY`.
    GameActorMoveBy playerMove;
    /// Player position and rotation, sent together as message 0x3E9.
    ActorTransform playerPlacement;
    /// Reply buffer for message 0x3F8; `queryMode` selects query mode 8.
    byte replyBuf[0x14];
    s32  queryMode;
    /// Command the actor broadcasts to the scene's actors.
    ActorCommand            broadcast;
    DesertChaserSphereBody  objs[3];
    DesertChaserCapsuleBody capsuleBody;
    /// Light / colour matrices the spawn handler binds to the model.
    MATRIX light;
    MATRIX color;
    byte   pad_E44[0x20];
    /// Frames before another hit registers, seeded from the hit's parameter.
    s16 hitCooldown;
    /// Damage accumulated since the counter was last cleared.
    u16  damageTotal;
    byte pad_E68[8];
    /// Offset from the actor to the player.
    SVECTOR playerDelta;
    s16     field_E78;
    byte    pad_E7A[2];
    /// Animation-set table the caught player plays from, the front or the
    /// rear one. With `params` it lies where an `AnimationPlayRequest` keeps
    /// its table and playback words, and the pair is sent as one.
    AnimationSet** animCommand;
    s32            params[4];
    /// Last actor command received and, above it, the catch frame counter.
    DesertChaserLastCommand actorId;
    Task*                   field_E94;
    Task*                   field_E98;
    /// One-shot "already reported" latch.
    s16  reported;
    s16  distance;
    byte pad_EA0[2];
    /// One pose row latched from the pose table, and the yaw one step behind.
    s16  poseVy;
    u16  poseVx;
    s16  poseVz;
    u16  poseYaw;
    s16  poseYawPrev;
    s16  field_EAC;
    byte pad_EAE[2];
#endif
} DesertChaserWork;
#if DESERT_CHASER_BUILD == DESERT_CHASER_CUTSCENE
STATIC_ASSERT_SIZEOF(DesertChaserWork, 0x934);
#elif DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
STATIC_ASSERT_SIZEOF(DesertChaserWork, 0xC30);
#else
STATIC_ASSERT_SIZEOF(DesertChaserWork, 0xEB0);
#endif

#if DESERT_CHASER_BUILD != DESERT_CHASER_CUTSCENE
/// Whether any of the capsule's contacts, up to the first empty one, is a
/// room-grid contact (kind 0x10).
static __inline__ s16 desertChaserCapsuleTouchesGrid(Task* arg0)
{
    DesertChaserWork* work  = arg0->work;
    s16               found = 0;
    s16               i;

    for (i = 0; i < DESERT_CHASER_CONTACTS; i++) {
        if (!work->capsuleBody.contacts[i].key.value) {
            break;
        }
        if ((work->capsuleBody.contacts[i].key.value & 0xFFFF0000) == 0x100000) {
            found = 1;
        }
    }
    return found;
}
#endif

/// 0x1C-byte block `func_actor_323000_801645A4` pushes on the scratch stack:
/// the model root's world position for `Gp_UpdateActorColor`, and the local
/// point walked up the coordinate chain into view space.
typedef struct DesertChaserTickScratch {
    VECTOR  pos;
    SVECTOR local;
    s32     pad_18;
} DesertChaserTickScratch;
STATIC_ASSERT_SIZEOF(DesertChaserTickScratch, 0x1C);

void desertChaserBlendTick(Task* task);
void desertChaserAnimTick(Task* task);
void desertChaserSpawn(Enemy* enemy, Task* task);
s32  desertChaserSetVisibility(Task* task, s32 arg1, s32 arg2, s32 arg3);

/* Defined by each package. */
s32 desertChaserAnimCues(Task* task, DesertChaserWork* work);

void desertChaserFrameState(Enemy* enemy, Task* task);
void desertChaserPartEffect(Task* arg0, s16 part, s16 flags);
void desertChaserTask(Task* task);
void desertChaserHideState(Enemy* arg0, Task* arg1);
s32  desertChaserMsgPlayAnim(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);
void desertChaserExit(Task* task);

#if DESERT_CHASER_BUILD != DESERT_CHASER_CUTSCENE
void desertChaserPursue(Task* arg0);
void desertChaserRoam(Task* arg0);
void desertChaserApproach(Task* arg0);
void desertChaserStrike(Task* arg0);
void desertChaserTurnStep(Task* arg0);
void desertChaserTurnStepProbe(Task* arg0);
void desertChaserHitEffect(Task* arg0, s16 arg1, s32 arg2);
void desertChaserSpawnAim(Task* arg0);
void desertChaserSteer(Task* arg0);
void desertChaserStunned(Task* arg0);
void desertChaserFlinch(Task* arg0);
void desertChaserStagger(Task* arg0);
void desertChaserCollapse(Task* arg0);
#endif

#endif /* SRC_SHARED_DESERT_CHASER_H */
