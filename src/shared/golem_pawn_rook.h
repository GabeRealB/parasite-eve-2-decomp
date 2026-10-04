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
 * GOLEM_PAWN_ROOK_TYPE is GOLEM_PAWN or GOLEM_ROOK, GOLEM_PAWN_ROOK_WEAPON is
 * GOLEM_BEAM_SWORD (actor_02000, actor_02300) or GOLEM_GRENADE_LAUNCHER
 * (actor_05600, actor_05700); the parameters below follow from them.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The package defines its tables at its own positions under these
 * names:
 *
 *   s16 gGolemPawnRookAnimBlendFrames[32]  blend-in length per animation, in frames
 *   s32 gGolemPawnRookVoiceCues[17]        the voice-cue sound ids of its sound bank
 *   s32 gGolemPawnRookSwingCue             the sword strike's cue (Beam Sword only)
 *   s32 gGolemPawnRookImpactSound          the grenade's impact cue (Grenade Launcher only)
 */

#ifndef SRC_SHARED_GOLEM_PAWN_ROOK_H
#define SRC_SHARED_GOLEM_PAWN_ROOK_H

#define GOLEM_PAWN             1
#define GOLEM_ROOK             2
#define GOLEM_BEAM_SWORD       1
#define GOLEM_GRENADE_LAUNCHER 2

/* Per weapon: the lunge cycle's wind-up turn rate and lunge range, and the
 * state and animation it hands over to for the lunge and for the walk-in (the
 * Grenade Launcher's state table is the longer one). The Beam Sword's charge
 * turns at GOLEM_PAWN_ROOK_CHARGE_TURN, which only the Pawn does, and a hit
 * that leaves it under GOLEM_PAWN_ROOK_LOW_HP of its maximum takes the heavy
 * reaction. */
#if GOLEM_PAWN_ROOK_WEAPON == GOLEM_BEAM_SWORD
#define GOLEM_PAWN_ROOK_WIND_UP_TURN 0x3C
#define GOLEM_PAWN_ROOK_LUNGE_RANGE  0x8CA
#define GOLEM_PAWN_ROOK_LUNGE_STATE  4
#define GOLEM_PAWN_ROOK_LUNGE_ANIM   8
#define GOLEM_PAWN_ROOK_WALK_STATE   3
#define GOLEM_PAWN_ROOK_WALK_ANIM    5
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_PAWN
#define GOLEM_PAWN_ROOK_CHARGE_TURN 0xF
#define GOLEM_PAWN_ROOK_LOW_HP(max) ((max) * 15 / 100)
#else
#define GOLEM_PAWN_ROOK_CHARGE_TURN 0
#define GOLEM_PAWN_ROOK_LOW_HP(max) ((max) / 4)
#endif
#else
#define GOLEM_PAWN_ROOK_WIND_UP_TURN 0x1E
#define GOLEM_PAWN_ROOK_LUNGE_RANGE  0x7D0
#define GOLEM_PAWN_ROOK_LUNGE_STATE  7
#define GOLEM_PAWN_ROOK_LUNGE_ANIM   0x10
#define GOLEM_PAWN_ROOK_WALK_STATE   6
#define GOLEM_PAWN_ROOK_WALK_ANIM    0xC
#endif

/* Per build: the type id (also in its collision keys, 0x30000 | id); and for
 * the Grenade Launcher's approach cycle, how long it backs away and how many
 * strikes it counts before the follow-up. */
#if GOLEM_PAWN_ROOK_WEAPON == GOLEM_GRENADE_LAUNCHER
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_PAWN
#define GOLEM_PAWN_ROOK_ID             0x38
#define GOLEM_PAWN_ROOK_BACKOFF_FRAMES 0x1E
#define GOLEM_PAWN_ROOK_STRIKE_LIMIT   6
#else
#define GOLEM_PAWN_ROOK_ID             0x39
#define GOLEM_PAWN_ROOK_BACKOFF_FRAMES 0x3C
#define GOLEM_PAWN_ROOK_STRIKE_LIMIT   3
#endif
#endif

#include "types.h"

