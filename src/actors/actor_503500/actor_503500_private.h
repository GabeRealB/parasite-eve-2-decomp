#ifndef SRC_ACTORS_ACTOR_503500_ACTOR_503500_PRIVATE_H
#define SRC_ACTORS_ACTOR_503500_ACTOR_503500_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"
#include "actors/actor_503500.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/session_types.h"
#include "main/task_types.h"

/// Commands shared by the intro script and its slab-model slider tasks.
///
/// Stop clears both path and shake timer. Path commands restart the selected
/// 360-update path; shake leaves the model still for a finite countdown.
enum {
    ACTOR_503500_SLIDER_COMMAND_STOP        = 0,
    ACTOR_503500_SLIDER_COMMAND_PATH_FIRST  = 1,
    ACTOR_503500_SLIDER_COMMAND_PATH_SECOND = 2,
    ACTOR_503500_SLIDER_COMMAND_SHAKE       = 3,
};

/// Boss scene actions; the receiver ignores the command record's context tags.
enum {
    ACTOR_503500_BOSS_COMMAND_RESUME                 = 0,
    ACTOR_503500_BOSS_COMMAND_HOLD                   = 1,
    ACTOR_503500_BOSS_COMMAND_COLLAPSE               = 2,
    ACTOR_503500_BOSS_COMMAND_EXIT                   = 3,
    ACTOR_503500_BOSS_COMMAND_BEGIN_PHASE_TRANSITION = 4,
    ACTOR_503500_BOSS_COMMAND_RESTORE_ROOT           = 5,
};

/// Scripted tentacle actions; context tags are ignored and commands are borrowed.
///
/// Stop clears velocity; move sets the first leg's velocity; collapse sets
/// the second leg's velocity and squash motion; exit invokes immediate teardown.
enum {
    ACTOR_503500_TENTACLE_COMMAND_STOP     = 0,
    ACTOR_503500_TENTACLE_COMMAND_MOVE     = 1,
    ACTOR_503500_TENTACLE_COMMAND_COLLAPSE = 2,
    ACTOR_503500_TENTACLE_COMMAND_EXIT     = 3,
};

/// Animation turn-limit classes before penalties for missing boss parts.
///
/// Limits are 7, 6 and 1 angle units per actor tick; angles use 4096 per turn.
enum {
    ACTOR_503500_TURN_RATE_NORMAL  = 0,
    ACTOR_503500_TURN_RATE_REDUCED = 1,
    ACTOR_503500_TURN_RATE_MINIMUM = 2,
};

/// Height and relative-bearing bands accepted by each attached target slot.
///
/// Heights use player room Y: upper below -3000, middle -3000..-1000,
/// lower at least -999. Forward is |bearing| < 1024, rear is the rest;
/// strict side windows (768,1152) and (-1152,-768) override those bands.
/// Bearings use 4096 per turn relative to the boss's yaw.
enum {
    ACTOR_503500_TARGET_HEIGHT_UPPER          = 1 << 0,
    ACTOR_503500_TARGET_HEIGHT_MIDDLE         = 1 << 1,
    ACTOR_503500_TARGET_HEIGHT_LOWER          = 1 << 2,
    ACTOR_503500_TARGET_BEARING_FORWARD       = 1 << 3,
    ACTOR_503500_TARGET_BEARING_REAR          = 1 << 4,
    ACTOR_503500_TARGET_BEARING_POSITIVE_SIDE = 1 << 5,
    ACTOR_503500_TARGET_BEARING_NEGATIVE_SIDE = 1 << 6,
};

struct Actor503500Work;

/// An attack of the boss: a routine the attack state calls once a frame, from
/// the frame it is picked until it reports a result.
///
/// `task` is the boss's task and `work` the work block `task->work` points at.
/// An attack keeps its progress in the block's `attackPhase`, `attackFrames`
/// and `attackSlot`. The first two are zeroed on every change of state, so an
/// attack always starts at phase 0; `attackSlot` is not, and holds the last
/// attack's slot until this one commands its own.
///
/// Returns 0 while the attack is still running. Any other result ends it and
/// is stored as `attackDelay`, the count of frames the idle state waits out
/// before the next pick. An attack may end on its first call, having commanded
/// nothing because its slots were not ready; most then return 1, so the next
/// pick follows almost at once. An attack interrupted by a change of state is
/// not called again, and the next visit to the attack state picks afresh.
typedef s32 (*Actor503500AttackFn)(Task* task, struct Actor503500Work* work);

/// One entry of a weighted attack list, which a NULL `attack` ends.
///
/// The boss keeps one list for each band of the player's height and bearing,
/// in two sets it changes between once in the fight. Picking walks the list
/// with a random byte, taking the first entry at which the weights summed so
/// far reach it. A list's weights may total less than 255: a roll above the
/// total picks nothing, and the boss idles briefly instead.
typedef struct {
    Actor503500AttackFn attack; // Attack this entry picks; NULL in the entry closing the list
    u32                 weight; // Width of the entry's share of the 0-255 roll
} Actor503500AttackChoice;
STATIC_ASSERT_SIZEOF(Actor503500AttackChoice, 0x8);

/// A signed 16.16 fixed-point XYZ vector with fixed-point and SDK vector views.
///
/// The package keeps its sub-unit motion in these: Euler angles, velocities
/// and positions that are stepped a fraction of a unit per frame. `fixed`
/// gives each component's whole word and its halves, so a caller can apply
/// the signed integer half to a coordinate or an `SVECTOR` and keep the
/// fraction as the carry. `vector` is the same three words as the SDK type,
/// for matrix routines that transform the vector in place; a 16.16 input
/// gives a 16.16 result. Changing views performs no conversion.
typedef union {
    VECTOR vector;  // SDK XYZ view; the SDK's fourth word is unused
    struct {
        Fixed16 vx; // X component in signed 16.16 units
        Fixed16 vy; // Y component in signed 16.16 units
        Fixed16 vz; // Z component in signed 16.16 units
    } fixed;        // Whole word, fraction and signed integer half of each component
} Actor503500FixedVector;
STATIC_ASSERT_SIZEOF(Actor503500FixedVector, 0x10);

/// Descriptor indices of the boss and the attached enemies these attacks command.
enum {
    ACTOR_503500_SLOT_BODY                 = 0,
    ACTOR_503500_SLOT_PINK_FLASH_EMITTER   = 1,
    ACTOR_503500_SLOT_LARGE_CHAIN_0        = 2,
    ACTOR_503500_SLOT_LARGE_CHAIN_1        = 3,
    ACTOR_503500_SLOT_LARGE_ORB_EMITTER_0  = 4,
    ACTOR_503500_SLOT_LARGE_ORB_EMITTER_1  = 5,
    ACTOR_503500_SLOT_REAR_PART            = 6,
    ACTOR_503500_SLOT_CHAIN_BASE_0         = 7,
    ACTOR_503500_SLOT_CHAIN_BASE_1         = 8,
    ACTOR_503500_SLOT_SMALL_ORB_EMITTER    = 9,
    ACTOR_503500_SLOT_ARM_0                = 10,
    ACTOR_503500_SLOT_ARM_1                = 11,
    ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER = 12,
    ACTOR_503500_SLOT_LUNGING_CHAIN_0      = 13,
    ACTOR_503500_SLOT_LUNGING_CHAIN_1      = 14,
    ACTOR_503500_SLOT_LUNGING_CHAIN_2      = 15,
    ACTOR_503500_SLOT_LUNGING_CHAIN_3      = 16,
};

/// Requests delivered to a slot enemy in `Task::killCountdown`.
///
/// Zero means no request. The receiving task consumes a request in its state
/// handler; slot 0 keeps its request in the boss work's `selfAttackCommand`.
enum {
    ACTOR_503500_SLOT_COMMAND_NONE          = 0,
    ACTOR_503500_SLOT_COMMAND_ATTACK        = 2, // Start the slot's attack
    ACTOR_503500_SLOT_COMMAND_BECOME_TARGET = 8, // Expose an arm/emitter, or unfold a newly split lunging chain
    ACTOR_503500_SLOT_COMMAND_REGROW        = 9, // Start a newly spawned lunging chain's regrowth
};

/// What the boss is doing, as held in `Actor503500Work::state`.
typedef enum {
    ACTOR_503500_STATE_IDLE      = 0, // Waits out the delay before its next attack
    ACTOR_503500_STATE_ATTACK    = 1, // Picks an attack and runs it until it reports a result
    ACTOR_503500_STATE_PART_LOST = 2, // Plays the recoil animation after one of its slot enemies is destroyed
    ACTOR_503500_STATE_STUNNED   = 3, // Held by a build-up reaction until it wears off
    ACTOR_503500_STATE_DEFEATED  = 4, // Health exhausted: stops being a target and reports to the room
    ACTOR_503500_STATE_HELD      = 5, // Entered by actor command 1; does nothing of its own
    ACTOR_503500_STATE_SCRIPTED  = 6, // Entered by actor command 4; runs the sequence that command starts
    ACTOR_503500_STATE_COLLAPSE  = 7, // Entered by actor command 2; squashes flat and burns away
} Actor503500State;

