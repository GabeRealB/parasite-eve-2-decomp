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
 * per-transition blend-length table and mixes a second rig into slots 1-10;
 * it eases a look turn spread over the neck parts 2-4 and a counter-turn of
 * the waist, part 10, and plays the sound its per-package cue step returns.
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

/// The Desert Chaser enemy task's handlers, indexed by `Task::state`.
///
/// Each package defines one table, which the task entry copies to its stack
/// and calls through every frame. The states are spawn, the per-frame driver
/// and teardown. The regular build has four: a state of its own ahead of
/// teardown settles the release the chaser may still owe, then advances.
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

/// Allocation holding the cutscene build's hit-effect argument record.
///
/// `effectArg` is the record the armed builds keep in their work block as
/// `DesertChaserWork::effectArg` and hand the effect spawner with every hit.
/// The cutscene build keeps it as a global instead: its spawn state binds it
/// to the model root's coordinate with the armed builds' two arguments, and
/// nothing reads it afterwards, since this build takes no hits and spawns no
/// effect from it.
///
/// In both cutscene packages 88 zero bytes follow the record and run to the
/// last byte of the image. No access to them is recovered, so whether they
/// are trailing fields of this object or separate unreferenced variables is
/// unproven; they stay in this allocation only so the image keeps its length.
typedef struct {
    EffectSpawnArg effectArg;     // Root coordinate, spawn-argument low half 0x100 and high half 2, stored once by the spawn state; never read
    u8             unknown_8[88]; // Zero in the image; no access established and role unproven
} DesertChaserEffectArgStorage;
STATIC_ASSERT_SIZEOF(DesertChaserEffectArgStorage, 96);
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
#define DESERT_CHASER_AVOID_BEARINGS   16 /* bearings the avoid walk's scratch block holds */
#define DESERT_CHASER_STATE_COUNT      39 /* handlers `DesertChaserWork::state` indexes */
#define DESERT_CHASER_RUN_SEQUENCE     0
#define DESERT_CHASER_STATE_TURN_RIGHT 8
#define DESERT_CHASER_STATE_TURN_LEFT  9
#define DESERT_CHASER_CLIP_STAGGER     0x10
#define DESERT_CHASER_CLIP_COLLAPSE    0x13
#define DESERT_CHASER_CLIP_STUNNED     0x15
#define DESERT_CHASER_CLIP_TURN_STEP   0x11
#define DESERT_CHASER_CLIP_TURN_PROBE  0x12
#define DESERT_CHASER_SLOT_RATE(work)  ((work)->baseRate) /* the chaser's own rate */
/* seeing the player raises the alert and starts the chase */
#define DESERT_CHASER_NOTICE(work) (Gp_ArmStateF0(1), (work)->state = 0x26)
/* how near the player has to be before a steering chaser closes in */
#define DESERT_CHASER_CLOSE_IN 2000
#else
#define DESERT_CHASER_CONTACTS         12
#define DESERT_CHASER_AVOID_BEARINGS   8  /* half the regular build's, though its tables hold more contacts */
#define DESERT_CHASER_STATE_COUNT      40 /* the run adds one state to the regular build's */
#define DESERT_CHASER_RUN_SEQUENCE     1
#define DESERT_CHASER_STATE_TURN_RIGHT 9
#define DESERT_CHASER_STATE_TURN_LEFT  10
#define DESERT_CHASER_CLIP_STAGGER     0x13 /* three more clips before these */
#define DESERT_CHASER_CLIP_COLLAPSE    0x16
#define DESERT_CHASER_CLIP_STUNNED     0x18
#define DESERT_CHASER_CLIP_TURN_STEP   0x14
#define DESERT_CHASER_CLIP_TURN_PROBE  0x15
#define DESERT_CHASER_SLOT_RATE(work)  0x10 /* every runner alike */
#define DESERT_CHASER_NOTICE(work)     ((work)->state = 0x1C)
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

