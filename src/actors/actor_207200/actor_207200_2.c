#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actor_207200_private.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/object_fields.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern EnemyParams   D_actor_207200_8014E7D4;
extern AnimationSet* D_actor_207200_80153ED4[13];
/// `forwardSpeed` for frames 20..39 of the crawl animation, indexed by frame - 20.
extern s16 D_actor_207200_80153F20[];
/// Base damage the shatter hit doubles, before a 0..99 roll is added.
/// Effect offsets `func_800FDB18` is handed for the two hit tables.
extern SVECTOR D_actor_207200_80153F08;
extern SVECTOR D_actor_207200_80153F10;

/// Values of `_Actor207200CreepingStrangerWork::state`.
enum {
    ACTOR_207200_STATE_DORMANT     = 0, // idles until the player comes near or the room alerts its enemies; a headless body only rests here between recoils
    ACTOR_207200_STATE_ACTIVE      = 1, // crawls, turns and attacks, stepped by `activeStage`
    ACTOR_207200_STATE_STATUS_HOLD = 3, // held, replaying the fidget, until the status buildup runs out
    ACTOR_207200_STATE_RECOIL      = 4  // plays the recoil out, then wakes
};

/// Values of `_Actor207200CreepingStrangerWork::activeStage`.
enum {
    ACTOR_207200_ACTIVE_STAGE_DELAY         = 0, // idles for `wakeDelay` frames
    ACTOR_207200_ACTIVE_STAGE_CRAWL         = 1, // crawls forward; picks an attack or a turn from the player's bearing
    ACTOR_207200_ACTIVE_STAGE_TURN_YAW_UP   = 2, // turns on the spot towards higher heading
    ACTOR_207200_ACTIVE_STAGE_TURN_YAW_DOWN = 3, // turns on the spot towards lower heading
    ACTOR_207200_ACTIVE_STAGE_FRONT_ATTACK  = 4, // strikes at a player close ahead
    ACTOR_207200_ACTIVE_STAGE_SIDE_ATTACK   = 5, // strikes at a player touching it from outside its facing
    ACTOR_207200_ACTIVE_STAGE_CRY           = 6  // voiced pause that follows four front attacks in ten
};

/// Values of `_Actor207200CreepingStrangerWork::deathPhase`.
enum {
    ACTOR_207200_DEATH_PHASE_BEGIN   = 0, // saves the root matrix and unlinks the lock-on node and the five bodies
    ACTOR_207200_DEATH_PHASE_SETTLE  = 1, // hides a burst body; otherwise lets a running recoil reach frame 100
    ACTOR_207200_DEATH_PHASE_FLATTEN = 2, // 61 frames: the saved root transform is squashed along Y and the body burns away
    ACTOR_207200_DEATH_PHASE_DESTROY = 3  // destroys the enemy
};

/// Values of `_Actor207200CreepingStrangerWork::animId`: indices into the
/// package's table of animation sets, whose entry 0 is empty. The table's
/// twelfth set is never requested.
enum {
    ACTOR_207200_ANIM_NONE                 = 0,  // as `appliedAnim`: forces `animId` to be applied again
    ACTOR_207200_ANIM_IDLE                 = 1,  // dormant loop, restarted every 91 frames
    ACTOR_207200_ANIM_CRAWL                = 2,  // 75-frame crawl cycle; frames 20..39 carry the root forward
    ACTOR_207200_ANIM_TURN_YAW_UP          = 3,  // turn on the spot towards higher heading
    ACTOR_207200_ANIM_TURN_YAW_DOWN        = 4,  // turn on the spot towards lower heading
    ACTOR_207200_ANIM_RECOIL               = 5,  // recoil from a hit; also played when the head bursts off
    ACTOR_207200_ANIM_SIDE_ATTACK_YAW_UP   = 6,  // side attack at a player whose bearing is zero or positive
    ACTOR_207200_ANIM_SIDE_ATTACK_YAW_DOWN = 7,  // side attack at a player whose bearing is negative
    ACTOR_207200_ANIM_FRONT_ATTACK         = 8,  // front attack
    ACTOR_207200_ANIM_FIDGET               = 9,  // voiced break in the dormant loop; the status hold replays it
    ACTOR_207200_ANIM_CRY                  = 10, // voiced pause after a front attack
    ACTOR_207200_ANIM_DEATH                = 11  // collapse of the headless body
};

/// Work block of the Creeping Stranger task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It holds
/// the animation rig of the model's seven parts, the matrices the model is lit
/// through, five collision spheres, each followed by its own contact table, and
/// the state machine: `state` picks the behaviour, `activeStage` the step of
/// the active one, and `deathPhase` the step of the task's death state.
///
/// The enemy dies in two steps. Only hits on the head sphere hurt it while the
/// head is there. Running out of health or a critical hit takes the head off
/// (`headLost`): the body stops moving and attacking, its own sphere becomes
/// the hit records, and once the task's 20-frame kill countdown has run out the
/// next hit that deals damage starts the death state. A head hit of the
/// shattering classes skips all that and bursts the whole body.
///
/// Headings are 4096ths of a turn and scales 0x1000 for 1.0.
typedef struct {
    ActorAnimRig7         rig;                    // playback of the model's seven parts; slots 1..6 play `animId`, 0 is never started
    MATRIX                colorMtx;               // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;               // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    senseBody;              // radius-2000 sphere on the root with no key of its own; a player-body contact wakes the dormant enemy, which then disables it
    WorldCollisionContact senseContacts[1];       // contact of `senseBody`
    WorldCollisionBody    body;                   // radius-300 sphere above the root, tested against the room grid, the floor and other bodies
    WorldCollisionContact bodyContacts[6];        // contacts of `body`: wall push-back, the player's touch, hits and other enemies; the enemy's hit records once the head is lost
    WorldCollisionBody    headBody;               // radius-150 sphere on model part 3, tested against the room grid and other bodies; unlinked when the head is lost
    WorldCollisionContact headContacts[6];        // contacts of `headBody`: wall push-back and the hits that hurt; the enemy's hit records until the head is lost
    WorldCollisionBody    frontAttackBody;        // radius-300 sphere ahead of model part 3 carrying the enemy's first attack; enabled on frames 42..44 of the front attack
    WorldCollisionContact frontAttackContacts[1]; // contact of `frontAttackBody`
    WorldCollisionBody    sideAttackBody;         // radius-300 sphere beside model part 6 carrying the enemy's second attack; enabled on frames 30..59 of the side attack
    WorldCollisionContact sideAttackContacts[1];  // contact of `sideAttackBody`
    EffectSpawnArg        headHitEffectArg;       // argument record of the effect a survived head hit spawns, hung off model part 3
    EffectSpawnArg        bodyHitEffectArg;       // argument record of the effect a body hit spawns, hung off model part 1
    EffectSpawnArg        headLossEffectArg;      // argument record of the effect pair spawned as the head is lost, hung off model part 3
    byte                  field_3FC[0x50];        // never accessed
    SVECTOR               rotation;               // root rotation the turns rebuild the root matrix from; only `vy`, the heading, is ever non-zero
    VECTOR3               prevRootPos;            // root position before the last step; restored when the collision step reports a conflict
    byte                  field_460[4];           // never accessed
    MATRIX                savedRootMtx;           // root matrix when the death state began; each flatten frame rescales a copy of it
    s16                   turnStep;               // heading change per frame of a turn, 25 or -25
    s16                   state;                  // `ACTOR_207200_STATE_*`
    s16                   deathPhase;             // `ACTOR_207200_DEATH_PHASE_*`
    s16                   phaseFrames;            // frames of the status hold since the fidget was last restarted, or frames of the death flatten
    s16                   animId;                 // requested animation, `ACTOR_207200_ANIM_*`
    s16                   appliedAnim;            // animation last applied to slots 1..6
    s16                   animFrames;             // frames since `animId` was applied or its loop was last restarted; the states time their steps with it
    s16                   forwardSpeed;           // distance the root moves along its facing each frame
    s16                   blocked;                // 1 once a collision conflict has stopped the body; makes the crawl turn, and ends the turn it started
    byte                  field_496[2];           // never accessed
    s16                   field_498;              // set to 1 by the dormant loop and 0 by the crawl, never read; role unproven
    s16                   activeStage;            // `ACTOR_207200_ACTIVE_STAGE_*`
    s16                   flattenScaleY;          // Y scale of the death flatten; falls 0x50 a frame from 0x1000 until it is 0x200 or less
    s16                   hitCooldown;            // frames before another hit is taken; set from the hit's id parameter 2
    s16                   frontAttackLanded;      // 1 once `frontAttackBody` has touched something during the current front attack; frame 45 then plays the hit sound
    s16                   wakeRequested;          // set by a player-body contact on `senseBody` or the end of a recoil; wakes the enemy on its next dormant frame
    s16                   headBurst;              // 1 once a critical hit has burst the head off; never read
    s16                   headLost;               // 1 once the head is gone, burst or not: model parts 2 and 3 are collapsed and only a hit on `body` can finish the enemy
    s16                   hasBurst;               // 1 once a shattering hit has burst the whole body; the death state then hides the model at once
    s16                   wakeDelay;              // frames the enemy stays put after waking, drawn from 0..89
} _Actor207200CreepingStrangerWork;
STATIC_ASSERT_SIZEOF(_Actor207200CreepingStrangerWork, 0x4AC);

