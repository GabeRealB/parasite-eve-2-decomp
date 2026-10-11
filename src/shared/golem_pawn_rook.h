/* The behaviour the Rook GOLEM (Beam Sword, actor_102300), the Pawn GOLEM
 * (Grenade Launcher, actor_105600) and the Rook GOLEM (Grenade Launcher,
 * actor_105700) share; the packages differ in sound bank and some attack
 * states. It covers the idle and approach states with their player-proximity
 * check, the knocked-down and recoil states, the dead state that files the
 * enemy's pose, and three per-frame helpers: turning the root toward a target
 * yaw, easing out the hit tilt, and playing an animation's voice cues. Each
 * package defines its own voice-cue table and per-animation blend lengths
 * under the shared names.
 *
 * Each package states its type and weapon before including this header:
 * `GOLEM_PAWN_ROOK_TYPE` must be `GOLEM_PAWN_ROOK_KIND_PAWN` or
 * `GOLEM_PAWN_ROOK_KIND_ROOK`; `GOLEM_PAWN_ROOK_WEAPON` must be
 * `GOLEM_PAWN_ROOK_WEAPON_BEAM_SWORD` (actor_02000, actor_02300) or
 * `GOLEM_PAWN_ROOK_WEAPON_GRENADE_LAUNCHER` (actor_05600, actor_05700).
 * Selector replacement tokens are expanded after their definitions below.
 * All slot variants of a carrier use the same kind/weapon pair.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The package defines its tables at its own positions under these
 * names:
 *
 *   s16 _gGolemPawnRookAnimBlendFrames[32]  blend-in length per animation, in frames
 *   s32 _gGolemPawnRookMotionCues[17]        animation/fall script IDs with a zero instance byte
 *   s32 _gGolemPawnRookSwingCue             sword swing/charge script ID (Beam Sword only)
 *   s32 _gGolemPawnRookGrenadeLaunchCue    grenade launch script ID (Grenade Launcher only)
 *   s32 gGolemPawnRookImpactSound          the grenade's impact cue (Grenade Launcher only)
 *
 * The hit fragment requires each carrier's own signed-halfword shield tables,
 * `_gGolemPawnRookShieldWeaponRows[46]` and `_gGolemPawnRookShieldPeRows[56]`,
 * declared before it is included. A nonzero row absorbs a covered frontal hit.
 * The contact key's low seven bits index the table; bit 15 selects PE rows.
 * Keys reaching shield lookup must select a weapon row in 0..45 or a PE row
 * in 0..54, the latter also required by the damage API. No range checks run.
 */

#ifndef SRC_SHARED_GOLEM_PAWN_ROOK_H
#define SRC_SHARED_GOLEM_PAWN_ROOK_H

/// Kind selector for the sword/launcher Pawns in actor_02000/actor_05600.
#define GOLEM_PAWN_ROOK_KIND_PAWN 1
/// Kind selector for the shielded Rooks in actor_02300/actor_05700.
#define GOLEM_PAWN_ROOK_KIND_ROOK 2
/// Weapon selector for the sword carriers actor_02000/actor_02300.
#define GOLEM_PAWN_ROOK_WEAPON_BEAM_SWORD 1
/// Weapon selector for the launcher carriers actor_05600/actor_05700.
#define GOLEM_PAWN_ROOK_WEAPON_GRENADE_LAUNCHER 2