/// Indices into `DesertChaserWork::spheres`.
enum {
    DESERT_CHASER_SPHERE_FRONT = 0, // on part 2, the base of the neck: takes the hits and steers round other bodies; its table is also the enemy's hit records
    DESERT_CHASER_SPHERE_REAR  = 1, // on part 10, the waist, 0x100 behind its origin: takes the hits and steers round other bodies
    DESERT_CHASER_SPHERE_ROOT  = 2  // on the model root, 0x11C above it: the room grid pushes the root with its contacts
};

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

/// Values of `DesertChaserWork::animRequest` and
/// `DesertChaserWork::blendRequest`.
///
/// A zero-filled block holds 0, on which the driver only advances the slots.
/// The blend rig is never asked to blend in.
enum {
    DESERT_CHASER_ANIM_REQUEST_BLEND   = 1, // seek the slots to the clip, blending over the frames the transition table gives
    DESERT_CHASER_ANIM_REQUEST_RESET   = 2, // restart the slots on the clip
    DESERT_CHASER_ANIM_REQUEST_PLAYING = 3  // the request has been applied
};

/// Work block of the Desert Chaser task, in all three builds.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the state machine, both animation rigs and their driver's state, and
/// storage for the model's matrices; the armed builds add the collision
/// bodies with their contact records, the records sent to the player a lunge
/// catches, and the tuning the spawn argument selects.
///
/// The builds share the head, up to and including `lastCueFrames`, and differ
/// after it: 0x934 bytes in the cutscene build, 0xC30 in the regular one and
/// 0xEB0 in the Water Tower one. An unnamed member carries its offset in the
/// build that declares it.
///
/// The model is a quadruped: parts 2 to 4 are the neck, part 10 the waist the
/// hindquarters hang from, parts 7 and 9 the forelegs and 14 and 17 the hind
/// legs. Angles are 4096ths of a turn and positions are in the root
/// coordinate's parent space unless a field says otherwise. Clip ids index the
/// package's animation bank; rates are sixteenths of a frame per tick.
typedef struct {
    s16              state;             // index of the handler the per-frame tick runs; 0 is hidden in every build
    s16              prevState;         // `state` the tick last ran; -1 makes the next tick enter `state` afresh
    s16              stateEntered;      // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    s16              stateTimer;        // counter of the state: most count ticks up from 0, the downed state counts down, the walks count ticks the room grid held them back
    s16              stateCounter;      // second counter of the pursuit, the roams and the leap back: ticks spent winding up, ticks roamed, pushes the room grid gave
    byte             field_A[2];        // never accessed
    ActorPatrolPoint patrolPoints[2];   // the spawn position and a point 5000 units ahead along the spawn facing; the roam replaces either end
    s16              patrolTarget;      // index into `patrolPoints` of the end being walked toward
    s16              placedYaw;         // heading of the root after the last placement message; never read
    byte             field_18[4];       // never accessed
    ActorAnimRig18   rig;               // playback of the model's parts; slot 1's status and frame time the states
    ActorAnimRig18   blend;             // second playback of the same model, mixed into parts 1 to 10 while `blendActive`
    byte             field_824[4];      // never accessed
    u16              animRequest;       // `DESERT_CHASER_ANIM_REQUEST_*` for `rig`
    s16              blendActive;       // 1 while `blend`'s clip is mixed in; cleared when its slot 1 finishes
    s16              appliedAnim;       // clip `rig` was last started on; a blend request for the same clip leaves the slots running
    s16              animId;            // clip requested of `rig`
    u16              animFrames;        // ticks since `animRequest` was last applied; never read
    u16              animRate;          // playback rate of `rig`'s slots, which play 3 slower while `blendActive`
    u16              baseRate;          // `animRate` the walking and attacking states start on: 0xF or 0x11 by the place index in the regular build, 0x10 in the others; also scales the regular leaps' steps
    s16              blendRequest;      // `DESERT_CHASER_ANIM_REQUEST_*` for `blend`; only `RESET` is requested
    s16              blendAnimId;       // clip requested of `blend`
    u16              blendRate;         // playback rate of `blend`'s slots: 0x10 from the regular spawn, 0x20 from each restart in the other builds
    s16              blendWeight;       // share of `rig`'s pose in the mix, of 0x1000, 0x800 from each restart; only the cutscene build reads it, the armed ones mix by fixed per-part weights
    u16              waistYawTarget;    // what `waistYaw` moves toward: the turn the walking states made this tick, scaled up by the roam
    s16              lookYawTarget;     // bearing to what the state faces, relative to the facing
    s16              waistYaw;          // eased toward `waistYawTarget`, clamped to +-0x200, by 0xC a tick; part 10 turns against it
    s16              lookYaw;           // eased toward `lookYawTarget` by 0x71 a tick; within +-0x500, parts 2 and 3 turn by a third of it each and part 4 by half
    byte             field_846[2];      // never accessed
    s32              lastCueFrames[18]; // per slot, its frame when a cue last fired on it, so a held frame fires once; cleared whole on a tick that meets no cue frame
#if DESERT_CHASER_BUILD == DESERT_CHASER_CUTSCENE
    byte   field_890[4];                // never accessed
    MATRIX lightMtx;                    // storage for the model's `TmdObject::lightMtx`
    MATRIX colorMtx;                    // storage for the model's `TmdObject::colorMtx`
    byte   field_8D4[0x48];             // never accessed
    u8     commandBytes[3];             // stage, area and low command byte of the last actor command received; never read
    byte   field_91F[0x15];             // never accessed
#elif DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    EffectSpawnArg           effectArg;          // argument record of the hit effects, hung off the part the hit offset names
    SVECTOR                  effectOffset;       // offset of the last dust effect from its part
    SVECTOR                  hitOffset;          // offset of the last hit effect from its part, with the part's index in `pad`
    SVECTOR                  burnPosFront;       // view-space anchor of the first corpse-burn effect: part 2's origin, lowered to the root's height once spawned
    SVECTOR                  burnPosForeleg;     // view-space anchor of the second corpse-burn effect: part 9's origin at the root's height
    byte                     field_8B8[8];       // never accessed
    ActorTransform           playerPlacement;    // where the player is placed as a lunge catches them: where they stand, turned along or against the chaser's facing
    GameActorMoveBy          playerMove;         // the push sent to the caught player, halved or cleared as the catch goes on
    GameActorButtonPressHold playerButtonHold;   // the button-press hold sent to the player as a lunge connects; only `pressCount` is filled in
    ActorCommand             broadcast;          // command sent to every actor of the scene, in the chasers' own stage 9, area 1 namespace: 1 as a pursuit starts, 4 as its lunge starts or a hit empties this chaser's health
    DesertChaserSphereBody   spheres[3];         // the collision spheres, indexed by `DESERT_CHASER_SPHERE_*`
    DesertChaserCapsuleBody  wallProbe;          // capsule along the facing that only the room grid tests; the walks shorten their step while it touches
    MATRIX                   lightMtx;           // storage for the model's `TmdObject::lightMtx`
    MATRIX                   colorMtx;           // storage for the model's `TmdObject::colorMtx`; the leap in scales it up over its first 30 ticks
    byte                     field_BC0[0x20];    // never accessed
    s16                      hitCooldown;        // ticks before another hit is taken; set from the hit's id parameter 2
    u16                      recentDamage;       // damage taken since a light hit last found no flinch blended in; 0x47 of it knocks the chaser down
    u16                      hitFlag;            // 1 from each hit taken, cleared as most reaction states begin; never read
    byte                     field_BE6[0xA];     // never accessed
    SVECTOR                  playerDelta;        // offset from the root to the player, followed while the pursuit winds up; the windup turns toward it
    AnimationPlayRequest     playerAnim;         // animation the caught player is sent, from the front or the rear set table; `animationId` 1..5 also tracks the catch's stage
    DesertChaserLastCommand  lastCommand;        // last actor command received; this build stores it and never reads it
    byte                     field_C10[8];       // never accessed
    s16                      playerHeld;         // 1 while the player is in a catch this enemy started
    s16                      lungeDistance;      // distance the pursuit's lunge has covered; under 1000 the catch is the close one
    byte                     field_C1C[2];       // never accessed
    s16                      windupFrames;       // ticks the pursuit crouches before it may lunge; second value of the variant record
    u16                      downFramesBase;     // ticks the downed state lasts, before a random 0..15 more; first value of the variant record
    s16                      roamLookDelay;      // ticks a roam runs before it looks for the player; third value of the variant record
    u16                      chaseHoldoffFrames; // `chaseHoldoff` a pack command 3 starts; fourth value of the variant record
    s16                      chaseHoldoff;       // ticks before the roam may start a pursuit; 0x5A from pack command 1, counted down every tick
    s16                      playerAnimFrames;   // ticks since `playerAnim` was last sent, timing each stage of the catch
    s16                      deathPending;       // 1 from the hit that empties the health until the battle reference is released, which waits for the player's release
    byte                     field_C2C[4];       // never accessed
#elif DESERT_CHASER_BUILD == DESERT_CHASER_WATER_TOWER
    EffectSpawnArg           effectArg;          // argument record of the hit effects, hung off the part the hit offset names
    SVECTOR                  hitOffset;          // offset of the last hit effect from its part, with the part's index in `pad`
    s8                       roomNotified;       // 1 once the dead chaser has told the room the pack is spent, cleared as its death starts; never read
    byte                     field_8A1[3];       // never accessed
    GameActorMoveBy          playerMove;         // the push sent to the caught player, halved or cleared as the catch goes on
    ActorTransform           playerPlacement;    // where the player is placed as a lunge catches them: where they stand, turned along or against the chaser's facing
    GameActorButtonPressHold playerButtonHold;   // the button-press hold sent to the player as a lunge connects; only `pressCount` is filled in
    ActorCommand             broadcast;          // command sent to every actor of the scene, in the chasers' own stage 9, area 1 namespace: 1 as a pursuit starts, 3 as a hit empties this chaser's health
    DesertChaserSphereBody   spheres[3];         // the collision spheres, indexed by `DESERT_CHASER_SPHERE_*`
    DesertChaserCapsuleBody  wallProbe;          // capsule along the facing that only the room grid tests; the walks shorten their step while it touches
    MATRIX                   lightMtx;           // storage for the model's `TmdObject::lightMtx`
    MATRIX                   colorMtx;           // storage for the model's `TmdObject::colorMtx`
    byte                     field_E44[0x20];    // never accessed
    s16                      hitCooldown;        // ticks before another hit is taken; set from the hit's id parameter 2
    u16                      recentDamage;       // damage taken since a light hit last found no flinch blended in; 0x47 of it knocks the chaser down
    byte                     field_E68[8];       // never accessed
    SVECTOR                  playerDelta;        // offset from the root to the player, followed while the pursuit winds up; the windup turns toward it
    s16                      fleeZone;           // arena zone the flee state runs to and hides in: 1 when the player's zone is 7 or above, else 0xB
    byte                     field_E7A[2];       // never accessed
    AnimationPlayRequest     playerAnim;         // animation the caught player is sent, from the front or the rear set table; `animationId` 1..5 also tracks the catch's stage
    DesertChaserLastCommand  lastCommand;        // last actor command received, which the states branch on, and the catch's frame counter above it
    Task*                    childTask0;         // killed by the exit callback when set; nothing sets it
    Task*                    childTask1;         // killed by the exit callback when set; nothing sets it
    s16                      playerHeld;         // 1 while the player is in a catch this enemy started
    s16                      lungeDistance;      // distance the pursuit's lunge has covered; under 1000 the catch is the close one
    byte                     field_EA0[2];       // never accessed
    s16                      windupFrames;       // ticks the pursuit crouches before it may lunge, and the route roam runs before it looks for the player; second value of the variant record
    u16                      downFramesBase;     // ticks the downed state lasts, before a random 0..15 more; first value of the variant record
    s16                      roamLookDelay;      // ticks the roam runs before it looks for the player; third value of the variant record
    u16                      chaseHoldoffFrames; // `chaseHoldoff` a pack command 1 starts; fourth value of the variant record
    s16                      chaseHoldoff;       // ticks before a roam may start a pursuit; the roams count it down
    s16                      roomEventCountdown; // 0x1E from each room event message, counted down while the dead chaser waits to return; never read
    byte                     field_EAE[2];       // never accessed
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
        if (!work->wallProbe.contacts[i].key.value) {
            break;
        }
        if ((work->wallProbe.contacts[i].key.value & 0xFFFF0000) == 0x100000) {
            found = 1;
        }
    }
    return found;
}
#endif