/// Work block of the boss, the package's root enemy.
///
/// The boss is a body that turns and walks toward the player inside a fixed
/// box, and a set of separately targetable enemies riding on its model. Each
/// of those occupies one slot, numbered by its entry in the package's enemy
/// table; slot 0 is the boss itself. The boss attacks by commanding a slot:
/// it marks the slot busy, gives it a cooldown, and waits for the slot to
/// report that it is at rest again. A slot whose enemy is destroyed stays
/// empty until something respawns it.
///
/// The block opens with the rig that plays the body's animations. One static
/// instance exists; `Task::work` of the boss task points at it, and the slot
/// enemies reach it through the accessors this package exports.
typedef struct Actor503500Work {
    ActorAnimRig20        rig;                                   // Playback storage of the body model; slots 1 to 19 are driven
    MATRIX                lightMtx;                              // Light matrix the body model is lit with
    MATRIX                colorMtx;                              // Colour matrix the body model is lit with
    GfxCoord              savedRootCoord;                        // Root coordinate as it was when actor command 4 arrived; command 5 puts it back
    GfxCoord              part5Parent;                           // Scaled copy of model part 4 that part 5 hangs from while its `scaledParts` bit is set
    GfxCoord              part11Parent;                          // Scaled copy of model part 10 that part 11 hangs from while its `scaledParts` bit is set
    VECTOR                part5Scale;                            // Scale applied to `part5Parent`; 0x1000 is 1.0
    VECTOR                part11Scale;                           // Scale applied to `part11Parent`; 0x1000 is 1.0
    VECTOR                part16Scale;                           // Scale applied to model part 16 in place; 0x1000 is 1.0
    WorldCollisionBody    body;                                  // Collision sphere of the boss's own target, carried on model part 3
    WorldCollisionContact contacts[8];                           // Contact table of `body`, also the boss enemy's hit records
    VECTOR                velocity;                              // Per-frame step of `position` along the facing, 16.16; only X and Z are used
    VECTOR                position;                              // Root translation in 16.16; its integer halves are what the root coordinate gets
    VECTOR                previousTranslation;                   // Root coordinate's translation before this frame's walk, in whole units
    EffectSpawnArg        hitEffect;                             // Record hit effects on the boss are spawned with, bound to model part 3
    Enemy*                enemies[ACTOR_503500_SLOT_COUNT];      // Enemy in each slot, NULL once destroyed; entry 0 is the boss's own
    s16                   slotBusy[ACTOR_503500_SLOT_COUNT];     // 1 from the frame a slot is commanded until it reports being at rest
    s16                   slotCooldown[ACTOR_503500_SLOT_COUNT]; // Frames before a slot may be commanded again; counts down only while idle
    /// Things that have happened once in the fight.
    ///
    /// Bits 0, 1 and 2 record that slot 10, 11 or 12 was told to retire
    /// because slot 4, 5 or 1 was lost. Bit 3 records that the room has been
    /// sent its actor event for the slots lost so far; it also selects the
    /// second attack table.
    u32                 progressFlags;
    Actor503500AttackFn runningAttack;        // Attack picked for this visit to the attack state
    MATRIX              unscaledRotation;     // Root rotation saved on entering the collapse; only the rotation is kept
    Task*               scriptedEffectTask;   // Effect task the scripted sequence spawns on part 3 and ends itself; NULL if none
    s32                 walkSpeed;            // Speed along the facing, 16.16 units per frame; negative walks backward
    s32                 walkSpeedLimit;       // Top walk speed, 16.16; a thirty-second of it is the per-frame acceleration
    s32                 turnSpeed;            // Yaw rate in 16.16 angle units per frame
    s32                 scaledParts;          // Bit N set while model part N (5, 11 or 16) is being scaled
    s16                 state;                // An `Actor503500State`
    u16                 stunAnimationTimer;   // Frames until the stunned state restarts its animation
    s16                 hitCooldown;          // Frames during which further hits on the boss are ignored
    s16                 yaw;                  // Facing, in 4096ths of a turn
    s16                 targetYaw;            // Facing to turn toward: the bearing to the player plus `targetYawOffset`
    s16                 playerBearing;        // Bearing to the player relative to `yaw`, in [-0x800, 0x800)
    s16                 stateFrames;          // Frames counted by the current state's step
    s16                 attackFrames;         // Frames an attack has waited on its slot
    s16                 selfAttackFrames;     // Frames counted by the current phase of the boss's own attack
    s16                 attackSlot;           // Slot the running attack commanded
    byte                pad_7C4[0x4];         // No access found; role unproven
    u16                 randomRoll;           // High half of the random state, drawn once a frame
    s16                 attackDelay;          // Frames the idle state waits before attacking; an attack's result reloads it
    s16                 targetableDelay;      // Frames until `targetablePending` is applied; 0 when nothing is pending
    s16                 collapseScaleY;       // Vertical scale during the collapse, 0x1000 down to 0x200
    s16                 savedYaw;             // `yaw` saved and restored with `savedRootCoord`
    s16                 targetYawOffset;      // Added to the player's bearing to give `targetYaw`; 0 faces the player
    s8                  animationStarted;     // 1 once the slots have been played and ticked since the rig was last bound
    s8                  animationId;          // Animation last requested, -1 before the first
    s8                  animationSourceIndex; // Set-table index the rig is bound to, -1 before the first request
    byte                pad_7D7[0x1];         // No access found; role unproven
    s8                  advancing;            // 1 while the walk accelerates toward its limit, 0 while it slows to a stop
    s8                  bufferFreeCountdown;  // Frames until the model's buffers are freed after it is hidden; negative when idle
    u8                  stateStep;            // Step within the current state, restarted on every state change
    u8                  attackPhase;          // Phase of the running attack (0 issue the command, 1 wait for the slot)
    s8                  heightBand;           // Band of the player's Y (0 most negative to 2 least), switched with hysteresis
    s8                  previousHeightBand;   // `heightBand` before the latest attack was picked
    s8                  bearingBand;          // Band of the `playerBearing` magnitude the latest attack was picked from
    s8                  previousBearingBand;  // `bearingBand` before that pick
    s8                  selfAttackCommand;    // Command given to slot 0; nonzero while the boss runs its own attack
    s8                  selfAttackPhase;      // Phase of the boss's own attack
    s8                  targetablePending;    // Whether the boss becomes a target (1) or stops being one (0) when the delay ends
    s8                  rootCoordRestored;    // 1 once actor command 5 has put `savedRootCoord` back
    s8                  controlPaused;        // 1 while scene actor control 1 (paused, still drawn) has been applied
    s8                  controlHidden;        // 1 while scene actor control 2 (hidden) has been applied
    s8                  defeated;             // 1 once a hit has exhausted the boss's health
    s8                  mutedForMenu;         // 1 while the boss's sounds are muted for a pending menu
} Actor503500Work;
STATIC_ASSERT_SIZEOF(Actor503500Work, 0x7E8);

/// Scratch block of laying a chain's parts along a polyline, on the scratch
/// stack for one walk from the chain's root to its tip.
///
/// Each of the package's two chains samples a curve into world points and
/// then aims its model parts along them. The walk carries the world rotation
/// of the part it has reached, takes the step to the next point into that
/// part's frame, and gives the next part a rotation whose Z axis lies along
/// the step and a translation at the step's end. Lengths are in model units;
/// rotations and `direction` use 4096 for 1.0. The block is not cleared when
/// it is reserved.
typedef struct {
    SVECTOR segment;           // Step from the current point to the next, in world axes
    SVECTOR up;                // Y-axis hint the next part's rotation is built with, (0, 4096, 0) in the current part's frame
    SVECTOR direction;         // `localSegment` normalised: the next part's Z axis
    SVECTOR parentTranslation; // World position of the chain root's parent, written while the starting `worldRotation` is composed and never read
    VECTOR  localSegment;      // `segment` in the current part's frame, which the next part's translation is taken from
    MATRIX  basis;             // The next part's rotation before it is renormalised; only the walk that pulses the part lengths uses it
    MATRIX  inverseRotation;   // Transpose of `worldRotation`, taking world axes into the current part's frame
    MATRIX  worldRotation;     // World rotation of the current part; starts as that of the root's parent and takes in each part's own rotation in turn
} Actor503500ChainScratch;
STATIC_ASSERT_SIZEOF(Actor503500ChainScratch, 0x90);