#if GOLEM_PAWN_ROOK_WEAPON == GOLEM_PAWN_ROOK_WEAPON_BEAM_SWORD
/// Engagement tracking step per call, in angular units (4096 per turn).
#define GOLEM_PAWN_ROOK_ENGAGE_TRACK_TURN_RATE 60
/// Strict horizontal player-distance cutoff for a sword swing, in game units.
#define GOLEM_PAWN_ROOK_CLOSE_ATTACK_RANGE 2250
/// Close-attack index into `GolemPawnRookWork::behavior`'s dispatch table.
#define GOLEM_PAWN_ROOK_CLOSE_ATTACK_BEHAVIOR GOLEM_PAWN_ROOK_BEHAVIOR_SWORD_SWING
/// Animation-table index of the close Beam Sword swing.
#define GOLEM_PAWN_ROOK_CLOSE_ATTACK_ANIM 8
/// Weapon-approach index into `GolemPawnRookWork::behavior`'s dispatch table.
#define GOLEM_PAWN_ROOK_WEAPON_APPROACH_BEHAVIOR GOLEM_PAWN_ROOK_BEHAVIOR_SWORD_CHARGE
/// Animation-table index of the Beam Sword charge wind-up.
#define GOLEM_PAWN_ROOK_WEAPON_APPROACH_ANIM 5
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_PAWN_ROOK_KIND_PAWN
/// Pawn running-charge steering step per call (4096 angular units per turn).
#define GOLEM_PAWN_ROOK_SWORD_CHARGE_TURN_RATE 15
#else
/// Rook running-charge steering step; zero holds its heading.
#define GOLEM_PAWN_ROOK_SWORD_CHARGE_TURN_RATE 0
#endif
#else
/// Engagement tracking step per call, in angular units (4096 per turn).
#define GOLEM_PAWN_ROOK_ENGAGE_TRACK_TURN_RATE   30
/// Strict horizontal player-distance cutoff for a launcher strike, in game units.
#define GOLEM_PAWN_ROOK_CLOSE_ATTACK_RANGE       2000
/// Close-attack index into `GolemPawnRookWork::behavior`'s dispatch table.
#define GOLEM_PAWN_ROOK_CLOSE_ATTACK_BEHAVIOR    GOLEM_PAWN_ROOK_BEHAVIOR_LAUNCHER_STRIKE
/// Animation-table index of the close launcher strike.
#define GOLEM_PAWN_ROOK_CLOSE_ATTACK_ANIM        16
/// Weapon-approach index into `GolemPawnRookWork::behavior`'s dispatch table.
#define GOLEM_PAWN_ROOK_WEAPON_APPROACH_BEHAVIOR GOLEM_PAWN_ROOK_BEHAVIOR_GRENADE_BURST
/// Animation-table index of the grenade burst's opening pose.
#define GOLEM_PAWN_ROOK_WEAPON_APPROACH_ANIM     12
#endif

/// HP boundary below which a surviving hit requests a knockdown or downed flinch.
///
/// hpMax is an s32 maximum-HP count in 0..65535, evaluated once. Integer
/// division truncates: sword Rook uses 25%, all other builds 15%. The caller
/// compares surviving HP strictly below this result; equality does not qualify.
#if GOLEM_PAWN_ROOK_WEAPON == GOLEM_PAWN_ROOK_WEAPON_BEAM_SWORD && GOLEM_PAWN_ROOK_TYPE == GOLEM_PAWN_ROOK_KIND_ROOK
#define GOLEM_PAWN_ROOK_KNOCKDOWN_HP_THRESHOLD(hpMax) ((hpMax) / 4)
#else
#define GOLEM_PAWN_ROOK_KNOCKDOWN_HP_THRESHOLD(hpMax) ((hpMax) * 15 / 100)
#endif

#if GOLEM_PAWN_ROOK_WEAPON == GOLEM_PAWN_ROOK_WEAPON_GRENADE_LAUNCHER
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_PAWN_ROOK_KIND_PAWN
/// Launcher Pawn actor number, also its sound-bank file argument and body-key ID.
#define GOLEM_PAWN_ROOK_ACTOR_ID 0x38
/// Launcher Pawn retreat duration; leaves backoff when its call count reaches 30.
#define GOLEM_PAWN_ROOK_BURST_BACKOFF_FRAMES 30
/// Launcher Pawn grenades requested per burst, independent of reload capacity.
#define GOLEM_PAWN_ROOK_BURST_SHOT_LIMIT 6
#else
/// Launcher Rook actor number, also its sound-bank file argument and body-key ID.
#define GOLEM_PAWN_ROOK_ACTOR_ID             0x39
/// Launcher Rook retreat duration; leaves backoff when its call count reaches 60.
#define GOLEM_PAWN_ROOK_BURST_BACKOFF_FRAMES 60
/// Launcher Rook grenades requested per burst, independent of reload capacity.
#define GOLEM_PAWN_ROOK_BURST_SHOT_LIMIT     3
#endif
#endif