#if DESERT_CHASER_BUILD != DESERT_CHASER_CUTSCENE
/// Scratch-stack block of `desertChaserAvoidWalk`, which nudges the chaser's
/// root away from the bodies its front sphere is touching.
///
/// The walk collects the bearing from `origin` of each contact of kind
/// 0x10000, the category of the player's body, or 0x30000, the category enemy
/// bodies carry; drops every pair of bearings more than 0x400 apart; and steps
/// the coordinate a short way away from each bearing that survives. It is the
/// `ActorContactSteerScratch` walk with one more word, `nonBlocking`, and a
/// per-build number of bearings.
typedef struct {
    MATRIX   rot;                                   // Yaw rotation built for the current push, whose Z axis gives its direction
    SVECTOR  dir;                                   // First the coordinate's normalised Y-axis column, then each push step
    SVECTOR3 origin;                                // Coordinate's world translation, the point bearings are taken from
    s32      kind;                                  // Current record's key masked to its kind half
    s32      nonBlocking;                           // Bit 0x80 of the current record's key; a kind 0x10000 record sets `blocked` only while it is clear. What sets that key bit is unproven
    s16      bearing[DESERT_CHASER_AVOID_BEARINGS]; // Bearings of the collected contact records
    s8       kept[DESERT_CHASER_AVOID_BEARINGS];    // Per bearing: 1 until it falls more than 0x400 from another bearing
    s16      heading;                               // Coordinate's heading in the plane the bearings are measured in
    s16      diff;                                  // Wrapped difference between two bearings, then the push's yaw
    u8       i;                                     // Outer cursor over records, then over bearings
    u8       j;                                     // Inner cursor over the bearings paired with `i`
    u8       count;                                 // Number of bearings collected
    u8       blocked;                               // Set when a kind 0x10000 record without key bit 0x80 was seen; the walk's result
} DesertChaserAvoidScratch;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
STATIC_ASSERT_SIZEOF(DesertChaserAvoidScratch, 0x70);
#else
STATIC_ASSERT_SIZEOF(DesertChaserAvoidScratch, 0x58);
#endif
#endif