extern AnimationSet gBrahmanIdleClip;

extern AnimationSet gBrahmanLeftArmStrikeClip;

extern AnimationSet gBrahmanRightArmStrikeClip;

extern AnimationSet gBrahmanOrangeFlashWindupClip;

extern AnimationSet gBrahmanOrangeFlashChargeClip;

extern AnimationSet gBrahmanOrangeFlashStrikeClip;

extern AnimationSet gBrahmanOrangeFlashRecoverClip;

extern AnimationSet gBrahmanOrangeFlashFinishClip;

extern AnimationSet gBrahmanLargeOrbAttackClip;

extern AnimationSet gBrahmanChainBaseAttackClip;

extern AnimationSet gBrahmanSmallOrbAttackClip;

extern AnimationSet gBrahmanYellowFlashAttackClip;

extern AnimationSet gBrahmanYellowFlashRecoverClip;

extern AnimationSet gBrahmanRecoilClip;

extern AnimationSet gBrahmanPinkFlashAttackClip;

extern AnimationSet gBrahmanPinkFlashRecoverClip;

extern AnimationSet gBrahmanPhaseTransitionClip;

extern AnimationSet gBrahmanIntro1Clip;

extern AnimationSet gBrahmanIntro2Clip;

/// Damage table borrowed while a ballistic shot packs its attack sphere's key.
///
/// Spawn kind selects row 0 (chain shot, power 20) or 1 (small orb, power 25);
/// both apply paralysis. Carrier element 0 exports the private rows; row bounds
/// are unchecked. Linking copies the packed key and retains no row pointer.
extern DamageAttack* gActor503500BallisticShotAttackTables[1];

/// Damage table borrowed while a lingering shot packs its attack sphere's key.
///
/// Spawn kind selects row 0 (large orb, poison) or 1 (chain-base projectile,
/// paralysis); both have power 15. Carrier element 0 exports the private rows;
/// bounds are unchecked. Linking retains only the packed key.
extern DamageAttack* gActor503500LingeringShotAttackTables[1];

/// Flash damage-table indices: pink capsule and player-centred yellow sphere.
enum {
    ACTOR_503500_FLASH_ATTACK_PINK   = 0,
    ACTOR_503500_FLASH_ATTACK_YELLOW = 1,
    ACTOR_503500_FLASH_ATTACK_COUNT  = 2,
};

/// Single-row damage tables borrowed by the pink and yellow flash attacks.
///
/// Indices 0/1 select pink/yellow; each receiver packs row 0 while linking its body.
/// Keep this overlay loaded for the table and rows' lifetimes.
extern DamageAttack* gActor503500FlashAttacks[ACTOR_503500_FLASH_ATTACK_COUNT];

/// Damage table borrowed while an orange-flash capsule packs its attack key.
///
/// Carrier element 0 exports the private row. Only damage row 0 is valid:
/// power 120 with body-blast hit presentation. Linking retains only the packed
/// key; neither the carrier nor damage row is retained by the collision body.
extern DamageAttack* gActor503500OrangeFlashAttackTables[1];

/// Shared enemy tuning indexed by the boss or attached part's spawn slot (0..16).
///
/// Aligned with `gActor503500EnemyTaskDescs`. Initialization borrows the record
/// and seeds health from hpMax; arms also read it when restoring health. Attack
/// rows are borrowed. Rewards use the resident result units; critical and damage-
/// over-time chances are percentages and reaction durations are base step counts.
/// Keep the overlay loaded while any enemy retains its parameter pointer.
extern EnemyParams gActor503500EnemyParams[ACTOR_503500_SLOT_COUNT];

/// Player height/bearing eligibility masks for boss and attached target slots.
///
/// Slots 0..16 follow `ACTOR_503500_SLOT_*`; the three trailing bytes are
/// stored zero and unread. Bits use `ACTOR_503500_TARGET_*`. Bearing failure
/// or an active event blocks lock-on and scan retention; height-only failure
/// blocks lock-on but preserves scanning. All twenty stored bytes are retained.
extern u8 gActor503500SlotTargetEligibilityMasks[20];

/// Borrowed handle published by the boss entry scene's fade-from-black spawn.
///
/// Spawn failure and explicit room reset publish NULL. Fade teardown does not
/// clear the handle, so a non-NULL value may be stale and requires an independently
/// live fade task before dereference. This cache owns no task or work storage.
extern Task* gActor503500FadeFromBlackTask;

/// Retained player-placement snapshot for Shelter R48's temporary scene.
///
/// Position uses root-parent game units and rotation uses 4096 units per turn.
/// All-zero XYZ means no saved placement, so a snapshot at the origin cannot
/// be restored. Clearing invalidates only position; synchronous placement
/// dispatch borrows the snapshot without consuming it. Lifetime is the overlay's.
extern ActorTransform gActor503500SavedPlayerTransform;

/// Extents and shared indices of the boss's clip and attack-selection tables.
enum {
    ACTOR_503500_BOSS_ANIMATION_SOURCE_COUNT = 2,
    ACTOR_503500_BOSS_ANIMATION_PRESET_COUNT = 20,
    ACTOR_503500_BOSS_IDLE_BLEND_PRESET      = 1,
    ACTOR_503500_SIDE_CHAIN_CANDIDATE_COUNT  = 4,
    ACTOR_503500_SIDE_COUNT                  = 2,
    ACTOR_503500_LUNGING_CHAIN_COUNT         = ACTOR_503500_SLOT_LUNGING_CHAIN_3 - ACTOR_503500_SLOT_LUNGING_CHAIN_0 + 1,
    ACTOR_503500_KNOCKBACK_RESTORE_SET       = 5,
    ACTOR_503500_KNOCKBACK_SET_COUNT         = ACTOR_503500_KNOCKBACK_RESTORE_SET + 1,
};

/// Turn-limit class for each boss clip, indexed by animation ID 0..19.
///
/// Values are `ACTOR_503500_TURN_RATE_*`; the turn step reads one signed byte
/// without checking bounds. Only idle and attack states use the table.
extern s8 gActor503500BossAnimationTurnRateClasses[ACTOR_503500_BOSS_ANIMATION_PRESET_COUNT];

/// Boss clip tables selected by the play request's source index (0 or 1).
///
/// Source 0 accepts clip ids 1..19; source 1 accepts 1..4. Both reserve id 0
/// as NULL. The tables and loaded clip data must outlive their borrowed rigs.
extern AnimationSet** gActor503500BossAnimationSources[ACTOR_503500_BOSS_ANIMATION_SOURCE_COUNT];

/// Boss playback presets: reset idle, blended idle, then source-0 clips 2..19.
///
/// Indexed by preset 0..19. Blend durations count whole animation frames;
/// every request disables animation-driven world collision.
extern AnimationPlayRequest gActor503500BossAnimationPresets[ACTOR_503500_BOSS_ANIMATION_PRESET_COUNT];

/// Boss idle restart request: source 0, clip 1, twenty-frame blend, collision disabled.
///
/// An addressable AnimationPlayRequest lvalue in the preset table, with no
/// separate storage. The table remains loaded for this overlay's lifetime.
#define ACTOR_503500_BOSS_IDLE_BLEND_REQUEST (gActor503500BossAnimationPresets[ACTOR_503500_BOSS_IDLE_BLEND_PRESET])

/// Boss target/sphere center in part-3 local game units, also the orange-flash origin.
extern SVECTOR gActor503500BossTargetOffset;

/// Dimensions of the fight-set, height and absolute-bearing attack policy.
enum {
    ACTOR_503500_ATTACK_SET_COUNT          = 2,
    ACTOR_503500_ATTACK_HEIGHT_BAND_COUNT  = 3,
    ACTOR_503500_ATTACK_BEARING_BAND_COUNT = 4,
};

/// One height band's four weighted attack lists, ordered by absolute bearing.
///
/// Band indices 0..3 correspond to `Actor503500AttackLimitsByBearing`.
/// Each immutable pointer names a NULL-attack-terminated list; the lists and
/// their callbacks remain live for the actor overlay lifetime.
typedef const Actor503500AttackChoice* const Actor503500AttackChoicesByBearing[ACTOR_503500_ATTACK_BEARING_BAND_COUNT];
STATIC_ASSERT_SIZEOF(Actor503500AttackChoicesByBearing, 0x10);

/// One height band's four inclusive absolute-bearing upper limits.
///
/// Immutable signed halfwords use 4096 units per turn and ascend to 2048.
/// The first limit >= the normalized bearing's magnitude selects band 0..3
/// in `Actor503500AttackChoicesByBearing`, including the half-turn endpoint.
typedef const s16 Actor503500AttackLimitsByBearing[ACTOR_503500_ATTACK_BEARING_BAND_COUNT];
STATIC_ASSERT_SIZEOF(Actor503500AttackLimitsByBearing, 0x8);