/// Scratch-stack block of the Creeping Stranger's contact pass.
///
/// The pass reserves one block a frame. It has the push-back of the room's
/// collision grid resolved into `delta`, first from the head sphere's contact
/// records and then from the body sphere's, and adds the whole units of each
/// correction to the root. It then walks both tables, reusing `delta` for each
/// record. A damaging contact, kind 0x20000, takes the offset from the root to
/// the player, whose length is the range the hit's damage is worked out for.
/// A contact with another enemy's body, kind 0x30000, takes the offset from
/// that body's centre, which is normalised into `normal` and turned back into
/// `delta` in the frame of the collision grid's coordinate; a crawling enemy
/// is pushed along it by the depth of the overlap, the record's summed radii
/// less that offset's length on X and Z. Nothing clears the block when it is
/// reserved and nothing in it carries over to the next frame. Every way out
/// of the pass releases the block except the two a head hit takes when it
/// bursts the whole body or takes the head off: those return with it still
/// reserved.
///
/// The block opens as `ActorContactDeltaScratch` does. No pass touches the
/// bytes either side of `delta`, so what they were laid out to hold is
/// unproven.
typedef struct {
    byte                unknown_0[0x20]; // Reserved with the block and never accessed; role unproven
    WorldCollisionDelta delta;           // Correction resolved from the contact records, in signed 16.16 units; then, in whole world units, the offset to the player or from the centre of the body being tested; then `normal` in the grid coordinate's frame, 4096 = 1.0
    byte                unknown_30[0x8]; // Reserved with the block and never accessed; role unproven
    VECTOR              normal;          // Offset from the body being tested, normalised: away from that body, 4096 = 1.0
} _Actor207200ContactScratch;
STATIC_ASSERT_SIZEOF(_Actor207200ContactScratch, 0x48);

/// The records closing three of the overlay's model streams, handed to the
/// spawned effect as its model through `D_800626EC[5].data.model`.
static TmdSource _gActor207200CreepingStrangerBurstLeg;
static TmdSource _gActor207200CreepingStrangerBurstArm;
static TmdSource _gActor207200CreepingStrangerBurstHead;
extern SVECTOR   D_actor_207200_80153F18;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void func_actor_207200_8014B278(Enemy* arg0, Task* arg1);
static void func_actor_207200_8014C870(Task* arg0, s32 arg1);
static s32  func_actor_207200_8014CE20(GfxCoord* arg0, u32* arg1);
static void func_actor_207200_8014CA84(Enemy* arg0, Task* arg1);
static void func_actor_207200_8014D2DC(Enemy* arg0, Task* arg1);
static void func_actor_207200_8014CFEC(Task* arg0);
static void func_actor_207200_8014D128(Task* arg0);
static void func_actor_207200_8014D41C(Task* arg0);
static void func_actor_207200_8014D49C(Task* arg0);
static void func_actor_207200_8014D5C4(Task* arg0);
static void func_actor_207200_8014D65C(Task* arg0);
static void func_actor_207200_8014D70C(Enemy* arg0, Task* task);
static void func_actor_207200_8014D77C(Task* task);
static void func_actor_207200_8014D7E8(Task* arg0);
static void func_actor_207200_8014D8DC(Task* arg0);
static void func_actor_207200_8014D97C(Task* arg0, GfxCoord* arg1);
static void func_actor_207200_8014DAF8(Task* dst, Task* src);
static void func_actor_207200_8014DB4C(Task* arg0);

/// The large enemy's state handlers - spawn, live tick and teardown tick -
/// which `func_actor_207200_8014D280` dispatches through by task state.
static const EnemyTaskFuncTable3 D_actor_207200_80149E30 = {
    { func_actor_207200_8014B278, func_actor_207200_8014D2DC, func_actor_207200_8014CA84 }
};

void func_actor_207200_8014D280(Task*);

EnemyParams D_actor_207200_8014E7D4 = { D_actor_207200_8014E7CC, 250, 15, 48, 1, 50, 10, 0, 0 };

static TmdBone _gActor207200CreepingStrangerBodySkeleton[7] = {
#include "assets/creeping_stranger_body_skeleton.inc"
};

static u32 _gActor207200CreepingStrangerBodyPartVerts[7] = {
#include "assets/creeping_stranger_body_partVerts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBodyVerts[125] = {
#include "assets/creeping_stranger_body_verts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBodyNormals[136] = {
#include "assets/creeping_stranger_body_normals.inc"
};

static u32 _gActor207200CreepingStrangerBodyStream[1592] = {
#include "assets/creeping_stranger_body_stream.inc"
};

static TmdSource _gActor207200CreepingStrangerBody = {
    0,
    8356,
    2504,
    7,
    _gActor207200CreepingStrangerBodyPartVerts,
    _gActor207200CreepingStrangerBodyVerts,
    _gActor207200CreepingStrangerBodyNormals,
    _gActor207200CreepingStrangerBodySkeleton,
    _gActor207200CreepingStrangerBodyStream,
};

static TmdBone _gActor207200CreepingStrangerBurstLegSkeleton[1] = {
#include "assets/creeping_stranger_burst_leg_skeleton.inc"
};

static u32 _gActor207200CreepingStrangerBurstLegPartVerts[1] = {
#include "assets/creeping_stranger_burst_leg_partVerts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBurstLegVerts[8] = {
#include "assets/creeping_stranger_burst_leg_verts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBurstLegNormals[9] = {
#include "assets/creeping_stranger_burst_leg_normals.inc"
};

static u32 _gActor207200CreepingStrangerBurstLegStream[61] = {
#include "assets/creeping_stranger_burst_leg_stream.inc"
};

static TmdSource _gActor207200CreepingStrangerBurstLeg = {
    0,
    368,
    0,
    1,
    _gActor207200CreepingStrangerBurstLegPartVerts,
    _gActor207200CreepingStrangerBurstLegVerts,
    _gActor207200CreepingStrangerBurstLegNormals,
    _gActor207200CreepingStrangerBurstLegSkeleton,
    _gActor207200CreepingStrangerBurstLegStream,
};

static TmdBone _gActor207200CreepingStrangerBurstArmSkeleton[1] = {
#include "assets/creeping_stranger_burst_arm_skeleton.inc"
};

static u32 _gActor207200CreepingStrangerBurstArmPartVerts[1] = {
#include "assets/creeping_stranger_burst_arm_partVerts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBurstArmVerts[20] = {
#include "assets/creeping_stranger_burst_arm_verts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBurstArmNormals[22] = {
#include "assets/creeping_stranger_burst_arm_normals.inc"
};

static u32 _gActor207200CreepingStrangerBurstArmStream[195] = {
#include "assets/creeping_stranger_burst_arm_stream.inc"
};

static TmdSource _gActor207200CreepingStrangerBurstArm = {
    0,
    1272,
    0,
    1,
    _gActor207200CreepingStrangerBurstArmPartVerts,
    _gActor207200CreepingStrangerBurstArmVerts,
    _gActor207200CreepingStrangerBurstArmNormals,
    _gActor207200CreepingStrangerBurstArmSkeleton,
    _gActor207200CreepingStrangerBurstArmStream,
};

static TmdBone _gActor207200CreepingStrangerBurstHeadSkeleton[1] = {
#include "assets/creeping_stranger_burst_head_skeleton.inc"
};

static u32 _gActor207200CreepingStrangerBurstHeadPartVerts[1] = {
#include "assets/creeping_stranger_burst_head_partVerts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBurstHeadVerts[33] = {
#include "assets/creeping_stranger_burst_head_verts.inc"
};

static SVECTOR _gActor207200CreepingStrangerBurstHeadNormals[40] = {
#include "assets/creeping_stranger_burst_head_normals.inc"
};

static u32 _gActor207200CreepingStrangerBurstHeadStream[316] = {
#include "assets/creeping_stranger_burst_head_stream.inc"
};

static TmdSource _gActor207200CreepingStrangerBurstHead = {
    0,
    2116,
    0,
    1,
    _gActor207200CreepingStrangerBurstHeadPartVerts,
    _gActor207200CreepingStrangerBurstHeadVerts,
    _gActor207200CreepingStrangerBurstHeadNormals,
    _gActor207200CreepingStrangerBurstHeadSkeleton,
    _gActor207200CreepingStrangerBurstHeadStream,
};

static AnimationPackedPose _gActor207200Animation07BF8Bank1[8] = {
#include "assets/actor_207200_animation_07BF8_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation07BF8Bank4[27] = {
#include "assets/actor_207200_animation_07BF8_bank4.inc"
};

static AnimationRecord _gActor207200Animation07BF8Records[72] = {
#include "assets/actor_207200_animation_07BF8_records.inc"
};

static u16 _gActor207200Animation07BF8Indices[8] = {
#include "assets/actor_207200_animation_07BF8_indices.inc"
};