#include "types.h"

#include "actors/actor.h"

#include "gameplay/enemy.h"

#include "main/task_types.h"

/// Shield-table row values; front-facing covered hits redirect HP loss to the shield.
enum {
    GOLEM_PAWN_ROOK_SHIELD_BYPASS = 0,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB = 1,
};

/// Values of `GolemPawnRookWork::behavior`, the entries of each package's
/// `_gGolemPawnRookBehaviorHandlers`.
///
/// Every package uses the same numbering and fills the entries its build has
/// no handler for with `_golemPawnRookNopState`: the two sword entries in a
/// Grenade Launcher build, the two launcher entries in a Beam Sword build and
/// the scream in a Pawn build.
enum {
    GOLEM_PAWN_ROOK_BEHAVIOR_IDLE            = 0,  // stands, listening and looking for the player
    GOLEM_PAWN_ROOK_BEHAVIOR_PATROL          = 1,  // walks its patrol leg and turns about at the end
    GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE          = 2,  // turns to face the player and picks the next attack
    GOLEM_PAWN_ROOK_BEHAVIOR_SWORD_CHARGE    = 3,  // Beam Sword: runs the player down and slashes
    GOLEM_PAWN_ROOK_BEHAVIOR_SWORD_SWING     = 4,  // Beam Sword: close-range swing
    GOLEM_PAWN_ROOK_BEHAVIOR_SCREAM          = 5,  // Rook: the scream that inflicts Silence
    GOLEM_PAWN_ROOK_BEHAVIOR_GRENADE_BURST   = 6,  // Grenade Launcher: backs away, aims and fires bursts
    GOLEM_PAWN_ROOK_BEHAVIOR_LAUNCHER_STRIKE = 7,  // Grenade Launcher: close-range strike
    GOLEM_PAWN_ROOK_BEHAVIOR_STAGGER         = 8,  // reels from a critical hit or from damage that broke its attack
    GOLEM_PAWN_ROOK_BEHAVIOR_RECOIL          = 9,  // heavier reel, animated by the side the hit came from; also taken when the shield or the last scream charge breaks
    GOLEM_PAWN_ROOK_BEHAVIOR_BUILDUP         = 10, // held by the buildup reaction until its timer fires
    GOLEM_PAWN_ROOK_BEHAVIOR_KNOCKDOWN       = 11, // falls at low hit points and lies there
    GOLEM_PAWN_ROOK_BEHAVIOR_DOWNED_HIT      = 12, // flinches from a hit taken lying down
    GOLEM_PAWN_ROOK_BEHAVIOR_COLLAPSE        = 13, // falls dead from standing
    GOLEM_PAWN_ROOK_BEHAVIOR_DOWNED_DEATH    = 14, // dies lying down
};

/// Animation requests used by the shared GOLEM behaviour handlers.
enum {
    GOLEM_PAWN_ROOK_ANIM_WALK              = 2,
    GOLEM_PAWN_ROOK_ANIM_LISTEN            = 4,
    GOLEM_PAWN_ROOK_ANIM_DOWNED_HIT_BEHIND = 0x17,
    GOLEM_PAWN_ROOK_ANIM_DOWNED_HIT_FRONT  = 0x1B,
};