/// Weighted boss attack lists selected by fight set, player height and bearing.
///
/// Index the root by progressFlags room-event bit (0/1), then height band
/// (0 upper, 1 middle, 2 lower). Each row supplies four bearing-band lists,
/// parallel to `gActor503500AttackBearingLimits`; NULL attack ends each list.
/// All rows and lists remain live for the actor overlay lifetime.
extern Actor503500AttackChoicesByBearing* const gActor503500AttackChoices[ACTOR_503500_ATTACK_SET_COUNT][ACTOR_503500_ATTACK_HEIGHT_BAND_COUNT];

/// Inclusive absolute-bearing limits parallel to the boss's attack-choice rows.
///
/// Index by progressFlags room-event bit (0/1) and height band (0 upper,
/// 1 middle, 2 lower). Four ascending limits classify |playerBearing| at
/// 4096 units per turn; equality stays in the current band. Every row ends
/// at the half-turn limit, so normalized bearings select band 0..3.
extern Actor503500AttackLimitsByBearing* gActor503500AttackBearingLimits[ACTOR_503500_ATTACK_SET_COUNT][ACTOR_503500_ATTACK_HEIGHT_BAND_COUNT];

/// Cooldowns in boss upkeep frames, parallel to either side's four chain candidates.
///
/// The large chain and two lunging chains wait one frame; the base waits 100.
extern s16 gActor503500SideChainCooldownFrames[ACTOR_503500_SIDE_CHAIN_CANDIDATE_COUNT];

/// Positive-side selection order: large chain, outer/inner lunging chains, base.
///
/// Entries are boss descriptor slots, paired with `gActor503500SideChainCooldownFrames`.
extern s16 gActor503500PositiveSideChainCandidates[ACTOR_503500_SIDE_CHAIN_CANDIDATE_COUNT];

/// Negative-side selection order: large chain, outer/inner lunging chains, base.
///
/// Entries are boss descriptor slots, paired with `gActor503500SideChainCooldownFrames`.
extern s16 gActor503500NegativeSideChainCandidates[ACTOR_503500_SIDE_CHAIN_CANDIDATE_COUNT];

/// Seven hit-puff spawn positions in boss-part-3 local game units.
///
/// The phase-transition spray chooses cyclic and random entries. Effect
/// placement copies XYZ and retains the pointer; storage outlives the puffs.
extern SVECTOR gActor503500PhaseTransitionPuffOffsets[7];

/// Source mesh for the boss's four-face obstacle in the room collision grid.
///
/// Four Q12 normals and eight game-unit vertices are local to boss model part 1;
/// updates transform them into the reserved leading pools of the live room grid.
/// The source remains loaded with the actor overlay and is never rebuilt in place.
extern WorldCollisionGrid gActor503500BossBodyCollisionGrid;

/// Pink-flash emitter attachment position in boss-part-8 local game units.
extern SVECTOR gActor503500PinkFlashEmitterRootOffset;

/// Pink-flash emitter target/sphere center in emitter-root local game units.
extern SVECTOR gActor503500PinkFlashEmitterTargetOffset;

/// Shared origin of the two pink-flash attacks in emitter-root local game units.
extern SVECTOR gActor503500PinkFlashEmitterAttackOffset;

/// Three dying pink-emitter smoke positions in emitter-root local game units.
///
/// Successive smoke puffs cycle through all entries; storage outlives the effects.
extern SVECTOR gActor503500PinkFlashEmitterDeathSmokeOffsets[3];

/// Large-chain root positions in boss-part-1 local game units, ordered by slots 2, 3.
extern SVECTOR gActor503500LargeChainRootOffsets[ACTOR_503500_SIDE_COUNT];

/// Large-chain root XYZ Euler rotations, ordered by slots 2, 3, at 4096 units per turn.
extern SVECTOR gActor503500LargeChainRootAngles[ACTOR_503500_SIDE_COUNT];

/// Large-chain target/sphere center in tip-coordinate local game units.
extern SVECTOR gActor503500LargeChainTipTargetOffset;

/// Large-chain resting tip positions in boss-part-1 local game units, ordered by slots 2, 3.
///
/// Initialization seeds tip position and target; idle orbit offsets are added
/// to these same parent-frame positions.
extern SVECTOR gActor503500LargeChainRestTipOffsets[ACTOR_503500_SIDE_COUNT];

/// 1000-unit local vector rotated about each large chain's resting tip position.
extern const SVECTOR gActor503500LargeChainIdleOrbitOffset;

/// Three link-local smoke positions cycled from tip toward root on dying large chains.
///
/// Each link supplies its own coordinate frame; storage outlives the spawned puffs.
extern SVECTOR gActor503500LargeChainDeathSmokeOffsets[3];

/// Boss model mount-part indices for large-orb emitter slots 4, 5 (parts 11, 5).
extern const s32 gActor503500LargeOrbEmitterParentParts[ACTOR_503500_SIDE_COUNT];

/// Large-orb emitter target/sphere centers in root-local game units, ordered by slots 4, 5.
extern SVECTOR gActor503500LargeOrbEmitterTargetOffsets[ACTOR_503500_SIDE_COUNT];

/// Source VRAM row copied to (0, 261) when either large-orb emitter initializes.
///
/// RECT X/width count VRAM words and Y/height count rows; this transfers 256 words.
extern RECT gActor503500LargeOrbEmitterVramRowSourceRect;

enum {
    ACTOR_503500_LARGE_ORB_LAUNCH_POINT_COUNT = 2,
    ACTOR_503500_LARGE_ORB_LAUNCH_ANGLE_COUNT = 3,
    ACTOR_503500_LARGE_ORB_LAUNCH_ANGLE_INDEX = 2,
};

/// Large-orb launch points in emitter-local game units, ordered front then rear.
///
/// The other emitter side mirrors X; the actor overlay owns this storage.
extern SVECTOR gActor503500LargeOrbLaunchOffsets[ACTOR_503500_LARGE_ORB_LAUNCH_POINT_COUNT];

/// Local launch rotations for each large-orb point, at 4096 units per turn.
///
/// Six XYZ Euler vectors form front and rear triplets. Launches select the
/// last vector of each triplet (indices 2 and 5), mirroring yaw on the other
/// emitter side. The other four vectors are not selected by the current
/// launcher; their intended use is unproven.
extern const SVECTOR gActor503500LargeOrbLaunchAngles[ACTOR_503500_LARGE_ORB_LAUNCH_POINT_COUNT * ACTOR_503500_LARGE_ORB_LAUNCH_ANGLE_COUNT];

/// Texture and palette sources for the boss's large-orb-emitter death repaint.
///
/// RECT X/width count VRAM words; Y/height count rows. Each texture source is
/// a 128-by-256-pixel 8-bit region paired with its 256-color CLUT source.
/// The rectangles are overlay-owned; source pixels must be present in VRAM
/// when the copies run.
typedef struct {
    RECT textureRect; // 64-word-wide texture source copied over the boss's side
    RECT paletteRect; // 256-color CLUT source copied to that side's palette row
} Actor503500LargeOrbRepaintSources;
STATIC_ASSERT_SIZEOF(Actor503500LargeOrbRepaintSources, 0x10);

/// Texture-block and row sources for large-orb emitter death, ordered by slots 4, 5.
///
/// RECT X/width count VRAM words and Y/height count rows. The texture blocks
/// copy to (448, 256)/(576, 256); the rows copy to (0, 249)/(0, 251).
extern Actor503500LargeOrbRepaintSources gActor503500LargeOrbEmitterDeathRepaintSources[ACTOR_503500_SIDE_COUNT];

/// Nine base death-effect positions in large-orb-emitter local game units.
///
/// The death step cycles over these, subtracts frame-dependent X/Y drift,
/// mirrors X for slot 5, then emits another pair with Z mirrored.
extern SVECTOR gActor503500LargeOrbEmitterDeathEffectOffsets[9];

/// Rear-part target/sphere center in root-local game units (root attached to boss part 16).
extern SVECTOR gActor503500RearPartTargetOffset;

/// Eighteen rear-part-local death-effect positions in game units.
///
/// Each dying hit puff and drifting sprite chooses an independent random entry;
/// storage outlives the effects.
extern SVECTOR gActor503500RearPartDeathEffectOffsets[18];

/// Chain-base target/sphere centers in root-local game units, ordered by slots 7, 8.
extern SVECTOR gActor503500ChainBaseTargetOffsets[ACTOR_503500_SIDE_COUNT];

/// Chain-base projectile launch position in base-local game units for slot 7.
///
/// Slot 8 mirrors X before the base's composed rotation takes it into world space.
extern const SVECTOR gActor503500ChainBaseLaunchOffset;

