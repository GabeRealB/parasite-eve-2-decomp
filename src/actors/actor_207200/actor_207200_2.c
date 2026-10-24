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
#include "gameplay/actor_render_shadow_types.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
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
/// spawned effect as its model through `gTaskBank8Descs[TASK_BANK8_BURST_MODEL_PART_SLOT].data.model`.
static TmdSource _gSkullStalkerAndCreepingStrangerCreepingStrangerBurstLeg;
static TmdSource _gSkullStalkerAndCreepingStrangerCreepingStrangerBurstArm;
static TmdSource _gSkullStalkerAndCreepingStrangerCreepingStrangerBurstHead;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Restores a root transform and applies signed Q12 Y scale in borrowed scratch.
///
/// Replaces the complete local matrix from `unscaled` before multiplying its
/// local Y basis column by `*scaleY` (4096 = unity); X/Z scale stay at unity.
/// Translation is retained, so repeated calls do not compound the flattening.
/// All pointers must be live and separate; the caller reserves/releases one
/// `ActorScaleScratch`. Marks composition dirty and changes GTE state.
static inline void _actor207200CreepingStrangerApplyRootScale(GfxCoord* rootCoord, const MATRIX* unscaled, const s16* scaleY, ActorScaleScratch* scratch)
{
    scratch->scale.vx = ONE;
    scratch->scale.vy = *scaleY;
    scratch->scale.vz = ONE;
    rootCoord->coord  = *unscaled;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&rootCoord->coord, &scratch->matrix);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Rig parts used by head loss and shattering, and the body's post-loss hit delay.
enum {
    ACTOR_207200_CREEPING_STRANGER_HEAD_PART           = 3,
    ACTOR_207200_CREEPING_STRANGER_ARM_PART            = 5,
    ACTOR_207200_CREEPING_STRANGER_LEG_PART            = 2,
    ACTOR_207200_CREEPING_STRANGER_HEAD_LOSS_HIT_DELAY = 20,
};

/// Shift of this enemy placement's instance tag within a sound-script request.
enum { ACTOR_207200_CREEPING_STRANGER_SOUND_INSTANCE_SHIFT = 8 };

static void        _actor207200CreepingStrangerSpawnState(Enemy* enemy, Task* task);
static void        _actor207200CreepingStrangerDormantTick(Task* task);
static void        _actor207200CreepingStrangerActiveTick(Task* task);
static inline void _actor207200CreepingStrangerTickAnimation(Task* task);
static void        _actor207200CreepingStrangerApplyHeadDamage(Task* task, s32 damage);
static s32         _actor207200CreepingStrangerMeasurePlayer(GfxCoord* reference, u32* rangeOut);
static void        _actor207200CreepingStrangerDeathState(Enemy* enemy, Task* task);
static void        _actor207200CreepingStrangerLiveState(Enemy* enemy, Task* task);
static void        _actor207200CreepingStrangerBurstHead(Task* task);
static void        _actor207200CreepingStrangerBurstRandomPart(Task* task);
static void        _actor207200CreepingStrangerConsumeReactions(Task* task);
static void        _actor207200CreepingStrangerUpdateBehavior(Task* task);
static void        _actor207200CreepingStrangerStepForward(Task* task);
static void        _actor207200CreepingStrangerAnimate(Task* task);
static void        _actor207200CreepingStrangerUpdateLiveColor(Enemy* enemy, Task* task);
static void        _actor207200CreepingStrangerDrawGroundShadow(Task* task);
static void        _actor207200CreepingStrangerFlatten(Task* task);
static void        _actor207200CreepingStrangerUpdateTarget(Task* task);
static void        _actor207200CreepingStrangerCollapseHeadPart(Task* task, GfxCoord* partCoord);
static void        _actor207200CreepingStrangerCopyBurstTextures(Task* burstTask, Task* sourceTask);
static void        _actor207200CreepingStrangerExit(Task* task);

/// Creeping Stranger lifecycle: 0 spawn, 1 live combat update, 2 death sequence.
///
/// Task state indexes a complete twelve-byte value copy, without a terminator.
/// Each handler takes the live Enemy spawn payload and owning Task; the death
/// sequence eventually releases both. Keep this package loaded through death.
static const EnemyTaskFuncTable3 _gActor207200CreepingStrangerTaskStates = {
    { _actor207200CreepingStrangerSpawnState, _actor207200CreepingStrangerLiveState, _actor207200CreepingStrangerDeathState }
};

static void _actor207200CreepingStrangerTask(Task* task);

/// Creeping Stranger combat tuning and rewards, borrowed by its live Enemy records.
///
/// Starts at 250 HP; release credits 15 experience, 48 BP and 1 MP. Critical
/// base is 50 percent and buildup lasts ten 31-frame steps before grade scaling.
/// Damage-over-time chance and pulse count are zero. The front/side attack
/// table and this writable record must remain loaded until enemy teardown.
static EnemyParams _gActor207200CreepingStrangerParams = { gActor207200CreepingStrangerAttacks, 250, 15, 48, 1, 50, 10, 0, 0 };

#include "assets/encounters/skull_stalker_and_creeping_stranger/creeping_stranger_body.inc"

#include "assets/encounters/skull_stalker_and_creeping_stranger/creeping_stranger_burst_leg.inc"

#include "assets/encounters/skull_stalker_and_creeping_stranger/creeping_stranger_burst_arm.inc"

#include "assets/encounters/skull_stalker_and_creeping_stranger/creeping_stranger_burst_head.inc"

#include "assets/encounters/skull_stalker_and_creeping_stranger/package_2_clips_2.inc"

TaskDesc gActor207200CreepingStrangerTaskDesc = { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _actor207200CreepingStrangerTask, { .model = &_gSkullStalkerAndCreepingStrangerCreepingStrangerBody } };

/// Creeping Stranger body clips indexed by `ACTOR_207200_ANIM_*`.
///
/// Slot 0 is empty and behavior selects clips 1..11. The thirteenth pointer
/// is retained without a recovered selector. Live rigs borrow the table and
/// all clip data for their lifetime, so this package must remain loaded.
#include "assets/encounters/skull_stalker_and_creeping_stranger/creeping_stranger_bank.inc"

/// Zero local XYZ offset for damage-hit effects at the Creeping Stranger's head part.
///
/// Effects place at coordinate 3's origin. The offset stays borrowed while an
/// effect uses it, so this overlay must remain loaded; the fourth halfword is unused.
static SVECTOR _gActor207200HeadHitOffset = { 0 };

/// Local XYZ offset of body damage-hit effects, in whole game-coordinate units.
///
/// Added in body coordinate 1's frame: X 0, Y -100, Z -150. The offset stays
/// borrowed while an effect uses it; the fourth halfword is unused.
static SVECTOR _gActor207200BodyHitOffset = { 0, -100, -150, 0 };

/// Head-loss splatter's zero local offset, also selecting a random spray heading.
///
/// Both lethal head damage and critical head bursts use it for two spray
/// spawns at the Creeping Stranger's head origin. Spawning copies XYZ; the
/// spray normalizes that copy or chooses random XYZ when all three are zero.
/// The retained pointer is not followed by this spray callback, and the fourth
/// halfword is not a coordinate component.
static SVECTOR _gActor207200HeadLossSplatterOffset = { 0 };