/// Saved downed poses, shared task lifecycle, and destinations in other handlers.
///
/// The placement index supplies the sound request's 8-bit instance tag.
enum {
    GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT     = 8,
    GOLEM_PAWN_ROOK_DOWNED_BEHIND            = 1,
    GOLEM_PAWN_ROOK_DOWNED_FRONT             = 2,
    GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP      = 0,
    GOLEM_PAWN_ROOK_TASK_RUNNING             = 1,
    GOLEM_PAWN_ROOK_TASK_TEARDOWN            = 2,
    GOLEM_PAWN_ROOK_ENGAGE_LISTEN_STEP       = 2,
    GOLEM_PAWN_ROOK_DOWNED_HIT_BEHIND_FRAMES = 0x10,
    GOLEM_PAWN_ROOK_DOWNED_HIT_FRONT_FRAMES  = 0x16,
};

/// Work block of a Pawn or Rook GOLEM, allocated at this size by the body
/// task's spawn state and kept at `Task::work`.
///
/// It holds the animation rig, the matrices the body and its child models are
/// lit by, the five collision bodies with their contact tables, and the
/// halfwords the per-frame tick and the behaviour handlers drive. The weapon,
/// grenade and shield child tasks reach it through their parent to share the
/// matrices and to take requests from the behaviour handlers.
///
/// `behavior` indexes `_gGolemPawnRookBehaviorHandlers` and `step` is the position inside
/// that handler; a handler asks for an animation in `anim` and for movement in
/// `forwardSpeed` and `turnRate`, and the tick carries them out.
typedef struct {
    ActorAnimRig19        rig;                // animation context, slots and pose buffers of the body's nineteen parts
    MATRIX                colorMtx;           // colour matrix of the body model, shared by the child models
    MATRIX                lightMtx;           // light matrix of the body model, shared by the child models
    WorldCollisionBody    sightBody;          // capsule on part 4 the enemy sees the player with; a handler that has spotted the player switches it off until it looks again
    WorldCollisionCapsule sightCapsule;       // its shape: radius 0x5DC at the part, narrowing to 0x3E8 at 0x1F40 ahead
    WorldCollisionContact sightContacts[1];   // what the sight capsule touched this frame
    WorldCollisionBody    hurtBody;           // sphere on part 3 that takes weapon hits and pushes; resized and grid-enabled when knocked down
    WorldCollisionContact hurtContacts[5];    // its hits and overlaps, also published as `Enemy::recs`
    WorldCollisionBody    groundBody;         // sphere resting on the root that keeps the standing enemy on the floor and out of walls
    WorldCollisionContact groundContacts[4];  // its grid contacts
    WorldCollisionBody    strikeBody;         // sphere on the weapon child's coordinate; pair-enabled with an attack key while a strike is live
    WorldCollisionContact strikeContacts[1];  // what the strike landed on
    WorldCollisionBody    laserBody;          // capsule on the root the Grenade Launcher's laser sight probes with; linked by those packages only
    WorldCollisionCapsule laserCapsule;       // its shape in root space: `ends[1]` the muzzle and `ends[0]` the far end, both rebuilt every aimed frame
    WorldCollisionContact laserContacts[1];   // where the laser stopped
    TaskDesc*             taskTable;          // the package's task table, kept so the launcher child can spawn its grenade entry
    EffectSpawnArg        hitEffectArg;       // argument record of the hit effect spawned on part 3
    VECTOR                prevRootPos;        // root translation before this frame's step, put back when the collision resolve rejects the move
    SVECTOR               hitTilt;            // flinch rotation applied to part 3, eased back to zero 0x20 a frame
    EffectWork*           screamEffect;       // effect of the Silence scream in progress, told to end when the scream is broken; NULL otherwise
    s16                   anim;               // animation the behaviour asks for
    s16                   playingAnim;        // animation the slots were last started on; a difference from `anim` restarts them with a blend
    s16                   animFrame;          // frames since `playingAnim` started
    s16                   hitCooldown;        // frames during which further weapon hits are ignored, set by the weapon that hit
    s16                   forwardSpeed;       // distance the root moves along its facing each frame; negative backs away
    s16                   turnRate;           // angle the root turns toward `targetYaw` each frame; 0 holds the heading
    u16                   prevCueFlags;       // cue bits of the previous frame's animation record, so a cue plays once on its falling edge
    s16                   yaw;                // heading of the root, 0..0xFFF
    s16                   targetYaw;          // heading the root turns toward, 0..0xFFF
    s16                   behavior;           // running handler, a `GOLEM_PAWN_ROOK_BEHAVIOR_` index into `_gGolemPawnRookBehaviorHandlers`
    s16                   step;               // step inside that handler; each handler numbers its own
    s16                   hitFromFront;       // side the last weapon hit came from (1 in front of the enemy, 0 behind)
    s16                   patrols;            // bit 0 of the placement's mode (0 stands idle, 1 walks a patrol); read at spawn only
    s16                   timer;              // frame counter of the running handler, counted up or down as that handler needs
    s16                   dustTimer;          // frames since the broken Rook last shed a spark; one every third frame
    s16                   playerSpotted;      // set for the frame the player is heard close by or seen without a wall between
    s16                   hitTiltActive;      // set while `hitTilt` is still easing out
    s16                   interruptDamage;    // damage taken since an attack or scream opened; enough of it breaks that off
    s16                   downedPose;         // 0 standing, 1 knocked down by a hit from behind, 2 by one from the front; filed as the saved pose on death
    s16                   fireRequest;        // raised to make the launcher child spawn a grenade, which clears it
    s16                   shotsSinceReload;   // grenades fired since the last reload; the sixth is followed by the reload animation
    s16                   burstShots;         // grenades fired in the current burst, up to `GOLEM_PAWN_ROOK_BURST_SHOT_LIMIT`
    s16                   screamCount;        // Silence screams started; each one halves the chance of the next
    s16                   screamActive;       // set while a Silence scream is charging, so damage counts toward breaking it
    s16                   screamCharges;      // screams the Rook can have broken before it is damaged (1 or 2 at spawn, 0 for a Pawn); while any remain hits whose key carries 0x8000 are quartered and light hits do not stagger
    byte                  unknown_6C6[4];     // never accessed; role and layout unproven
    s16                   actorId;            // number of the actor package (0x14, 0x17, 0x38, 0x39), as used in its collision keys and sound-bank request
    s16                   attackActive;       // set while a charge or a grenade burst is under way, so damage counts toward breaking it
    s16                   shieldRaised;       // set while the Rook's shield covers its front: such hits wear the shield down instead
    s16                   shieldHp;           // damage the shield can still absorb (250 at spawn, 0 for a Pawn)
    s16                   shieldBreakStep;    // request to the shield child (0 attached, 1 burst this frame, 2 burst and due to be destroyed)
    s16                   fallingDown;        // set while a knockdown animation is still playing; a hit taken meanwhile starts no downed reaction
    s16                   soundSet;           // the room's sound variant (0 none, 1..4): picks the footstep and fall cues and the bank loaded at spawn
    s16                   swordTrailDelay;    // frames until the Beam Sword child spawns its trail effect; 0 once spawned
    s16                   patrolDistanceLeft; // distance left on the current patrol leg; at 0 the enemy turns about
    s16                   turnedAround;       // set once the lunge wind-up has turned about, so it does not turn twice running
    s16                   knockdownStage;     // 0 upright, 1 on the frame a knockdown starts, 2 after it; from 2 the root is no longer pressed to the floor
    s16                   buildupActive;      // set while the buildup reaction holds the enemy; staggers wait until it is over
} GolemPawnRookWork;
STATIC_ASSERT_SIZEOF(GolemPawnRookWork, 0x6E4);