/// Chain-base projectile XYZ Euler rotation at 4096 units per turn for slot 7.
///
/// Slot 8 mirrors yaw; the launch composes this ZYX rotation onto the base's facing.
extern SVECTOR gActor503500ChainBaseLaunchAngles;

/// Number of effect positions consumed by either chain base's death burst.
enum { ACTOR_503500_CHAIN_BASE_DEATH_OFFSET_COUNT = 3 };

/// Three chain-base death-effect positions in slot-7 local game units.
///
/// The hit-puff cycle and final three-sprite burst use the whole table;
/// storage outlives the effects.
extern SVECTOR gActor503500ChainBaseDeathEffectOffsetsSide0[ACTOR_503500_CHAIN_BASE_DEATH_OFFSET_COUNT];

/// Slot-8 chain-base death-effect positions in base-local game units.
///
/// Only the first three entries feed the hit-puff cycle and sprite burst.
/// The remaining six stored positions have no established use; their role is
/// unproven. Storage outlives the effects.
extern SVECTOR gActor503500ChainBaseDeathEffectOffsetsSide1[9];

/// Small-orb emitter target/sphere center in emitter-root local game units.
extern SVECTOR gActor503500SmallOrbEmitterTargetOffset;

/// Six shots per small-orb volley; speed entries have four fractional bits.
enum {
    ACTOR_503500_SMALL_ORB_EMITTER_VOLLEY_SHOTS = 6,
    ACTOR_503500_SMALL_ORB_LAUNCH_SPEED_ONE     = 1 << 4,
};

/// Per-shot small-orb launch speeds in sixteenths of a game unit per frame.
///
/// Volley indices 0..5 parallel `gActor503500SmallOrbEmitterLaunchAngles`.
/// Shifting left 12 converts these entries to signed 16.16 speed before the
/// launch adds its player-height correction.
extern const s16 gActor503500SmallOrbEmitterLaunchSpeeds[ACTOR_503500_SMALL_ORB_EMITTER_VOLLEY_SHOTS];

/// Per-shot small-orb XYZ Euler rotations, at 4096 units per turn.
///
/// Volley indices 0..5 parallel `gActor503500SmallOrbEmitterLaunchSpeeds`;
/// launch composes each rotation onto the emitter's world facing.
extern SVECTOR gActor503500SmallOrbEmitterLaunchAngles[ACTOR_503500_SMALL_ORB_EMITTER_VOLLEY_SHOTS];

/// Nine small-orb-emitter-local death-effect positions in game units.
///
/// Hit puffs cycle through all entries; odd frames choose a random entry for
/// a drifting sprite. Storage outlives the effects.
extern SVECTOR gActor503500SmallOrbEmitterDeathEffectOffsets[9];

/// Source VRAM texture region copied to (321, 298) when the small-orb emitter dies.
///
/// RECT X/width count VRAM words and Y/height count rows.
extern RECT gActor503500SmallOrbEmitterDeathRepaintSourceRect;

/// Yellow-flash emitter target/sphere center in emitter-root local game units.
extern SVECTOR gActor503500YellowFlashEmitterTargetOffset;

/// Six yellow-flash-emitter-local death-effect positions in game units.
///
/// Hit puffs cycle through all entries; drifting sprites cycle through entries
/// 3..5 on odd frames. Storage outlives the effects.
extern SVECTOR gActor503500YellowFlashEmitterDeathEffectOffsets[6];

/// Source VRAM texture region copied to (320, 256) when the yellow-flash emitter dies.
///
/// RECT X/width count VRAM words and Y/height count rows.
extern RECT gActor503500YellowFlashEmitterDeathRepaintSourceRect;

/// Lunging-chain root positions in boss-part-1 local game units, ordered by slots 13..16.
extern SVECTOR gActor503500LungingChainRootOffsets[ACTOR_503500_LUNGING_CHAIN_COUNT];

/// Lunging-chain root XYZ Euler rotations, ordered by slots 13..16, at 4096 units per turn.
extern SVECTOR gActor503500LungingChainRootAngles[ACTOR_503500_LUNGING_CHAIN_COUNT];

/// Lunging-chain target/sphere center in tip-coordinate local game units.
extern SVECTOR gActor503500LungingChainTipTargetOffset;

/// Lunging-chain attack-sphere centers in tip-local game units, ordered by slots 13..16.
///
/// The separate target sphere has its own center; these offsets belong to the
/// player-damaging sphere.
extern SVECTOR gActor503500LungingChainTipAttackOffsets[ACTOR_503500_LUNGING_CHAIN_COUNT];

/// Resting tip positions in boss-part-1 local game units, ordered by slots 13..16.
///
/// Initialization seeds both tip position and target; idle steps restore the target.
extern SVECTOR gActor503500LungingChainRestTipOffsets[ACTOR_503500_LUNGING_CHAIN_COUNT];

/// Stored lunging-chain curl pitches by model part, in 4096ths of a turn.
///
/// Pose updates read parts 2..8 and scale by the Q12 curl weight; only parts
/// 2..7 receive that pitch. Entries 0, 1 and 9 are not read by this package.
/// All ten stored halfwords are retained; the unused entries' purpose is unproven.
extern const s16 gActor503500LungingChainCurlPitchByPart[10];

/// Three link-local smoke positions cycled from tip toward root on dying lunging chains.
///
/// Each link supplies its own coordinate frame; storage outlives the spawned puffs.
extern SVECTOR gActor503500LungingChainDeathSmokeOffsets[3];

/// Boss model mount-part indices for arm slots 10, 11 (parts 11, 5).
extern s32 gActor503500ArmParentParts[ACTOR_503500_SIDE_COUNT];

/// Bodyless priority-192 recipe for the player's arm-strike knockback sequence.
///
/// Spawn at descriptor index 0 with fall direction 0/1 in argument 1 and a
/// borrowed MATRIX rotation in argument 2. Keep that rotation live and unchanged
/// until the task's first active update copies it. The callback requires this
/// overlay and the player to remain live through recovery.
extern TaskDesc gActor503500KnockbackTaskDesc;

/// Detached arm model's root offset on its boss mount part, in local game units.
extern const SVECTOR gActor503500ArmRootOffset;

/// Arm target/sphere centers in arm-root local game units, ordered by slots 10, 11.
extern SVECTOR gActor503500ArmTargetOffsets[ACTOR_503500_SIDE_COUNT];

/// Attachment damage rows that destroy an arm regardless of remaining HP.
///
/// Indexed by the attack key's low seven bits only when bit 0x8000 is set.
/// Live attachment rows are 0..54; the stored final entry 55 is also zero.
/// Values are 0 (normal HP/reaction rules) or 1 (destroy immediately).
extern const s8 gActor503500ArmDestroyingAttachmentRows[56];

/// Player knockback clips: NULL, two falls, two recoveries, and the weapon's restore pose.
///
/// Entry 5 is writable: knockback copies set 7 from the loaded character/weapon
/// bank immediately before playing it. Keep both overlays loaded during playback.
extern AnimationSet* gActor503500PlayerKnockbackAnimationSets[ACTOR_503500_KNOCKBACK_SET_COUNT];

/// Number of player fall directions selected by the sign of the rotated arm Z.
enum { ACTOR_503500_KNOCKBACK_FALL_DIRECTION_COUNT = 2 };

/// Player fall requests selected by the arm position rotated into player axes.
///
/// Index 0 selects clip 1 for negative Z; index 1 selects clip 2 for nonnegative Z.
/// Each resets playback and enables world collision. Dispatch reads the request
/// synchronously; the clip table and data must remain loaded throughout playback.
extern const AnimationPlayRequest gActor503500PlayerKnockbackFallRequests[ACTOR_503500_KNOCKBACK_FALL_DIRECTION_COUNT];

/// Player recovery requests for the same fall-direction indices 0/1.
///
/// Selects clip 3/4 with a three-frame blend and world collision enabled after
/// the fall finishes. Dispatch reads the request synchronously; playback borrows
/// the clip table and data until the selected recovery finishes.
extern const AnimationPlayRequest gActor503500PlayerKnockbackRecoveryRequests[ACTOR_503500_KNOCKBACK_FALL_DIRECTION_COUNT];

/// Restores the equipped weapon's player pose after knockback recovery.
///
/// Plays the writable restore-set slot with a three-frame blend and world
/// collision enabled. The slot must first contain the live weapon bank's set 7;
/// its table and loaded clip data must outlive playback.
extern const AnimationPlayRequest gActor503500PlayerKnockbackRestoreRequest;