#if DESERT_CHASER_BUILD != DESERT_CHASER_CUTSCENE
/// The armed chaser's state handlers, indexed by `DesertChaserWork::state`.
///
/// Each armed package defines one table, and its per-frame update copies the
/// table to the stack before calling the entry of the current state. The call
/// is unconditional, so a `NULL` entry marks a state the chaser must never be
/// put in.
typedef struct {
    TaskFunc handlers[DESERT_CHASER_STATE_COUNT]; // Handler of each state, taking the chaser's task
} DesertChaserStateTable;
STATIC_ASSERT_SIZEOF(DesertChaserStateTable, DESERT_CHASER_STATE_COUNT * sizeof(TaskFunc));

/// One tuning of the armed chaser: the four timings a chaser keeps in its
/// work block from the moment it is set up.
///
/// Both armed packages define the same four tunings. The low four bits of the
/// spawn argument pick one of the first three -- 2 the first, 1 the third,
/// anything else the second -- and their down time and windup shorten in
/// that order. The regular build gives the fourth to a chaser the Dryfield
/// breezeway's command 2 sets pursuing; the Water Tower build never reads it.
/// Each member seeds the `DesertChaserWork` member of the same name.
typedef struct {
    u16 downFramesBase;     // Ticks the downed state lasts, before a random 0..15 more
    s16 windupFrames;       // Ticks the pursuit crouches before it may lunge
    s16 roamLookDelay;      // Ticks a roam runs before it looks for the player
    u16 chaseHoldoffFrames; // Ticks a pack command keeps the roam from starting a pursuit
} DesertChaserVariant;
STATIC_ASSERT_SIZEOF(DesertChaserVariant, 0x8);