/// Work block of a grenade the Grenade Launcher fires, allocated at this size
/// by the grenade task's spawn state and kept at `Task::work`.
///
/// It holds the matrices the grenade's model is lit by, the collision bodies
/// that end its flight, and the counters its flight and teardown states run
/// on. Two spheres at the grenade's origin share one contact: one carries the
/// package's grenade attack to the player, the other strikes the room's other
/// enemies. A thin capsule trailing the grenade finds the room surface it has
/// flown into. The flight ends, and the burst effect is spawned, on a contact
/// of either sphere, on a surface that blocks probes, or after 0x5A frames.
typedef struct {
    MATRIX                colorMtx;          // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;          // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    playerStrikeBody;  // sphere of radius 0x64 whose key carries entry 3 of the package's `DamageAttack` table to the player
    WorldCollisionContact strikeContacts[1]; // contact table both strike spheres share: what the grenade touched; a hit on the player also starts a pad rumble
    WorldCollisionBody    enemyStrikeBody;   // sphere of the same size whose key, 0x22B2B, is an attack of the category enemies take damage from
    WorldCollisionBody    wallBody;          // capsule tested against the room grid, clipped at its first contact
    WorldCollisionCapsule wallCapsule;       // its shape: radius 1, from the grenade's origin to 0x1F4 behind it along the axis it flies on
    WorldCollisionContact wallContacts[1];   // the surface the capsule met; cleared once read, since one that lets probes through does not stop the grenade
    s16                   timer;             // frame counter of the running state: in flight, frames since the last smoke puff (one every fourth); in teardown, frames since the bodies were unlinked
    s16                   flightFrames;      // frames flown; the grenade bursts at 0x5A
    s16                   teardownStep;      // step of the teardown state (0 unlinks the three bodies, 1 waits 0x3D frames and destroys the task)
    s16                   burstStyle;        // spawn argument of the burst effect (0 rings and bouncing sparks, for a grenade whose attack inflicts Darkness; 1 a spray of smoke puffs)
} GolemPawnRookGrenadeWork;
STATIC_ASSERT_SIZEOF(GolemPawnRookGrenadeWork, 0xF0);