/// Crawl displacement per tick in parent-coordinate units, along the root's facing.
///
/// Animation frames 20..39 index this curve at frame minus 20; all other crawl
/// frames use zero speed. Values rise to 27 and fall back to zero.
static s16 _gActor207200CreepingStrangerCrawlSpeeds[20] = {
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

static void        _actor207200CreepingStrangerScanContacts(Task* task);
static inline void _actor207200CreepingStrangerUpdateColor(Enemy* enemy, Task* task);

/// Creates the Creeping Stranger's seven-part rig and five collision spheres.
///
/// Requires the live model task and its owning Enemy. The zeroed work block is
/// owned by the task; model lighting, contacts and effect records borrow it.
/// Starts slots 1..6 at idle, acquires a battle reference and advances task
/// state 0 to 1. Allocation failure destroys the enemy and task immediately.
static void _actor207200CreepingStrangerSpawnState(Enemy* enemy, Task* task)
{
    enum { ACTOR_207200_CREEPING_STRANGER_BODY_ID                = 0x2B,
           ACTOR_207200_CREEPING_STRANGER_HIT_EFFECT_SCALE       = 0x100,
           ACTOR_207200_CREEPING_STRANGER_HEAD_LOSS_EFFECT_SCALE = 0x400 };
    _Actor207200CreepingStrangerWork* work;
    TmdObject*                        model;
    GfxCoord*                         rootCoord;
    GfxCoord*                         sideAttackCoord;
    GfxCoord*                         headCoord;
    s32                               slotIndex;

    // Bind the task-owned rig and lighting, then publish the head hit table.
    model           = task->extra.tmd;
    rootCoord       = model->coords;
    work            = memCalloc(sizeof(*work), false);
    sideAttackCoord = rootCoord + 6;
    headCoord       = rootCoord + 3;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work              = work;
    model->flags            = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = rootCoord;
    enemy->node.state.parts.flags = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &_gActor207200CreepingStrangerParams;
    enemy->recs                   = work->headContacts;
    enemy->hp                     = _gActor207200CreepingStrangerParams.hpMax;
    work->rotation.vy             = rootCoord->param.rot.vy;
    animationInitContext(&work->rig.anim, _gActor207200CreepingStrangerAnimationSets, model, work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, ACTOR_207200_ANIM_IDLE);
    }
    sceneAcquireBattleRef(0);

    work->animId      = ACTOR_207200_ANIM_IDLE;
    work->appliedAnim = ACTOR_207200_ANIM_IDLE;
    work->headBurst   = 0;
    work->headLost    = 0;
    work->deathPhase  = ACTOR_207200_DEATH_PHASE_BEGIN;
    work->blocked     = 0;
    work->hitCooldown = 0;

    // Link sensing, body and head spheres; attack spheres start disabled.
    work->senseBody.coord            = rootCoord;
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
    work->body.coord            = rootCoord;
    work->body.context.contacts = work->bodyContacts;
    work->body.pos.vx           = 0;
    work->body.key              = (WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_207200_CREEPING_STRANGER_BODY_ID);
    work->body.radius           = 0x12C;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->senseBody.flags      |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);

    work->headBody.coord            = headCoord;
    work->headBody.context.contacts = work->headContacts;
    work->headBody.pos.vx           = 0;
    work->headBody.pos.vy           = 0;
    work->headBody.pos.vz           = 0;
    work->headBody.key              = (WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_207200_CREEPING_STRANGER_BODY_ID);
    work->headBody.radius           = 0x96;
    work->headBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->body.flags               |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->headBody);
    worldCollisionInitContacts(work->headContacts, ARRAY_SIZE(work->headContacts), 0);

    work->frontAttackBody.coord            = headCoord;
    work->frontAttackBody.context.contacts = work->frontAttackContacts;
    work->frontAttackBody.pos.vx           = 0;
    work->frontAttackBody.pos.vy           = 0x50;
    work->frontAttackBody.pos.vz           = 0x8C;
    work->headBody.flags                  |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->frontAttackBody.key              = damagePackAttackKey(gActor207200CreepingStrangerAttacks, ACTOR_207200_CREEPING_STRANGER_ATTACK_FRONT);
    work->frontAttackBody.radius           = 0x12C;
    work->frontAttackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->frontAttackBody);
    worldCollisionInitContacts(work->frontAttackContacts, ARRAY_SIZE(work->frontAttackContacts), 0);

    work->sideAttackBody.coord            = sideAttackCoord;
    work->sideAttackBody.context.contacts = work->sideAttackContacts;
    work->sideAttackBody.pos.vx           = 0xFA;
    work->sideAttackBody.pos.vy           = 0;
    work->sideAttackBody.pos.vz           = 0;
    work->frontAttackBody.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->sideAttackBody.key              = damagePackAttackKey(gActor207200CreepingStrangerAttacks, ACTOR_207200_CREEPING_STRANGER_ATTACK_SIDE);
    work->sideAttackBody.radius           = 0x12C;
    work->sideAttackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sideAttackBody);
    worldCollisionInitContacts(work->sideAttackContacts, ARRAY_SIZE(work->sideAttackContacts), 0);
    work->sideAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    // Keep hit effects attached to the struck part throughout its lifetime.
    work->headHitEffectArg.coord       = task->extra.tmd->coords + 3;
    work->headHitEffectArg.spawnArgLo  = ACTOR_207200_CREEPING_STRANGER_HIT_EFFECT_SCALE;
    work->headHitEffectArg.spawnArgHi  = 1;
    work->headLossEffectArg.coord      = task->extra.tmd->coords + 3;
    work->headLossEffectArg.spawnArgLo = ACTOR_207200_CREEPING_STRANGER_HEAD_LOSS_EFFECT_SCALE;
    work->headLossEffectArg.spawnArgHi = 3;
    work->bodyHitEffectArg.coord       = task->extra.tmd->coords + 1;
    work->bodyHitEffectArg.spawnArgLo  = ACTOR_207200_CREEPING_STRANGER_HIT_EFFECT_SCALE;
    work->bodyHitEffectArg.spawnArgHi  = 1;
    work->hasBurst                     = 0;
    task->exitCallback                 = _actor207200CreepingStrangerExit;
    task->state++;
}

