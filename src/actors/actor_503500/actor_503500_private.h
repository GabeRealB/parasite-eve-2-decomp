#ifndef SRC_ACTORS_ACTOR_503500_ACTOR_503500_PRIVATE_H
#define SRC_ACTORS_ACTOR_503500_ACTOR_503500_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

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

enum {
    ACTOR_503500_SLOT_COUNT = 17, // Slots of the boss: its own and those of the enemies riding on it
};

/// Descriptor indices of the boss and the attached enemies these attacks command.
enum {
    ACTOR_503500_SLOT_BODY                 = 0,
    ACTOR_503500_SLOT_PINK_FLASH_EMITTER   = 1,
    ACTOR_503500_SLOT_LARGE_CHAIN_0        = 2,
    ACTOR_503500_SLOT_LARGE_CHAIN_1        = 3,
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

extern AnimationSet gActor503500Animation2DB14;

extern AnimationSet gActor503500Animation2E4DC;

extern AnimationSet gActor503500Animation2EF88;

extern AnimationSet gActor503500Animation2FC70;

extern AnimationSet gActor503500Animation306E0;

extern AnimationSet gActor503500Animation30F1C;

extern AnimationSet gActor503500Animation31788;

extern AnimationSet gActor503500Animation31D8C;

extern AnimationSet gActor503500Animation32E24;

extern AnimationSet gActor503500Animation333DC;

extern AnimationSet gActor503500Animation33C14;

extern AnimationSet gActor503500Animation33EBC;

extern AnimationSet gActor503500Animation341D8;

extern AnimationSet gActor503500Animation350C8;

extern AnimationSet gActor503500Animation35390;

extern AnimationSet gActor503500Animation356DC;

extern AnimationSet gActor503500Animation38AE0;

extern AnimationSet gActor503500Animation3A190;

extern AnimationSet gActor503500Animation3C968;

extern DamageAttack* D_actor_503500_8016E7CC[1];

extern DamageAttack* D_actor_503500_8016E7D0[1];

extern DamageAttack* D_actor_503500_8016E7D4[2];

extern DamageAttack* D_actor_503500_8016E7DC[1];

extern EnemyParams D_actor_503500_8016E7EC[17];

extern s8 D_actor_503500_8016E8FC[20];

extern u8 D_actor_503500_8016E910[20];

extern TaskDesc D_actor_503500_8016E924[17];

extern Task* D_actor_503500_80176558;

extern ActorTransform D_actor_503500_8017655C;

extern AnimationSet** D_actor_503500_8016EAB8[2];

extern AnimationPlayRequest D_actor_503500_8016EAC0[1];

extern AnimationPlayRequest D_actor_503500_8016EAD4;

extern SVECTOR D_actor_503500_8016EC50;

extern Actor503500AttackChoice** D_actor_503500_8016EF10[2][3];

extern s16* D_actor_503500_8016EF28[2][3];

extern s16 D_actor_503500_8016EF40[4];

extern s16 D_actor_503500_8016EF48[4];

extern s16 D_actor_503500_8016EF50[4];

extern SVECTOR D_actor_503500_8016EF58[7];

extern WorldCollisionGrid D_actor_503500_8016F03C;

extern SVECTOR D_actor_503500_8016F060;

extern SVECTOR D_actor_503500_8016F068;

extern SVECTOR D_actor_503500_8016F070;

extern SVECTOR D_actor_503500_8016F078[3];

extern SVECTOR D_actor_503500_8016F090[2];

extern SVECTOR D_actor_503500_8016F0A0[1];

extern SVECTOR D_actor_503500_8016F0A8[1];

extern SVECTOR D_actor_503500_8016F0B0;

extern SVECTOR D_actor_503500_8016F0B8[2];

extern SVECTOR D_actor_503500_8016F0C8;

extern SVECTOR D_actor_503500_8016F0D0[3];

extern s32 D_actor_503500_8016F0E8[2];

extern SVECTOR D_actor_503500_8016F0F0[2];

extern RECT D_actor_503500_8016F100;

extern SVECTOR D_actor_503500_8016F108[4];

extern SVECTOR D_actor_503500_8016F128[1][3];

extern RECT D_actor_503500_8016F148[2][2];

extern SVECTOR D_actor_503500_8016F168[9];

extern SVECTOR D_actor_503500_8016F1B0;

extern SVECTOR D_actor_503500_8016F1B8[18];

extern SVECTOR D_actor_503500_8016F248[2];

extern SVECTOR D_actor_503500_8016F258;

extern SVECTOR D_actor_503500_8016F260;

extern SVECTOR D_actor_503500_8016F278[3];

extern SVECTOR D_actor_503500_8016F290[9];

extern SVECTOR D_actor_503500_8016F2D8;

extern s16 D_actor_503500_8016F2E0[6];

extern SVECTOR D_actor_503500_8016F2EC[6];

extern SVECTOR D_actor_503500_8016F31C[9];

extern RECT D_actor_503500_8016F364;

extern SVECTOR D_actor_503500_8016F36C;

extern SVECTOR D_actor_503500_8016F374[6];

extern RECT D_actor_503500_8016F3A4;

extern SVECTOR D_actor_503500_8016F3AC[4];

extern SVECTOR D_actor_503500_8016F3CC[4];

extern SVECTOR D_actor_503500_8016F3EC;

extern SVECTOR D_actor_503500_8016F3F4[4];

extern SVECTOR D_actor_503500_8016F414[4];

extern s16 D_actor_503500_8016F434[10];

extern SVECTOR D_actor_503500_8016F448[3];

extern s32 D_actor_503500_80171464[2];

extern TaskDesc D_actor_503500_8017146C;

extern SVECTOR D_actor_503500_80171478;

extern SVECTOR D_actor_503500_80171480[2];

extern s8 D_actor_503500_80171490[56];

extern s32 D_actor_503500_801714DC;

extern AnimationPlayRequest D_actor_503500_801714E0[2];

extern AnimationPlayRequest D_actor_503500_80171508[2];

extern AnimationPlayRequest D_actor_503500_80171530;

extern GameActorButtonPressHold D_actor_503500_80171544;

extern RECT D_actor_503500_8017155C;

extern SVECTOR D_actor_503500_80171564[5];

extern SVECTOR D_actor_503500_8017158C;

extern SVECTOR D_actor_503500_80171594;

extern PadScriptCmd D_actor_503500_8017159C[2];

extern PadScriptVibrationSegment D_actor_503500_801715A4[2];

extern SVECTOR D_actor_503500_801715AC;

extern SVECTOR D_actor_503500_801715B4;

extern s32 D_actor_503500_801715BC[2];

extern TaskDesc D_actor_503500_8016E9F0[5];

extern TaskMessageEntry D_actor_503500_8016EA2C[];

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

void func_actor_503500_8013BE8C(Task* task);

void func_actor_503500_8013CA8C(Task* task);

void func_actor_503500_8013DBF4(Task* task);

void func_actor_503500_8013EC64(Task* task);

void func_actor_503500_8013FA1C(Task* task);

void func_actor_503500_80142370(Task* task);

void func_actor_503500_801442A8(Task* task);

void func_actor_503500_80144890(Task* task);

void func_actor_503500_80144E34(Task* task);

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

void func_actor_503500_80137238(Task*);

void func_actor_503500_801384D4(Task*);

void func_actor_503500_8013AD0C(Task*);

void func_actor_503500_80143AC0(Task*);

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

s32 func_actor_503500_80135B74(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);

s32 func_actor_503500_80137088(Task* task, s32 msgId, ActorTransform* args, s32 arg3);

s32 func_actor_503500_80137158(Task*, s32, s32, s32);

#endif // SRC_ACTORS_ACTOR_503500_ACTOR_503500_PRIVATE_H