/// Scratch-stack block of the per-frame hit and push handler.
///
/// Reserved for the length of the call. Positions taken from a coordinate's
/// composed matrix and from a contact are in the space the coordinates are
/// composed into; nothing in the block carries from one frame to the next.
typedef struct {
    WorldCollisionDelta delta;         // push-back resolved from a contact table, in its fixed-point view; then, as a whole-unit vector, the attacking player's position less the root's for a weapon hit, or part 3's position less the contact's for an overlap
    VECTOR              normal;        // `delta` of the deepest overlap, normalised (0x1000 for 1)
    VECTOR              pushDirection; // `normal` turned onto the room grid's axes; the root is pushed along its x and z by the overlap's depth
    SVECTOR             effectOffset;  // where the hit effect appears, as an offset from part 3 along that part's axes; afterwards the position of the player's part 4, where the sight check starts
    SVECTOR             rootPos;       // position of the golem's root, where the sight check ends
} GolemPawnRookHitScratch;
STATIC_ASSERT_SIZEOF(GolemPawnRookHitScratch, 0x40);

/// Attaches a child model to a live GOLEM part and borrows the body's lighting.
///
/// child must be a TMD task parented to a live GOLEM body. bodyPart is a
/// coordinate index 0..18 in that body's model (weapon 7, shield 11). Starts
/// the child's running state without changing its local transform or removing
/// it from the parent's teardown tree. The returned work and borrowed lighting
/// matrices remain valid only while the body task is alive.
static inline GolemPawnRookWork* _golemPawnRookAttachChildModel(Task* child, s32 bodyPart)
{
    Task*              body;
    TmdObject*         childModel;
    GolemPawnRookWork* work;
    GfxCoord*          childRoot;
    GfxCoord*          bodyCoords;

    body                    = child->parent;
    childModel              = child->extra.tmd;
    bodyCoords              = body->extra.tmd->coords;
    childRoot               = childModel->coords;
    work                    = body->work;
    childRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    childRoot->parent       = &bodyCoords[bodyPart];
    childModel->lightMtx    = &work->lightMtx;
    childModel->flags       = 0;
    childModel->colorMtx    = &work->colorMtx;
    child->state            = GOLEM_PAWN_ROOK_TASK_RUNNING;
    return work;
}