/// Scratch-stack block of the armed chaser's damage step, which runs every
/// frame the chaser has health left.
///
/// The step looks for a damaging contact, kind 0x20000, on the front sphere
/// and then the rear one. When it finds one it works out where the hit landed
/// relative to the facing, picks the reaction, and scales the damage by the
/// player's range, a critical roll and the state the chaser was caught in.
/// The damage-over-time tick that follows reuses `damage` alone. Nothing
/// carries over from one frame to the next. Angles are 4096ths of a turn.
typedef struct {
    VECTOR  toPlayer;       // Player's position minus the root's; never read back, and `pad` is never written
    SVECTOR hitOffset;      // `hitPos` minus the root's composed translation; `vx` and `vz` give the hit's bearing, and `pad` is never written
    SVECTOR hitPos;         // Point of the contact found; `pad` is never written
    s32     hitKey;         // Key of the contact found: the kind over the attack's packed id; 0 when neither sphere holds a damaging contact
    s32     damage;         // Damage of the hit: the roll for the range, quadrupled by a critical roll and doubled in the stunned and reaction states; then the damage of the over-time tick
    s32     playerDistance; // Length of `toPlayer`, the range the damage is rolled for; never read back
    s16     hitYaw;         // Bearing of `hitOffset` off the chaser's facing, wrapped to [-0x800, 0x800]
    s16     criticalEffect; // Spawn argument of the critical-hit effect (-1 none spawned, 0 a critical roll, 3 a doubled hit that dealt damage)
} DesertChaserDamageScratch;
STATIC_ASSERT_SIZEOF(DesertChaserDamageScratch, 0x30);

