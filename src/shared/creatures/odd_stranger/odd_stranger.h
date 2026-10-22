/* The animation driver of the Odd Stranger (actor_401000, actor_401800): a
 * 19-slot rig plus a second blend context. State handlers request a clip change (cross-fade through a 45x45
 * transition table, or a hard restart) and can overlay a secondary clip on
 * slots 1-10, mixed by weight. Each frame the driver also eases a head yaw
 * toward its target by up to 0x100 and turns joints 5 and 2 by 2/3 and 1/2 of
 * it, then consumes the current clip's sound cue. An animation message selects
 * a script clip and reenters DOWN without requesting a playback restart.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_ODD_STRANGER_H
#define SRC_SHARED_ODD_STRANGER_H

/// Build discriminator for actor_401000's Odd Stranger.
///
/// Preprocessor integer for ODD_STRANGER_BUILD, bound before this guarded
/// header and kept through all fragments. Selects the compiled work layout,
/// state dispatch and combat/animation behavior, independently of spawn tuning.
/// Includes effectOffset and places grabCooldown after commandBytes.
#define ODD_STRANGER_BUILD_401000 1

/// Build discriminator for actor_401800's Odd Stranger.
///
/// Preprocessor integer for ODD_STRANGER_BUILD, bound before this guarded
/// header and kept through all fragments. Selects the compiled work layout,
/// state dispatch and combat/animation behavior, independently of spawn tuning.
/// Omits effectOffset and places grabCooldown before the collision bodies.
#define ODD_STRANGER_BUILD_401800 2

/// Requires one fixed Odd Stranger build for the shared interface and fragments.
///
/// Each carrier binds `ODD_STRANGER_BUILD` to its named build before including
/// this header and keeps it unchanged through the last shared fragment. These
/// values select work layout, constants and implementation at preprocessing
/// time; they have no runtime storage or units. The spawn argument independently
/// selects each enemy's opening state and combat tuning. A binding may name a
/// build before this header defines it, since expansion occurs at each use.
#ifndef ODD_STRANGER_BUILD
#error "define ODD_STRANGER_BUILD before including odd_stranger.h"
#endif

#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401000
/// Whether the work block contains `effectOffset` (1 in actor_401000, 0 in actor_401800).
#define ODD_STRANGER_HAS_EFFECT_OFFSET 1

/// First WALK sound cue: pose-record index 17 or 16, returning character sound 2.
///
/// Cue indices are the primary slot's low ten record-index bits, not elapsed frames.
#define ODD_STRANGER_WALK_FIRST_SOUND_CUE 0x11

/// Second WALK sound cue: pose-record index 27 or 22, returning character sound 1.
#define ODD_STRANGER_WALK_SECOND_SOUND_CUE 0x1B

/// Normal body-sphere radius, 430 or 300 game-coordinate units.
///
/// The actor_401800 build also uses it for the root grid sphere and chase/fall
/// movement clearance.
#define ODD_STRANGER_BODY_RADIUS 0x1AE

/// Hit-body radius during circling dash and sidestep, 215 or 150 game-coordinate units.
#define ODD_STRANGER_EVASION_BODY_RADIUS 0xD7

/// Root-grid enable choice for WATCH, DEATH_BURN, PLAY_WALK/RUN/DOWN and FLINCH.
///
/// These in-place handlers enable the grid test in actor_401000 and disable it
/// in actor_401800. Other states select their own policy; this does not unlink
/// the body.
#define ODD_STRANGER_IN_PLACE_GRID_ENABLED 1

/// Whether circling-dash and turn-around exits require sight before grabbing.
///
/// Zero in actor_401000, one in actor_401800; ordinary chase, stalk and patrol sight
/// checks are independent of this choice.
#define ODD_STRANGER_DASH_GRAB_REQUIRES_SIGHT 0

/// Patrol and burst-death forward step, 10 or 7 parent-coordinate units per tick.
///
/// Patrol steps only without secondary blending; walking death also steps while blended.
#define ODD_STRANGER_WALK_STEP 0xA

/// Patrol root-turn limit per tick, 32 or 24 angle units (4096 per turn).
///
/// The look target retains the full waypoint bearing before this clamp.
#define ODD_STRANGER_PATROL_TURN_CLAMP 0x20

/// Initial STALK primary-animation rate, 36 or 48 sixteenths of a frame per tick.
#define ODD_STRANGER_STALK_RATE 0x24

/// Blocked-sight stalking veer, 768 or 1024 angle units (4096 per turn).
///
/// Added or subtracted by side before limiting the root turn; the look stays unbiased.
#define ODD_STRANGER_STALK_TURN_BIAS 0x300

/// Unblended STALK forward step, 22 or 21 parent-coordinate units per tick.
///
/// Secondary blending selects a separate five-unit step.
#define ODD_STRANGER_STALK_STEP 0x16

/// WATCH animation-bank index, 19 in actor_401000 or the alert animation in actor_401800.
#define ODD_STRANGER_ANIM_WATCH 0x13

/// Retained low argument of scripted-dormant and grab-strike weapon puffs.
///
/// Stored in signed-halfword `spawnArgLo` as 0x100 or 0x200. The selected puff
/// recipe ignores this half; it is not an effect scale.
#define ODD_STRANGER_PUFF_UNUSED_LOW_ARG 0x100

/// Grab-pull puff record's coordinate index, 5 in actor_401000 or 1 in actor_401800.
///
/// Both models contain this part. The selected puff recipe uses the separate
/// call coordinate 5; this binding changes only the retained record coordinate.
#define ODD_STRANGER_GRAB_PULL_EFFECT_RECORD_PART 5

/// Mask selecting extra DOWN ticks from the LCG high word (0..15 or 0..7).
///
/// Added to `downFramesBase` before the signed-halfword countdown is stored.
#define ODD_STRANGER_DOWN_JITTER_MASK 0xF

/// Recent standard-hit damage selecting knockdown, 76 or 56 HP damage.
///
/// Outside downed/status poses, reaching this threshold selects a directional
/// fall instead of a flinch overlay; a critical hit also selects the fall.
#define ODD_STRANGER_KNOCKDOWN_DAMAGE_THRESHOLD 0x4C

/// Player resume mode when death releases a hold on the no-new-hit path.
///
/// The actor_401000 build preserves part 1's horizontal offset; actor_401800
/// bakes it into the root. The receiver must still be under scripted control
/// for either to act.
#define ODD_STRANGER_DEATH_PLAYER_RESUME_MODE PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET

/// The burst debris models the shared walking death spawns as parts 9 and 3: this build's own.
#define ODD_STRANGER_BURST_PART_3_MODEL _gStrangerOddBBurstHead
#define ODD_STRANGER_BURST_PART_9_MODEL _gStrangerBurstHand

/// Borrowed model source for walking death's second debris spawn, at state tick 5.
///
/// A `TmdSource*` published into the burst descriptor before spawning at part 1.
/// Each carrier must declare its selected model before expanding this binding,
/// and keep the source loaded through the debris task's lifetime.
#define ODD_STRANGER_WALK_DEATH_SECOND_DEBRIS_MODEL (&_gStrangerOddBBurstTorso)
#elif ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401800
#define ODD_STRANGER_HAS_EFFECT_OFFSET              0
#define ODD_STRANGER_WALK_FIRST_SOUND_CUE           0x10
#define ODD_STRANGER_WALK_SECOND_SOUND_CUE          0x16
#define ODD_STRANGER_BODY_RADIUS                    0x12C
#define ODD_STRANGER_EVASION_BODY_RADIUS            0x96
#define ODD_STRANGER_IN_PLACE_GRID_ENABLED          0
#define ODD_STRANGER_DASH_GRAB_REQUIRES_SIGHT       1
#define ODD_STRANGER_WALK_STEP                      7
#define ODD_STRANGER_PATROL_TURN_CLAMP              0x18
#define ODD_STRANGER_STALK_RATE                     0x30
#define ODD_STRANGER_STALK_TURN_BIAS                0x400
#define ODD_STRANGER_STALK_STEP                     0x15
#define ODD_STRANGER_ANIM_WATCH                     ODD_STRANGER_ANIM_ALERT
#define ODD_STRANGER_PUFF_UNUSED_LOW_ARG            0x200
#define ODD_STRANGER_GRAB_PULL_EFFECT_RECORD_PART   1
#define ODD_STRANGER_DOWN_JITTER_MASK               7
#define ODD_STRANGER_KNOCKDOWN_DAMAGE_THRESHOLD     0x38
#define ODD_STRANGER_DEATH_PLAYER_RESUME_MODE       PLAYER_ACTOR_END_SCRIPTED_DEFAULT
/// The burst debris models the shared walking death spawns as parts 9 and 3: this build's own.
#define ODD_STRANGER_BURST_PART_3_MODEL             _gStrangerOddBurstHead
#define ODD_STRANGER_BURST_PART_9_MODEL             _gStrangerOddBurstArm
#define ODD_STRANGER_WALK_DEATH_SECOND_DEBRIS_MODEL (&ODD_STRANGER_BURST_PART_9_MODEL)
#else
#error "ODD_STRANGER_BUILD must be ODD_STRANGER_BUILD_401000 or ODD_STRANGER_BUILD_401800"
#endif

#include "common.h"

#include "actors/actor.h"

#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "overlay.h"

/// Uniform model-root scale for yaw rebuilds in both variants, with 12 fractional bits.
enum { ODD_STRANGER_ROOT_SCALE = 0x1194 };

/// Values of `OddStrangerWork::state`: the index of the handler the per-frame
/// tick runs.
///
/// A knockdown keeps the side it fell on through the rest and the recovery:
/// a hit from the front leads through the `_BACK` states, one from behind
/// through the `_FRONT` states. The numbering is the Horned Stranger's
/// (actor_401300) as far as the two enemies share behavior.
enum {
    ODD_STRANGER_STATE_HIDDEN           = 0x00, // not drawn and not lockable; a spawn can start here, and the burst deaths end here
    ODD_STRANGER_STATE_PLAY_WALK        = 0x01, // loops the walk animation in place; never selected
    ODD_STRANGER_STATE_PLAY_RUN         = 0x02, // loops the run animation in place; never selected
    ODD_STRANGER_STATE_PLAY_DOWN        = 0x03, // holds the lying animation; never selected
    ODD_STRANGER_STATE_STATUS_HOLD      = 0x04, // twitches where it lies until the status buildup runs out
    ODD_STRANGER_STATE_FLINCH           = 0x05, // recoils from damage over time, then chases
    ODD_STRANGER_STATE_ALERT            = 0x06, // turns to the player and raises the combat alert
    ODD_STRANGER_STATE_CHASE            = 0x07, // runs at the player, then sidesteps or grabs; a stalking spawn stalks instead
    ODD_STRANGER_STATE_CIRCLE_DASH      = 0x08, // dashes round the player and grabs from behind; selected only by `TURN_AROUND`
    ODD_STRANGER_STATE_TURN_AROUND      = 0x09, // swings `turnYaw` round to `turnYawTarget`; selected only by `SLIDE`
    ODD_STRANGER_STATE_SIDESTEP         = 0x0A, // hops along `sidestepDir`, then chases
    ODD_STRANGER_STATE_GRAB             = 0x0B, // reaches for the player; a player in reach is held
    ODD_STRANGER_STATE_GRAB_PULL        = 0x0C, // places the held player and itself for the strike
    ODD_STRANGER_STATE_GRAB_STRIKE      = 0x0D, // damages the held player
    ODD_STRANGER_STATE_GRAB_RELEASE     = 0x0E, // lets go and steps back, then turns to the player again or sidesteps
    ODD_STRANGER_STATE_RISE_BACK        = 0x0F, // gets up after `FALL_BACK`
    ODD_STRANGER_STATE_RISE_FRONT       = 0x10, // gets up after `FALL_FRONT`
    ODD_STRANGER_STATE_DOWN             = 0x11, // lies where it fell until `stateTimer` runs out
    ODD_STRANGER_STATE_UNUSED_12        = 0x12, // has no handler and is never selected
    ODD_STRANGER_STATE_FALL_BACK        = 0x13, // hit from the front: staggers backward and falls
    ODD_STRANGER_STATE_FALL_FRONT       = 0x14, // hit from behind: falls forward
    ODD_STRANGER_STATE_DEATH_BURN       = 0x15, // burns away: the corpse-burn effect, then the model flattens and fades
    ODD_STRANGER_STATE_DORMANT          = 0x16, // idles until the player comes near or makes noise
    ODD_STRANGER_STATE_DORMANT_SCRIPTED = 0x17, // the same wait, entered by a room command, on an animation of its own
    ODD_STRANGER_STATE_PATROL           = 0x18, // walks between the two `patrolPoints`
    ODD_STRANGER_STATE_BACK_OFF         = 0x19, // faces the player, backs away and turns aside; never selected
    ODD_STRANGER_STATE_SLIDE            = 0x1A, // coasts forward by the shrinking `slideStep`; selected only by `CIRCLE_DASH`
    ODD_STRANGER_STATE_WATCH            = 0x1B, // stands through one animation with only its look following the player, then chases
    ODD_STRANGER_STATE_AMBUSH           = 0x1C, // waits where a room command put it: actor_401000 bends over, straightens and chases; actor_401800 stands unlockable and grabs once its look settles on the player
    ODD_STRANGER_STATE_DEATH_BURST      = 0x1D, // bursts into body parts where it stands
    ODD_STRANGER_STATE_STALK            = 0x1E, // walks at the player and grabs once close and facing
    ODD_STRANGER_STATE_REFALL_BACK      = 0x1F, // knocked down again while in `RISE_BACK`
    ODD_STRANGER_STATE_REFALL_FRONT     = 0x20, // knocked down again while in `RISE_FRONT`
    ODD_STRANGER_STATE_DEATH_BURST_WALK = 0x21, // walks a few steps, bursts and burns away
    ODD_STRANGER_STATE_COUNT                    // number of states, and of the handlers in `OddStrangerStateTable`
};

/// Values of `OddStrangerWork::animRequest` and `OddStrangerWork::blendRequest`.
///
/// A zero-filled block holds 0, on which the driver only advances the slots.
/// The blend rig is never asked to blend in.
enum {
    ODD_STRANGER_ANIM_REQUEST_BLEND   = 1, // seek the slots to the animation, blending over the frames the transition table gives
    ODD_STRANGER_ANIM_REQUEST_RESET   = 2, // restart the slots on the animation
    ODD_STRANGER_ANIM_REQUEST_PLAYING = 3  // the request has been applied
};

/// Animation-set indices used by the shared state and cue handlers.
///
/// These select each carrier's bank, not state-table slots. The status clips
/// alias the ordinary lying clips; the script slots are assigned by message
/// selector, with their contents not established by this interface.
enum {
    ODD_STRANGER_ANIM_WALK              = 2,
    ODD_STRANGER_ANIM_RUN               = 3,
    ODD_STRANGER_ANIM_GRAB_REACH        = 4,
    ODD_STRANGER_ANIM_GRAB_PULL         = 5,
    ODD_STRANGER_ANIM_GRAB_STRIKE       = 6,
    ODD_STRANGER_ANIM_GRAB_RELEASE      = 7,
    ODD_STRANGER_ANIM_RISE_BACK         = 8,
    ODD_STRANGER_ANIM_ALERT             = 9,
    ODD_STRANGER_ANIM_DOWN_BACK         = 11,
    ODD_STRANGER_ANIM_DOWN_FRONT        = 12,
    ODD_STRANGER_ANIM_FLINCH            = 13,
    ODD_STRANGER_ANIM_DORMANT_SCRIPTED  = 16,
    ODD_STRANGER_ANIM_SIDESTEP_NEGATIVE = 20,
    ODD_STRANGER_ANIM_SIDESTEP_POSITIVE = 21,
    ODD_STRANGER_ANIM_STATUS_BACK       = 23,
    ODD_STRANGER_ANIM_STATUS_FRONT      = 24,
    ODD_STRANGER_ANIM_REFALL_FRONT      = 25,
    ODD_STRANGER_ANIM_SCRIPT_0          = 34,
    ODD_STRANGER_ANIM_SCRIPT_1          = 35,
    ODD_STRANGER_ANIM_SCRIPT_2          = 36,
    ODD_STRANGER_ANIM_SCRIPT_3          = 37,
    ODD_STRANGER_ANIM_SCRIPT_4          = 39
};

/// Stored clip-bank capacity and the independent crossfade index domain.
enum {
    ODD_STRANGER_ANIM_SET_CAPACITY     = 46,
    ODD_STRANGER_TRANSITION_ANIM_COUNT = 45
};

/// The model root is placed separately; secondary rotation mixing covers parts 1..10.
enum {
    ODD_STRANGER_FIRST_ANIMATED_SLOT = 1,
    ODD_STRANGER_LAST_BLENDED_SLOT   = 10
};

/// Stop distance in parent-coordinate units; player detection adds its 150-unit margin.
enum { ODD_STRANGER_MOVE_STOP_DISTANCE = 300 };

/// Pose and hit-effect settings shared by the pull and strike of a held player.
enum {
    ODD_STRANGER_GRAB_PITCH            = -ACTOR_TRANSFORM_ANGLE_TURN / 32, // 4096 units per turn
    ODD_STRANGER_GRAB_HIT_EFFECT_KEY   = 0x1001,                           // Weapon-property row 1; bit 12 is ignored by the effect lookup
    ODD_STRANGER_GRAB_EXTRA_PUFF_COUNT = 2                                 // Puffs added after the weapon-puff recipe's initial spawns
};

/// Work block of the Odd Stranger task, in both of its packages.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`; the
/// exit callback unlinks its three collision bodies. It holds the state
/// machine, both animation rigs and their driver's state, the bodies with
/// their contact records, storage for the model's matrices, and the tuning
/// the spawn argument selects.
///
/// The two builds lay it out differently. Only actor_401000's keeps
/// `effectOffset`, so every later member sits 8 bytes lower in actor_401800's,
/// and `grabCooldown` is before the bodies in actor_401800's and after
/// `commandBytes` in actor_401000's. The unnamed members carry their offsets
/// in actor_401000's build.
///
/// Angles are 4096ths of a turn and positions are in the root coordinate's
/// parent space unless a field says otherwise. Animation ids index the
/// package's animation bank; rates are sixteenths of a frame per tick.
typedef struct {
    s16              state;           // `ODD_STRANGER_STATE_*`
    s16              prevState;       // `state` the tick last ran; -1 makes the next tick enter `state` afresh
    s16              stateEntered;    // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    s16              stateTimer;      // counter of the state: most count ticks up from 0, `DOWN` counts down, `PATROL` counts ticks the room grid held it back, `DORMANT_SCRIPTED` keeps 1 once its sound is queued
    s16              exitCounter;     // second counter of `CHASE`, `CIRCLE_DASH` and `STALK`, each of which ends at a limit: ticks chased, ticks the room grid pushed the dash back, ticks stalked with the player in sight
    byte             field_A[0x2];    // never accessed
    ActorPatrolPoint patrolPoints[2]; // the spawn position and a point 2000 units ahead along the spawn facing
    s16              patrolTarget;    // index into `patrolPoints` of the end being walked toward
    s16              placedYaw;       // heading of the root after the last placement message; never read
    byte             field_18[0x4];   // never accessed
    ActorAnimRig19   rig;             // playback of the model's parts; slot 1's status and frame time the states
    ActorAnimRig19   blend;           // second playback of the same model, mixed into parts 1 to 10 while `blendActive`
    s32              grabAnimFrame;   // slot 1's frame on each tick of `GRAB_STRIKE`; never read
    s16              animRequest;     // `ODD_STRANGER_ANIM_REQUEST_*` for `rig`
    s16              blendActive;     // 1 while `blend`'s animation is mixed in; cleared when its slot 1 reaches a boundary
    s16              appliedAnim;     // animation `rig` was last started on
    s16              animId;          // animation requested of `rig`
    u16              animFrames;      // ticks since `animRequest` was last applied; never read
    s16              animRate;        // playback rate of `rig`'s slots, which play 3 slower while `blendActive`
    s16              chaseRate;       // `animRate` of the chase, dormant and rise states, 0xE to 0x12 by the place index; also scales the chase step
    s16              blendRequest;    // `ODD_STRANGER_ANIM_REQUEST_*` for `blend`; only `RESET` is requested
    s16              blendAnimId;     // animation requested of `blend`
    u16              blendRate;       // playback rate of `blend`'s slots, 0x30 from each restart
    s16              blendWeight;     // share of `blend`'s pose in the mix, of 0x1000; 0x800 from each restart
    s16              lookYawTarget;   // bearing to what the state faces, relative to the facing
    s16              lookYaw;         // eased toward `lookYawTarget` by 0x100 a tick; within +-0x400, parts 5 and 2 turn by 2/3 and 1/2 of it
    byte             field_8B2[0x2];  // never accessed
    s32              lastCueFrame;    // slot 1's frame when a cue last fired, so a held frame fires once; 0 off a cue frame
    EffectSpawnArg   effectArg;       // argument record of the hit and grab effects, hung off a model part
#if ODD_STRANGER_HAS_EFFECT_OFFSET
    SVECTOR effectOffset;             // offset of the last hit effect from its part, or the direction a burst part was thrown in
#endif
    byte field_8C8[0x2];              // never accessed
#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401800
    u8 grabCooldown;                  // ticks before another grab may start, 10 from each `GRAB`; the pursuit states count it down
#else
    byte field_8CA; // never accessed
#endif
    byte                  field_8CB[0x5];    // never accessed
    WorldCollisionBody    hitBody;           // sphere at part 2; takes the hits and is pushed off other bodies
    WorldCollisionContact hitContacts[12];   // contacts of `hitBody`; also the enemy's hit records
    WorldCollisionBody    gridBody;          // sphere on the root, 0xAC above it, that the room grid pushes the root with
    WorldCollisionContact gridContacts[12];  // contacts of `gridBody`
    WorldCollisionBody    attackBody;        // sphere on part 6, linked where the Horned Stranger's striking body is; never enabled
    WorldCollisionContact attackContacts[1]; // the one contact of `attackBody`
    MATRIX                lightMtx;          // storage for the model's `TmdObject::lightMtx`
    MATRIX                colorMtx;          // storage for the model's `TmdObject::colorMtx`
    MATRIX                savedColorMtx;     // `colorMtx` as `DORMANT` found it; never read
    s16                   hitCooldown;       // ticks before another hit is taken; set from the hit's id parameter 2
    s16                   recentDamage;      // damage taken since the hits last paused; `ODD_STRANGER_KNOCKDOWN_DAMAGE_THRESHOLD` of it knocks the actor down
    s16                   recentDamageTimer; // ticks before `recentDamage` is forgotten, 5 from each hit; a hit landing while it runs pushes the root back a quarter as far
    byte                  field_BEE[0x2];    // never accessed
    SVECTOR               sidestepDir;       // unit direction of the sidestep, to one side of the bearing to the player
    SVECTOR               grabStartPos;      // root position as `GRAB` began; the root returns there when `GRAB` or `GRAB_STRIKE` ends
    s16                   turnYaw;           // heading the turn-around holds the root at, moved 0x89 a tick
    s16                   turnYawTarget;     // heading the turn-around ends on: the facing plus twice the bearing to the player
    s16                   slideStep;         // forward step of `CIRCLE_DASH` (eight times `animRate`; halved while `blendActive`, 2 once the room grid has pushed back) and of `SLIDE`, 10 less each tick
    s16                   dashRateStep;      // change of `animRate` per tick of `CIRCLE_DASH`: 8 up to a rate of 0x18, -1 down to 0x12, then 0
    s16                   sidestepSide;      // side the next sidestep takes (1 or -1), flipped by each; 0 draws one at random. `STALK` veers to it while its sight is blocked
    s16                   sidestepStep;      // length of the sidestep's step, 0xDE; halved while `blendActive` and by each push of the room grid
    s16                   releaseStep;       // step of `GRAB_RELEASE` along the facing, -0x78; halved by each push of the room grid
    byte                  field_C0E[0x2];    // never accessed
    u16                   downFramesBase;    // ticks `DOWN` lasts, before a random 0..15 more (0..7 in actor_401800); first value of the variant record
    s16                   sidestepAngle;     // angle between the bearing to the player and `sidestepDir`; second value of the variant record
    s16                   sidestepDelay;     // `stateTimer` count `CHASE` must pass, plus half `sidestepCount`, before it sidesteps; third value of the variant record
    u16                   noticeRadius;      // distance at which the dormant states and `PATROL` notice the player; fourth value of the variant record
    u8                    commandBytes[3];   // first three bytes of the last actor command received; never read
#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401000
    u8 grabCooldown;                         // ticks before another grab may start, 10 from each `GRAB`; the pursuit states count it down
#else
    byte field_C1B; // never accessed
#endif
    Task*   childTask0;        // killed by the exit callback when set; nothing sets it
    Task*   childTask1;        // killed by the exit callback when set; nothing sets it
    s16     dashCount;         // `CIRCLE_DASH` entries since the last hit, chase or stalk; `TURN_AROUND` grabs only after two
    s16     sidestepCount;     // sidesteps since the last hit or grab; the first is thrown 0x171 wider
    s16     playerHeld;        // 1 while the player is in a hold this enemy started
    byte    field_C2A[0x2];    // never accessed
    SVECTOR bodyPosHistory[7]; // ring of part 2's view-space position on the last seven ticks; a sidestep aims the enemy's target point at the oldest
    byte    field_C64[0x18];   // never accessed
    s16     bodyPosCursor;     // index of the next entry of `bodyPosHistory` to write
} OddStrangerWork;
STATIC_ASSERT_SIZEOF(OddStrangerWork, ODD_STRANGER_HAS_EFFECT_OFFSET ? 0xC80 : 0xC78);

/// The Odd Stranger's state handlers, indexed by `OddStrangerWork::state`.
///
/// Each package's tick initializes a stack table with its own handlers before
/// calling the current state's entry. The call is unconditional, so the `NULL` entry of
/// `ODD_STRANGER_STATE_UNUSED_12` marks a state the actor must never be put in.
typedef struct {
    TaskFunc handlers[ODD_STRANGER_STATE_COUNT]; // Handler of each state, taking the actor's task
} OddStrangerStateTable;
STATIC_ASSERT_SIZEOF(OddStrangerStateTable, ODD_STRANGER_STATE_COUNT * sizeof(TaskFunc));

/// Allocation holding the placement a grab hands the player.
///
/// `placement` is the payload of `GAME_ACTOR_MESSAGE_PLACE`, kept in static
/// storage and lent to the player for the length of the dispatch, which
/// consumes it. The grab fills it in as it takes hold: the player keeps their
/// position and takes the bearing to the enemy as their yaw, and the enemy
/// stands 1000 units away along that bearing.
///
/// In both packages eight zero bytes separate the record from the button-press
/// record that follows. No access to them is recovered, so whether they are
/// trailing fields of this object or a separate unreferenced variable is
/// unproven; they stay in this allocation only to keep the data after it at
/// its address.
typedef struct {
    ActorTransform placement;     // Player's own position, with the yaw of the bearing from the player to the enemy and no pitch or roll
    u8             unknown_18[8]; // Zero in the image; no access established and role unproven
} OddStrangerTransformStorage;
STATIC_ASSERT_SIZEOF(OddStrangerTransformStorage, 0x20);

#include "main/task_types.h"

static void _oddStrangerTickBlendedAnimation(Task* task);
static void _oddStrangerDriveAnimation(Task* task);
static s32  _oddStrangerPlayMessage(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unused);

static s32 _oddStrangerTakeAnimationSound(OddStrangerWork* work);

static void _oddStrangerExit(Task* task);
static s32  _oddStrangerApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unused);
static void _oddStrangerGrabRelease(Task* task);
static void _oddStrangerBackOff(Task* task);
static void _oddStrangerSidestep(Task* task);
static void _oddStrangerSlide(Task* task);
static void _oddStrangerAlert(Task* task);
static void _oddStrangerRiseBack(Task* task);
static void _oddStrangerFlinch(Task* task);
static void _oddStrangerPlayRun(Task* task);
static void _oddStrangerPlayDown(Task* task);
static void _oddStrangerPlayWalk(Task* task);
static void _oddStrangerDeathBurn(Task* task);
static void _oddStrangerWatch(Task* task);

static void _oddStrangerSpawnHitEffect(Task* task, s16 hitYaw, s32 attackKey);
static void _oddStrangerDown(Task* task);
static void _oddStrangerDormantScripted(Task* task);
static void _oddStrangerGrabPull(Task* task);
static void _oddStrangerGrabStrike(Task* task);
static void _oddStrangerStatusHold(Task* task);
static void _oddStrangerTurnAround(Task* task);

static void _oddStrangerCircleDash(Task* task);
static void _oddStrangerPatrol(Task* task);

/// Returns whether the XZ offset reaches or exceeds the radius from its origin.
///
/// `offset` and `radius` use the same model-coordinate units; Y is ignored.
/// Signed halfwords are squared in s32, so a negative radius has the same
/// result as its magnitude. Requires one free `OverlayRangeScratch` on the
/// initialized scratch stack, restored before returning 0 or 1. The sum of
/// the squared offsets must fit s32 (the two -32768 extremes together do not).
static inline s32 _oddStrangerOutOfRange(const SVECTOR* offset, s16 radius)
{
    OverlayRangeScratch* savedCursor;
    OverlayRangeScratch* scratch;
    s32                  outside;

    savedCursor                               = SCRATCH_STACK_CURSOR(OverlayRangeScratch);
    scratch                                   = savedCursor - 1;
    scratch->dx                               = offset->vx;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = scratch;
    scratch->dz                               = offset->vz;
    scratch->radius                           = radius;
    scratch->dx                              *= scratch->dx;
    scratch->dz                              *= scratch->dz;
    scratch->radius                          *= scratch->radius;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = savedCursor;
    outside                                   = scratch->dx + scratch->dz >= scratch->radius;
    return outside;
}

static void _oddStrangerStalk(Task* task);

static void _oddStrangerWalkingDeath(Task* task);

static void _oddStrangerTakeHit(Task* task);

/// Pushes the model root away from player/companion and enemy-body contacts.
///
/// Requires a live TMD task, readable `contactCount` contacts (0..12), parts
/// 0 and 1, and one free body-push scratch block plus any callee scratch space.
/// Stops at the first zero key. Each push is measured from part 1's composed
/// position. The actor_401000 build removes the room view basis, caps at 107
/// units and uses an arithmetic right shift by two; actor_401800 uses the
/// world-offset resolver, caps at 150 units and divides by two with truncation
/// toward zero. X/Z are
/// added to the root's parent-space translation; inputs are borrowed for the call.
/// Composed positions and separations must fit signed halfwords, and squared
/// X/Z sums must fit s32. Requires the active grid view basis in actor_401000 and
/// the composed global view basis in actor_401800; neither basis is refreshed here.
/// Returns 1 on seeing an eligible body even if its push is zero; freeze or
/// view-ready state returns 0 without changing the model. Releases scratch
/// storage before reading the result, while its bytes remain intact.
static s32 _oddStrangerApplyBodyPushback(Task* task, const WorldCollisionContact* contacts, s16 contactCount);

#endif /* SRC_SHARED_ODD_STRANGER_H */