/// Runs dormant sensing and the idle/fidget cycle.
///
/// A headed enemy wakes on player contact or the room alert and chooses a
/// delay in 0..89 frames. Sensing is then disabled. Headless bodies keep the
/// idle cycle without sensing or random fidgets; the fidget sounds at frame 5.
static void _actor207200CreepingStrangerDormantTick(Task* task)
{
    enum { ACTOR_207200_CREEPING_STRANGER_DORMANT_SCRATCH_BYTES = 8,
           ACTOR_207200_CREEPING_STRANGER_WAKE_DELAY_LIMIT      = 90,
           ACTOR_207200_CREEPING_STRANGER_IDLE_FRAMES           = 91,
           ACTOR_207200_CREEPING_STRANGER_FIDGET_CHANCE_PERCENT = 30,
           ACTOR_207200_CREEPING_STRANGER_FIDGET_SOUND_FRAME    = 5,
           ACTOR_207200_CREEPING_STRANGER_FIDGET_FRAMES         = 45,
           ACTOR_207200_CREEPING_STRANGER_SOUND_FIDGET          = 0x40480004 };
    _Actor207200CreepingStrangerWork* work;
    GfxCoord*                         rootCoord;
    s32                               soundEventId;
    s32                               audioPan;
    u32                               randomState;
    Enemy*                            enemy;
    u16                               randomHigh;

    // Retain the untouched scratch reservation around sensing and idle playback.
    SCRATCH_STACK_RESERVE_BYTES(ACTOR_207200_CREEPING_STRANGER_DORMANT_SCRATCH_BYTES);
    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    if (work->headLost == 0) {
        if (worldCollisionCountContactsByKind(work->senseContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
            work->wakeRequested = 1;
        }
        if (work->wakeRequested != 0 || gSceneCombatState.signals.bytes.enemyAlert != 0) {
            randomState            = RANDOM_LCG_NEXT(gRandomLcgState);
            randomHigh             = randomState >> 16;
            work->activeStage      = ACTOR_207200_ACTIVE_STAGE_DELAY;
            work->forwardSpeed     = 0;
            work->state            = ACTOR_207200_STATE_ACTIVE;
            gRandomLcgState        = randomState;
            work->senseBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->wakeDelay        = randomHigh % ACTOR_207200_CREEPING_STRANGER_WAKE_DELAY_LIMIT;
            sceneEngageBattle(1);
        }
        worldCollisionClearContacts(work->senseContacts);
    }
    switch (work->animId) {
        case ACTOR_207200_ANIM_IDLE:
            work->field_498    = 1;
            work->forwardSpeed = 0;
            if (work->animFrames >= ACTOR_207200_CREEPING_STRANGER_IDLE_FRAMES) {
                work->animFrames  = 0;
                work->appliedAnim = ACTOR_207200_ANIM_NONE;
                if (work->headLost == 0) {
                    RANDOM_LCG_ADVANCE();
                    if ((u16)(RANDOM_LCG_DRAW() % 100) < ACTOR_207200_CREEPING_STRANGER_FIDGET_CHANCE_PERCENT) {
                        work->animId = ACTOR_207200_ANIM_FIDGET;
                    }
                }
            }
            break;
        case ACTOR_207200_ANIM_FIDGET:
            if (work->animFrames == ACTOR_207200_CREEPING_STRANGER_FIDGET_SOUND_FRAME) {
                enemy        = task->spawnArg2.pointer;
                soundEventId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_207200_CREEPING_STRANGER_SOUND_INSTANCE_SHIFT) | ACTOR_207200_CREEPING_STRANGER_SOUND_FIDGET;
                audioPan     = (s8)worldCoordGetOriginAudioPan(rootCoord);
                sndEvtRequestScriptStart(soundEventId, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            }
            if (work->animFrames >= ACTOR_207200_CREEPING_STRANGER_FIDGET_FRAMES) {
                work->animId     = ACTOR_207200_ANIM_IDLE;
                work->animFrames = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_207200_CREEPING_STRANGER_DORMANT_SCRATCH_BYTES);
}

/// Advances the delayed crawl, turns, attacks and cry of an active enemy.
///
/// Requires the initialized rig, contacts and live player. Decisions use
/// bearing in 4096ths of a turn and planar range in game-coordinate units.
/// Crawl speed samples frames 20..39; front strikes are enabled on 42..44
/// and side strikes on 30..59. Head loss returns completed turns, attacks and
/// cries to idle; the crawl stage retains its speed sampling.
static void _actor207200CreepingStrangerActiveTick(Task* task)
{
    enum { ACTOR_207200_CREEPING_STRANGER_FRONT_ATTACK_RANGE_LIMIT = 901,
           ACTOR_207200_CREEPING_STRANGER_FRONT_ATTACK_BEARING     = 512,
           ACTOR_207200_CREEPING_STRANGER_CRAWL_STEP_FIRST_FRAME   = 20,
           ACTOR_207200_CREEPING_STRANGER_CRAWL_STEP_END_FRAME     = 40,
           ACTOR_207200_CREEPING_STRANGER_CRAWL_FRAMES             = 75,
           ACTOR_207200_CREEPING_STRANGER_TURN_FIRST_FRAME         = 30,
           ACTOR_207200_CREEPING_STRANGER_TURN_LAST_FRAME          = 50,
           ACTOR_207200_CREEPING_STRANGER_TURN_FRAMES              = 60,
           ACTOR_207200_CREEPING_STRANGER_TURN_STEP                = 25,
           ACTOR_207200_CREEPING_STRANGER_SIDE_STRIKE_FIRST_FRAME  = 30,
           ACTOR_207200_CREEPING_STRANGER_FRONT_ATTACK_SOUND_FRAME = 30,
           ACTOR_207200_CREEPING_STRANGER_FRONT_ATTACK_FRAMES      = 60,
           ACTOR_207200_CREEPING_STRANGER_CRY_FRAMES               = 90,
           ACTOR_207200_CREEPING_STRANGER_FRONT_STRIKE_FIRST_FRAME = 42,
           ACTOR_207200_CREEPING_STRANGER_FRONT_STRIKE_END_FRAME   = 45,
           ACTOR_207200_CREEPING_STRANGER_SIDE_STRIKE_END_FRAME    = 60,
           ACTOR_207200_CREEPING_STRANGER_SIDE_ATTACK_FRAMES       = 90,
           ACTOR_207200_CREEPING_STRANGER_CRY_CHANCE_PERCENT       = 40,
           ACTOR_207200_CREEPING_STRANGER_CRY_SOUND_FRAME          = 10,
           ACTOR_207200_CREEPING_STRANGER_SOUND_FRONT_ATTACK       = 0x40480002,
           ACTOR_207200_CREEPING_STRANGER_SOUND_FRONT_HIT          = 0x40480005,
           ACTOR_207200_CREEPING_STRANGER_SOUND_CRY                = 0x40480006 };
    _Actor207200CreepingStrangerWork* work;
    GfxCoord*                         rootCoord;
    s32                               playerBearing;
    u32                               playerRange;
    s16                               turnAnim;
    Enemy*                            soundEnemy;
    s32                               soundEventId;

/// Starts a spatial Creeping Stranger script tagged with the live placement index.
///
/// Requires the actor's live Enemy payload and composed audio origin; reloads the
/// payload at every use, including after damage callbacks. Stable actorTask/scriptId
/// arguments occur once, audioOrigin twice, and the writable Enemy* enemyLocal and
/// s32 requestId lvalues twice. Arguments must have no side effects. Script bits
/// 8..15 must be clear for the placement tag; pan/depth narrow to signed bytes.
/// Captures the package instance shift, forms one compound statement and retains
/// no pointers. The action and head-damage handlers use the same operation.
#define ACTOR_207200_CREEPING_STRANGER_PLAY_PLACEMENT_SOUND(actorTask, audioOrigin, scriptId, enemyLocal, requestId)                              \
    {                                                                                                                                             \
        (enemyLocal) = (actorTask)->spawnArg2.pointer;                                                                                            \
        (requestId)  = (((enemyLocal)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_207200_CREEPING_STRANGER_SOUND_INSTANCE_SHIFT) | (scriptId); \
        sndEvtRequestScriptStart((requestId), (s8)worldCoordGetOriginAudioPan(audioOrigin), (s8)worldCoordGetOriginAudioDepth(audioOrigin));      \
    }

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    switch (work->activeStage) {
        case ACTOR_207200_ACTIVE_STAGE_DELAY:
            work->animId = ACTOR_207200_ANIM_IDLE;
            if (work->animFrames > work->wakeDelay) {
                work->activeStage = ACTOR_207200_ACTIVE_STAGE_CRAWL;
            }
            break;
        case ACTOR_207200_ACTIVE_STAGE_CRAWL:
            playerBearing = _actor207200CreepingStrangerMeasurePlayer(task->extra.tmd->coords, &playerRange);
            if (work->headLost == 0 && playerRange < ACTOR_207200_CREEPING_STRANGER_FRONT_ATTACK_RANGE_LIMIT && ABS(playerBearing) < ACTOR_207200_CREEPING_STRANGER_FRONT_ATTACK_BEARING) {
                work->forwardSpeed    = 0;
                work->activeStage     = ACTOR_207200_ACTIVE_STAGE_FRONT_ATTACK;
                work->headBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                break;
            }
            work->animId          = ACTOR_207200_ANIM_CRAWL;
            work->field_498       = 0;
            work->headBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            if (work->animFrames >= ACTOR_207200_CREEPING_STRANGER_CRAWL_STEP_FIRST_FRAME && work->animFrames < ACTOR_207200_CREEPING_STRANGER_CRAWL_STEP_END_FRAME) {
                work->forwardSpeed = _gActor207200CreepingStrangerCrawlSpeeds[work->animFrames - ACTOR_207200_CREEPING_STRANGER_CRAWL_STEP_FIRST_FRAME];
            } else {
                work->forwardSpeed = 0;
            }
            if (work->animFrames >= ACTOR_207200_CREEPING_STRANGER_CRAWL_FRAMES) {
                work->animFrames = 0;
                if (work->headLost == 0) {
                    if (ABS(playerBearing) > ACTOR_207200_CREEPING_STRANGER_FRONT_ATTACK_BEARING || work->blocked != 0) {
                        if (playerBearing < 0) {
                            work->turnStep    = -ACTOR_207200_CREEPING_STRANGER_TURN_STEP;
                            work->activeStage = ACTOR_207200_ACTIVE_STAGE_TURN_YAW_DOWN;
                            work->animId      = ACTOR_207200_ANIM_TURN_YAW_DOWN;
                        } else {
                            work->turnStep    = ACTOR_207200_CREEPING_STRANGER_TURN_STEP;
                            work->activeStage = ACTOR_207200_ACTIVE_STAGE_TURN_YAW_UP;
                            work->animId      = ACTOR_207200_ANIM_TURN_YAW_UP;
                        }
                    }
                }
            }
            break;
        case ACTOR_207200_ACTIVE_STAGE_TURN_YAW_UP:
            turnAnim           = ACTOR_207200_ANIM_TURN_YAW_UP;
            work->forwardSpeed = 0;
            work->animId       = turnAnim;
            if (work->animFrames >= ACTOR_207200_CREEPING_STRANGER_TURN_FIRST_FRAME && work->animFrames <= ACTOR_207200_CREEPING_STRANGER_TURN_LAST_FRAME) {
                work->rotation.vx  = 0;
                work->rotation.vz  = 0;
                work->rotation.vy += work->turnStep;
                RotMatrix(&work->rotation, &rootCoord->coord);
            }
            if (work->animFrames >= ACTOR_207200_CREEPING_STRANGER_TURN_FRAMES) {
                if (work->headLost != 0) {
                    work->state  = ACTOR_207200_STATE_DORMANT;
                    work->animId = ACTOR_207200_ANIM_IDLE;
                } else {
                    playerBearing = _actor207200CreepingStrangerMeasurePlayer(task->extra.tmd->coords, &playerRange);
                    if (ABS(playerBearing) < ACTOR_207200_CREEPING_STRANGER_FRONT_ATTACK_BEARING || work->blocked != 0) {
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_CRAWL;
                        work->blocked     = 0;
                        work->animFrames  = 0;
                        work->animId      = ACTOR_207200_ANIM_CRAWL;
                    } else if (playerBearing < 0) {
                        work->turnStep    = -ACTOR_207200_CREEPING_STRANGER_TURN_STEP;
                        work->animFrames  = 0;
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_TURN_YAW_DOWN;
                        work->animId      = ACTOR_207200_ANIM_TURN_YAW_DOWN;
                    } else {
                        work->turnStep    = ACTOR_207200_CREEPING_STRANGER_TURN_STEP;
                        work->animFrames  = 0;
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_TURN_YAW_UP;
                        work->animId      = turnAnim;
                    }
                }
            }
            break;
        case ACTOR_207200_ACTIVE_STAGE_TURN_YAW_DOWN:
            turnAnim           = ACTOR_207200_ANIM_TURN_YAW_DOWN;
            work->forwardSpeed = 0;
            work->animId       = turnAnim;
            if (work->animFrames >= ACTOR_207200_CREEPING_STRANGER_TURN_FIRST_FRAME && work->animFrames <= ACTOR_207200_CREEPING_STRANGER_TURN_LAST_FRAME) {
                work->rotation.vx  = 0;
                work->rotation.vz  = 0;
                work->rotation.vy += work->turnStep;
                RotMatrix(&work->rotation, &rootCoord->coord);
            }
            if (work->animFrames >= ACTOR_207200_CREEPING_STRANGER_TURN_FRAMES) {
                if (work->headLost != 0) {
                    work->state  = ACTOR_207200_STATE_DORMANT;
                    work->animId = ACTOR_207200_ANIM_IDLE;
                } else {
                    playerBearing = _actor207200CreepingStrangerMeasurePlayer(task->extra.tmd->coords, &playerRange);
                    if (ABS(playerBearing) < ACTOR_207200_CREEPING_STRANGER_FRONT_ATTACK_BEARING || work->blocked != 0) {
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_CRAWL;
                        work->blocked     = 0;
                        work->animFrames  = 0;
                        work->animId      = ACTOR_207200_ANIM_CRAWL;
                    } else if (playerBearing < 0) {
                        work->turnStep    = -ACTOR_207200_CREEPING_STRANGER_TURN_STEP;
                        work->animFrames  = 0;
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_TURN_YAW_DOWN;
                        work->animId      = turnAnim;
                    } else {
                        work->turnStep    = ACTOR_207200_CREEPING_STRANGER_TURN_STEP;
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_TURN_YAW_UP;
                        work->animFrames  = 0;
                        work->animId      = ACTOR_207200_ANIM_TURN_YAW_UP;
                    }
                }
            }
            break;
        case ACTOR_207200_ACTIVE_STAGE_FRONT_ATTACK:
            work->forwardSpeed = 0;
            if (work->animFrames == ACTOR_207200_CREEPING_STRANGER_FRONT_ATTACK_SOUND_FRAME) {
                work->frontAttackLanded = 0;
                ACTOR_207200_CREEPING_STRANGER_PLAY_PLACEMENT_SOUND(task, rootCoord, ACTOR_207200_CREEPING_STRANGER_SOUND_FRONT_ATTACK, soundEnemy, soundEventId);
            }
            // Pair tests run only during the strike; sound a registered hit afterward.
            if (work->animFrames == ACTOR_207200_CREEPING_STRANGER_FRONT_STRIKE_FIRST_FRAME) {
                work->frontAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->animFrames == ACTOR_207200_CREEPING_STRANGER_FRONT_STRIKE_END_FRAME) {
                work->frontAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->frontAttackLanded != 0 && work->animFrames == ACTOR_207200_CREEPING_STRANGER_FRONT_STRIKE_END_FRAME) {
                ACTOR_207200_CREEPING_STRANGER_PLAY_PLACEMENT_SOUND(task, rootCoord, ACTOR_207200_CREEPING_STRANGER_SOUND_FRONT_HIT, soundEnemy, soundEventId);
            }
            if (work->animId == ACTOR_207200_ANIM_FRONT_ATTACK && work->animFrames >= ACTOR_207200_CREEPING_STRANGER_FRONT_ATTACK_FRAMES) {
                if (work->headLost != 0) {
                    work->state  = ACTOR_207200_STATE_DORMANT;
                    work->animId = ACTOR_207200_ANIM_IDLE;
                } else {
                    RANDOM_LCG_ADVANCE();
                    if ((u16)(RANDOM_LCG_DRAW() % 100) < ACTOR_207200_CREEPING_STRANGER_CRY_CHANCE_PERCENT) {
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
            if (work->animFrames == ACTOR_207200_CREEPING_STRANGER_SIDE_STRIKE_FIRST_FRAME) {
                work->sideAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->animFrames == ACTOR_207200_CREEPING_STRANGER_SIDE_STRIKE_END_FRAME) {
                work->sideAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->animFrames >= ACTOR_207200_CREEPING_STRANGER_SIDE_ATTACK_FRAMES) {
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
            if (work->animFrames == ACTOR_207200_CREEPING_STRANGER_CRY_SOUND_FRAME) {
                ACTOR_207200_CREEPING_STRANGER_PLAY_PLACEMENT_SOUND(task, rootCoord, ACTOR_207200_CREEPING_STRANGER_SOUND_CRY, soundEnemy, soundEventId);
            }
            if (work->animFrames >= ACTOR_207200_CREEPING_STRANGER_CRY_FRAMES) {
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

/// Applies Creeping Stranger grid correction or restores the pre-movement root.
///
/// Requires live work/rootCoord and readable contactCount records (six at each
/// caller); the signed count is in 0..32768 within their extent. contacts and
/// contactCount are evaluated once, and contacts are not cleared. pushback is
/// writable disjoint WorldCollisionDelta storage: grid hits add signed 16.16
/// integer halves in root-parent units; opposed normals restore prevRootPos
/// and latch blocked only while headed. Head loss stops refreshing prevRootPos,
/// but rollback still uses it. Other results leave the root unchanged. Stable
/// pointer arguments are required because work/rootCoord/pushback repeat.
/// Captures no locals; leaves composition to the caller. Invoke within braces.
#define ACTOR_207200_CREEPING_STRANGER_RESOLVE_GRID_CONTACTS(work, rootCoord, pushback, contacts, contactCount) \
    {                                                                                                           \
        switch (worldCollisionResolvePushback((contacts), (pushback), (contactCount), NULL)) {                  \
            case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:                                                          \
                break;                                                                                          \
            case WORLD_COLLISION_PUSHBACK_GRID_HIT:                                                             \
                (rootCoord)->coord.t[0] += (pushback)->fixed.vx.halves.integer;                                 \
                (rootCoord)->coord.t[1] += (pushback)->fixed.vy.halves.integer;                                 \
                (rootCoord)->coord.t[2] += (pushback)->fixed.vz.halves.integer;                                 \
                break;                                                                                          \
            case WORLD_COLLISION_PUSHBACK_OPPOSED:                                                              \
                (rootCoord)->coord.t[0] = (work)->prevRootPos.vx;                                               \
                (rootCoord)->coord.t[1] = (work)->prevRootPos.vy;                                               \
                (rootCoord)->coord.t[2] = (work)->prevRootPos.vz;                                               \
                if ((work)->headLost == 0 && (work)->blocked == 0) {                                            \
                    (work)->blocked = 1;                                                                        \
                }                                                                                               \
                break;                                                                                          \
        }                                                                                                       \
    }

/// Resolves Creeping Stranger grid/pair contacts, hits and attack-contact latches.
///
/// Requires matching live task/Enemy/work, composed model/grid frames, valid
/// attack keys and player resources. Scans all six body and head records: grid
/// conflicts restore the saved root; a nearby off-axis player touch starts a
/// side attack, and crawling bodies push out of other enemy spheres. Only head
/// hits hurt a headed actor; damaging a headless body after its kill delay starts
/// death. Shatter and critical-head paths return before clearing contacts or
/// releasing the 72-byte scratch block. The main loop resets that reservation
/// next iteration; later borrowers in the same iteration use the lowered cursor.
/// A headless-body death releases the block without clearing contacts. Normal
/// completion clears contact tables and releases the block. Separation uses
/// signed Q12; cooldowns count frames and hit magnitudes use HP.
static void _actor207200CreepingStrangerScanContacts(Task* task)
{
    enum {
        ACTOR_207200_CREEPING_STRANGER_SIDE_TOUCH_MIN_YAW  = 512,
        ACTOR_207200_CREEPING_STRANGER_SIDE_TOUCH_RANGE    = 2000,
        ACTOR_207200_CREEPING_STRANGER_SOUND_BODY_RECOIL   = 0x40480006,
        ACTOR_207200_CREEPING_STRANGER_SHATTER_ATTRIBUTE_4 = 4,
        ACTOR_207200_CREEPING_STRANGER_SHATTER_ATTRIBUTE_5 = 5,
        ACTOR_207200_CREEPING_STRANGER_BUILDUP_ATTRIBUTE_8 = 8,
        ACTOR_207200_CREEPING_STRANGER_BUILDUP_ATTRIBUTE_9 = 9,
        ACTOR_207200_CREEPING_STRANGER_SHATTER_BURST_STYLE = 2,
        ACTOR_207200_CREEPING_STRANGER_BASIS_FRACTION_BITS = 12,
        ACTOR_207200_CREEPING_STRANGER_HEAD_HIT_PART       = 3,
        ACTOR_207200_CREEPING_STRANGER_BODY_HIT_PART       = 1
    };

    _Actor207200CreepingStrangerWork* work;
    _Actor207200ContactScratch*       scratch;
    GfxCoord*                         rootCoord;
    u32                               playerDistanceXZ;
    Enemy*                            enemy;
    s32                               contactIndex;
    s32                               playerYaw;
    s32                               contactMagnitude;
    s32                               clampedOverlap;
    s32                               hitReaction;
    s32                               contactValue;
    s32                               soundId;
    Enemy*                            soundEnemy;

    work      = task->work;
    scratch   = SCRATCH_STACK_RESERVE_BLOCK(_Actor207200ContactScratch);
    rootCoord = task->extra.tmd->coords;
    enemy     = task->spawnArg2.pointer;

    // Resolve head and body grid corrections before consuming pair contacts.
    ACTOR_207200_CREEPING_STRANGER_RESOLVE_GRID_CONTACTS(work, rootCoord, &scratch->delta, work->headContacts, ARRAY_SIZE(work->headContacts));
    ACTOR_207200_CREEPING_STRANGER_RESOLVE_GRID_CONTACTS(work, rootCoord, &scratch->delta, work->bodyContacts, ARRAY_SIZE(work->bodyContacts));
    if (work->hitCooldown != 0 && --work->hitCooldown <= 0) {
        work->hitCooldown = 0;
    }

    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->bodyContacts); contactIndex++) {
        switch ((u32)work->bodyContacts[contactIndex].key.parts.kind) {
            case (WORLD_COLLISION_CONTACT_PLAYER_BODY >> 16):
                if (work->headLost == 0 && (u16)work->activeStage - ACTOR_207200_ACTIVE_STAGE_CRAWL <
                                               (u32)(ACTOR_207200_ACTIVE_STAGE_FRONT_ATTACK - ACTOR_207200_ACTIVE_STAGE_CRAWL)) {
                    playerYaw = _actor207200CreepingStrangerMeasurePlayer(task->extra.tmd->coords, &playerDistanceXZ);
                    if (abs(playerYaw) > ACTOR_207200_CREEPING_STRANGER_SIDE_TOUCH_MIN_YAW && playerDistanceXZ < ACTOR_207200_CREEPING_STRANGER_SIDE_TOUCH_RANGE) {
                        work->animId      = playerYaw < 0 ? ACTOR_207200_ANIM_SIDE_ATTACK_YAW_DOWN : ACTOR_207200_ANIM_SIDE_ATTACK_YAW_UP;
                        work->animFrames  = 0;
                        work->activeStage = ACTOR_207200_ACTIVE_STAGE_SIDE_ATTACK;
                    }
                }
                break;
            case (WORLD_COLLISION_CONTACT_ATTACK >> 16):
                if (work->hitCooldown != 0) {
                    break;
                }
                scratch->delta.vector.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
                scratch->delta.vector.vy = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
                scratch->delta.vector.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
                contactMagnitude         = SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx +
                                                       scratch->delta.vector.vy * scratch->delta.vector.vy +
                                                       scratch->delta.vector.vz * scratch->delta.vector.vz);
                damageGetPlayerAttackReaction(work->bodyContacts[contactIndex].key.value);
                contactMagnitude = damageComputePlayerAttack(work->bodyContacts[contactIndex].key.value, contactMagnitude, 0, 0);
                effectSpawnHit(damageGetPlayerAttackEffectId(work->bodyContacts[contactIndex].key.value),
                               task->extra.tmd->coords + ACTOR_207200_CREEPING_STRANGER_BODY_HIT_PART, &_gActor207200BodyHitOffset, &work->bodyHitEffectArg);
                contactValue = damageGetPlayerAttackHitCooldown(work->bodyContacts[contactIndex].key.value);
                if ((s16)contactValue > 0) {
                    work->hitCooldown = contactValue;
                }
                if (work->headLost != 0 && task->killCountdown == 0) {
                    worldTargetAddReadoutAmount(&enemy->node, contactMagnitude, 0);
                    if (contactMagnitude != 0) {
                        task->state++;
                        work->animId = ACTOR_207200_ANIM_DEATH;
                        SCRATCH_STACK_RELEASE_BLOCK(_Actor207200ContactScratch);
                        return;
                    }
                } else {
                    worldTargetAddReadoutAmount(&enemy->node, 0, 0);
                }
                if (work->state == ACTOR_207200_STATE_DORMANT) {
                    soundEnemy = task->spawnArg2.pointer;
                    soundId    = ((soundEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_207200_CREEPING_STRANGER_SOUND_INSTANCE_SHIFT) | ACTOR_207200_CREEPING_STRANGER_SOUND_BODY_RECOIL;
                    sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
                    work->animId        = ACTOR_207200_ANIM_RECOIL;
                    work->animFrames    = 0;
                    work->state         = ACTOR_207200_STATE_RECOIL;
                    work->forwardSpeed  = 0;
                    work->wakeRequested = 0;
                }
                break;
            case (WORLD_COLLISION_CONTACT_ENEMY_BODY >> 16):
                scratch->delta.vector.vx = rootCoord->workm.t[0] - work->bodyContacts[contactIndex].point.vx;
                scratch->delta.vector.vy = 0;
                scratch->delta.vector.vz = rootCoord->workm.t[2] - work->bodyContacts[contactIndex].point.vz;
                contactMagnitude         = work->bodyContacts[contactIndex].distance -
                                   SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vz * scratch->delta.vector.vz);
                clampedOverlap = contactMagnitude;
                if (contactMagnitude <= 0) {
                    clampedOverlap = 0;
                }
                contactMagnitude         = clampedOverlap;
                scratch->delta.vector.vx = rootCoord->workm.t[0] - work->bodyContacts[contactIndex].point.vx;
                scratch->delta.vector.vy = rootCoord->workm.t[1] - work->bodyContacts[contactIndex].point.vy;
                scratch->delta.vector.vz = rootCoord->workm.t[2] - work->bodyContacts[contactIndex].point.vz;
                // Rotate the normalized separation into the grid's room-axis frame.
                VectorNormal(&scratch->delta.vector, &scratch->normal);
                ApplyTransposeMatrixLV(&gWorldCollisionActiveGrid->viewCoord->workm, &scratch->normal, &scratch->delta.vector);
                if (work->animId == ACTOR_207200_ANIM_CRAWL) {
                    rootCoord->coord.t[0] += (contactMagnitude * scratch->delta.vector.vx) >> ACTOR_207200_CREEPING_STRANGER_BASIS_FRACTION_BITS;
                    contactValue           = contactMagnitude * scratch->delta.vector.vy;
                    if (contactValue < 0) {
                        rootCoord->coord.t[1] += contactValue >> ACTOR_207200_CREEPING_STRANGER_BASIS_FRACTION_BITS;
                    }
                    rootCoord->coord.t[2] += (contactMagnitude * scratch->delta.vector.vz) >> ACTOR_207200_CREEPING_STRANGER_BASIS_FRACTION_BITS;
                }
                break;
        }
    }

    // Head hits can remove the head or shatter the whole body.
    if (work->headLost == 0) {
        for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->headContacts); contactIndex++) {
            if ((work->headContacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_ATTACK) {
                continue;
            }
            if (work->hitCooldown != 0) {
                break;
            }
            scratch->delta.vector.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            scratch->delta.vector.vy = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
            scratch->delta.vector.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            contactMagnitude         = SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy +
                                                   scratch->delta.vector.vz * scratch->delta.vector.vz);
            hitReaction              = damageGetPlayerAttackReaction(work->headContacts[contactIndex].key.value);
            contactMagnitude         = damageComputePlayerAttack(work->headContacts[contactIndex].key.value, contactMagnitude, 0, 0);
            switch ((u16)hitReaction) {
                case DAMAGE_PLAYER_REACTION_STAGGER:
                case ACTOR_207200_CREEPING_STRANGER_SHATTER_ATTRIBUTE_4:
                case ACTOR_207200_CREEPING_STRANGER_SHATTER_ATTRIBUTE_5:
                case DAMAGE_PLAYER_REACTION_EXPLOSION:
                    effectSpawn(EFFECT_CRITICAL_HIT, task->extra.tmd->coords, ACTOR_207200_CREEPING_STRANGER_SHATTER_BURST_STYLE, NULL);
                    RANDOM_LCG_ADVANCE();
                    worldTargetAddReadoutAmount(&enemy->node, _gActor207200CreepingStrangerParams.hpMax * 2 + (u16)(RANDOM_LCG_DRAW() % 100), 0);
                    _actor207200CreepingStrangerBurstRandomPart(task);
                    work->hasBurst = 1;
                    task->state++;
                    // Retained reservation ends at the next main-loop scratch reset.
                    return;
                case ACTOR_207200_CREEPING_STRANGER_BUILDUP_ATTRIBUTE_8:
                case ACTOR_207200_CREEPING_STRANGER_BUILDUP_ATTRIBUTE_9:
                    damageStartEnemyBuildup(enemy, work->headContacts[contactIndex].key.value, 0);
                default:
                    if ((damageRollCriticalHit(task->spawnArg2.pointer, work->headContacts[contactIndex].key.value, 0) != 0 ||
                         work->state == ACTOR_207200_STATE_STATUS_HOLD) &&
                        contactMagnitude != 0) {
                        damageAccumulateLifeDrainHp(enemy, work->headContacts[contactIndex].key.value, contactMagnitude, 0);
                        _actor207200CreepingStrangerBurstHead(task);
                        // This head-burst path also retains the scratch reservation.
                        return;
                    }
                    damageAccumulateLifeDrainHp(enemy, work->headContacts[contactIndex].key.value, contactMagnitude, 0);
                    _actor207200CreepingStrangerApplyHeadDamage(task, contactMagnitude);
                    effectSpawnHit(damageGetPlayerAttackEffectId(work->headContacts[contactIndex].key.value),
                                   task->extra.tmd->coords + ACTOR_207200_CREEPING_STRANGER_HEAD_HIT_PART, &_gActor207200HeadHitOffset, &work->headHitEffectArg);
                    contactValue = damageGetPlayerAttackHitCooldown(work->headContacts[contactIndex].key.value);
                    if ((s16)contactValue > 0) {
                        work->hitCooldown = contactValue;
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

#undef ACTOR_207200_CREEPING_STRANGER_RESOLVE_GRID_CONTACTS

/// Applies a head hit, recoiling from idle or removing the head at zero HP.
///
/// Requires the live enemy in spawnArg2 and initialized Creeping Stranger work.
/// Damage narrows through the HP halfword and is reported to the target readout.
/// First lethal damage leaves one HP, disables both attacks, unlinks the head,
/// switches hit records to the body and starts a twenty-tick hit delay. A
/// survived idle hit requests recoil and stops forward motion; other survived
/// hits only voice the hit. Calls can change task payloads, so sound instance
/// tags are read again after the HP/readout operations.
static void _actor207200CreepingStrangerApplyHeadDamage(Task* task, s32 damage)
{
    enum {
        ACTOR_207200_CREEPING_STRANGER_SOUND_HEAD_LOSS   = 0x40480003,
        ACTOR_207200_CREEPING_STRANGER_SOUND_IDLE_RECOIL = 0x40480006,
        ACTOR_207200_CREEPING_STRANGER_SOUND_HIT         = 0x40480001,
    };

    _Actor207200CreepingStrangerWork* work;
    Enemy*                            enemy;
    GfxCoord*                         rootCoord;
    EffectSpawnArg*                   headLossArg;
    Enemy*                            soundEnemy;
    s32                               soundId;

    work      = task->work;
    enemy     = task->spawnArg2.pointer;
    rootCoord = task->extra.tmd->coords;

    enemy->hp = (s16)((u16)enemy->hp - damage);
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    // A lethal head hit leaves one HP for a later hit on the headless body.
    if ((s16)enemy->hp <= 0) {
        if (work->headLost == 0) {
            enemy->hp = 1;
            ACTOR_207200_CREEPING_STRANGER_PLAY_PLACEMENT_SOUND(task, rootCoord, ACTOR_207200_CREEPING_STRANGER_SOUND_HEAD_LOSS, soundEnemy, soundId);
            headLossArg = &work->headLossEffectArg;
            effectSpawnHit(EFFECT_HIT_KIND_SPLATTER, task->extra.tmd->coords + ACTOR_207200_CREEPING_STRANGER_HEAD_PART, &_gActor207200HeadLossSplatterOffset, headLossArg);
            effectSpawnHit(EFFECT_HIT_KIND_SPLATTER, task->extra.tmd->coords + ACTOR_207200_CREEPING_STRANGER_HEAD_PART, &_gActor207200HeadLossSplatterOffset, headLossArg);
            work->frontAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->sideAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            worldCollisionUnlinkBody(&work->headBody);
            work->headLost      = 1;
            enemy->recs         = work->bodyContacts;
            work->deathPhase    = ACTOR_207200_DEATH_PHASE_BEGIN;
            task->killCountdown = ACTOR_207200_CREEPING_STRANGER_HEAD_LOSS_HIT_DELAY;
        }
    } else {
        if (work->animId == ACTOR_207200_ANIM_IDLE) {
            ACTOR_207200_CREEPING_STRANGER_PLAY_PLACEMENT_SOUND(task, rootCoord, ACTOR_207200_CREEPING_STRANGER_SOUND_IDLE_RECOIL, soundEnemy, soundId);
            work->animId        = ACTOR_207200_ANIM_RECOIL;
            work->animFrames    = 0;
            work->state         = ACTOR_207200_STATE_RECOIL;
            work->forwardSpeed  = 0;
            work->wakeRequested = 0;
            return;
        }
        ACTOR_207200_CREEPING_STRANGER_PLAY_PLACEMENT_SOUND(task, rootCoord, ACTOR_207200_CREEPING_STRANGER_SOUND_HIT, soundEnemy, soundId);
    }

#undef ACTOR_207200_CREEPING_STRANGER_PLAY_PLACEMENT_SOUND
}

/// Applies a changed animation request or advances the six animated parts.
///
/// Requires the initialized seven-part rig: slot 0 is retained, slots 1..6
/// blend from their current poses over eight frames. A changed request resets
/// `animFrames`; an unchanged request increments the signed halfword counter.
static inline void _actor207200CreepingStrangerTickAnimation(Task* task)
{
    enum { ACTOR_207200_CREEPING_STRANGER_ANIMATION_BLEND_FRAMES = 8 };
    _Actor207200CreepingStrangerWork* work;
    s32                               slotIndex;

    work = task->work;
    if (work->animId != work->appliedAnim) {
        work->appliedAnim = work->animId;
        work->animFrames  = 0;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0, ACTOR_207200_CREEPING_STRANGER_ANIMATION_BLEND_FRAMES);
        }
    } else {
        work->animFrames++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

/// Updates the Creeping Stranger's actor colour from its composed body position.
///
/// Requires the live enemy and model part 1's current world matrix. Borrows a
/// full VECTOR-sized scratch block, writes XYZ in world units and leaves its
/// unused pad word untouched. The colour call consumes the position before
/// the saved cursor slot releases the block.
static inline void _actor207200CreepingStrangerUpdateColor(Enemy* enemy, Task* task)
{
    GfxCoord* bodyCoord;
    void**    cursorSlot;
    u8*       cursorBytes;
    VECTOR*   worldPosition;

    bodyCoord                                 = &task->extra.tmd->coords[1];
    cursorSlot                                = SCRATCH_STACK_CURSOR_SLOT;
    cursorBytes                               = SCRATCH_STACK_CURSOR_AT(cursorSlot, void);
    worldPosition                             = (VECTOR*)(cursorBytes - sizeof(*worldPosition));
    worldPosition->vx                         = bodyCoord->workm.t[0];
    worldPosition->vy                         = bodyCoord->workm.t[1];
    worldPosition->vz                         = bodyCoord->workm.t[2];
    SCRATCH_STACK_CURSOR_AT(cursorSlot, void) = worldPosition;
    worldCoordUpdateActorColor(enemy, worldPosition, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES_AT(cursorSlot, sizeof(*worldPosition));
}

/// Detaches the dying enemy from targeting and its five collision bodies.
///
/// Requires the live enemy and its initialized work. Ends the borrowed hit-table
/// access before unlinking. Sense and attack bodies leave the enemy-attack list;
/// body and head leave the enemy-body list. Retains task, work and model for death.
static inline void _actor207200CreepingStrangerUnlinkDeathBodies(Enemy* enemy, _Actor207200CreepingStrangerWork* work)
{
    enemy->recs = NULL;
    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->senseBody);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->headBody);
    worldCollisionUnlinkBody(&work->frontAttackBody);
    worldCollisionUnlinkBody(&work->sideAttackBody);
}

/// Applies head-loss part transforms and refreshes the body's composed coordinate.
///
/// Requires initialized work and the live seven-part model. Part transforms are
/// updated before invalidating the root and body caches; lighting and shadows
/// consume the refreshed body cache. Borrows all storage without retaining it.
static inline void _actor207200CreepingStrangerComposeBody(Task* task)
{
    enum { ACTOR_207200_CREEPING_STRANGER_BODY_PART = 1 };

    _actor207200CreepingStrangerCollapseHeadPart(task, &task->extra.tmd->coords[ACTOR_207200_CREEPING_STRANGER_LEG_PART]);
    _actor207200CreepingStrangerCollapseHeadPart(task, &task->extra.tmd->coords[ACTOR_207200_CREEPING_STRANGER_HEAD_PART]);
    task->extra.tmd->coords[0].composeStamp                                        = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[ACTOR_207200_CREEPING_STRANGER_BODY_PART].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[ACTOR_207200_CREEPING_STRANGER_BODY_PART]);
}

/// Advances the Creeping Stranger's recoil settle, corpse burn and destruction.
///
/// State 2 requires a live enemy, initialized work and seven-part TMD body.
/// Paused actors do nothing; hidden actors become untargetable without aging.
/// Death begins by releasing rewards, snapshotting the root and unlinking all
/// collision. Recoil waits for frame 100; other poses and burst bodies advance
/// immediately. Flattening lasts 61 running ticks, becomes translucent on tick
/// 10 and starts two burn bursts on tick 15. The following destroy phase frees
/// enemy and task. Earlier phases still animate, compose and relight the body.
static void _actor207200CreepingStrangerDeathState(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_207200_CREEPING_STRANGER_DEATH_ACTOR_ID      = 0x2B,
        ACTOR_207200_CREEPING_STRANGER_DEATH_RECOIL_FRAMES = 100,
        ACTOR_207200_CREEPING_STRANGER_FLATTEN_TICKS       = 61,
        ACTOR_207200_CREEPING_STRANGER_TRANSLUCENT_TICK    = 10,
        ACTOR_207200_CREEPING_STRANGER_BURN_TICK           = 15,
        ACTOR_207200_CREEPING_STRANGER_BURN_BURSTS         = 2,
    };
    _Actor207200CreepingStrangerWork* work;
    TmdObject*                        model;
    GfxCoord*                         rootCoord;
    s16                               deathPhase;

    model     = task->extra.tmd;
    work      = task->work;
    rootCoord = model->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            deathPhase = work->deathPhase;
            switch (deathPhase) {
                case ACTOR_207200_DEATH_PHASE_BEGIN:
                    // Retire combat participation while keeping the death model alive.
                    sceneReleaseBattleRefWithRewards(task, ACTOR_207200_CREEPING_STRANGER_DEATH_ACTOR_ID);
                    work->deathPhase    = ACTOR_207200_DEATH_PHASE_SETTLE;
                    work->phaseFrames   = 0;
                    work->flattenScaleY = ONE;
                    work->savedRootMtx  = rootCoord->coord;
                    _actor207200CreepingStrangerUnlinkDeathBodies(enemy, work);
                    break;
                case ACTOR_207200_DEATH_PHASE_SETTLE:
                    if (work->hasBurst == 0) {
                        if (work->animId == ACTOR_207200_ANIM_RECOIL) {
                            if (work->animFrames >= ACTOR_207200_CREEPING_STRANGER_DEATH_RECOIL_FRAMES) {
                                work->deathPhase = ACTOR_207200_DEATH_PHASE_FLATTEN;
                            }
                        } else {
                            work->deathPhase = ACTOR_207200_DEATH_PHASE_FLATTEN;
                        }
                    } else {
                        model->flags     = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                        work->deathPhase = ACTOR_207200_DEATH_PHASE_FLATTEN;
                    }
                    break;
                case ACTOR_207200_DEATH_PHASE_FLATTEN:
                    work->phaseFrames++;
                    if (work->phaseFrames >= ACTOR_207200_CREEPING_STRANGER_FLATTEN_TICKS) {
                        work->deathPhase = ACTOR_207200_DEATH_PHASE_DESTROY;
                    }
                    if (work->hasBurst == 0) {
                        _actor207200CreepingStrangerFlatten(task);
                        if (work->phaseFrames == ACTOR_207200_CREEPING_STRANGER_TRANSLUCENT_TICK) {
                            model->flags = TMD_OBJECT_SEMI_TRANS;
                        }
                        if (work->phaseFrames == ACTOR_207200_CREEPING_STRANGER_BURN_TICK) {
                            effectSpawn(EFFECT_CORPSE_BURN, rootCoord, ACTOR_207200_CREEPING_STRANGER_BURN_BURSTS, NULL);
                        }
                    }
                    break;
                case ACTOR_207200_DEATH_PHASE_DESTROY:
                    enemyDestroy(enemy, task);
                    return;
            }
            _actor207200CreepingStrangerTickAnimation(task);
            _actor207200CreepingStrangerComposeBody(task);
            _actor207200CreepingStrangerUpdateColor(enemy, task);
            break;
    }
}

/// Returns the player's bearing and writes its planar range from `reference`.
///
/// Bearing uses composed caches in the same frame, in 4096ths of a turn,
/// wrapped to -2048..2048. Range uses local X/Z translations in a common parent
/// frame, narrowing each difference to signed 16 bits before squaring; the
/// squared sum must fit a nonnegative signed word. `rangeOut` receives game
/// units. Requires the live player, both coordinates and a writable output;
/// borrows and releases one `ActorBearingScratch` and changes GTE state.
static s32 _actor207200CreepingStrangerMeasurePlayer(GfxCoord* reference, u32* rangeOut)
{
    GfxCoord*            playerCoord;
    ActorBearingScratch* scratch;
    s32                  playerBearing;

    playerCoord       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    scratch           = SCRATCH_STACK_RESERVE_BLOCK(ActorBearingScratch);
    playerBearing     = _actorAngleBearingInFrame(scratch, reference, playerCoord);
    scratch->delta.vx = playerCoord->coord.t[0] - reference->coord.t[0];
    scratch->delta.vz = playerCoord->coord.t[2] - reference->coord.t[2];
    *rangeOut         = SquareRoot0(scratch->delta.vx * scratch->delta.vx + scratch->delta.vz * scratch->delta.vz);
    SCRATCH_STACK_RELEASE_BLOCK(ActorBearingScratch);
    return playerBearing;
}

/// Bursts the Creeping Stranger's head on a critical head hit, leaving one HP.
///
/// Requires live enemy/work/model and loaded burst-head resources. Reports HP
/// minus one, spawns the head model with copied textures and two splatter effects,
/// then requests recoil, redirects hit records to the body and unlinks the head.
/// A twenty-tick delay protects the body; behavior state and forwardSpeed are
/// retained. The bank-8 burst descriptor is selected before the synchronous spawn.
static void _actor207200CreepingStrangerBurstHead(Task* task)
{
    EffectSpawnArg*                   headLossArg;
    EffectWork*                       burstEffect;
    _Actor207200CreepingStrangerWork* work;
    Enemy*                            enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;

    effectSpawn(EFFECT_CRITICAL_HIT, task->extra.tmd->coords, 0, NULL);
    worldTargetAddReadoutAmount(&enemy->node, enemy->hp - 1, 0);
    gTaskBank8Descs[TASK_BANK8_BURST_MODEL_PART_SLOT].data.model = &_gSkullStalkerAndCreepingStrangerCreepingStrangerBurstHead;
    burstEffect                                                  = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, task->extra.tmd->coords + ACTOR_207200_CREEPING_STRANGER_HEAD_PART, 0, NULL);
    if (burstEffect != NULL) {
        _actor207200CreepingStrangerCopyBurstTextures(burstEffect->task, task);
    }
    headLossArg = &work->headLossEffectArg;
    effectSpawnHit(EFFECT_HIT_KIND_SPLATTER, task->extra.tmd->coords + ACTOR_207200_CREEPING_STRANGER_HEAD_PART, &_gActor207200HeadLossSplatterOffset, headLossArg);
    effectSpawnHit(EFFECT_HIT_KIND_SPLATTER, task->extra.tmd->coords + ACTOR_207200_CREEPING_STRANGER_HEAD_PART, &_gActor207200HeadLossSplatterOffset, headLossArg);
    // Critical head loss redirects later hits without selecting a new behavior state.
    work->headBurst       = 1;
    work->animId          = ACTOR_207200_ANIM_RECOIL;
    work->headLost        = 1;
    enemy->recs           = work->bodyContacts;
    enemy->hp             = 1;
    work->headBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionUnlinkBody(&work->headBody);
    task->killCountdown = ACTOR_207200_CREEPING_STRANGER_HEAD_LOSS_HIT_DELAY;
}

/// Spawns a random Creeping Stranger burst part and head/leg gravity particles.
///
/// Requires the live seven-part model and loaded burst resources. One LCG step
/// selects head with probability one half, arm or leg with one quarter each.
/// The bank-8 descriptor is set before synchronous spawn, and a successful model
/// effect receives the actor's texture offsets. Two gravity particles use speed
/// 768 in their normalized-direction step. The caller selects death and hasBurst.
static void _actor207200CreepingStrangerBurstRandomPart(Task* task)
{
/// Spawns a Creeping Stranger part model and copies the actor's texture placement.
///
/// Requires loaded modelSource and a live sourceTask with seven model coordinates;
/// current calls select parts 1, 5 or 4. Publishes the model before synchronous
/// bank-8 spawn. Stable modelSource/partIndex occur once; sourceTask once, or twice
/// on success, and writable EffectWork* effectLocal twice, or three times on success.
/// Arguments must have no side effects. Captures only the package texture copier,
/// forms one compound statement, and skips texture copying on allocation failure.
#define ACTOR_207200_CREEPING_STRANGER_SPAWN_BURST_PART(modelSource, partIndex, sourceTask, effectLocal)                                                                  \
    {                                                                                                                                                                     \
        gTaskBank8Descs[TASK_BANK8_BURST_MODEL_PART_SLOT].data.model = (modelSource);                                                                                     \
        (effectLocal)                                                = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, (sourceTask)->extra.tmd->coords + (partIndex), 0, NULL); \
        if ((effectLocal) != NULL) {                                                                                                                                      \
            _actor207200CreepingStrangerCopyBurstTextures((effectLocal)->task, (sourceTask));                                                                             \
        }                                                                                                                                                                 \
    }

    enum {
        ACTOR_207200_CREEPING_STRANGER_BURST_PARTICLE_SPEED = 768,
    };

    EffectWork* burstEffect;
    s32         partRoll;

    RANDOM_LCG_ADVANCE();
    partRoll = RANDOM_LCG_DRAW() & 3;
    switch (partRoll) {
        case 0:
        case 1:
            ACTOR_207200_CREEPING_STRANGER_SPAWN_BURST_PART(&_gSkullStalkerAndCreepingStrangerCreepingStrangerBurstHead, ACTOR_207200_CREEPING_STRANGER_HEAD_PART, task, burstEffect);
            break;
        case 2:
            ACTOR_207200_CREEPING_STRANGER_SPAWN_BURST_PART(&_gSkullStalkerAndCreepingStrangerCreepingStrangerBurstArm, ACTOR_207200_CREEPING_STRANGER_ARM_PART, task, burstEffect);
            break;
        case 3:
            ACTOR_207200_CREEPING_STRANGER_SPAWN_BURST_PART(&_gSkullStalkerAndCreepingStrangerCreepingStrangerBurstLeg, ACTOR_207200_CREEPING_STRANGER_LEG_PART, task, burstEffect);
            break;
    }
    effectSpawn(EFFECT_030, task->extra.tmd->coords + ACTOR_207200_CREEPING_STRANGER_HEAD_PART, ACTOR_207200_CREEPING_STRANGER_BURST_PARTICLE_SPEED, NULL);
    effectSpawn(EFFECT_030, task->extra.tmd->coords + ACTOR_207200_CREEPING_STRANGER_LEG_PART, ACTOR_207200_CREEPING_STRANGER_BURST_PARTICLE_SPEED, NULL);

#undef ACTOR_207200_CREEPING_STRANGER_SPAWN_BURST_PART
}

/// Dispatches task states 0 spawn, 1 live and 2 death for the Creeping Stranger.
///
/// Requires a live Enemy in `spawnArg2.pointer` and a state in 0..2. The
/// selected handler may destroy the enemy and task; neither is used afterward.
static void _actor207200CreepingStrangerTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = _gActor207200CreepingStrangerTaskStates;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Updates the Creeping Stranger's live combat behavior and body presentation.
///
/// State 1 requires a live enemy, initialized work and seven-part TMD body.
/// Paused actors only relight and draw the shadow; hidden actors become
/// untargetable and return. Running control restores model and targeting flags;
/// other control values also run the update without restoring those flags.
/// Reactions and target/contact scans precede behavior, forward motion and
/// animation. Head-loss transforms and body composition precede lighting and
/// the shadow. A contact can request death for the next task dispatch.
static void _actor207200CreepingStrangerLiveState(Enemy* enemy, Task* task)
{
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor207200CreepingStrangerUpdateLiveColor(enemy, task);
            _actor207200CreepingStrangerDrawGroundShadow(task);
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            task->extra.tmd->flags        = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    // Resolve requests and contacts before advancing movement and the pose.
    _actor207200CreepingStrangerConsumeReactions(task);
    _actor207200CreepingStrangerUpdateTarget(task);
    _actor207200CreepingStrangerScanContacts(task);
    _actor207200CreepingStrangerUpdateBehavior(task);
    _actor207200CreepingStrangerStepForward(task);
    _actor207200CreepingStrangerAnimate(task);
    _actor207200CreepingStrangerComposeBody(task);
    _actor207200CreepingStrangerUpdateLiveColor(enemy, task);
    _actor207200CreepingStrangerDrawGroundShadow(task);
}

/// Consumes hit-reaction requests and starts the buildup hold when requested.
///
/// Requires initialized task work and a live Enemy in `spawnArg2.pointer`.
/// Stagger and damage-over-time requests are acknowledged without a new pose;
/// buildup stops forward motion and requests the fidget afresh.
static void _actor207200CreepingStrangerConsumeReactions(Task* task)
{
    Enemy*                            enemy;
    _Actor207200CreepingStrangerWork* work;
    u8                                pendingReactions;

    enemy            = task->spawnArg2.pointer;
    pendingReactions = enemy->reactionFlags;
    work             = task->work;
    if (pendingReactions != 0) {
        if (pendingReactions & ENEMY_REACTION_STAGGER) {
            enemy->reactionFlags = pendingReactions & ENEMY_REACTION_STAGGER_CLEAR;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags = enemy->reactionFlags & ENEMY_REACTION_BUILDUP_CLEAR;
            work->state          = ACTOR_207200_STATE_STATUS_HOLD;
            work->appliedAnim    = ACTOR_207200_ANIM_IDLE;
            work->phaseFrames    = 0;
            work->forwardSpeed   = 0;
            work->animId         = ACTOR_207200_ANIM_FIDGET;
            work->animFrames     = 0;
        }
        pendingReactions = enemy->reactionFlags;
        if (pendingReactions & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            enemy->reactionFlags = pendingReactions & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

/// Ticks the kill countdown and dispatches the current behavior.
///
/// The countdown narrows back to signed 16 bits before clamping at zero.
/// Buildup holds replay the fidget every 61 ticks until the damage subsystem
/// ends the hold. Recoil ends at frame 105; a headed enemy then requests wake.
static void _actor207200CreepingStrangerUpdateBehavior(Task* task)
{
    enum { ACTOR_207200_CREEPING_STRANGER_STATUS_HOLD_LOOP_FRAMES = 61,
           ACTOR_207200_CREEPING_STRANGER_RECOIL_FRAMES           = 105 };
    _Actor207200CreepingStrangerWork* work;
    s16                               countdown;

    // Decrement in the retained 16-bit domain, then clamp a negative result.
    work                = task->work;
    countdown           = (u16)task->killCountdown - 1;
    task->killCountdown = countdown;
    if (countdown < 0) {
        task->killCountdown = 0;
    }
    switch (work->state) {
        case ACTOR_207200_STATE_DORMANT:
            _actor207200CreepingStrangerDormantTick(task);
            break;
        case ACTOR_207200_STATE_ACTIVE:
            _actor207200CreepingStrangerActiveTick(task);
            break;
        case ACTOR_207200_STATE_STATUS_HOLD:
            work->phaseFrames = work->phaseFrames + 1;
            if (work->phaseFrames >= ACTOR_207200_CREEPING_STRANGER_STATUS_HOLD_LOOP_FRAMES) {
                work->appliedAnim = ACTOR_207200_ANIM_IDLE;
                work->animId      = ACTOR_207200_ANIM_FIDGET;
                work->animFrames  = 0;
                work->phaseFrames = 0;
            }
            if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
                work->state = ACTOR_207200_STATE_DORMANT;
            }
            break;
        case ACTOR_207200_STATE_RECOIL:
            if (work->animFrames >= ACTOR_207200_CREEPING_STRANGER_RECOIL_FRAMES) {
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

/// Advances the root along its local Z basis and adds the per-tick Y step.
///
/// `forwardSpeed` is in parent-frame game units and the basis is Q12. X/Z
/// products shift arithmetically; Y gains 128 even when forward speed is zero
/// or the head is lost. A headed enemy first saves its position for collision
/// rollback. Composition is invalidated by the caller after the pose update.
static void _actor207200CreepingStrangerStepForward(Task* task)
{
    enum { ACTOR_207200_CREEPING_STRANGER_BASIS_FRACTION_BITS = 12,
           ACTOR_207200_CREEPING_STRANGER_ROOT_Y_STEP         = 128 };
    _Actor207200CreepingStrangerWork* work;
    GfxCoord*                         rootCoord;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    if (work->headLost == 0) {
        work->prevRootPos.vx = rootCoord->coord.t[0];
        work->prevRootPos.vy = rootCoord->coord.t[1];
        work->prevRootPos.vz = rootCoord->coord.t[2];
    }
    // Keep the X, Y, Z update order and signed Q12 products.
    rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * work->forwardSpeed) >> ACTOR_207200_CREEPING_STRANGER_BASIS_FRACTION_BITS;
    rootCoord->coord.t[1] += ACTOR_207200_CREEPING_STRANGER_ROOT_Y_STEP;
    rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * work->forwardSpeed) >> ACTOR_207200_CREEPING_STRANGER_BASIS_FRACTION_BITS;
}

/// Updates the live Creeping Stranger's requested animation and frame counter.
///
/// Uses the same six-slot playback operation as the death tick.
static void _actor207200CreepingStrangerAnimate(Task* task)
{
    _actor207200CreepingStrangerTickAnimation(task);
}

/// Refreshes the live Creeping Stranger's colour from composed model part 1.
///
/// Requires matching live Enemy/model and an up-to-date part-1 matrix.
/// The out-of-line live-state entry shares the death tick's colour operation;
/// its temporary VECTOR is consumed synchronously and released before return.
static void _actor207200CreepingStrangerUpdateLiveColor(Enemy* enemy, Task* task)
{
    GfxCoord* bodyCoord;
    void**    cursorSlot;
    VECTOR*   savedCursor;
    VECTOR*   worldPosition;

    bodyCoord                                 = &task->extra.tmd->coords[1];
    cursorSlot                                = SCRATCH_STACK_CURSOR_SLOT;
    savedCursor                               = SCRATCH_STACK_CURSOR_AT(cursorSlot, VECTOR);
    worldPosition                             = savedCursor - 1;
    worldPosition->vx                         = bodyCoord->workm.t[0];
    worldPosition->vy                         = bodyCoord->workm.t[1];
    worldPosition->vz                         = bodyCoord->workm.t[2];
    SCRATCH_STACK_CURSOR_AT(cursorSlot, void) = worldPosition;
    worldCoordUpdateActorColor(enemy, worldPosition, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES_AT(cursorSlot, sizeof(*worldPosition));
}

/// Draws the raw-texture ground shadow at the composed model root.
///
/// The quad half-side is 448 game units. Borrows all 24 bytes of an
/// `ActorRenderGroundShadowCentreScratch` for the call; only its leading
/// centre vector is accessed. Requires an up-to-date root composition cache.
static void _actor207200CreepingStrangerDrawGroundShadow(Task* task)
{
    enum { ACTOR_207200_CREEPING_STRANGER_SHADOW_HALF_SIDE   = 448,
           ACTOR_207200_CREEPING_STRANGER_SHADOW_RAW_TEXTURE = 0 };
    GfxCoord*                             rootCoord;
    ActorRenderGroundShadowCentreScratch* shadowScratch;

    rootCoord                = task->extra.tmd->coords;
    shadowScratch            = SCRATCH_STACK_RESERVE_BLOCK(ActorRenderGroundShadowCentreScratch);
    shadowScratch->centre.vx = rootCoord->workm.t[0];
    shadowScratch->centre.vy = rootCoord->workm.t[1];
    shadowScratch->centre.vz = rootCoord->workm.t[2];
    effectDrawGroundShadow(&shadowScratch->centre, ACTOR_207200_CREEPING_STRANGER_SHADOW_HALF_SIDE, ACTOR_207200_CREEPING_STRANGER_SHADOW_RAW_TEXTURE);
    SCRATCH_STACK_RELEASE_BLOCK(ActorRenderGroundShadowCentreScratch);
}

/// Flattens the corpse from the saved death transform without compounding scale.
///
/// Decreases the signed Q12 Y factor by 80 while it exceeds 512; the final step
/// may undershoot. X/Z stay at unity. Requires the saved root matrix and one
/// free `ActorScaleScratch`; restores the root before scaling and marks it dirty.
static void _actor207200CreepingStrangerFlatten(Task* task)
{
    enum { ACTOR_207200_CREEPING_STRANGER_FLATTEN_CUTOFF_Q12 = 0x200,
           ACTOR_207200_CREEPING_STRANGER_FLATTEN_STEP_Q12   = 0x50 };
    GfxCoord*                         rootCoord;
    ActorScaleScratch*                scratchHead;
    ActorScaleScratch*                scratch;
    _Actor207200CreepingStrangerWork* work;

    scratchHead                             = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = task->work;
    scratch                                 = scratchHead - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    rootCoord                               = task->extra.tmd->coords;
    if (work->flattenScaleY > ACTOR_207200_CREEPING_STRANGER_FLATTEN_CUTOFF_Q12) {
        work->flattenScaleY -= ACTOR_207200_CREEPING_STRANGER_FLATTEN_STEP_Q12;
    }
    _actor207200CreepingStrangerApplyRootScale(rootCoord, &work->savedRootMtx, &work->flattenScaleY, scratch);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}

/// Publishes and relinks the model part used for lock-on.
///
/// A headless enemy targets part 1. With a head, a player bearing strictly
/// inside a quarter turn targets part 3, otherwise part 1. Bearing uses the
/// composed root/player caches; model parts and the Enemy must stay live.
static void _actor207200CreepingStrangerUpdateTarget(Task* task)
{
    _Actor207200CreepingStrangerWork* work;
    Enemy*                            enemy;
    GfxCoord*                         targetCoord;
    u32                               playerRange;
    s32                               playerBearing;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->headLost != 0) {
        targetCoord = task->extra.tmd->coords + 1;
    } else {
        playerBearing = _actor207200CreepingStrangerMeasurePlayer(task->extra.tmd->coords, &playerRange);
        if (playerBearing < 0) {
            playerBearing = -playerBearing;
        }
        if (playerBearing < (ACTOR_TRANSFORM_ANGLE_TURN / 4)) {
            targetCoord = task->extra.tmd->coords + 3;
        } else {
            targetCoord = task->extra.tmd->coords + 1;
        }
    }
    enemy->coord = targetCoord;
    worldTargetLinkNode(&enemy->node);
}

/// Collapses one head-associated part after head loss, retaining its translation.
///
/// Callers pass model parts 2 and 3. Each Q12 basis column is multiplied by
/// zero through the GTE, then composition is marked dirty. Requires initialized
/// work and a live part coordinate; changes GTE interpolation registers.
static void _actor207200CreepingStrangerCollapseHeadPart(Task* task, GfxCoord* partCoord)
{
    SVECTOR                           columnValue;
    MATRIX*                           rotation;
    _Actor207200CreepingStrangerWork* work;

    /// Collapses one Q12 basis column through the GTE, retaining translation.
    ///
    /// columnIndex must be a compile-time constant in 0..2. Pointer arguments
    /// must be stable, side-effect-free expressions and are evaluated repeatedly.
    /// Borrows columnValue; changes GTE state. Use within a compound statement.
#define ACTOR_207200_CREEPING_STRANGER_COLLAPSE_COLUMN(rotation, columnIndex, columnValue) \
    gte_ReadMatrixColumn((rotation), (columnIndex), (columnValue));                        \
    gte_lddp(0);                                                                           \
    gte_ldsv((columnValue));                                                               \
    gte_gpf12();                                                                           \
    gte_stsv((columnValue));                                                               \
    gte_WriteMatrixColumn((columnValue), (rotation), (columnIndex))

    work = task->work;
    if (work->headLost != 0) {
        rotation = &partCoord->coord;
        ACTOR_207200_CREEPING_STRANGER_COLLAPSE_COLUMN(rotation, 0, &columnValue);

        ACTOR_207200_CREEPING_STRANGER_COLLAPSE_COLUMN(rotation, 1, &columnValue);

        ACTOR_207200_CREEPING_STRANGER_COLLAPSE_COLUMN(rotation, 2, &columnValue);

        partCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
#undef ACTOR_207200_CREEPING_STRANGER_COLLAPSE_COLUMN
}

/// Gives a newly spawned burst model the enemy model's texture placement.
///
/// Requires two live TMD tasks. Copies the texture-page and CLUT-row offsets;
/// if the burst model owns a primitive buffer, rebuilds both alternating halves
/// so they reflect the new placement. Neither task or model is retained.
static void _actor207200CreepingStrangerCopyBurstTextures(Task* burstTask, Task* sourceTask)
{
    TmdObject* burstModel;
    TmdObject* sourceModel;

    sourceModel                   = sourceTask->extra.tmd;
    burstModel                    = burstTask->extra.tmd;
    burstModel->texturePageOffset = sourceModel->texturePageOffset;
    burstModel->clutRowOffset     = sourceModel->clutRowOffset;
    if (burstModel->buffer != NULL) {
        tmdBuildBufferHalf(burstModel);
        tmdBuildBufferHalf(burstModel);
    }
}

/// Detaches the Creeping Stranger's targets and collision bodies before release.
///
/// Exit callback installed only after successful setup. Clears the Enemy's
/// borrowed contact table, unlinks its target and all five bodies, then releases
/// the Enemy and task through `enemyTaskExit`; both are invalid afterward.
static void _actor207200CreepingStrangerExit(Task* task)
{
    Enemy*                            enemy;
    _Actor207200CreepingStrangerWork* work;

    enemy       = task->spawnArg2.pointer;
    work        = task->work;
    enemy->recs = NULL;
    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->senseBody);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->headBody);
    worldCollisionUnlinkBody(&work->frontAttackBody);
    worldCollisionUnlinkBody(&work->sideAttackBody);
    enemyTaskExit(task);
}