/// Scratch-stack block of the armed chaser's two turn-step states, in which
/// it turns a little further each tick while it shuffles along its facing.
///
/// A state reserves one block a tick. It measures the turn to the player,
/// spreads it over the ticks the state has left, rebuilds the root's
/// rotation about Y at the heading that gives, and slides the root along the
/// new facing: backward in the step, forward in the probe. The block is
/// released before the state returns, and nothing in it carries over to the
/// next tick. Angles are 4096ths of a turn.
typedef struct {
    SVECTOR offset;       // Player's position minus the root's, world units; `vx` and `vz` give the bearing. Then the root's Z axis after the turn, normalised and scaled to the slide added to the root's X and Z
    s16     turn;         // Turn from the facing to the player, wrapped to [-0x800, 0x800]. Past a quarter turn on one side - the negative in the step, the positive in the probe - half a turn is taken off, so the chaser turns its back on the player instead
    s16     heading;      // Heading the root's rotation is rebuilt at: the facing plus `turn` / `stepsLeft`
    s16     stepsLeft;    // Ticks the turn is spread over: 30 less the ticks the state has run, with 1 in place of 0
    byte    unknown_E[2]; // Reserved with the block and never accessed; role unproven
} DesertChaserTurnStepScratch;
STATIC_ASSERT_SIZEOF(DesertChaserTurnStepScratch, 0x10);

/// Scratch-stack block of the armed chaser's roam, in which it walks back and
/// forth between two patrol points while it watches for the player.
///
/// The roam reserves one block a tick and releases it before it returns;
/// the patrol points themselves are kept in the work block. On the tick that
/// enters the state it places the first point 1000 units from the root, off
/// to one side of the player. Every later tick it turns a limited step toward
/// the current point and walks, and when it arrives, or has been pushed about
/// for 21 ticks while facing the point, it places the other point 2000 units
/// away, to alternate sides of the player. The look that ends the roam
/// measures the player's range from `toPlayer` and compares which way the
/// player faces with where the chaser stands. Angles are 4096ths of a turn,
/// and a wrapped one lies in [-0x800, 0x800].
typedef struct {
    SVECTOR toPatrolPoint; // Current patrol point's position minus the root's on X and Z, with `vy` zero; then lent to the steer round obstacles, which leaves the displacement it moved the chaser by, and the look takes its bearing from that. On the entering tick: the player's offset, then the root's offset to the first point
    SVECTOR toPlayer;      // Player's position minus the root's, world units. A tick that places a patrol point overwrites it with the root's offset to that point, and the look refills it
    MATRIX  rotation;      // Turn about Y to the player's bearing swung to one side, by 0x3E8 on the entering tick and 0x2EE afterwards; its Z axis is the direction of the point being placed. Its translation is never set or read
    s16     turn;          // Wrapped turn from the facing to the patrol point; taken the other way round when it is past 0x600 and the player lies on the opposite side, limited to 0x20, then added to the heading: the yaw the root's rotation is rebuilt at. The look reuses it for the bearing of `toPatrolPoint` off the facing
    s16     fullTurn;      // `turn` as first measured, before it is redirected and limited; under 0x20 while the chaser faces its patrol point
    s16     yawFromPlayer; // Bearing from the player to the chaser, wrapped: the reverse of `toPlayer`'s
    s16     playerYaw;     // Heading the player faces; more than 0x600 from `yawFromPlayer` when the player has its back to the chaser
} DesertChaserRoamScratch;
STATIC_ASSERT_SIZEOF(DesertChaserRoamScratch, 0x38);