/// Zero-press player hold request used to take scripted control for arm knockback.
///
/// The player ignores the embedded animation and copies only the zero press
/// count during dispatch. Recovery can reject the hold; acceptance acquires
/// the attachment event lock and the caller immediately starts the fall clip.
extern const GameActorButtonPressHold gActor503500PlayerKnockbackHoldRequest;

/// Source VRAM row copied to (0, 253) before side-1's dying arm resource load.
///
/// X and width count VRAM words; Y and height count rows. The source is
/// (0, 261), 256 words wide and one row high; side 0 performs no row copy.
extern RECT gActor503500ArmDeathVramRowSourceRect;

/// Five stored smoke positions in arm-part local game units.
///
/// The dying arm restarts the index at zero for each burst on parts 0 and 1,
/// so it currently uses entries 0 and 1. Entries 2..4 remain unused. Effect work
/// borrows these positions; storage lives with the actor overlay.
extern SVECTOR gActor503500ArmDeathSmokeOffsets[5];

/// Side-0 falling-arm puff position (-1000, 0, 0) in arm-root local game units.
///
/// The effect borrows this position for its lifetime; keep the overlay loaded.
extern SVECTOR gActor503500ArmDeathFallPuffOffsetSide0;

/// Side-1 falling-arm puff position (1000, 0, 0) in arm-root local game units.
///
/// The effect borrows this position for its lifetime; keep the overlay loaded.
extern SVECTOR gActor503500ArmDeathFallPuffOffsetSide1;

/// Two-lane knockback vibration script: play segments 0/1, then stop both motors.
///
/// The on/off lane selects segment 0 and the variable lane segment 1. Both
/// command cursors visit entries 0 and 1 only. The independent script task
/// borrows this array and its segment table until teardown.
extern const PadScriptCmd gActor503500KnockbackPadScriptCommands[2];

/// Knockback motor segments: an eight-frame hold and eight frames at full intensity.
///
/// Segment 0 serves the binary motor, which ignores intensity; segment 1 serves
/// the variable motor at 255 throughout. The stored fourth bytes 0/1 are not
/// read by the interpreter. Storage must outlive the borrowed vibration script.
extern const PadScriptVibrationSegment gActor503500KnockbackPadScriptSegments[2];

/// Ballistic shot attack-sphere center in shot-root local game units.
extern const SVECTOR gActor503500BallisticShotBodyOffset;

/// Lingering shot attack-sphere center in shot-root local game units.
extern const SVECTOR gActor503500LingeringShotBodyOffset;

enum { ACTOR_503500_LINGERING_SHOT_KIND_COUNT = 2 };

/// Linger-count limits in active updates: kind 0 large orb, kind 1 projectile.
///
/// Pair tests stop when the incremented count exceeds 180 or 150, respectively:
/// the 181st or 151st lingering update. Storage lives with the actor overlay.
extern const s32 gActor503500LingeringShotFrameLimits[ACTOR_503500_LINGERING_SHOT_KIND_COUNT];

/// Spawn-recipe indices of the independent shot and flash attack tasks.
enum {
    ACTOR_503500_ATTACK_TASK_BALLISTIC_SHOT = 0,
    ACTOR_503500_ATTACK_TASK_LINGERING_SHOT = 1,
    ACTOR_503500_ATTACK_TASK_PINK_FLASH     = 2,
    ACTOR_503500_ATTACK_TASK_YELLOW_FLASH   = 3,
    ACTOR_503500_ATTACK_TASK_ORANGE_FLASH   = 4,
    ACTOR_503500_ATTACK_TASK_COUNT          = 5,
};

/// Coordinate-body attack recipes, all dispatched at priority 192.
///
/// Shot argument 1 selects kind 0/1 and argument 2 is signed 16.16 launch speed.
/// Ballistic kinds are chain shot/small orb; lingering kinds are large orb/chain-
/// base projectile. Pink argument 1 selects side 0/1 and argument 2 is unused;
/// yellow ignores both. Orange argument 1 is its charge-effect tick countdown
/// and argument 2 borrows the live boss task. Spawn reads descriptors synchronously;
/// callbacks and attack resources require this overlay to remain loaded.
extern TaskDesc gActor503500AttackTaskDescs[ACTOR_503500_ATTACK_TASK_COUNT];

/// Boss message handlers for animation, placement, model draw mode and commands.
///
/// Four ID/callback entries followed by TASK_MESSAGE_TABLE_END. Unknown IDs
/// return zero. The live boss borrows this read-only table and callback code
/// from the actor overlay for its lifetime; payload types belong to each ID.
extern const TaskMessageEntry gActor503500BossMessageHandlers[];

/// Inherits an attached child's draw suppression, translucency and buffer policy.
///
/// Requires a live child model and parent model while its root remains attached
/// to anything other than the view. A view-parented model is left alone. Clearing
/// inherited auto-buffer suppression allocates the child's primitive buffer;
/// setting it schedules release in two caller update frames through the writable
/// signed-byte `bufferFreeCountdown`. The caller owns that countdown and release.
void actor503500SyncAttachedModelDrawState(Task* task, s8* bufferFreeCountdown);

/// Clears the boss's enemy pointer for slot 0..16.
///
/// This only empties the slot; the caller retains responsibility for tearing
/// down the enemy. Its busy flag and cooldown are retained. `unusedTask` is
/// ignored because the package has one boss work block.
void actor503500ClearSlotEnemy(Task* unusedTask, s32 slot);

/// Spawns an attached slot enemy under the boss and publishes it in its slot.
///
/// `task` is the boss task; `slot` is a descriptor index in 0..16. The parent
/// enemy's placement index must select a live placement in the current area's
/// loaded variant. Copies that placement's texture-page and CLUT-row offsets
/// into the child model and refreshes both packet halves if allocated.
/// Returns the new enemy or NULL on spawn failure, leaving the old slot intact
/// on failure. The enemy/task teardown owns the child; this function does not
/// destroy an existing slot enemy before replacing its pointer.
Enemy* actor503500SpawnSlotEnemy(Task* task, s32 slot);

/// Returns 1 when boss slot 0..16 has no enemy, otherwise 0.
///
/// `unusedTask` is ignored; the query reads the package's one boss work block.
s32 actor503500IsSlotEmpty(Task* unusedTask, s32 slot);

/// Enables a scale on boss model part 5, 11 or 16, using 4096 for one unit.
///
/// `task` must be the live boss with its twenty-part model; `scale` is borrowed
/// for this call and its XYZ values are copied. Parts 5/11 are reparented to
/// private copies of parts 4/10; part 16 is scaled in place by pose updates.
/// The enable bit persists until work initialization. Other indices in 0..19
/// set only their bit and do not copy a scale or change the hierarchy.
void actor503500SetBossPartScale(Task* task, s32 partIndex, const SVECTOR* scale);

/// Stores a slot enemy's busy report in the boss work block.
///
/// `slot` is 0..16. State setters pass 0 while at rest and 1 while occupied
/// with an attack or transition; the value is stored without normalization.
/// `unusedTask` is ignored. The report does not change the slot's cooldown.
void actor503500SetSlotBusy(Task* unusedTask, s32 slot, s16 busy);

/// Sets the boss's track rates and applies one of its animation requests.
///
/// `presetIndex` is 0..19: 0 resets animation 1, 1 blends animation 1, and
/// 2..19 select the animation with that ID. `rate` is a signed step in
/// sixteenths of a frame per tick, narrowed to a signed byte; 0 selects
/// `ANIMATION_RATE_ONE`. Only tracks 1..16 receive this rate, while playback
/// drives tracks 1..19. A reset or source change replaces those rates with
/// the normal rate before the first pose is ticked.
void actor503500PlayAnimationPreset(Task* task, s32 presetIndex, s32 rate);

/// Tests whether the boss's last requested animation has finished or jumped.
///
/// Returns -1 if `animationId` differs from the last request; otherwise 1
/// when the first driven track's latest tick settled or followed a control
/// jump, and 0 while it continues. Callers treat every nonzero result as done.
/// `unusedTask` is ignored; the query reads the package's one boss work block.
s32 actor503500HasAnimationFinished(Task* unusedTask, s32 animationId);

/// Starts the boss's recoil after a slot enemy is destroyed.
///
/// Enters `ACTOR_503500_STATE_PART_LOST`, restarts the state and attack
/// progress, schedules removal of the boss's target after three upkeep ticks,
/// and restores ordinary ordering-table depth.
void actor503500EnterPartLostState(Task* task);

/// Returns 1 while the boss is recoiling, stunned or defeated, otherwise 0.
///
/// Slot attacks and flash attack tasks use this to abort their current attack.
/// `unusedTask` is ignored; the query reads the package's one boss work block.
s32 actor503500ShouldInterruptAttack(Task* unusedTask);