#include "actors/actor.h"

#include "gameplay/enemy.h"

#include "main/task_types.h"

/// Values of `GolemPawnRookWork::behavior`, the entries of each package's
/// `gGolemPawnRookStates`.
///
/// Every package uses the same numbering and fills the entries its build has
/// no handler for with `golemPawnRookNopState`: the two sword entries in a
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

/// Work block of a Pawn or Rook GOLEM, allocated at this size by the body
/// task's spawn state and kept at `Task::work`.
///
/// It holds the animation rig, the matrices the body and its child models are
/// lit by, the five collision bodies with their contact tables, and the
/// halfwords the per-frame tick and the behaviour handlers drive. The weapon,
/// grenade and shield child tasks reach it through their parent to share the
/// matrices and to take requests from the behaviour handlers.
///
/// `behavior` indexes `gGolemPawnRookStates` and `step` is the position inside
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
    s16                   behavior;           // running handler, a `GOLEM_PAWN_ROOK_BEHAVIOR_` index into `gGolemPawnRookStates`
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
    s16                   burstShots;         // grenades fired in the current burst, up to `GOLEM_PAWN_ROOK_STRIKE_LIMIT`
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

void golemPawnRookIdleState(Task* arg0);
void golemPawnRookApproachState(Task* arg0);
void golemPawnRookCheckProximity(Task* arg0);
void golemPawnRookRecoilState(Task* arg0);
void golemPawnRookCollapseState(Task* arg0);
void golemPawnRookDownedShiftState(Task* arg0);
void golemPawnRookDownedFinishState(Task* arg0);
void golemPawnRookTurnTowardTarget(Task* arg0);
void golemPawnRookDecayHitTilt(Task* arg0);
void golemPawnRookPlayAnimCues(Task* arg0);
void golemPawnRookDeadState(Enemy* arg0, Task* arg1);

/* Implemented by each package's hit and push handler. */
void golemPawnRookTakeHits(Task* arg0);
void golemPawnRookKnockdownState(Task* arg0);
void golemPawnRookChargeState(Task* arg0);
void golemPawnRookLungeCycle(Task* arg0);
void golemPawnRookCompanionCycle(Task* arg0);
void golemPawnRookSpawn(Enemy* ctx, Task* actor);
void golemPawnRookLungeStrikeState(Task* arg0);
void golemPawnRookAimLaserSight(Task* arg0);
void golemPawnRookDrawLaserBeam(Task* arg0, SVECTOR* arg1, SVECTOR* arg2);
void golemPawnRookBulletSpawn(Enemy* arg0, Task* arg1);
void golemPawnRookBulletFly(Enemy* arg0, Task* arg1);
void golemPawnRookGunTick(Enemy* enemy, Task* task);
void golemPawnRookBulletDestroy(Enemy* arg0, Task* arg1);

void golemPawnRookSilenceScreamState(Task* arg0);
void golemPawnRookFrameState(Enemy* ctx, Task* actor);
void golemPawnRookBurstPartTick(Enemy* arg0, Task* arg1);

static inline void golemPawnRookSpawnDust(Task* actor);
static inline void golemPawnRookApplyReaction(Task* actor);
static inline void golemPawnRookStepRoot(Task* actor);
static inline void golemPawnRookTickAnim(Task* actor);
static inline void golemPawnRookDraw(Task* actor, GfxCoord* coord);

void golemPawnRookFrameStateNoDust(Enemy* ctx, Task* actor);
void golemPawnRookBeamSwingState(Task* arg0);
void golemPawnRookHitReactionState(Task* task);
void golemPawnRookFlagWaitState(Task* task);
void golemPawnRookNopState(Task* task);
void golemPawnRookDelayedEffectSpawn(Enemy* arg0, Task* task);
void golemPawnRookDelayedEffectTick(Enemy* arg0, Task* task);
void golemPawnRookGunSpawn(Enemy* arg0, Task* task);
void golemPawnRookBurstPartSpawn(Enemy* arg0, Task* task);

#endif /* SRC_SHARED_GOLEM_PAWN_ROOK_H */