static void _golemPawnRookIdleState(Task* actor);
static void _golemPawnRookPatrolState(Task* actor);
static void _golemPawnRookCheckPlayerNoise(Task* actor);
static void _golemPawnRookRecoilState(Task* actor);
static void _golemPawnRookCollapseState(Task* actor);
static void _golemPawnRookDownedHitState(Task* actor);
static void _golemPawnRookDownedDeathState(Task* actor);
static void _golemPawnRookTurnTowardTarget(Task* actor);
static void _golemPawnRookDecayHitTilt(Task* actor);
static void _golemPawnRookPlayAnimCues(Task* actor);
static void _golemPawnRookDeadState(Enemy* enemy, Task* actor);

/* Implemented by each package's hit and push handler. */
static void _golemPawnRookTakeHits(Task* actor);
static void _golemPawnRookKnockdownState(Task* actor);
static void _golemPawnRookEngageState(Task* actor);
static void _golemPawnRookGrenadeBurstState(Task* actor);
static void _golemPawnRookSpawn(Enemy* enemy, Task* actor);
static void _golemPawnRookLauncherStrikeState(Task* actor);
static void _golemPawnRookAimLaserSight(Task* actor);
static void _golemPawnRookDrawLaserBeam(Task* actor, const SVECTOR* farEnd, const SVECTOR* nearEnd);
static void _golemPawnRookGrenadeFly(Enemy* unusedEnemy, Task* grenade);

static void _golemPawnRookSilenceScreamState(Task* actor);
static void _golemPawnRookFrameState(Enemy* enemy, Task* actor);
static void _golemPawnRookShieldTick(Enemy* unusedEnemy, Task* shield);

static inline void _golemPawnRookSpawnDamageSparks(Task* actor);
static inline void _golemPawnRookApplyBuildupReaction(Task* actor);
static inline void _golemPawnRookStepRoot(Task* actor);
static inline void _golemPawnRookTickAnim(Task* actor);
static inline void _golemPawnRookUpdateColorAndDrawShadow(Task* actor, GfxCoord* bodyRoot);

static void _golemPawnRookFrameStateNoSparks(Enemy* enemy, Task* actor);
static void _golemPawnRookStaggerState(Task* task);
static void _golemPawnRookBuildupState(Task* task);
static void _golemPawnRookNopState(Task* task);
static void _golemPawnRookSwordTick(Enemy* unusedEnemy, Task* sword);

#if GOLEM_PAWN_ROOK_WEAPON == GOLEM_PAWN_ROOK_WEAPON_BEAM_SWORD
static void _golemPawnRookSwordChargeState(Task* actor);
static void _golemPawnRookSwordSwingState(Task* actor);
static void _golemPawnRookSwordSpawn(Enemy* unusedEnemy, Task* task);
#endif

#if GOLEM_PAWN_ROOK_WEAPON == GOLEM_PAWN_ROOK_WEAPON_GRENADE_LAUNCHER
static void _golemPawnRookGrenadeSpawn(Enemy* enemy, Task* grenade);
static void _golemPawnRookGrenadeDestroy(Enemy* enemy, Task* grenade);
static void _golemPawnRookLauncherSpawn(Enemy* unusedEnemy, Task* task);
static void _golemPawnRookLauncherTick(Enemy* enemy, Task* task);
#endif

#if GOLEM_PAWN_ROOK_TYPE == GOLEM_PAWN_ROOK_KIND_ROOK
static void _golemPawnRookShieldSpawn(Enemy* unusedEnemy, Task* task);
#endif

#endif /* SRC_SHARED_GOLEM_PAWN_ROOK_H */