/// Tries to replace a slot's share of the package's effect budget.
///
/// `slot` is an enemy slot 0..16; live projectile tasks contribute separately
/// through budget entry 17. `effectCost` is a budget cost in 0..32767, not a
/// count of effects spawned this frame. If the new cost plus all other entries
/// is strictly below 9, stores it and returns 1; otherwise retains the old
/// reservation and returns 0. Stored costs are halfwords and are summed as
/// signed halfwords. A reservation lasts until replaced or released.
s32 actor503500TryReserveSlotEffects(s32 slot, s32 effectCost);

/// Releases enemy slot 0..16's entire effect-budget reservation.
void actor503500ReleaseSlotEffects(s32 slot);

/// Measures the player's bearing relative to the supplied model root's heading.
///
/// Returns [-2048, 2048) at 4096 units per turn. Both translations must share a
/// parent frame; X/Z differences narrow to signed halfwords before the bearing
/// is measured. Heading comes from the root matrix's Z axis. No transform is
/// composed and the boss's cached bearing is not changed.
s16 actor503500MeasurePlayerBearing(Task* task);

/// Returns the boss's latched health-exhausted flag.
///
/// Slot enemies use this to suppress their hit handling once the boss is
/// defeated. The flag remains set until the boss work block is initialized.
s32 actor503500IsDefeated(void);

/// Returns the boss's last attack-state bearing to the player.
///
/// The signed angle is relative to the boss's stored yaw, at 4096 units per
/// turn in [-2048, 2048). Initialization gives zero; only attack-state updates
/// refresh it, so slot enemies can read a bearing from an earlier attack.
s16 actor503500GetPlayerBearing(void);

/// Acquires a projectile task's lifetime share of the package's effect budget.
///
/// `effectCost` is its `ACTOR_503500_PROJECTILE_EFFECT_COST_*` weight. Adds to
/// aggregate entry 17 without admission checking and narrows to an unsigned
/// halfword; budget admission later sums it as signed. A normal task exit
/// balances the acquisition through `actor503500ReleaseProjectileEffectCost`.
void actor503500AcquireProjectileEffectCost(s32 effectCost);

/// Lifetime budget weights released by the package's projectile task exits.
enum {
    ACTOR_503500_PROJECTILE_EFFECT_COST_BALLISTIC    = 1,
    ACTOR_503500_PROJECTILE_EFFECT_COST_LINGERING    = 3,
    ACTOR_503500_PROJECTILE_EFFECT_COST_PINK_FLASH   = 6,
    ACTOR_503500_PROJECTILE_EFFECT_COST_YELLOW_FLASH = 6,
    ACTOR_503500_PROJECTILE_EFFECT_COST_ORANGE_FLASH = 8,
};

/// Releases a projectile task's lifetime share of the package's effect budget.
///
/// `effectCost` is the task's weight: 1 for a ballistic shot, 3 for a lingering
/// shot (large orb or chain-base projectile), 6 for a pink/yellow flash and 8
/// for an orange flash. Normal exits balance the earlier acquisition; effect
/// creation failure also calls cleanup before that acquisition has happened.
/// Subtracts from aggregate entry 17 without checking. The stored unsigned
/// halfword wraps on underflow, while budget admission sums it as signed.
void actor503500ReleaseProjectileEffectCost(s32 effectCost);

/// Dispatches an upper-limb large-orb emitter through initialization, update and exit.
///
/// Requires a coordinate body, the boss as parent, slot 4 or 5 in
/// `spawnArg1.value` and its live enemy in `spawnArg2.pointer`. `state` is
/// 0..2; initialization binds side-specific static work, which exit retains.
/// Update pauses for paused/hidden actors; initialization and exit still run.
void actor503500LargeOrbEmitterTask(Task* task);

/// Dispatches the boss's rear target through initialization, update and exit.
///
/// Requires a coordinate body, the boss as parent, slot 6 in `spawnArg1.value`
/// and its live enemy in `spawnArg2.pointer`. `state` is 0..2; initialization
/// binds singleton static work, which exit retains. Update pauses for
/// paused/hidden actors; initialization and exit still run.
void actor503500RearPartTask(Task* task);

/// Dispatches a covered or exposed chain base through initialization, update and exit.
///
/// Requires a coordinate body, the boss as parent, slot 7 or 8 in
/// `spawnArg1.value` and its live enemy in `spawnArg2.pointer`. `state` is
/// 0..2; initialization binds side-specific static work, which exit retains.
/// Update pauses for paused/hidden actors; initialization and exit still run.
void actor503500ChainBaseTask(Task* task);

/// Dispatches the small-orb volley emitter through initialization, update and exit.
///
/// Requires a coordinate body, the boss as parent, slot 9 in `spawnArg1.value`
/// and its live enemy in `spawnArg2.pointer`. `state` is 0..2; initialization
/// binds singleton static work, which exit retains. Update pauses for
/// paused/hidden actors; initialization and exit still run.
void actor503500SmallOrbEmitterTask(Task* task);

/// Dispatches the yellow-flash emitter through initialization, update and exit.
///
/// Requires a coordinate body, the boss as parent, slot 12 in `spawnArg1.value`
/// and its live enemy in `spawnArg2.pointer`. `state` is 0..2; initialization
/// binds singleton static work, which exit retains. Update pauses for
/// paused/hidden actors; initialization and exit still run.
void actor503500YellowFlashEmitterTask(Task* task);

/// Dispatches a lunging chain through initialization, update and exit.
///
/// Requires a nine-part TMD body, the boss as parent, slot 13..16 in
/// `spawnArg1.value` and its live enemy in `spawnArg2.pointer`. `state` is
/// 0..2; initialization binds the slot's static work, which exit retains.
/// Initial commands BECOME_TARGET and REGROW select unfolding and regrowth.
/// Paused/hidden updates still maintain model buffers; behavior pauses.
void actor503500LungingChainTask(Task* task);

/// Dispatches a striking arm through initialization, update and exit.
///
/// Requires a four-part TMD body, the boss as parent, slot 10 or 11 in
/// `spawnArg1.value` and its live enemy in `spawnArg2.pointer`. `state` is
/// 0..2; initialization binds side-specific static work, which exit retains.
/// Behavior pauses for paused/hidden actors; initialization and exit still run.
void actor503500ArmTask(Task* task);

/// Dispatches a ballistic small-orb shot through launch, flight/burst updates and exit.
///
/// Requires a coordinate body placed and aimed in world space. `state` is
/// 0..2; `spawnArg1.value` selects attack 0 or 1 and `spawnArg2.value` is
/// the forward launch speed in signed 16.16 world units per update
/// (0 gives no initial velocity).
/// Launch owns allocated work and a child effect; exit releases them.
/// Update pauses for paused/hidden actors; launch and exit still run.
void actor503500BallisticShotTask(Task* task);

/// Dispatches a lingering orb or projectile through launch, movement and exit.
///
/// Requires a world-space coordinate body and `state` in 0..2. Spawn argument 1
/// is 0 (large orb) or 1 (projectile); argument 2 is signed 16.16 forward speed,
/// with zero selecting the default 16 units per movement step and identity
/// rotation. Launch owns allocated work and a child effect until exit.
/// Movement pauses for paused/hidden actors; launch and exit still run.
void actor503500LingeringShotTask(Task* task);

/// Dispatches one pink-flash sweeping capsule through initialization, update and exit.
///
/// `task->state` must be 0..2 with a live coordinate body. Spawn argument 1
/// selects the negative sweep at zero (which owns the flash effect), positive
/// otherwise. Update pauses for paused/hidden actors; initialization and exit run.
void actor503500PinkFlashAttackTask(Task* task);

/// Dispatches the player-attached yellow-flash sphere through its three task states.
///
/// `task->state` must be 0..2 with a live coordinate body and player root.
/// Update pauses for paused/hidden actors; initialization and exit run.
void actor503500YellowFlashAttackTask(Task* task);

/// Dispatches the orange-flash capsule through initialization, update and exit.
///
/// `task->state` must be 0..2 with a live coordinate body. Spawn argument 1
/// sets the charge effect's positive countdown in updating ticks; argument 2 is
/// the boss task for interruption. Update pauses for paused/hidden actors;
/// initialization and exit run.
void actor503500OrangeFlashAttackTask(Task* task);

// Callbacks referenced by the overlay's shared data tables.
/// Commands an arm strike, then waits for the selected arm to rest.
///
/// With bearing band 0, tries slots 10 and 11 in random-bit order. Even if
/// neither is ready, enters the wait phase using the previous `attackSlot`.
/// Other bands choose slot 10 for positive player bearing and slot 11 otherwise.
/// A command starts a 120-frame cooldown and clears the target yaw offset.
/// `task` is unused; `work` is the boss work block. Returns 0 while running
/// and a 1-frame idle delay on completion or a failed directional start,
/// following `Actor503500AttackFn`.
s32 actor503500AttackArmStrike(Task* task, Actor503500Work* work);