/// Scratch-stack block of the armed chaser's pursuit, reserved on every tick
/// but the one that enters the state.
///
/// The pursuit turns toward where it last saw the player, then lunges. While
/// lunging it catches a player its front sphere touches who is nearly dead
/// ahead: it faces the player toward or away from itself, takes the damage
/// and starts the player's caught animation. The block holds the angles of
/// that tick and is released before the pursuit returns. Angles are 4096ths
/// of a turn, and a wrapped one lies in [-0x800, 0x800].
typedef struct {
    SVECTOR offset;            // Lent to the avoid walk first, which leaves the displacement it moved the chaser by. Then the player's position minus the root's, world units. During a catch: the root's Z axis, which is the facing, then that axis reversed on X and Z with `vy` zeroed; `pad` is never written
    u32     pushLengthSquared; // Squared X and Z length of the correction the root sphere's grid contacts moved the chaser by that tick, world units; a lunge the grid pushes back by more than 60 units is broken off
    s16     playerTurn;        // Wrapped turn from the player's heading to the reverse of the chaser's facing: under a quarter turn when the player faces the chaser, so is caught from the front
    u16     catchYaw;          // Bearing of the player off the facing, wrapped; a catch needs it under 0x180. Then the heading the caught player is placed at: opposite the chaser's facing, or along it when caught from behind
    s16     turn;              // Wrapped turn from the facing to the player's remembered offset, limited to 0x40 a tick, then added to the heading: the yaw the root's rotation is rebuilt at while it winds up
    s16     playerBearing;     // Wrapped turn from the facing to the player that tick; once the windup has run its ticks the lunge starts when it is under 0x80, and a lunge is abandoned past 0x600
    s16     playerKilled;      // Reply to the damage sent to the player: 1 when it took the player's last health. Not written by a run-sequence chaser that has no health left, which then reads what the scratch stack held
    byte    unknown_16[0x2];   // Reserved with the block and never accessed; role unproven
} DesertChaserPursueScratch;
STATIC_ASSERT_SIZEOF(DesertChaserPursueScratch, 0x18);
#endif

#if DESERT_CHASER_BUILD == DESERT_CHASER_CUTSCENE
/// Scratch-stack block of the cutscene chaser's per-frame driver, held across
/// the state handler the driver runs.
///
/// The driver relights the enemy at the model root's world position before it
/// runs the frame's state, and afterwards refreshes the enemy's body position:
/// the origin of the third model part, carried up its parent chain into view
/// space. Both points are built here. The armed builds have per-frame drivers
/// of their own and do not use this block.
typedef struct {
    VECTOR  rootPos;         // Model root's world position, world units: the translation of its composed matrix, and the point the enemy is lit for
    SVECTOR bodyPos;         // Zeroed as the third model part's own origin, then that point in view space, which becomes `Enemy::bodyPos`; left zero if the part's chain never reaches the view. `pad` is never written
    byte    unknown_18[0x4]; // Reserved with the block and never accessed; role unproven
} DesertChaserFrameScratch;
STATIC_ASSERT_SIZEOF(DesertChaserFrameScratch, 0x1C);
#endif

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