static AnimationSet _gActor207200Animation07BF8 = {
    _gActor207200Animation07BF8Records,
    _gActor207200Animation07BF8Indices,
    { NULL, _gActor207200Animation07BF8Bank1, NULL, NULL, _gActor207200Animation07BF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation07F34Bank1[16] = {
#include "assets/actor_207200_animation_07F34_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation07F34Bank4[55] = {
#include "assets/actor_207200_animation_07F34_bank4.inc"
};

static AnimationRecord _gActor207200Animation07F34Records[90] = {
#include "assets/actor_207200_animation_07F34_records.inc"
};

static u16 _gActor207200Animation07F34Indices[8] = {
#include "assets/actor_207200_animation_07F34_indices.inc"
};

static AnimationSet _gActor207200Animation07F34 = {
    _gActor207200Animation07F34Records,
    _gActor207200Animation07F34Indices,
    { NULL, _gActor207200Animation07F34Bank1, NULL, NULL, _gActor207200Animation07F34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation08248Bank1[14] = {
#include "assets/actor_207200_animation_08248_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation08248Bank4[55] = {
#include "assets/actor_207200_animation_08248_bank4.inc"
};

static AnimationRecord _gActor207200Animation08248Records[86] = {
#include "assets/actor_207200_animation_08248_records.inc"
};

static u16 _gActor207200Animation08248Indices[8] = {
#include "assets/actor_207200_animation_08248_indices.inc"
};

static AnimationSet _gActor207200Animation08248 = {
    _gActor207200Animation08248Records,
    _gActor207200Animation08248Indices,
    { NULL, _gActor207200Animation08248Bank1, NULL, NULL, _gActor207200Animation08248Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation08540Bank1[14] = {
#include "assets/actor_207200_animation_08540_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation08540Bank4[51] = {
#include "assets/actor_207200_animation_08540_bank4.inc"
};

static AnimationRecord _gActor207200Animation08540Records[83] = {
#include "assets/actor_207200_animation_08540_records.inc"
};

static u16 _gActor207200Animation08540Indices[8] = {
#include "assets/actor_207200_animation_08540_indices.inc"
};

static AnimationSet _gActor207200Animation08540 = {
    _gActor207200Animation08540Records,
    _gActor207200Animation08540Indices,
    { NULL, _gActor207200Animation08540Bank1, NULL, NULL, _gActor207200Animation08540Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation08B98Bank1[31] = {
#include "assets/actor_207200_animation_08B98_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation08B98Bank4[121] = {
#include "assets/actor_207200_animation_08B98_bank4.inc"
};

static AnimationRecord _gActor207200Animation08B98Records[178] = {
#include "assets/actor_207200_animation_08B98_records.inc"
};

static u16 _gActor207200Animation08B98Indices[8] = {
#include "assets/actor_207200_animation_08B98_indices.inc"
};

static AnimationSet _gActor207200Animation08B98 = {
    _gActor207200Animation08B98Records,
    _gActor207200Animation08B98Indices,
    { NULL, _gActor207200Animation08B98Bank1, NULL, NULL, _gActor207200Animation08B98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation09018Bank1[21] = {
#include "assets/actor_207200_animation_09018_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation09018Bank4[90] = {
#include "assets/actor_207200_animation_09018_bank4.inc"
};

static AnimationRecord _gActor207200Animation09018Records[121] = {
#include "assets/actor_207200_animation_09018_records.inc"
};

static u16 _gActor207200Animation09018Indices[8] = {
#include "assets/actor_207200_animation_09018_indices.inc"
};

static AnimationSet _gActor207200Animation09018 = {
    _gActor207200Animation09018Records,
    _gActor207200Animation09018Indices,
    { NULL, _gActor207200Animation09018Bank1, NULL, NULL, _gActor207200Animation09018Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation093ECBank1[19] = {
#include "assets/actor_207200_animation_093EC_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation093ECBank4[72] = {
#include "assets/actor_207200_animation_093EC_bank4.inc"
};

static AnimationRecord _gActor207200Animation093ECRecords[102] = {
#include "assets/actor_207200_animation_093EC_records.inc"
};

static u16 _gActor207200Animation093ECIndices[8] = {
#include "assets/actor_207200_animation_093EC_indices.inc"
};

static AnimationSet _gActor207200Animation093EC = {
    _gActor207200Animation093ECRecords,
    _gActor207200Animation093ECIndices,
    { NULL, _gActor207200Animation093ECBank1, NULL, NULL, _gActor207200Animation093ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation09704Bank1[14] = {
#include "assets/actor_207200_animation_09704_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation09704Bank4[59] = {
#include "assets/actor_207200_animation_09704_bank4.inc"
};

static AnimationRecord _gActor207200Animation09704Records[83] = {
#include "assets/actor_207200_animation_09704_records.inc"
};

static u16 _gActor207200Animation09704Indices[8] = {
#include "assets/actor_207200_animation_09704_indices.inc"
};

static AnimationSet _gActor207200Animation09704 = {
    _gActor207200Animation09704Records,
    _gActor207200Animation09704Indices,
    { NULL, _gActor207200Animation09704Bank1, NULL, NULL, _gActor207200Animation09704Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation099F4Bank1[14] = {
#include "assets/actor_207200_animation_099F4_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation099F4Bank4[54] = {
#include "assets/actor_207200_animation_099F4_bank4.inc"
};

static AnimationRecord _gActor207200Animation099F4Records[78] = {
#include "assets/actor_207200_animation_099F4_records.inc"
};

static u16 _gActor207200Animation099F4Indices[8] = {
#include "assets/actor_207200_animation_099F4_indices.inc"
};

static AnimationSet _gActor207200Animation099F4 = {
    _gActor207200Animation099F4Records,
    _gActor207200Animation099F4Indices,
    { NULL, _gActor207200Animation099F4Bank1, NULL, NULL, _gActor207200Animation099F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation09D08Bank1[13] = {
#include "assets/actor_207200_animation_09D08_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation09D08Bank4[59] = {
#include "assets/actor_207200_animation_09D08_bank4.inc"
};

static AnimationRecord _gActor207200Animation09D08Records[85] = {
#include "assets/actor_207200_animation_09D08_records.inc"
};

static u16 _gActor207200Animation09D08Indices[8] = {
#include "assets/actor_207200_animation_09D08_indices.inc"
};

static AnimationSet _gActor207200Animation09D08 = {
    _gActor207200Animation09D08Records,
    _gActor207200Animation09D08Indices,
    { NULL, _gActor207200Animation09D08Bank1, NULL, NULL, _gActor207200Animation09D08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation09FE4Bank1[15] = {
#include "assets/actor_207200_animation_09FE4_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation09FE4Bank4[48] = {
#include "assets/actor_207200_animation_09FE4_bank4.inc"
};

static AnimationRecord _gActor207200Animation09FE4Records[76] = {
#include "assets/actor_207200_animation_09FE4_records.inc"
};

static u16 _gActor207200Animation09FE4Indices[8] = {
#include "assets/actor_207200_animation_09FE4_indices.inc"
};

static AnimationSet _gActor207200Animation09FE4 = {
    _gActor207200Animation09FE4Records,
    _gActor207200Animation09FE4Indices,
    { NULL, _gActor207200Animation09FE4Bank1, NULL, NULL, _gActor207200Animation09FE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation0A080Bank1[2] = {
#include "assets/actor_207200_animation_0A080_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation0A080Bank4[5] = {
#include "assets/actor_207200_animation_0A080_bank4.inc"
};

static AnimationRecord _gActor207200Animation0A080Records[14] = {
#include "assets/actor_207200_animation_0A080_records.inc"
};

static u16 _gActor207200Animation0A080Indices[8] = {
#include "assets/actor_207200_animation_0A080_indices.inc"
};

static AnimationSet _gActor207200Animation0A080 = {
    _gActor207200Animation0A080Records,
    _gActor207200Animation0A080Indices,
    { NULL, _gActor207200Animation0A080Bank1, NULL, NULL, _gActor207200Animation0A080Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_207200_80153EC8 = { { { TASK_BODY_TMD, 96 } }, func_actor_207200_8014D280, { .model = &_gActor207200CreepingStrangerBody } };

AnimationSet* D_actor_207200_80153ED4[13] = {
    NULL,
    &_gActor207200Animation07BF8,
    &_gActor207200Animation07F34,
    &_gActor207200Animation08248,
    &_gActor207200Animation08540,
    &_gActor207200Animation08B98,
    &_gActor207200Animation09018,
    &_gActor207200Animation093EC,
    &_gActor207200Animation09704,
    &_gActor207200Animation099F4,
    &_gActor207200Animation09D08,
    &_gActor207200Animation09FE4,
    &_gActor207200Animation0A080,
};

SVECTOR D_actor_207200_80153F08 = { 0 };

SVECTOR D_actor_207200_80153F10 = { 0, -100, -150, 0 };

SVECTOR D_actor_207200_80153F18 = { 0 };

s16 D_actor_207200_80153F20[20] = {
    0,
    3,
    6,
    9,
    12,
    15,
    18,
    21,
    24,
    27,
    24,
    21,
    18,
    15,
    12,
    9,
    6,
    3,
    0,
    0,
};

static void            func_actor_207200_8014B628(Task* arg0);
static void            func_actor_207200_8014B87C(Task* arg0);
static void            func_actor_207200_8014BEF4(Task* arg0);
static __inline__ void Actor207200_TickAnim(Task* arg0);
static __inline__ void Actor207200_UpdateColor(Enemy* enemy, Task* actor);

static void func_actor_207200_8014B278(Enemy* arg0, Task* arg1)
{
    _Actor207200CreepingStrangerWork* work;
    TmdObject*                        obj;
    GfxCoord*                         coord;
    GfxCoord*                         part6;
    GfxCoord*                         part3;
    s32                               i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(_Actor207200CreepingStrangerWork), false);
    part6 = coord + 6;
    part3 = coord + 3;
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    worldTargetLinkNode(&arg0->node);
    arg0->coord                  = coord;
    arg0->node.state.parts.flags = 0;
    arg0->bodyPos.vx             = 0;
    arg0->bodyPos.vy             = 0;
    arg0->bodyPos.vz             = 0;
    arg0->param                  = &D_actor_207200_8014E7D4;
    arg0->recs                   = work->headContacts;
    arg0->hp                     = (u16)D_actor_207200_8014E7D4.hpMax;
    work->rotation.vy            = (coord)->param.rot.vy;
    animationInitContext(&work->rig.anim, D_actor_207200_80153ED4, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationResetSlot(&work->rig.anim, i, ACTOR_207200_ANIM_IDLE);
    }
    (sceneAcquireBattleRef)(0);

    work->animId      = ACTOR_207200_ANIM_IDLE;
    work->appliedAnim = ACTOR_207200_ANIM_IDLE;
    work->headBurst   = 0;
    work->headLost    = 0;
    work->deathPhase  = ACTOR_207200_DEATH_PHASE_BEGIN;
    work->blocked     = 0;
    work->hitCooldown = 0;

    work->senseBody.coord            = coord;
    work->senseBody.context.contacts = work->senseContacts;
    work->senseBody.pos.vx           = 0;
    work->senseBody.pos.vy           = 0;
    work->senseBody.pos.vz           = 0;
    work->senseBody.key              = 0;
    work->senseBody.radius           = 0x7D0;
    work->senseBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->senseBody);
    worldCollisionInitContacts(work->senseContacts, ARRAY_SIZE(work->senseContacts), 0);

    work->body.pos.vy           = -0x12C;
    work->body.pos.vz           = -0xB4;
    work->body.coord            = coord;
    work->body.context.contacts = work->bodyContacts;
    work->body.pos.vx           = 0;
    work->body.key              = 0x3002B;
    work->body.radius           = 0x12C;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->senseBody.flags      |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);

    work->headBody.coord            = part3;
    work->headBody.context.contacts = work->headContacts;
    work->headBody.pos.vx           = 0;
    work->headBody.pos.vy           = 0;
    work->headBody.pos.vz           = 0;
    work->headBody.key              = 0x3002B;
    work->headBody.radius           = 0x96;
    work->headBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->body.flags               |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->headBody);
    worldCollisionInitContacts(work->headContacts, ARRAY_SIZE(work->headContacts), 0);

    work->frontAttackBody.coord            = part3;
    work->frontAttackBody.context.contacts = work->frontAttackContacts;
    work->frontAttackBody.pos.vx           = 0;
    work->frontAttackBody.pos.vy           = 0x50;
    work->frontAttackBody.pos.vz           = 0x8C;
    work->headBody.flags                  |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->frontAttackBody.key              = Gp_PackPair(D_actor_207200_8014E7CC, 0);
    work->frontAttackBody.radius           = 0x12C;
    work->frontAttackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->frontAttackBody);
    worldCollisionInitContacts(work->frontAttackContacts, ARRAY_SIZE(work->frontAttackContacts), 0);

    work->sideAttackBody.coord            = part6;
    work->sideAttackBody.context.contacts = work->sideAttackContacts;
    work->sideAttackBody.pos.vx           = 0xFA;
    work->sideAttackBody.pos.vy           = 0;
    work->sideAttackBody.pos.vz           = 0;
    work->frontAttackBody.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->sideAttackBody.key              = Gp_PackPair(D_actor_207200_8014E7CC, 1);
    work->sideAttackBody.radius           = 0x12C;
    work->sideAttackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sideAttackBody);
    worldCollisionInitContacts(work->sideAttackContacts, ARRAY_SIZE(work->sideAttackContacts), 0);
    work->sideAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->headHitEffectArg.coord       = arg1->extra.tmd->coords + 3;
    work->headHitEffectArg.spawnArgLo  = 0x100;
    work->headHitEffectArg.spawnArgHi  = 1;
    work->headLossEffectArg.coord      = arg1->extra.tmd->coords + 3;
    work->headLossEffectArg.spawnArgLo = 0x400;
    work->headLossEffectArg.spawnArgHi = 3;
    work->bodyHitEffectArg.coord       = arg1->extra.tmd->coords + 1;
    work->bodyHitEffectArg.spawnArgLo  = 0x100;
    work->bodyHitEffectArg.spawnArgHi  = 1;
    work->hasBurst                     = 0;
    arg1->exitCallback                 = func_actor_207200_8014DB4C;
    arg1->state++;
}

/// `ACTOR_207200_STATE_DORMANT` of the enemy: while it still has its head, a
/// player-body contact in `senseContacts` (through `wakeRequested`, or the
/// global flag `gSceneCombatState.signals.bytes.enemyAlert`) wakes it -
/// `ACTOR_207200_STATE_ACTIVE`, a random 0..89 delay in `wakeDelay` and
/// `Gp_ArmStateF0(1)`. Then runs the idle cycle in `animId`: the idle loop
/// restarts after 0x5B frames and rolls a 30% chance of the fidget, which plays
/// the room-tagged sound on frame 5 and returns to the idle loop after 0x2D
/// frames.
static void func_actor_207200_8014B628(Task* arg0)
{
    _Actor207200CreepingStrangerWork* work;
    GfxCoord*                         obj;
    s32                               id;
    s32                               pan;
    u32                               rnd;
    u16                               hi;

    SCRATCH_STACK_RESERVE_BYTES(8);
    work = arg0->work;
    obj  = arg0->extra.tmd->coords;
    if (work->headLost == 0) {
        if (worldCollisionCountContactsByKind(work->senseContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
            work->wakeRequested = 1;
        }
        if (work->wakeRequested != 0 || gSceneCombatState.signals.bytes.enemyAlert != 0) {
            rnd                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            hi                     = rnd >> 16;
            work->activeStage      = ACTOR_207200_ACTIVE_STAGE_DELAY;
            work->forwardSpeed     = 0;
            work->state            = ACTOR_207200_STATE_ACTIVE;
            gRandomLcgState        = rnd;
            work->senseBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->wakeDelay        = hi % 90;
            Gp_ArmStateF0(1);
        }
        worldCollisionClearContacts(work->senseContacts);
    }
    switch (work->animId) {
        case ACTOR_207200_ANIM_IDLE:
            work->field_498    = 1;
            work->forwardSpeed = 0;
            if (work->animFrames >= 0x5B) {
                work->animFrames  = 0;
                work->appliedAnim = ACTOR_207200_ANIM_NONE;
                if (work->headLost == 0) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if ((u16)((gRandomLcgState >> 16) % 100) < 30) {
                        work->animId = ACTOR_207200_ANIM_FIDGET;
                    }
                }
            }
            break;
        case ACTOR_207200_ANIM_FIDGET:
            if (work->animFrames == 5) {
                id  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480004;
                pan = (s8)worldCoordGetOriginAudioPan(obj);
                sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(obj));
            }
            if (work->animFrames >= 0x2D) {
                work->animId     = ACTOR_207200_ANIM_IDLE;
                work->animFrames = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// `ACTOR_207200_STATE_ACTIVE` of the enemy, stepped by `activeStage`. The
/// delay stage waits out the random delay in `wakeDelay`; the crawl moves to the
/// front attack once the player is within 0x385 and inside +/-0x200 of the
/// facing angle, otherwise crawls on and picks a turn direction every 75 frames;
/// the two turn stages turn the model by `turnStep` (+/-25) on frames 30..50
/// and re-check the angle after 60 frames; the attack and cry stages play the
/// room-tagged sounds and enable `frontAttackBody` or `sideAttackBody` for their
/// strike frames, the front attack rolling a 40% chance of the cry before
/// returning to the crawl. Every stage drops back to the dormant state once the
/// head is lost.
static void func_actor_207200_8014B87C(Task* arg0)
{
    _Actor207200CreepingStrangerWork* work;
    GfxCoord*                         coord;
    s32                               angle;
    u32                               dist;
    s32                               id;
    s16                               state;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->activeStage) {
        case ACTOR_207200_ACTIVE_STAGE_DELAY:
            work->animId = ACTOR_207200_ANIM_IDLE;
            if (work->animFrames > work->wakeDelay) {
                work->activeStage = ACTOR_207200_ACTIVE_STAGE_CRAWL;
            }
            break;
        case ACTOR_207200_ACTIVE_STAGE_CRAWL:
            angle = func_actor_207200_8014CE20(arg0->extra.tmd->coords, &dist);
            if (work->headLost == 0 && dist < 0x385 && ABS(angle) < 0x200) {
                work->forwardSpeed    = 0;
                work->activeStage     = ACTOR_207200_ACTIVE_STAGE_FRONT_ATTACK;
                work->headBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                break;
            }
            work->animId          = ACTOR_207200_ANIM_CRAWL;
            work->field_498       = 0;
            work->headBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            if (work->animFrames >= 20 && work->animFrames < 40) {
                work->forwardSpeed = D_actor_207200_80153F20[work->animFrames - 20];
            } else {
                work->forwardSpeed = 0;
            }
            if (work->animFrames >= 75) {
                work->animFrames = 0;
                if (work->headLost == 0) {
                    if (ABS(angle) > 0x200 || work->blocked != 0) {
                        if (angle < 0) {
                            work->turnStep    = -25;
                            work->activeStage = ACTOR_207200_ACTIVE_STAGE_TURN_YAW_DOWN;
                            work->animId      = ACTOR_207200_ANIM_TURN_YAW_DOWN;
                        } else {
                            work->turnStep    = 25;
                            work->activeStage = ACTOR_207200_ACTIVE_STAGE_TURN_YAW_UP;
                            work->animId      = ACTOR_207200_ANIM_TURN_YAW_UP;
                        }
                    }
                }
            }
            break;
        case ACTOR_207200_ACTIVE_STAGE_TURN_YAW_UP:
            state              = ACTOR_207200_ANIM_TURN_YAW_UP;
            work->forwardSpeed = 0;
            work->animId       = state;
            if (work->animFrames >= 30 && work->animFrames <= 50) {
                work->rotation.vx  = 0;
                work->rotation.vz  = 0;
                work->rotation.vy += work->turnStep;
                RotMatrix(&work->rotation, &coord->coord);
            }
            if (work->animFrames >= 60) {
                if (work->headLost != 0) {
                    work->state  = ACTOR_207200_STATE_DORMANT;
                    work->animId = ACTOR_207200_ANIM_IDLE;
                } else {
                    angle = func_actor_207200_8014CE20(arg0->extra.tmd->coords, &dist);
                    if (ABS(angle) < 0x200 || work->blocked != 0) {
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_CRAWL;
                        work->blocked     = 0;
                        work->animFrames  = 0;
                        work->animId      = ACTOR_207200_ANIM_CRAWL;
                    } else if (angle < 0) {
                        work->turnStep    = -25;
                        work->animFrames  = 0;
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_TURN_YAW_DOWN;
                        work->animId      = ACTOR_207200_ANIM_TURN_YAW_DOWN;
                    } else {
                        work->turnStep    = 25;
                        work->animFrames  = 0;
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_TURN_YAW_UP;
                        work->animId      = state;
                    }
                }
            }
            break;
        case ACTOR_207200_ACTIVE_STAGE_TURN_YAW_DOWN:
            state              = ACTOR_207200_ANIM_TURN_YAW_DOWN;
            work->forwardSpeed = 0;
            work->animId       = state;
            if (work->animFrames >= 30 && work->animFrames <= 50) {
                work->rotation.vx  = 0;
                work->rotation.vz  = 0;
                work->rotation.vy += work->turnStep;
                RotMatrix(&work->rotation, &coord->coord);
            }
            if (work->animFrames >= 60) {
                if (work->headLost != 0) {
                    work->state  = ACTOR_207200_STATE_DORMANT;
                    work->animId = ACTOR_207200_ANIM_IDLE;
                } else {
                    angle = func_actor_207200_8014CE20(arg0->extra.tmd->coords, &dist);
                    if (ABS(angle) < 0x200 || work->blocked != 0) {
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_CRAWL;
                        work->blocked     = 0;
                        work->animFrames  = 0;
                        work->animId      = ACTOR_207200_ANIM_CRAWL;
                    } else if (angle < 0) {
                        work->turnStep    = -25;
                        work->animFrames  = 0;
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_TURN_YAW_DOWN;
                        work->animId      = state;
                    } else {
                        work->turnStep    = 25;
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_TURN_YAW_UP;
                        work->animFrames  = 0;
                        work->animId      = ACTOR_207200_ANIM_TURN_YAW_UP;
                    }
                }
            }
            break;
        case ACTOR_207200_ACTIVE_STAGE_FRONT_ATTACK:
            work->forwardSpeed = 0;
            if (work->animFrames == 30) {
                work->frontAttackLanded = 0;
                id                      = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480002;
                sndEvtRequestScriptStart(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrames == 42) {
                work->frontAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->animFrames == 45) {
                work->frontAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->frontAttackLanded != 0 && work->animFrames == 45) {
                id = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480005;
                sndEvtRequestScriptStart(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animId == ACTOR_207200_ANIM_FRONT_ATTACK && work->animFrames >= 60) {
                if (work->headLost != 0) {
                    work->state  = ACTOR_207200_STATE_DORMANT;
                    work->animId = ACTOR_207200_ANIM_IDLE;
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if ((u16)((gRandomLcgState >> 16) % 100) < 40) {
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_CRY;
                        work->animId      = ACTOR_207200_ANIM_CRY;
                    } else {
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_CRAWL;
                        work->animId      = ACTOR_207200_ANIM_CRAWL;
                    }
                    work->animFrames = 0;
                }
            } else {
                work->animId = ACTOR_207200_ANIM_FRONT_ATTACK;
            }
            break;
        case ACTOR_207200_ACTIVE_STAGE_SIDE_ATTACK:
            work->forwardSpeed = 0;
            if (work->animFrames == 30) {
                work->sideAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->animFrames == 60) {
                work->sideAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->animFrames >= 90) {
                if (work->headLost != 0) {
                    work->state  = ACTOR_207200_STATE_DORMANT;
                    work->animId = ACTOR_207200_ANIM_IDLE;
                } else {
                    work->activeStage = ACTOR_207200_ACTIVE_STAGE_CRAWL;
                    work->animFrames  = 0;
                    work->animId      = ACTOR_207200_ANIM_CRAWL;
                }
            }
            break;
        case ACTOR_207200_ACTIVE_STAGE_CRY:
            work->animId       = ACTOR_207200_ANIM_CRY;
            work->forwardSpeed = 0;
            if (work->animFrames == 10) {
                id = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480006;
                sndEvtRequestScriptStart(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrames >= 90) {
                if (work->headLost != 0) {
                    work->state  = ACTOR_207200_STATE_DORMANT;
                    work->animId = ACTOR_207200_ANIM_IDLE;
                } else {
                    work->activeStage = ACTOR_207200_ACTIVE_STAGE_CRAWL;
                    work->animFrames  = 0;
                    work->animId      = ACTOR_207200_ANIM_CRAWL;
                }
            }
            break;
    }
}

/// Per-frame collision handling. Each six-record table's `func_800E0C10`
/// result pushes the model back (1) or snaps it to `prevRootPos` (2). Records of
/// `bodyContacts` then dispatch on their kind: 1 starts the side attack when the
/// player is off-angle and near, 2 is a hit, which only deals damage once
/// `headLost` is set and then starts the death state, 3 pushes the model out of
/// the record's radius. Unless `headLost` is set, each 0x20000 record of
/// `headContacts` applies damage too, and some ids end the tick through
/// `func_actor_207200_8014D128` / `8014CFEC`. The tables and, past the delay
/// stage of `activeStage`, the two attack contacts are cleared last.
static void func_actor_207200_8014BEF4(Task* arg0)
{
    _Actor207200CreepingStrangerWork* work;
    _Actor207200ContactScratch*       scratch;
    GfxCoord*                         coord;
    u32                               dist;
    Enemy*                            enemy;
    s32                               i;
    s32                               angle;
    s32                               damage;
    s32                               push;
    s32                               param;
    s32                               n;
    s32                               snd;

    work    = arg0->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor207200ContactScratch);
    coord   = arg0->extra.tmd->coords;
    enemy   = arg0->spawnArg2.pointer;

    switch (func_800E0C10(work->headContacts, &scratch->delta, ARRAY_SIZE(work->headContacts), NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->prevRootPos.vx;
            coord->coord.t[1] = work->prevRootPos.vy;
            coord->coord.t[2] = work->prevRootPos.vz;
            if (work->headLost == 0 && work->blocked == 0) {
                work->blocked = 1;
            }
            break;
    }
    switch (func_800E0C10(work->bodyContacts, &scratch->delta, ARRAY_SIZE(work->bodyContacts), NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->prevRootPos.vx;
            coord->coord.t[1] = work->prevRootPos.vy;
            coord->coord.t[2] = work->prevRootPos.vz;
            if (work->headLost == 0 && work->blocked == 0) {
                work->blocked = 1;
            }
            break;
    }
    if (work->hitCooldown != 0 && --work->hitCooldown <= 0) {
        work->hitCooldown = 0;
    }

    for (i = 0; i < ARRAY_SIZE(work->bodyContacts); i++) {
        switch ((u32)work->bodyContacts[i].key.parts.kind) {
            case 1:
                if (work->headLost == 0 && (u16)work->activeStage - ACTOR_207200_ACTIVE_STAGE_CRAWL < 3U) {
                    angle = func_actor_207200_8014CE20(arg0->extra.tmd->coords, &dist);
                    if (abs(angle) > 0x200 && dist < 2000) {
                        work->animId      = angle < 0 ? ACTOR_207200_ANIM_SIDE_ATTACK_YAW_DOWN : ACTOR_207200_ANIM_SIDE_ATTACK_YAW_UP;
                        work->animFrames  = 0;
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_SIDE_ATTACK;
                    }
                }
                break;
            case 2:
                if (work->hitCooldown != 0) {
                    break;
                }
                scratch->delta.vector.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                scratch->delta.vector.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
                scratch->delta.vector.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                damage                   = SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx +
                                                       scratch->delta.vector.vy * scratch->delta.vector.vy +
                                                       scratch->delta.vector.vz * scratch->delta.vector.vz);
                Gp_GetIdParam0(work->bodyContacts[i].key.value);
                damage = Gp_ComputeDamage(work->bodyContacts[i].key.value, damage, 0, 0);
                func_800FDB18((u16)Gp_GetIdParam1(work->bodyContacts[i].key.value),
                              arg0->extra.tmd->coords + 1, &D_actor_207200_80153F10, &work->bodyHitEffectArg);
                n = Gp_GetIdParam2(work->bodyContacts[i].key.value);
                if ((s16)n > 0) {
                    work->hitCooldown = n;
                }
                if (work->headLost != 0 && arg0->killCountdown == 0) {
                    func_800DA6E8(&enemy->node, damage, 0);
                    if (damage != 0) {
                        arg0->state++;
                        work->animId = ACTOR_207200_ANIM_DEATH;
                        SCRATCH_STACK_RELEASE_BLOCK(_Actor207200ContactScratch);
                        return;
                    }
                } else {
                    func_800DA6E8(&enemy->node, 0, 0);
                }
                if (work->state == ACTOR_207200_STATE_DORMANT) {
                    snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480006;
                    sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    work->animId        = ACTOR_207200_ANIM_RECOIL;
                    work->animFrames    = 0;
                    work->state         = ACTOR_207200_STATE_RECOIL;
                    work->forwardSpeed  = 0;
                    work->wakeRequested = 0;
                }
                break;
            case 3:
                scratch->delta.vector.vx = coord->workm.t[0] - work->bodyContacts[i].point.vx;
                scratch->delta.vector.vy = 0;
                scratch->delta.vector.vz = coord->workm.t[2] - work->bodyContacts[i].point.vz;
                damage                   = work->bodyContacts[i].distance -
                         SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vz * scratch->delta.vector.vz);
                // Clamped through a second variable: clamping `damage` in
                // place drops the copy the original makes.
                push = damage;
                if (damage <= 0) {
                    push = 0;
                }
                damage                   = push;
                scratch->delta.vector.vx = coord->workm.t[0] - work->bodyContacts[i].point.vx;
                scratch->delta.vector.vy = coord->workm.t[1] - work->bodyContacts[i].point.vy;
                scratch->delta.vector.vz = coord->workm.t[2] - work->bodyContacts[i].point.vz;
                VectorNormal(&scratch->delta.vector, &scratch->normal);
                ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->delta.vector);
                if (work->animId == ACTOR_207200_ANIM_CRAWL) {
                    coord->coord.t[0] += (damage * scratch->delta.vector.vx) >> 12;
                    n                  = damage * scratch->delta.vector.vy;
                    if (n < 0) {
                        coord->coord.t[1] += n >> 12;
                    }
                    coord->coord.t[2] += (damage * scratch->delta.vector.vz) >> 12;
                }
                break;
        }
    }

    if (work->headLost == 0) {
        for (i = 0; i < ARRAY_SIZE(work->headContacts); i++) {
            if ((work->headContacts[i].key.value & 0xFFFF0000) != 0x20000) {
                continue;
            }
            if (work->hitCooldown != 0) {
                break;
            }
            scratch->delta.vector.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            scratch->delta.vector.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
            scratch->delta.vector.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            damage                   = SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy +
                                                   scratch->delta.vector.vz * scratch->delta.vector.vz);
            param                    = Gp_GetIdParam0(work->headContacts[i].key.value);
            damage                   = Gp_ComputeDamage(work->headContacts[i].key.value, damage, 0, 0);
            switch ((u16)param) {
                case 1:
                case 4:
                case 5:
                case 6:
                    Gp_SpawnEff(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords, 2, NULL);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    func_800DA6E8(&enemy->node, D_actor_207200_8014E7D4.hpMax * 2 + (u16)((gRandomLcgState >> 16) % 100), 0);
                    func_actor_207200_8014D128(arg0);
                    work->hasBurst = 1;
                    arg0->state++;
                    return;
                case 8:
                case 9:
                    Gp_SetObjFlag2(enemy, work->headContacts[i].key.value, 0);
                default:
                    if ((Gp_RollEnemyChance(arg0->spawnArg2.pointer, work->headContacts[i].key.value, 0) != 0 ||
                         work->state == ACTOR_207200_STATE_STATUS_HOLD) &&
                        damage != 0) {
                        func_800E2C78(enemy, work->headContacts[i].key.value, damage, 0);
                        func_actor_207200_8014CFEC(arg0);
                        return;
                    }
                    func_800E2C78(enemy, work->headContacts[i].key.value, damage, 0);
                    func_actor_207200_8014C870(arg0, damage);
                    func_800FDB18((u16)Gp_GetIdParam1(work->headContacts[i].key.value),
                                  arg0->extra.tmd->coords + 3, &D_actor_207200_80153F08, &work->headHitEffectArg);
                    n = Gp_GetIdParam2(work->headContacts[i].key.value);
                    if ((s16)n > 0) {
                        work->hitCooldown = n;
                    }
                    break;
            }
        }
    } else {
        work->frontAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->sideAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    worldCollisionClearContacts(work->bodyContacts);
    worldCollisionClearContacts(work->headContacts);
    if (work->activeStage != ACTOR_207200_ACTIVE_STAGE_DELAY) {
        if (worldCollisionFindContactIndex(work->frontAttackContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
            work->frontAttackLanded      = 1;
            work->frontAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            worldCollisionClearContacts(work->frontAttackContacts);
        }
        if (worldCollisionFindContactIndex(work->sideAttackContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
            work->sideAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            worldCollisionClearContacts(work->sideAttackContacts);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor207200ContactScratch);
}

/// Ticks the shatter timers the enemy runs while it dies. Every time a timer
/// runs out the work is armed with a fresh sound effect - one per stage of the
/// death animation - and the frame it is handed plays.
static void func_actor_207200_8014C870(Task* arg0, s32 arg1)
{
    _Actor207200CreepingStrangerWork* work;
    Enemy*                            ctx;
    GfxCoord*                         coord;
    EffectSpawnArg*                   effArg;
    s32                               snd;

    work  = arg0->work;
    ctx   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;

    ctx->hp = (s16)((u16)ctx->hp - arg1);
    func_800DA6E8(&ctx->node, arg1, 0);
    if ((s16)ctx->hp <= 0) {
        if (work->headLost == 0) {
            ctx->hp = 1;
            snd     = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480003;
            sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            effArg = &work->headLossEffectArg;
            func_800FDB18(5, arg0->extra.tmd->coords + 3, &D_actor_207200_80153F18, effArg);
            func_800FDB18(5, arg0->extra.tmd->coords + 3, &D_actor_207200_80153F18, effArg);
            work->frontAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->sideAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            worldCollisionUnlinkBody(&work->headBody);
            work->headLost      = 1;
            ctx->recs           = work->bodyContacts;
            work->deathPhase    = ACTOR_207200_DEATH_PHASE_BEGIN;
            arg0->killCountdown = 0x14;
        }
    } else {
        if (work->animId == ACTOR_207200_ANIM_IDLE) {
            snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480006;
            sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            work->animId        = ACTOR_207200_ANIM_RECOIL;
            work->animFrames    = 0;
            work->state         = ACTOR_207200_STATE_RECOIL;
            work->forwardSpeed  = 0;
            work->wakeRequested = 0;
            return;
        }
        snd = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40480001;
        sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    }
}

/// `func_actor_207200_8014D65C`'s body, inlined: blend slots 1..6 into `animId`
/// when it differs from `appliedAnim`, otherwise advance them by one frame and
/// count it in `animFrames`.
static __inline__ void Actor207200_TickAnim(Task* arg0)
{
    _Actor207200CreepingStrangerWork* work;
    s32                               i;

    work = arg0->work;
    if (work->animId != work->appliedAnim) {
        work->appliedAnim = work->animId;
        work->animFrames  = 0;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->animId, 0, 8);
        }
    } else {
        work->animFrames++;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// `func_actor_207200_8014D70C`'s body, inlined: push the model's second coordinate's
/// world position onto the scratch stack and hand it to `worldCoordUpdateActorColor`.
static __inline__ void Actor207200_UpdateColor(Enemy* enemy, Task* actor)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;

    coord                          = &actor->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    worldCoordUpdateActorColor(enemy, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Teardown tick. Mode 2 of `gSceneCombatState.actorControl` hides the model, mode 1 does nothing;
/// otherwise the teardown stage in `deathPhase` advances: 0 releases the actor's
/// state reference, snapshots the model transform in `savedRootMtx` and unlinks
/// its node and five collision bodies; 1 moves on once the recoil animation has
/// run 100 frames (or at once for any other animation or once `hasBurst` is
/// set); 2 counts 61 frames in `phaseFrames`, flattening the model and spawning
/// an effect on frame 15; 3 destroys the enemy. Every stage
/// but the last then ticks the animation, the attach coordinates and the colour.
static void func_actor_207200_8014CA84(Enemy* arg0, Task* arg1)
{
    _Actor207200CreepingStrangerWork* work;
    TmdObject*                        obj;
    GfxCoord*                         coord;
    s16                               state;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    coord = obj->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            state = work->deathPhase;
            switch (state) {
                case ACTOR_207200_DEATH_PHASE_BEGIN:
                    Gp_ReleaseStateF0Add(arg1, 0x2B);
                    work->deathPhase    = ACTOR_207200_DEATH_PHASE_SETTLE;
                    work->phaseFrames   = 0;
                    work->flattenScaleY = 0x1000;
                    work->savedRootMtx  = coord->coord;
                    arg0->recs          = 0;
                    worldTargetUnlinkNode(&arg0->node);
                    worldCollisionUnlinkBody(&work->senseBody);
                    worldCollisionUnlinkBody(&work->body);
                    worldCollisionUnlinkBody(&work->headBody);
                    worldCollisionUnlinkBody(&work->frontAttackBody);
                    worldCollisionUnlinkBody(&work->sideAttackBody);
                    break;
                case ACTOR_207200_DEATH_PHASE_SETTLE:
                    if (work->hasBurst == 0) {
                        if (work->animId == ACTOR_207200_ANIM_RECOIL) {
                            if (work->animFrames >= 100) {
                                work->deathPhase = ACTOR_207200_DEATH_PHASE_FLATTEN;
                            }
                        } else {
                            work->deathPhase = ACTOR_207200_DEATH_PHASE_FLATTEN;
                        }
                    } else {
                        obj->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                        work->deathPhase = ACTOR_207200_DEATH_PHASE_FLATTEN;
                    }
                    break;
                case ACTOR_207200_DEATH_PHASE_FLATTEN:
                    work->phaseFrames++;
                    if (work->phaseFrames >= 0x3D) {
                        work->deathPhase = ACTOR_207200_DEATH_PHASE_DESTROY;
                    }
                    if (work->hasBurst == 0) {
                        func_actor_207200_8014D7E8(arg1);
                        if (work->phaseFrames == 0xA) {
                            obj->flags = TMD_OBJECT_SEMI_TRANS;
                        }
                        if (work->phaseFrames == 0xF) {
                            Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 2, NULL);
                        }
                    }
                    break;
                case ACTOR_207200_DEATH_PHASE_DESTROY:
                    enemyDestroy(arg0, arg1);
                    return;
            }
            Actor207200_TickAnim(arg1);
            func_actor_207200_8014D97C(arg1, &arg1->extra.tmd->coords[2]);
            func_actor_207200_8014D97C(arg1, &arg1->extra.tmd->coords[3]);
            arg1->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
            arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&arg1->extra.tmd->coords[1]);
            Actor207200_UpdateColor(arg0, arg1);
            break;
    }
}

/// Measures the model held in pointer slot 3 from coordinate `arg0`: returns
/// its bearing in `arg0`'s own frame, folded into -0x800..0x800, and stores
/// in `*arg1` the planar x/z distance between the two coordinates' local
/// translations. The work is staged in a block of the scratch stack.
static s32 func_actor_207200_8014CE20(GfxCoord* arg0, u32* arg1)
{
    GfxCoord*            other;
    ActorBearingScratch* blk;
    s32                  angle;

    other         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    blk           = SCRATCH_STACK_RESERVE_BLOCK(ActorBearingScratch);
    angle         = actorBearingInFrame(blk, arg0, other);
    blk->delta.vx = other->coord.t[0] - arg0->coord.t[0];
    blk->delta.vz = other->coord.t[2] - arg0->coord.t[2];
    *arg1         = SquareRoot0(blk->delta.vx * blk->delta.vx + blk->delta.vz * blk->delta.vz);
    SCRATCH_STACK_RELEASE_BLOCK(ActorBearingScratch);
    return angle;
}

/// Spawns the pair of effects that carry this actor's death animation, hands
/// the spawned task `_gActor207200CreepingStrangerBurstHead` as its setup argument, arms the
/// two timers on the work area and unlinks its third display object.
static void func_actor_207200_8014CFEC(Task* arg0)
{
    EffectSpawnArg*                   effArg;
    EffectWork*                       effect;
    _Actor207200CreepingStrangerWork* work;
    Enemy*                            ctx;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;

    Gp_SpawnEff(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords, 0, NULL);
    func_800DA6E8(&ctx->node, ctx->hp - 1, 0);
    D_800626EC[5].data.model = &_gActor207200CreepingStrangerBurstHead;
    effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 3, 0, NULL);
    if (effect != NULL) {
        func_actor_207200_8014DAF8(effect->task, arg0);
    }
    effArg = &work->headLossEffectArg;
    func_800FDB18(5, arg0->extra.tmd->coords + 3, &D_actor_207200_80153F18, effArg);
    func_800FDB18(5, arg0->extra.tmd->coords + 3, &D_actor_207200_80153F18, effArg);
    work->headBurst       = 1;
    work->animId          = ACTOR_207200_ANIM_RECOIL;
    work->headLost        = 1;
    ctx->recs             = work->bodyContacts;
    ctx->hp               = 1;
    work->headBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionUnlinkBody(&work->headBody);
    arg0->killCountdown = 0x14;
}

static void func_actor_207200_8014D128(Task* arg0)
{
    EffectWork* effect;
    s32         r;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    r               = (gRandomLcgState >> 16) & 3;
    switch (r) {
        case 0:
        case 1:
            D_800626EC[5].data.model = &_gActor207200CreepingStrangerBurstHead;
            effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 3, 0, NULL);
            if (effect != NULL) {
                func_actor_207200_8014DAF8(effect->task, arg0);
            }
            break;
        case 2:
            D_800626EC[5].data.model = &_gActor207200CreepingStrangerBurstArm;
            effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 5, 0, NULL);
            if (effect != NULL) {
                func_actor_207200_8014DAF8(effect->task, arg0);
            }
            break;
        case 3:
            D_800626EC[5].data.model = &_gActor207200CreepingStrangerBurstLeg;
            effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 2, 0, NULL);
            if (effect != NULL) {
                func_actor_207200_8014DAF8(effect->task, arg0);
            }
            break;
    }
    Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 3, 0x300, NULL);
    Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 2, 0x300, NULL);
}

void func_actor_207200_8014D280(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_207200_80149E30;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Per-frame tick of the actor's live state. `gSceneCombatState.actorControl` gates it: mode 1
/// skips the update and runs only the tail, mode 2 puts the model in its
/// hidden pose (part flag 0x80, node not lockable) and returns without updating,
/// mode 0 clears both flags before falling into the update, and any other mode
/// updates directly. The update drives the model's two attach coordinates,
/// clears the display flags of the first two parts and recomputes the second
/// part's world matrix; the tail then colours the actor from that part and
/// draws its ground shadow.
static void func_actor_207200_8014D2DC(Enemy* arg0, Task* arg1)
{
    switch (gSceneCombatState.actorControl) {
        case 1:
            func_actor_207200_8014D70C(arg0, arg1);
            func_actor_207200_8014D77C(arg1);
            return;
        case 0:
            arg1->extra.tmd->flags       = 0;
            arg0->node.state.parts.flags = 0;
            break;
        case 2:
            arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = 1;
            return;
    }
    func_actor_207200_8014D41C(arg1);
    func_actor_207200_8014D8DC(arg1);
    func_actor_207200_8014BEF4(arg1);
    func_actor_207200_8014D49C(arg1);
    func_actor_207200_8014D5C4(arg1);
    func_actor_207200_8014D65C(arg1);
    func_actor_207200_8014D97C(arg1, &arg1->extra.tmd->coords[2]);
    func_actor_207200_8014D97C(arg1, &arg1->extra.tmd->coords[3]);
    arg1->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg1->extra.tmd->coords[1]);
    func_actor_207200_8014D70C(arg0, arg1);
    func_actor_207200_8014D77C(arg1);
}

/// Consumes the pending flag bits on the actor's spawn object once the actor
/// has been set up. Bit 0x1 (the "flag 1" request) is cleared first; bit 0x2
/// then starts the status hold - `ACTOR_207200_STATE_STATUS_HOLD` with the
/// fidget animation requested afresh and every frame counter reset - and clears
/// itself; bits 0xC (the "flag 4"
/// request) are cleared last. Nothing happens while the whole byte is zero.
static void func_actor_207200_8014D41C(Task* arg0)
{
    Enemy*                            obj;
    _Actor207200CreepingStrangerWork* work;
    u8                                flags;

    obj   = arg0->spawnArg2.pointer;
    flags = obj->reactionFlags;
    work  = arg0->work;
    if (flags != 0) {
        if (flags & ENEMY_REACTION_STAGGER) {
            obj->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
        }
        if (obj->reactionFlags & ENEMY_REACTION_BUILDUP) {
            obj->reactionFlags = obj->reactionFlags & ENEMY_REACTION_BUILDUP_CLEAR;
            work->state        = ACTOR_207200_STATE_STATUS_HOLD;
            work->appliedAnim  = ACTOR_207200_ANIM_IDLE;
            work->phaseFrames  = 0;
            work->forwardSpeed = 0;
            work->animId       = ACTOR_207200_ANIM_FIDGET;
            work->animFrames   = 0;
        }
        flags = obj->reactionFlags;
        if (flags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            obj->reactionFlags = flags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

/// Per-frame tick of the enemy's behaviour, driven by `work->state`. The kill
/// countdown on the task is decremented first and clamped at zero. The dormant
/// and active states hand the actor to their own tick bodies; the status hold
/// counts `work->phaseFrames` up to 0x3D frames before requesting the fidget
/// animation afresh, and drops back to the dormant state once `Gp_TickObjFlag2`
/// reports that the status buildup is done; the recoil waits until
/// `work->animFrames` reaches 0x69 and then returns to the dormant state, with
/// the idle animation when the head is lost (`work->headLost != 0`) and with
/// `work->wakeRequested` set otherwise.
static void func_actor_207200_8014D49C(Task* arg0)
{
    _Actor207200CreepingStrangerWork* work;
    s16                               countdown;

    work                = arg0->work;
    countdown           = (u16)arg0->killCountdown - 1;
    arg0->killCountdown = countdown;
    if (countdown < 0) {
        arg0->killCountdown = 0;
    }
    switch (work->state) {
        case ACTOR_207200_STATE_DORMANT:
            func_actor_207200_8014B628(arg0);
            break;
        case ACTOR_207200_STATE_ACTIVE:
            func_actor_207200_8014B87C(arg0);
            break;
        case ACTOR_207200_STATE_STATUS_HOLD:
            work->phaseFrames = work->phaseFrames + 1;
            if (work->phaseFrames >= 0x3D) {
                work->appliedAnim = ACTOR_207200_ANIM_IDLE;
                work->animId      = ACTOR_207200_ANIM_FIDGET;
                work->animFrames  = 0;
                work->phaseFrames = 0;
            }
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                work->state = ACTOR_207200_STATE_DORMANT;
            }
            break;
        case ACTOR_207200_STATE_RECOIL:
            if (work->animFrames >= 0x69) {
                if (work->headLost != 0) {
                    work->state  = ACTOR_207200_STATE_DORMANT;
                    work->animId = ACTOR_207200_ANIM_IDLE;
                    break;
                }
                work->state         = ACTOR_207200_STATE_DORMANT;
                work->wakeRequested = 1;
            }
            break;
    }
}

/// Walks the model's root part forward. While the enemy still has its head
/// (`work->headLost == 0`) the part's current translation is remembered in
/// `work->prevRootPos`, and the part is then displaced along its own forward
/// axis - the third basis column of its local matrix, scaled by
/// `work->forwardSpeed` - and lifted by 0x80.
static void func_actor_207200_8014D5C4(Task* arg0)
{
    _Actor207200CreepingStrangerWork* work;
    GfxCoord*                         coord;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->headLost == 0) {
        work->prevRootPos.vx = coord->coord.t[0];
        work->prevRootPos.vy = coord->coord.t[1];
        work->prevRootPos.vz = coord->coord.t[2];
    }
    coord->coord.t[0] += (coord->coord.m[0][2] * work->forwardSpeed) >> 12;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->forwardSpeed) >> 12;
}

/// Applies the work's `animId` to slots 1..6. When it differs from
/// `appliedAnim` the applied id follows it, `animFrames` restarts and every slot
/// is blended into the new set over 8 frames; otherwise `animFrames` ticks and
/// the slots are simply advanced by one.
static void func_actor_207200_8014D65C(Task* arg0)
{
    Actor207200_TickAnim(arg0);
}

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off the scratch stack, fills it with that coordinate's
/// world position and hands it to `worldCoordUpdateActorColor` with zero for the unused
/// arguments. `arg0` is the colour target, passed straight through.
static void func_actor_207200_8014D70C(Enemy* arg0, Task* task)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;

    coord                          = &task->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    worldCoordUpdateActorColor(arg0, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Draws the enemy's ground quad under its model root, at the translation of
/// the root part's `workm`, staged in a `VECTOR3` on the scratch stack.
static void func_actor_207200_8014D77C(Task* task)
{
    GfxCoord* coord;
    VECTOR3*  vec;

    coord   = task->extra.tmd->coords;
    vec     = (VECTOR3*)SCRATCH_STACK_RESERVE_BYTES(0x18);
    vec->vx = coord->workm.t[0];
    vec->vy = coord->workm.t[1];
    vec->vz = coord->workm.t[2];
    effectDrawGroundShadow(vec, 0x1C0, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

/// Rebuilds the first coordinate node of the actor's model from the transform
/// stored in `work->savedRootMtx`, scaled along Y by `work->flattenScaleY` (a
/// 0x1000-per-unit scale, decaying by 0x50 a frame while it sits above 0x200). The
/// `ActorScaleScratch` block that holds the scaling matrix and its `VECTOR` is
/// borrowed from the scratch stack and released again; the node's
/// `composeStamp` is cleared so the next `actorRenderComposeCoord` recomputes it.
static void func_actor_207200_8014D7E8(Task* arg0)
{
    GfxCoord*                         coord;
    ActorScaleScratch*                head;
    ActorScaleScratch*                scratch;
    _Actor207200CreepingStrangerWork* work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->flattenScaleY >= 0x201) {
        work->flattenScaleY -= 0x50;
    }
    scratch->scale.vx                    = ONE;
    scratch->scale.vy                    = work->flattenScaleY;
    scratch->scale.vz                    = ONE;
    coord->coord                         = work->savedRootMtx;
    scratch->matrix.rotationWords.m00M01 = ONE;
    scratch->matrix.rotationWords.m02M10 = 0;
    scratch->matrix.rotationWords.m11M12 = ONE;
    scratch->matrix.rotationWords.m20M21 = 0;
    scratch->matrix.rotationWords.m22    = ONE;
    ScaleMatrix(&scratch->matrix.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}

/// Re-picks the model part the enemy's `coord` points at and relinks its
/// lock-on node. Once `headLost` is set it is always the second part;
/// before that it is the fourth part while the model in pointer slot 3 lies
/// within a quarter turn of the root's heading, and the second otherwise.
static void func_actor_207200_8014D8DC(Task* arg0)
{
    _Actor207200CreepingStrangerWork* work;
    Enemy*                            ctx;
    GfxCoord*                         coord;
    s32                               dist;
    s32                               angle;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->headLost != 0) {
        coord = arg0->extra.tmd->coords + 1;
    } else {
        angle = func_actor_207200_8014CE20(arg0->extra.tmd->coords, &dist);
        if (angle < 0) {
            angle = -angle;
        }
        if (angle < 0x400) {
            coord = arg0->extra.tmd->coords + 3;
        } else {
            coord = arg0->extra.tmd->coords + 1;
        }
    }
    ctx->coord = coord;
    worldTargetLinkNode(&ctx->node);
}

/// While `work->headLost` is set, runs each column of the node's rotation
/// matrix through GTE `gpf 12` with a zero interpolation factor, zeroing the
/// 3x3 part, and clears `composeStamp` so the node is recomputed.
static void func_actor_207200_8014D97C(Task* arg0, GfxCoord* arg1)
{
    SVECTOR vec;
    MATRIX* m;

    if (((_Actor207200CreepingStrangerWork*)arg0->work)->headLost != 0) {
        m = &arg1->coord;
        gte_ReadMatrixColumn(m, 0, &vec);
        gte_lddp(0);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        gte_WriteMatrixColumn(&vec, m, 0);

        gte_ReadMatrixColumn(m, 1, &vec);
        gte_lddp(0);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        gte_WriteMatrixColumn(&vec, m, 1);

        gte_ReadMatrixColumn(m, 2, &vec);
        gte_lddp(0);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        gte_WriteMatrixColumn(&vec, m, 2);

        arg1->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Copies the texture page and CLUT from `src`'s model onto `dst`'s and, when
/// `dst` has a stream buffer, processes it twice so both halves pick the new
/// pair up. The enemy calls it with a freshly spawned effect as `dst` and
/// itself as `src`.
static void func_actor_207200_8014DAF8(Task* dst, Task* src)
{
    TmdObject* to;
    TmdObject* from;

    from                  = src->extra.tmd;
    to                    = dst->extra.tmd;
    to->texturePageOffset = from->texturePageOffset;
    to->clutRowOffset     = from->clutRowOffset;
    if (to->buffer != NULL) {
        tmdBuildBufferHalf(to);
        tmdBuildBufferHalf(to);
    }
}

static void func_actor_207200_8014DB4C(Task* arg0)
{
    Enemy*                            ctx;
    _Actor207200CreepingStrangerWork* work;

    ctx       = arg0->spawnArg2.pointer;
    work      = arg0->work;
    ctx->recs = 0;
    worldTargetUnlinkNode(&ctx->node);
    worldCollisionUnlinkBody(&work->senseBody);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->headBody);
    worldCollisionUnlinkBody(&work->frontAttackBody);
    worldCollisionUnlinkBody(&work->sideAttackBody);
    enemyTaskExit(arg0);
}