/// Commands one side's chain attack and turns the boss toward its rear.
///
/// Positive player bearing tries slots 2, 13, 14, 7; nonpositive bearing
/// tries 3, 16, 15, 8. Each candidate has a bearing gate; a lunging chain's
/// gate also depends on its partner's presence. Bearings and the 2000-unit
/// target yaw offset use 4096 units per turn. The retained opposite-side
/// fallback tests cannot pass after the sign test. A failed start returns
/// a 1-frame idle delay and clears the yaw offset.
/// Once started, waits for rest or more than 90 calls in the wait phase;
/// slots 7/8 do not advance that timer. Completion returns an idle delay
/// of 15 frames for slots 2/3, 60 for 13..16, 90 for 7/8, or 30 otherwise.
/// Returns 0 while running, following `Actor503500AttackFn`. `task` and
/// `work` are the boss task and its work block.
s32 actor503500AttackSideChain(Task* task, Actor503500Work* work);

/// Commands the boss's own body attack and waits for slot 0 to rest.
///
/// Starts a 60-frame cooldown and clears the target yaw offset. `task` is
/// unused; `work` is the boss work block. Returns 0 while running, a 1-frame
/// idle delay if not ready, or 30 frames when done (`Actor503500AttackFn`).
s32 actor503500AttackBody(Task* task, Actor503500Work* work);

/// Commands each ready chain base and waits for either slot to rest.
///
/// Slots 7 and 8 are tested independently and receive 150-frame cooldowns.
/// At least one start sets the target yaw offset to 2000 (4096 units per turn).
/// The wait also ends after more than 90 wait-phase calls; the timer advances
/// only when both slots report busy. A missing or uncommanded idle partner
/// can therefore end the wait immediately. `task` is unused and `work` is
/// the boss work block. Returns 0 while running, a 30-frame idle delay if
/// neither starts, or 240 frames when done, following `Actor503500AttackFn`.
s32 actor503500AttackChainBases(Task* task, Actor503500Work* work);

/// Commands a forward pink flash, falling back to the yellow flash.
///
/// Prefers ready slot 1 when the player's bearing magnitude is strictly
/// below 500, in 4096ths of a turn; otherwise tries ready slot 12. Starts
/// a 150-frame cooldown, clears the target yaw offset and waits for the
/// chosen slot to rest. `task` is unused; `work` is the boss work block.
/// Returns 0 while running, a 1-frame idle delay if neither can start, or
/// 90 frames when done, following `Actor503500AttackFn`.
s32 actor503500AttackPinkOrYellowFlash(Task* task, Actor503500Work* work);

/// Commands the yellow-flash emitter and waits for slot 12 to rest.
///
/// Starts a 150-frame cooldown and clears the target yaw offset. `task` is
/// unused; `work` is the boss work block. Returns 0 while running, a 1-frame
/// idle delay if not ready, or 150 frames when done (`Actor503500AttackFn`).
s32 actor503500AttackYellowFlash(Task* task, Actor503500Work* work);

/// Commands the paired large-orb emitters and waits for both slots to rest.
///
/// Phase 0 commands slots 4 and 5 with 180-frame cooldowns if slot 4 is ready
/// outside bearing [-1535, -513], or slot 5 is ready outside [513, 1535].
/// Bearings use 4096 units per turn. Only the leading slot's readiness is
/// checked; an occupied partner is commanded even if busy or cooling down.
/// Slot 4 is tried first. Phase 1 waits for both slots to be empty or at rest.
/// `task` is unused; `work` is the boss work block.
/// Returns 0 while running, 1 if no command can start, or a 10-frame idle
/// delay once both slots rest, following `Actor503500AttackFn`.
s32 actor503500AttackLargeOrbPair(Task* task, Actor503500Work* work);

/// Commands the small-orb volley and waits for slot 9 to rest.
///
/// Clears the target yaw offset even when slot 9 is not ready. A start sets
/// a 60-frame cooldown. `task` is unused; `work` is the boss work block.
/// Returns 0 while running, a 1-frame idle delay if not ready, or 30 frames
/// when done, following `Actor503500AttackFn`.
s32 actor503500AttackSmallOrbVolley(Task* task, Actor503500Work* work);

/// Dispatches the boss task's setup, frame update or teardown.
///
/// `task->state` must be 0 (setup), 1 (update) or 2 (exit). Requires the
/// boss descriptor's model and its enemy in `spawnArg2.pointer`; setup binds
/// the singleton work and spawns attached enemies. Copies the callback table
/// by value before dispatch. The exit handler may destroy the task.
void actor503500BossTask(Task* task);

/// Dispatches the attached pink-flash emitter's setup, frame update or teardown.
///
/// `task->state` must be 0 (setup), 1 (update) or 2 (exit). Requires slot 1
/// in `spawnArg1.value`, its enemy in `spawnArg2.pointer` and the boss parent.
/// Setup borrows the singleton emitter work. Copies the callback table by
/// value before dispatch. The exit handler may destroy the task.
void actor503500PinkFlashEmitterTask(Task* task);

/// Dispatches either large chain's setup, frame update or teardown.
///
/// `task->state` must be 0 (setup), 1 (update) or 2 (exit). Requires slot 2
/// or 3 in `spawnArg1.value`, its enemy in `spawnArg2.pointer`, the matching
/// nine-part model and the boss parent. Setup borrows that slot's static
/// work. Copies the callback table by value before dispatch. The exit handler
/// may destroy the task.
void actor503500LargeChainTask(Task* task);

/// Pushes the player away from an arm strike, then runs recovery animations.
///
/// Requires the live player and one active knockback task: work is a singleton.
/// `spawnArg1.value` is hit side 0 or 1; `spawnArg2.pointer` borrows a word-aligned
/// MATRIX rotation through initialization. Copies exactly 18 rotation bytes,
/// preserving alignment bytes and translation. Only running actor control
/// advances the task; initial state 0 falls through into the first push.
/// Speed and fractional travel are signed 16.16. A refused move stops speed;
/// after more than 20 resting updates, animation completion restores control.
/// A dead player moves the task to state -1 without killing it here.
/// `killCountdown` counts active updates for the sound on update 17.
void actor503500KnockbackTask(Task* task);

// Callbacks referenced by the overlay's shared data tables.
/// Applies an animation request to the boss's nineteen driven model tracks.
///
/// `request` is borrowed for this call and is read only. Its source index is
/// 0 or 1; its animation ID is 1..19 for source 0 or 1..4 for source 1. A
/// changed source binds the rig before playback; a non-reset blend applies
/// only after the rig has started, with duration in whole normal-rate frames
/// (0..2047).
/// Tracks 1..19 are ticked once immediately and the enabled part scales are
/// reapplied. The rig borrows the loaded set tables while playback continues.
/// `messageId` and `unusedArg` are ignored. Returns 0.
s32 actor503500HandlePlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);

/// Applies a borrowed scene command to the boss, ignoring its context tags.
///
/// Commands 0/1/2 resume idle, hold the boss, or begin collapse; 3 advances the
/// task toward exit. Command 4 saves the root coordinate and yaw and starts
/// the fight's phase-transition sequence; 5 restores them and releases that
/// sequence's wait. Command 5 requires a preceding command 4. The model and
/// boss work must be live. Unsupported commands do nothing. `messageId` and
/// `unusedArg` are ignored; returns 0 in every case.
s32 actor503500HandleBossCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);

/// Places the live boss from a borrowed position and XYZ Euler rotation.
///
/// Position uses whole units in the root's parent frame; angles use 4096 per
/// turn. Rebuilds the local root matrix, invalidates composition, and seeds
/// the boss's yaw and signed 16.16 position for subsequent motion. Retains
/// motion rates and the pending target yaw. Reads the payload only during the
/// call. `messageId` and `unusedArg` are ignored; returns 0.
s32 actor503500HandlePlaceBoss(Task* task, s32 messageId, const ActorTransform* transform, s32 unusedArg);

/// Sets the live boss model's drawing and automatic-buffer policy.
///
/// Modes 0/1 hide with automatic buffers or show with immediate allocation;
/// 2 hides, suppresses automatic buffers and schedules release on the third
/// countdown update; 3 shows with automatic buffers suppressed. Other flags
/// remain intact. Modes other than 2 retain any pending release countdown.
/// Returns 0 for modes 0..3, 1 without changes otherwise. `messageId` and
/// `unusedArg` are ignored.
s32 actor503500HandleSetBossModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);

#endif // SRC_ACTORS_ACTOR_503500_ACTOR_503500_PRIVATE_H
