#include "actor_503500_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"
#include "actors/actor_503500.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"
#include "../../shared/actor/actor_motion.h"
#include "../../shared/actor/actor_messages.h"

/// Values of `_Actor503500YellowFlashAttackWork::phase`.
enum {
    ACTOR_503500_YELLOW_FLASH_ATTACK_CHARGE,   // Counting the charge; the sphere is off
    ACTOR_503500_YELLOW_FLASH_ATTACK_STRIKE,   // The sphere is pair-tested, if the player was high enough when the charge ended
    ACTOR_503500_YELLOW_FLASH_ATTACK_FINISHED, // The sphere is off again and the task moves on to its exit state
};

/// Work block of the yellow-flash attack task: the attack that charges under
/// the `EFFECT_SHELTER_R48_RING_FLASH_YELLOW` effect and then strikes the
/// player wherever they stand.
///
/// The damage comes from a sphere carried on the player's own coordinate
/// rather than on the attacker's, so position does not avoid it; it is only
/// switched on when the charge ends with the player more than 1000 units above
/// the room's origin, and only for the length of the strike.
typedef struct {
    WorldCollisionBody    body;        // Attack sphere (radius 300) on the player's coordinate; pair-tested only during the strike phase
    WorldCollisionContact contacts[1]; // Contact table of `body`, emptied every frame
    Task*                 effectTask;  // Task of the charge effect, made a child of the attack task; never read back
    s16                   phaseFrames; // Frames spent in `phase`
    byte                  field_3E[2]; // Never accessed; role unproven
    s8                    phase;       // (0 charge, 1 strike, 2 finished): `ACTOR_503500_YELLOW_FLASH_ATTACK_*`
} _Actor503500YellowFlashAttackWork;
STATIC_ASSERT_SIZEOF(_Actor503500YellowFlashAttackWork, 0x44);

/// Pink capsule radius at its origin, in game-coordinate units, at full Q12 scale.
///
/// Used both at initialization and as the near-end base during shrinking.
enum { ACTOR_503500_PINK_FLASH_ATTACK_NEAR_RADIUS = 1000 };

/// Values of `_Actor503500PinkFlashAttackWork::phase`.
enum {
    ACTOR_503500_PINK_FLASH_ATTACK_CHARGE,           // Counting the charge; both capsules are off
    ACTOR_503500_PINK_FLASH_ATTACK_HOLD,             // The first capsule is pair-tested, still pointing straight ahead
    ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_ACCELERATE, // Both capsules are pair-tested and turn away from each other, faster every frame
    ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_DECELERATE, // The turn slows by the same amount every frame until it stops
    ACTOR_503500_PINK_FLASH_ATTACK_FADE,             // Waiting for the radii to shrink to a quarter, where the capsule is switched off
    ACTOR_503500_PINK_FLASH_ATTACK_FINISHED,         // The task moves on to its exit state
};

/// Work block of a pink-flash attack task: one of the pair of tasks that
/// charge under the `EFFECT_SHELTER_R48_RING_FLASH_PINK` effect and then sweep
/// a capsule each to either side of the coordinate they were spawned on.
///
/// The two tasks are told apart by `Task::spawnArg1`. Task 0 spawns the
/// effect, switches its capsule on when the charge ends and sweeps towards
/// negative angles; task 1 spawns nothing, switches its capsule on when the
/// hold ends and sweeps towards positive ones. Each capsule starts straight
/// ahead, from the coordinate's origin to a point 6000 units along its Z axis
/// and 2000 along its Y, and is switched off at its first contact with the
/// player, so it lands once.
typedef struct {
    WorldCollisionBody    body;                 // Attack capsule on the task's own coordinate; pair-tested from the end of the charge (task 0) or of the hold (task 1) until it touches the player or its radii have shrunk to a quarter
    WorldCollisionCapsule capsule;              // Shape of `body`: from the coordinate's origin (radius 1000) to a far end (radius 2000) turned by `sweepRotation`; from the sweep on the radii are `radiusScale` of 1000 and 3000
    WorldCollisionContact contacts[4];          // Contact table of `capsule`, emptied every frame
    Task*                 effectTask;           // Task of the charge effect, made a child of the attack task; NULL in task 1, which spawns none; never read back
    MATRIX                sweepRotation;        // Rotation about Y by `sweepAngle` that places the capsule's far end; identity until the sweep, and only its rotation part is ever set or used
    Fixed16               sweepAngle;           // Angle the capsule has swept from straight ahead, 16.16 with 4096 to the turn; negative in task 0, positive in task 1
    s32                   sweepAngularVelocity; // Added to `sweepAngle` every frame of the sweep, in the same 16.16 units
    s16                   radiusScale;          // Scale of the capsule's radii, `ONE` for full size; shrinks every frame from the sweep on, down to an eighth
    s16                   phaseFrames;          // Frames spent in `phase`; counted during the charge and the hold only
    byte                  field_C8[4];          // Never accessed; role unproven
    s8                    phase;                // (0 charge, 1 hold, 2 sweep accelerating, 3 sweep decelerating, 4 fade, 5 finished): `ACTOR_503500_PINK_FLASH_ATTACK_*`
} _Actor503500PinkFlashAttackWork;
STATIC_ASSERT_SIZEOF(_Actor503500PinkFlashAttackWork, 0xD0);

/// Sound selector in SOUND_BANK_BRAHMAN used while the pink capsules sweep.
enum { ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_SOUND = 0x0B };

/// Values of `_Actor503500OrangeFlashAttackWork::phase`.
enum {
    ACTOR_503500_ORANGE_FLASH_ATTACK_CHARGE, // Counting the charge under a light pad rumble; the capsule is off
    ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE, // The capsule is pair-tested, outside scripted events, under a full pad rumble
    ACTOR_503500_ORANGE_FLASH_ATTACK_LINGER, // The capsule is off again; the task moves on to its exit state when the count ends
};

/// Work block of the orange-flash attack task: the attack that charges under
/// the `EFFECT_SHELTER_R48_RING_FLASH` effect and then strikes along the Z
/// axis of the coordinate it was spawned on.
///
/// The damage comes from a fixed tapered capsule reaching 6000 units out from
/// that coordinate. It is switched on when the charge ends and off again at
/// the first contact with the player, so it lands once. While
/// `GameSession::eventState` is set the attack is presentation only: it plays
/// a different charge sound and the capsule is never switched on.
typedef struct {
    WorldCollisionBody    body;        // Attack capsule on the task's own coordinate; pair-tested during the strike phase until it touches the player
    WorldCollisionCapsule capsule;     // Shape of `body`: fixed, from the coordinate's origin (radius 2000) to (0, 500, 6000) (radius 3000)
    WorldCollisionContact contacts[4]; // Contact table of `capsule`, emptied every frame
    Task*                 effectTask;  // Task of the charge effect, made a child of the attack task; never read back
    byte                  field_9C[8]; // Never accessed; role unproven
    s16                   phaseFrames; // Frames spent in `phase`
    byte                  field_A6[2]; // Never accessed; role unproven
    s8                    phase;       // (0 charge, 1 strike, 2 linger): `ACTOR_503500_ORANGE_FLASH_ATTACK_*`
} _Actor503500OrangeFlashAttackWork;
STATIC_ASSERT_SIZEOF(_Actor503500OrangeFlashAttackWork, 0xAC);

/// Values of `_Actor503500Actor361100Model06038Work::motion`.
enum {
    ACTOR_503500_ACTOR_361100_MODEL_06038_MOTION_IDLE,     // Nothing but the velocity moves the model
    ACTOR_503500_ACTOR_361100_MODEL_06038_MOTION_COLLAPSE, // The model squashes flat and burns away, then the task exits
};

/// Values of `_Actor503500Actor361100Model06038Work::motionStep`.
enum {
    ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_STEP_SAVE,   // Saves the root rotation and seeds the scale
    ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_STEP_WAIT,   // Holds the model at full height
    ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_STEP_SQUASH, // Rebuilds the root rotation squashed every frame and fires the cues
};

/// Work block of the actor drawn with `_gBrahmanTentacle`,
/// the nineteen-part model this package shares with actor_361100.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens with the head the nineteen-part play handler
/// runs on (`ActorMotion19PlayWork`), and the model object borrows
/// `model.light` and `model.color` for as long as the block lives. No
/// collision body is kept: the task exits through `enemyTaskExit` alone.
///
/// What follows moves the model in a straight line and ends it. Each tick
/// adds `velocity` to `carry` and moves the root coordinate by the whole
/// units that makes; an actor command sets `velocity` to one of two presets
/// or clears it, and nothing stops a move but another command. The command
/// that sets the second preset also starts the collapse, the sequence the
/// package's boss ends with too: after a wait the model is squashed flat
/// along its own Y axis, turns semi-transparent, burns as a corpse does and
/// its task exits.
///
/// `carry`, `motion` and `motionStep` sit where `ActorWalkState` would keep
/// them in a scripted walker's block, but this is not one: the saved rotation
/// and the velocity take the place of that type's target, velocity and
/// arrival fields. No code reads or writes the translation of
/// `unscaledRotation`, the word after `carry`, the fourth word of `velocity`,
/// or `pad_4C9`.
typedef struct {
    ActorAnimRig19  rig;              // Playback storage of the nineteen-part model; slots 1 to 18 are driven
    ActorModelState model;            // Clip and bank the rig plays, and the matrices the model is lit with
    MATRIX          unscaledRotation; // Root rotation saved as the collapse starts, put back every frame before the collapse scale is applied; only the rotation is kept
    Fixed16         carry[3];         // X, Y and Z displacement not yet applied; only the fractions survive a tick
    byte            pad_4AC[0x4];
    VECTOR          velocity;         // Displacement added each tick, in signed 16.16 units; zero while standing
    s16             motion;           // Handler the tick runs (0 idle, 1 collapse): `ACTOR_503500_ACTOR_361100_MODEL_06038_MOTION_*`
    s16             motionStep;       // Step of the collapse (0 save, 1 wait, 2 squash): `ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_STEP_*`
    s16             motionStepFrames; // Frames spent in `motionStep`; the wait and the squash each count from 0
    s16             collapseScaleY;   // Vertical scale during the collapse, `ONE` down to an eighth
    s8              freeCountdown;    // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    byte            pad_4C9[0x3];
} _Actor503500Actor361100Model06038Work;
STATIC_ASSERT_SIZEOF(_Actor503500Actor361100Model06038Work, 0x4CC);

static void _actor503500ExitTentacle(Task* task);
static void _actor503500BindTentacleLighting(Task* task);

static void _actor503500PinkFlashAttackExit(Task* task);
static void _actor503500PinkFlashAttackUpdatePhase(Task* task);
static void _actor503500PinkFlashAttackSweepContacts(Task* task);
static void _actor503500YellowFlashAttackUpdatePhase(Task* task);
static void _actor503500YellowFlashAttackExit(Task* task);
static void _actor503500YellowFlashAttackClearContacts(Task* task);
static void _actor503500OrangeFlashAttackUpdatePhase(Task* task);
static void _actor503500OrangeFlashAttackSweepContacts(Task* task);

static void _actor503500OrangeFlashAttackExit(Task* task);
static void _actor503500CollapseTentacle(Task* task);
static void _actor503500TentacleIdle(Task* unusedTask);

static void _actor503500PinkFlashAttackInit(Task* task);
static void _actor503500PinkFlashAttackUpdate(Task* task);
static void _actor503500YellowFlashAttackInit(Task* task);
static void _actor503500YellowFlashAttackUpdate(Task* task);
static void _actor503500OrangeFlashAttackInit(Task* task);
static void _actor503500OrangeFlashAttackUpdate(Task* task);
static void _actor503500InitTentacle(Task* task);
static void _actor503500TickTentacle(Task* task);

/// Pink-flash task handlers for states 0 initialize, 1 update and 2 exit.
///
/// Copied by value before unchecked state dispatch; every slot takes a live Task*.
static const TaskFuncTable3 _gActor503500PinkFlashAttackStateHandlers = {
    {
        _actor503500PinkFlashAttackInit,
        _actor503500PinkFlashAttackUpdate,
        _actor503500PinkFlashAttackExit,
    },
};

static AnimationSet _gBrahmanTentacleMoveClip;
static AnimationSet _gBrahmanTentacleCollapseClip;
static TmdSource    _gBrahmanTentacle;
static s32          _actorMsgPlaceEuler(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);
static s32          _actor503500SetTentacleDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg);
static s32          _actor503500ApplyTentacleCommand(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg);
static void         _actor503500TentacleTask(Task* task);

static AnimationSet _gStingerPuppetPlayerKnockbackFrontClip;
static AnimationSet _gStingerPuppetPlayerKnockbackBackClip;
static AnimationSet _gStingerPuppetPlayerKnockbackRecoverFrontClip;
static AnimationSet _gStingerPuppetPlayerKnockbackRecoverBackClip;

const TaskMessageEntry gActor503500BossMessageHandlers[] SECTION(".data") = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, actor503500HandlePlayAnimation },
    { ACTOR_MESSAGE_PLACE, actor503500HandlePlaceBoss },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actor503500HandleSetBossModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, actor503500HandleBossCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

enum {
    ACTOR_503500_BOSS_SOURCE_0_SET_COUNT = 20,
    ACTOR_503500_BOSS_SOURCE_1_SET_COUNT = 5,
    ACTOR_503500_TENTACLE_SET_COUNT      = 3,
};

/// Boss source 0: clip ids 1..19, with id 0 reserved as NULL.
#include "assets/enemies/brahman/boss_bank.inc"

/// Boss source 1: clip ids 1..4, with id 0 reserved as NULL.
#include "assets/enemies/brahman/boss_scene_bank.inc"

AnimationSet** gActor503500BossAnimationSources[ACTOR_503500_BOSS_ANIMATION_SOURCE_COUNT] = {
    _gActor503500BossAnimationSets0,
    _gActor503500BossAnimationSets1,
};

AnimationPlayRequest gActor503500BossAnimationPresets[ACTOR_503500_BOSS_ANIMATION_PRESET_COUNT] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 11, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 12, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 14, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 15, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 17, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 18, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

SVECTOR gActor503500BossTargetOffset = { 0, -500, 1600, 0 };

/// Set-0 upper-height attacks for absolute bearing 0..700 (4096 units per turn).
///
/// Weights accumulate against a random byte; a NULL callback ends the list.
static Actor503500AttackChoice _gActor503500AttackChoicesSet0UpperBearing0[4] = {
    { actor503500AttackPinkOrYellowFlash, 127 },
    { actor503500AttackArmStrike, 79 },
    { actor503500AttackLargeOrbPair, 47 },
    { NULL, 0 },
};

/// Set-0 upper-height attacks shared by absolute bearing bands 701..1000 and 1001..1300.
///
/// Angles use 4096 units per turn. Weights accumulate against a random byte;
/// a NULL callback ends the list.
static Actor503500AttackChoice _gActor503500AttackChoicesSet0UpperBearing1And2[4] = {
    { actor503500AttackArmStrike, 127 },
    { actor503500AttackYellowFlash, 79 },
    { actor503500AttackLargeOrbPair, 47 },
    { NULL, 0 },
};

/// Set-0 upper-height attacks for absolute bearing 1301..2048 (4096 units per turn).
///
/// Weights accumulate against a random byte; a NULL callback ends the list.
static Actor503500AttackChoice _gActor503500AttackChoicesSet0UpperBearing3[5] = {
    { actor503500AttackYellowFlash, 79 },
    { actor503500AttackSideChain, 63 },
    { actor503500AttackChainBases, 47 },
    { actor503500AttackLargeOrbPair, 31 },
    { NULL, 0 },
};

/// Set-0 middle-height attacks for absolute bearing 0..700 (4096 units per turn).
///
/// Weights accumulate against a random byte; a NULL callback ends the list.
static Actor503500AttackChoice _gActor503500AttackChoicesSet0MiddleBearing0[3] = {
    { actor503500AttackPinkOrYellowFlash, 159 },
    { actor503500AttackLargeOrbPair, 95 },
    { NULL, 0 },
};

/// Set-0 middle-height attacks for absolute bearing 701..1000.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet0MiddleBearing1[3] __attribute__((section(".data"))) = {
    { actor503500AttackLargeOrbPair, 159 },
    { actor503500AttackYellowFlash, 95 },
    { NULL, 0 },
};

/// Set-0 middle-height attacks for absolute bearing 1001..1300.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet0MiddleBearing2[4] __attribute__((section(".data"))) = {
    { actor503500AttackLargeOrbPair, 111 },
    { actor503500AttackSideChain, 79 },
    { actor503500AttackYellowFlash, 63 },
    { NULL, 0 },
};

/// Set-0 middle-height attacks for absolute bearing 1301..2048.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet0MiddleBearing3[4] __attribute__((section(".data"))) = {
    { actor503500AttackSideChain, 159 },
    { actor503500AttackChainBases, 63 },
    { actor503500AttackYellowFlash, 31 },
    { NULL, 0 },
};

/// Set-0 lower-height attacks for absolute bearing 0..300.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet0LowerBearing0[4] __attribute__((section(".data"))) = {
    { actor503500AttackSmallOrbVolley, 95 },
    { actor503500AttackPinkOrYellowFlash, 95 },
    { actor503500AttackLargeOrbPair, 63 },
    { NULL, 0 },
};

/// Set-0 lower-height attacks for absolute bearing 301..1300.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet0LowerBearing1And2[3] __attribute__((section(".data"))) = {
    { actor503500AttackLargeOrbPair, 159 },
    { actor503500AttackSideChain, 95 },
    { NULL, 0 },
};

/// Set-0 lower-height attacks for absolute bearing 1301..2048.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet0LowerBearing3[3] __attribute__((section(".data"))) = {
    { actor503500AttackSideChain, 223 },
    { actor503500AttackLargeOrbPair, 31 },
    { NULL, 0 },
};

/// Set-0 upper-height attack lists, in ascending absolute-bearing bands.
///
/// Indexed by bearing band 0..3; the matching limits select inclusive ends.
/// Lists and callbacks remain live for the actor overlay lifetime.
static Actor503500AttackChoicesByBearing _gActor503500AttackChoicesSet0Upper __attribute__((section(".data"))) = {
    _gActor503500AttackChoicesSet0UpperBearing0,
    _gActor503500AttackChoicesSet0UpperBearing1And2,
    _gActor503500AttackChoicesSet0UpperBearing1And2,
    _gActor503500AttackChoicesSet0UpperBearing3,
};

/// Inclusive absolute-bearing limits for set 0, upper player height.
///
/// Four ordered bands at 4096 units per turn; the final half-turn limit
/// covers the whole normalized bearing domain. Equality stays in that band.
static Actor503500AttackLimitsByBearing _gActor503500AttackBearingLimitsSet0Upper __attribute__((section(".data"))) = {
    700,
    1000,
    1300,
    ACTOR_TRANSFORM_ANGLE_HALF_TURN,
};

/// Set-0 middle-height attack lists, in ascending absolute-bearing bands.
///
/// Indexed by bearing band 0..3; the matching limits select inclusive ends.
/// Lists and callbacks remain live for the actor overlay lifetime.
static Actor503500AttackChoicesByBearing _gActor503500AttackChoicesSet0Middle __attribute__((section(".data"))) = {
    _gActor503500AttackChoicesSet0MiddleBearing0,
    _gActor503500AttackChoicesSet0MiddleBearing1,
    _gActor503500AttackChoicesSet0MiddleBearing2,
    _gActor503500AttackChoicesSet0MiddleBearing3,
};

/// Inclusive absolute-bearing limits for set 0, middle player height.
///
/// Four ordered bands at 4096 units per turn; the final half-turn limit
/// covers the whole normalized bearing domain. Equality stays in that band.
static Actor503500AttackLimitsByBearing _gActor503500AttackBearingLimitsSet0Middle __attribute__((section(".data"))) = {
    700,
    1000,
    1300,
    ACTOR_TRANSFORM_ANGLE_HALF_TURN,
};

/// Set-0 lower-height attack lists, in ascending absolute-bearing bands.
///
/// Indexed by bearing band 0..3; the matching limits select inclusive ends.
/// Lists and callbacks remain live for the actor overlay lifetime.
static Actor503500AttackChoicesByBearing _gActor503500AttackChoicesSet0Lower __attribute__((section(".data"))) = {
    _gActor503500AttackChoicesSet0LowerBearing0,
    _gActor503500AttackChoicesSet0LowerBearing1And2,
    _gActor503500AttackChoicesSet0LowerBearing1And2,
    _gActor503500AttackChoicesSet0LowerBearing3,
};

/// Inclusive absolute-bearing limits for set 0, lower player height.
///
/// Four ordered bands at 4096 units per turn; the final half-turn limit
/// covers the whole normalized bearing domain. Equality stays in that band.
static Actor503500AttackLimitsByBearing _gActor503500AttackBearingLimitsSet0Lower __attribute__((section(".data"))) = {
    300,
    1000,
    1300,
    ACTOR_TRANSFORM_ANGLE_HALF_TURN,
};

/// Set-1 upper-height attacks for absolute bearing 0..700.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet1UpperBearing0[4] __attribute__((section(".data"))) = {
    { actor503500AttackBody, 111 },
    { actor503500AttackPinkOrYellowFlash, 111 },
    { actor503500AttackArmStrike, 31 },
    { NULL, 0 },
};

/// Set-1 upper-height attacks for absolute bearing 701..1300.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet1UpperBearing1And2[3] __attribute__((section(".data"))) = {
    { actor503500AttackPinkOrYellowFlash, 159 },
    { actor503500AttackArmStrike, 95 },
    { NULL, 0 },
};

/// Set-1 upper-height attacks for absolute bearing 1301..2048.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet1UpperBearing3[2] __attribute__((section(".data"))) = {
    { actor503500AttackChainBases, 255 },
    { NULL, 0 },
};

/// Set-1 middle-height attacks for absolute bearing 0..700.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet1MiddleBearing0[3] __attribute__((section(".data"))) = {
    { actor503500AttackBody, 127 },
    { actor503500AttackPinkOrYellowFlash, 127 },
    { NULL, 0 },
};

/// Set-1 middle-height attacks for absolute bearing 701..1000.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet1MiddleBearing1[3] __attribute__((section(".data"))) = {
    { actor503500AttackLargeOrbPair, 159 },
    { actor503500AttackYellowFlash, 95 },
    { NULL, 0 },
};

/// Set-1 middle-height attacks for absolute bearing 1001..1300.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet1MiddleBearing2[4] __attribute__((section(".data"))) = {
    { actor503500AttackLargeOrbPair, 95 },
    { actor503500AttackSideChain, 79 },
    { actor503500AttackYellowFlash, 63 },
    { NULL, 0 },
};

/// Set-1 middle-height attacks for absolute bearing 1301..2048.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet1MiddleBearing3[4] __attribute__((section(".data"))) = {
    { actor503500AttackSideChain, 159 },
    { actor503500AttackChainBases, 63 },
    { actor503500AttackYellowFlash, 31 },
    { NULL, 0 },
};

/// Set-1 lower-height attacks for absolute bearing 0..500.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet1LowerBearing0[3] __attribute__((section(".data"))) = {
    { actor503500AttackBody, 127 },
    { actor503500AttackSmallOrbVolley, 127 },
    { NULL, 0 },
};

/// Set-1 lower-height attacks for absolute bearing 501..1300.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet1LowerBearing1And2[3] __attribute__((section(".data"))) = {
    { actor503500AttackLargeOrbPair, 159 },
    { actor503500AttackSideChain, 95 },
    { NULL, 0 },
};

/// Set-1 lower-height attacks for absolute bearing 1301..2048.
///
/// Angles use 4096 units per turn. The first cumulative weight reaching a
/// random byte wins; rolls beyond the total choose no attack. NULL ends the list.
static const Actor503500AttackChoice _gActor503500AttackChoicesSet1LowerBearing3[3] __attribute__((section(".data"))) = {
    { actor503500AttackSideChain, 127 },
    { actor503500AttackChainBases, 127 },
    { NULL, 0 },
};

/// Set-1 upper-height attack lists, in ascending absolute-bearing bands.
///
/// Indexed by bearing band 0..3; the matching limits select inclusive ends.
/// Lists and callbacks remain live for the actor overlay lifetime.
static Actor503500AttackChoicesByBearing _gActor503500AttackChoicesSet1Upper __attribute__((section(".data"))) = {
    _gActor503500AttackChoicesSet1UpperBearing0,
    _gActor503500AttackChoicesSet1UpperBearing1And2,
    _gActor503500AttackChoicesSet1UpperBearing1And2,
    _gActor503500AttackChoicesSet1UpperBearing3,
};

/// Inclusive absolute-bearing limits for set 1, upper player height.
///
/// Four ordered bands at 4096 units per turn; the final half-turn limit
/// covers the whole normalized bearing domain. Equality stays in that band.
static Actor503500AttackLimitsByBearing _gActor503500AttackBearingLimitsSet1Upper __attribute__((section(".data"))) = {
    700,
    1000,
    1300,
    ACTOR_TRANSFORM_ANGLE_HALF_TURN,
};

/// Set-1 middle-height attack lists, in ascending absolute-bearing bands.
///
/// Indexed by bearing band 0..3; the matching limits select inclusive ends.
/// Lists and callbacks remain live for the actor overlay lifetime.
static Actor503500AttackChoicesByBearing _gActor503500AttackChoicesSet1Middle __attribute__((section(".data"))) = {
    _gActor503500AttackChoicesSet1MiddleBearing0,
    _gActor503500AttackChoicesSet1MiddleBearing1,
    _gActor503500AttackChoicesSet1MiddleBearing2,
    _gActor503500AttackChoicesSet1MiddleBearing3,
};

/// Inclusive absolute-bearing limits for set 1, middle player height.
///
/// Four ordered bands at 4096 units per turn; the final half-turn limit
/// covers the whole normalized bearing domain. Equality stays in that band.
static Actor503500AttackLimitsByBearing _gActor503500AttackBearingLimitsSet1Middle __attribute__((section(".data"))) = {
    700,
    1000,
    1300,
    ACTOR_TRANSFORM_ANGLE_HALF_TURN,
};

/// Set-1 lower-height attack lists, in ascending absolute-bearing bands.
///
/// Indexed by bearing band 0..3; the matching limits select inclusive ends.
/// Lists and callbacks remain live for the actor overlay lifetime.
static Actor503500AttackChoicesByBearing _gActor503500AttackChoicesSet1Lower __attribute__((section(".data"))) = {
    _gActor503500AttackChoicesSet1LowerBearing0,
    _gActor503500AttackChoicesSet1LowerBearing1And2,
    _gActor503500AttackChoicesSet1LowerBearing1And2,
    _gActor503500AttackChoicesSet1LowerBearing3,
};

/// Inclusive absolute-bearing limits for set 1, lower player height.
///
/// Four ordered bands at 4096 units per turn; the final half-turn limit
/// covers the whole normalized bearing domain. Equality stays in that band.
static Actor503500AttackLimitsByBearing _gActor503500AttackBearingLimitsSet1Lower __attribute__((section(".data"))) = {
    500,
    1000,
    1300,
    ACTOR_TRANSFORM_ANGLE_HALF_TURN,
};

Actor503500AttackChoicesByBearing* const gActor503500AttackChoices[ACTOR_503500_ATTACK_SET_COUNT][ACTOR_503500_ATTACK_HEIGHT_BAND_COUNT] __attribute__((section(".data"))) = {
    { &_gActor503500AttackChoicesSet0Upper, &_gActor503500AttackChoicesSet0Middle, &_gActor503500AttackChoicesSet0Lower },
    { &_gActor503500AttackChoicesSet1Upper, &_gActor503500AttackChoicesSet1Middle, &_gActor503500AttackChoicesSet1Lower },
};

Actor503500AttackLimitsByBearing* gActor503500AttackBearingLimits[ACTOR_503500_ATTACK_SET_COUNT][ACTOR_503500_ATTACK_HEIGHT_BAND_COUNT] = {
    { &_gActor503500AttackBearingLimitsSet0Upper, &_gActor503500AttackBearingLimitsSet0Middle, &_gActor503500AttackBearingLimitsSet0Lower },
    { &_gActor503500AttackBearingLimitsSet1Upper, &_gActor503500AttackBearingLimitsSet1Middle, &_gActor503500AttackBearingLimitsSet1Lower },
};

enum {
    ACTOR_503500_SIDE_CHAIN_NEXT_FRAME_COOLDOWN  = 1,
    ACTOR_503500_SIDE_CHAIN_BASE_COOLDOWN_FRAMES = 100,
};

s16 gActor503500SideChainCooldownFrames[ACTOR_503500_SIDE_CHAIN_CANDIDATE_COUNT] = {
    ACTOR_503500_SIDE_CHAIN_NEXT_FRAME_COOLDOWN,
    ACTOR_503500_SIDE_CHAIN_NEXT_FRAME_COOLDOWN,
    ACTOR_503500_SIDE_CHAIN_NEXT_FRAME_COOLDOWN,
    ACTOR_503500_SIDE_CHAIN_BASE_COOLDOWN_FRAMES,
};

s16 gActor503500PositiveSideChainCandidates[ACTOR_503500_SIDE_CHAIN_CANDIDATE_COUNT] = {
    ACTOR_503500_SLOT_LARGE_CHAIN_0,
    ACTOR_503500_SLOT_LUNGING_CHAIN_0,
    ACTOR_503500_SLOT_LUNGING_CHAIN_1,
    ACTOR_503500_SLOT_CHAIN_BASE_0,
};

s16 gActor503500NegativeSideChainCandidates[ACTOR_503500_SIDE_CHAIN_CANDIDATE_COUNT] = {
    ACTOR_503500_SLOT_LARGE_CHAIN_1,
    ACTOR_503500_SLOT_LUNGING_CHAIN_3,
    ACTOR_503500_SLOT_LUNGING_CHAIN_2,
    ACTOR_503500_SLOT_CHAIN_BASE_1,
};

SVECTOR gActor503500PhaseTransitionPuffOffsets[7] = {
    { 0, -1000, 2000, 0 },
    { 0, -500, 2500, 0 },
    { 0, 0, 3000, 0 },
    { 200, -700, 2500, 0 },
    { -300, -200, 2200, 0 },
    { -200, -300, 2700, 0 },
    { 300, -800, 2500, 0 },
};

static SVECTOR _gActor503500BossBodyCollisionGridNormals[4] = {
#include "assets/actor_503500_collision_3D21C_normals.inc"
};

static SVECTOR _gActor503500BossBodyCollisionGridVerts[8] = {
#include "assets/actor_503500_collision_3D21C_verts.inc"
};

static WorldCollisionGridFace _gActor503500BossBodyCollisionGridFaces[4] = {
#include "assets/actor_503500_collision_3D21C_faces.inc"
};

static s16 _gActor503500BossBodyCollisionGridCells[10] = {
#include "assets/actor_503500_collision_3D21C_cells.inc"
};

/// Resolves a halfword offset into the boss-body grid's packed face-list pool.
///
/// Returns an s16* to a list of face indices ending in -1. The generated table
/// supplies list starts 0 and 6 within the ten-halfword pool. Evaluates
/// `halfwordOffset` once and captures the private pool. The collisionSource's
/// `cell_face_list_macro` binding selects this name for the included initializer;
/// the binding is undefined after the table.
#define ACTOR_503500_BOSS_BODY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gActor503500BossBodyCollisionGridCells[(halfwordOffset)])
static s16* _gActor503500BossBodyCollisionGridTable[2] = {
#include "assets/actor_503500_collision_3D21C_table.inc"
};
#undef ACTOR_503500_BOSS_BODY_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gActor503500BossBodyCollisionGrid = { NULL, _gActor503500BossBodyCollisionGridNormals, _gActor503500BossBodyCollisionGridVerts, _gActor503500BossBodyCollisionGridFaces, _gActor503500BossBodyCollisionGridTable, 2393, 1502, 2, 1, 4000, ARRAY_SIZE(_gActor503500BossBodyCollisionGridFaces) };

SVECTOR gActor503500PinkFlashEmitterRootOffset = { 0, -80, 596, 0 };

SVECTOR gActor503500PinkFlashEmitterTargetOffset = { 0, -80, 1000, 0 };

SVECTOR gActor503500PinkFlashEmitterAttackOffset = { 0, -200, 800, 0 };

SVECTOR gActor503500PinkFlashEmitterDeathSmokeOffsets[3] = {
    { 0, -300, -300, 0 },
    { 0, 300, 300, 0 },
    { 0, 300, -300, 0 },
};

SVECTOR gActor503500LargeChainRootOffsets[ACTOR_503500_SIDE_COUNT] = {
    { 1300, -1468, -2700, 0 },
    { -1300, -1468, -2700, 0 },
};

SVECTOR gActor503500LargeChainRootAngles[ACTOR_503500_SIDE_COUNT] = {
    { -364, 1479, 0, 0 },
    { -364, -1479, 0, 0 },
};

SVECTOR gActor503500LargeChainTipTargetOffset = { 0, 0, 400, 0 };

SVECTOR gActor503500LargeChainRestTipOffsets[ACTOR_503500_SIDE_COUNT] = {
    { 3500, -1500, -5200, 0 },
    { -3500, -1500, -5200, 0 },
};

const SVECTOR gActor503500LargeChainIdleOrbitOffset __attribute__((section(".data"))) = { 0, 0, 1000, 0 };

SVECTOR gActor503500LargeChainDeathSmokeOffsets[3] = {
    { 0, -600, 0, 0 },
    { -600, 0, 0, 0 },
    { 600, 0, 0, 0 },
};

/// Boss arm roots shared by the matching arm and large-orb-emitter sides.
enum {
    ACTOR_503500_BOSS_ARM_MOUNT_PART_SIDE_0 = 11,
    ACTOR_503500_BOSS_ARM_MOUNT_PART_SIDE_1 = 5,
};

const s32 gActor503500LargeOrbEmitterParentParts[ACTOR_503500_SIDE_COUNT] __attribute__((section(".data"))) = {
    ACTOR_503500_BOSS_ARM_MOUNT_PART_SIDE_0,
    ACTOR_503500_BOSS_ARM_MOUNT_PART_SIDE_1,
};

SVECTOR gActor503500LargeOrbEmitterTargetOffsets[ACTOR_503500_SIDE_COUNT] = {
    { 300, 0, 0, 0 },
    { -300, 0, 0, 0 },
};

RECT gActor503500LargeOrbEmitterVramRowSourceRect = { 0, 253, 256, 1 };

SVECTOR gActor503500LargeOrbLaunchOffsets[ACTOR_503500_LARGE_ORB_LAUNCH_POINT_COUNT] = {
    { 2000, 0, 1000, 0 },
    { 2000, 0, -1000, 0 },
};

const SVECTOR gActor503500LargeOrbLaunchAngles[ACTOR_503500_LARGE_ORB_LAUNCH_POINT_COUNT * ACTOR_503500_LARGE_ORB_LAUNCH_ANGLE_COUNT] __attribute__((section(".data"))) = {
    { -750, 300, 0, 0 },
    { -750, 600, 0, 0 },
    { -750, 900, 0, 0 },
    { -750, 1700, 0, 0 },
    { -750, 1400, 0, 0 },
    { -750, 1100, 0, 0 },
};

Actor503500LargeOrbRepaintSources gActor503500LargeOrbEmitterDeathRepaintSources[ACTOR_503500_SIDE_COUNT] = {
    { { 640, 256, 64, 256 }, { 0, 252, 256, 1 } },
    { { 704, 256, 64, 256 }, { 0, 261, 256, 1 } },
};

SVECTOR gActor503500LargeOrbEmitterDeathEffectOffsets[9] = {
    { 1600, 0, 1000, 0 },
    { 1200, -400, 1000, 0 },
    { 1600, 400, 1000, 0 },
    { 1400, 0, 1400, 0 },
    { 1000, -400, 1400, 0 },
    { 1400, 400, 1400, 0 },
    { 1800, 0, 600, 0 },
    { 1400, -400, 600, 0 },
    { 1800, 400, 600, 0 },
};

SVECTOR gActor503500RearPartTargetOffset = { 0, 500, -1500, 0 };

SVECTOR gActor503500RearPartDeathEffectOffsets[18] = {
    { 0, -400, -2000, 0 },
    { -800, -200, -1800, 0 },
    { 800, -200, -2200, 0 },
    { -1200, -100, -1400, 0 },
    { 1200, -100, -1400, 0 },
    { -1600, 0, -1200, 0 },
    { 1600, 0, -1200, 0 },
    { 200, 100, -2800, 0 },
    { -200, 100, -3100, 0 },
    { 0, -200, -2000, 0 },
    { -1200, 0, -2200, 0 },
    { 1200, 0, -2600, 0 },
    { -1600, 100, -1800, 0 },
    { 1600, 100, -1800, 0 },
    { -2000, 200, -1600, 0 },
    { 2000, 200, -1600, 0 },
    { 600, 300, -3200, 0 },
    { -600, 300, -3500, 0 },
};

SVECTOR gActor503500ChainBaseTargetOffsets[ACTOR_503500_SIDE_COUNT] = {
    { 1300, -1468, -2800, 0 },
    { -1300, -1468, -2800, 0 },
};

const SVECTOR gActor503500ChainBaseLaunchOffset __attribute__((section(".data"))) = { 1300, -1468, -2700, 0 };

SVECTOR gActor503500ChainBaseLaunchAngles = { 500, 1700, 0, 0 };

SVECTOR D_actor_503500_8016F268[2] = {
    { 500, 1400, 0, 0 },
    { 500, 1100, 0, 0 },
};

SVECTOR gActor503500ChainBaseDeathEffectOffsetsSide0[ACTOR_503500_CHAIN_BASE_DEATH_OFFSET_COUNT] = {
    { 1300, -1468, -2700, 0 },
    { 1700, -1668, -2900, 0 },
    { 2100, -1868, -3100, 0 },
};

SVECTOR gActor503500ChainBaseDeathEffectOffsetsSide1[9] = {
    { -1300, -1468, -2700, 0 },
    { -1700, -1668, -2900, 0 },
    { -2100, -1868, -3100, 0 },
    { 2000, -750, -2600, 0 },
    { 2400, -800, -2800, 0 },
    { 2800, -850, -3000, 0 },
    { -2000, -750, -2600, 0 },
    { -2400, -800, -2800, 0 },
    { -2800, -850, -3000, 0 },
};

SVECTOR gActor503500SmallOrbEmitterTargetOffset = { 0, 800, 2000, 0 };

const s16 gActor503500SmallOrbEmitterLaunchSpeeds[ACTOR_503500_SMALL_ORB_EMITTER_VOLLEY_SHOTS] __attribute__((section(".data"))) = {
    224 * ACTOR_503500_SMALL_ORB_LAUNCH_SPEED_ONE,
    208 * ACTOR_503500_SMALL_ORB_LAUNCH_SPEED_ONE,
    232 * ACTOR_503500_SMALL_ORB_LAUNCH_SPEED_ONE,
    216 * ACTOR_503500_SMALL_ORB_LAUNCH_SPEED_ONE,
    224 * ACTOR_503500_SMALL_ORB_LAUNCH_SPEED_ONE,
    232 * ACTOR_503500_SMALL_ORB_LAUNCH_SPEED_ONE,
};

SVECTOR gActor503500SmallOrbEmitterLaunchAngles[ACTOR_503500_SMALL_ORB_EMITTER_VOLLEY_SHOTS] = {
    { 712, 0, 0, 0 },
    { 688, 24, 0, 0 },
    { 780, 88, 0, 0 },
    { 745, -46, 0, 0 },
    { 656, -20, 0, 0 },
    { 780, -80, 0, 0 },
};

SVECTOR gActor503500SmallOrbEmitterDeathEffectOffsets[9] = {
    { 0, 800, 3000, 0 },
    { -50, 1000, 2800, 0 },
    { 100, 1200, 2700, 0 },
    { -400, 600, 3000, 0 },
    { 450, 800, 2800, 0 },
    { -600, 1000, 2700, 0 },
    { 400, 200, 3000, 0 },
    { -50, 1200, 2800, 0 },
    { 600, 600, 2700, 0 },
};

RECT gActor503500SmallOrbEmitterDeathRepaintSourceRect = { 358, 480, 25, 31 };

SVECTOR gActor503500YellowFlashEmitterTargetOffset = { 0, 0, 800, 0 };

SVECTOR gActor503500YellowFlashEmitterDeathEffectOffsets[6] = {
    { 0, 250, 800, 0 },
    { 400, 300, 1000, 0 },
    { -400, 350, 1100, 0 },
    { 400, -50, 1400, 0 },
    { -400, -100, 1200, 0 },
    { 0, -150, 1000, 0 },
};

RECT gActor503500YellowFlashEmitterDeathRepaintSourceRect = { 352, 256, 31, 40 };

SVECTOR gActor503500LungingChainRootOffsets[ACTOR_503500_LUNGING_CHAIN_COUNT] = {
    { 1300, -1468, -2700, 0 },
    { 1300, -1468, -2700, 0 },
    { -1300, -1468, -2700, 0 },
    { -1300, -1468, -2700, 0 },
};

SVECTOR gActor503500LungingChainRootAngles[ACTOR_503500_LUNGING_CHAIN_COUNT] = {
    { -364, 1479, 0, 0 },
    { -364, 1479, 0, 0 },
    { -364, -1479, 0, 0 },
    { -364, -1479, 0, 0 },
};

SVECTOR gActor503500LungingChainTipTargetOffset = { 0, 0, 400, 0 };

SVECTOR gActor503500LungingChainTipAttackOffsets[ACTOR_503500_LUNGING_CHAIN_COUNT] = {
    { -200, 0, 400, 0 },
    { 200, 0, 400, 0 },
    { -200, 0, 400, 0 },
    { 200, 0, 400, 0 },
};

SVECTOR gActor503500LungingChainRestTipOffsets[ACTOR_503500_LUNGING_CHAIN_COUNT] = {
    { 3500, -3000, -5200, 0 },
    { 1800, -2000, -5200, 0 },
    { -1800, -2000, -5200, 0 },
    { -3500, -3000, -5200, 0 },
};

const s16 gActor503500LungingChainCurlPitchByPart[10] SECTION(".data") = {
    0,
    10,
    30,
    60,
    80,
    120,
    180,
    200,
    300,
    0,
};

SVECTOR gActor503500LungingChainDeathSmokeOffsets[3] = {
    { 0, -600, 0, 0 },
    { -400, 0, 0, 0 },
    { 400, 0, 0, 0 },
};

#include "assets/enemies/brahman/clips_3.inc"

s32 gActor503500ArmParentParts[ACTOR_503500_SIDE_COUNT] = {
    ACTOR_503500_BOSS_ARM_MOUNT_PART_SIDE_0,
    ACTOR_503500_BOSS_ARM_MOUNT_PART_SIDE_1,
};

TaskDesc gActor503500KnockbackTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, actor503500KnockbackTask, { .value = 0 } };

const SVECTOR gActor503500ArmRootOffset SECTION(".data") = { 0 };

SVECTOR gActor503500ArmTargetOffsets[ACTOR_503500_SIDE_COUNT] = {
    { 500, 0, 0, 0 },
    { -500, 0, 0, 0 },
};

enum {
    ACTOR_503500_ARM_ATTACHMENT_NORMAL     = 0,
    ACTOR_503500_ARM_ATTACHMENT_DESTROYING = 1,
};

const s8 gActor503500ArmDestroyingAttachmentRows[56] __attribute__((section(".data"))) = {
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_DESTROYING,
    ACTOR_503500_ARM_ATTACHMENT_DESTROYING,
    ACTOR_503500_ARM_ATTACHMENT_DESTROYING,
    ACTOR_503500_ARM_ATTACHMENT_DESTROYING,
    ACTOR_503500_ARM_ATTACHMENT_DESTROYING,
    ACTOR_503500_ARM_ATTACHMENT_DESTROYING,
    ACTOR_503500_ARM_ATTACHMENT_DESTROYING,
    ACTOR_503500_ARM_ATTACHMENT_DESTROYING,
    ACTOR_503500_ARM_ATTACHMENT_DESTROYING,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_DESTROYING,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
    ACTOR_503500_ARM_ATTACHMENT_NORMAL,
};

#include "assets/enemies/brahman/player_knockback_bank.inc"

enum {
    ACTOR_503500_KNOCKBACK_FALL_SET_0     = 1,
    ACTOR_503500_KNOCKBACK_FALL_SET_1     = 2,
    ACTOR_503500_KNOCKBACK_RECOVERY_SET_0 = 3,
    ACTOR_503500_KNOCKBACK_RECOVERY_SET_1 = 4,
    ACTOR_503500_KNOCKBACK_BLEND_FRAMES   = 3,
};

const AnimationPlayRequest gActor503500PlayerKnockbackFallRequests[ACTOR_503500_KNOCKBACK_FALL_DIRECTION_COUNT] SECTION(".data") = {
    { { .sets = gActor503500PlayerKnockbackAnimationSets }, ACTOR_503500_KNOCKBACK_FALL_SET_0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .sets = gActor503500PlayerKnockbackAnimationSets }, ACTOR_503500_KNOCKBACK_FALL_SET_1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
};

const AnimationPlayRequest gActor503500PlayerKnockbackRecoveryRequests[ACTOR_503500_KNOCKBACK_FALL_DIRECTION_COUNT] SECTION(".data") = {
    { { .sets = gActor503500PlayerKnockbackAnimationSets }, ACTOR_503500_KNOCKBACK_RECOVERY_SET_0, ANIMATION_BLEND_INTERPOLATE, ACTOR_503500_KNOCKBACK_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .sets = gActor503500PlayerKnockbackAnimationSets }, ACTOR_503500_KNOCKBACK_RECOVERY_SET_1, ANIMATION_BLEND_INTERPOLATE, ACTOR_503500_KNOCKBACK_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
};

const AnimationPlayRequest gActor503500PlayerKnockbackRestoreRequest SECTION(".data") = { { .sets = gActor503500PlayerKnockbackAnimationSets }, ACTOR_503500_KNOCKBACK_RESTORE_SET, ANIMATION_BLEND_INTERPOLATE, ACTOR_503500_KNOCKBACK_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

const GameActorButtonPressHold gActor503500PlayerKnockbackHoldRequest SECTION(".data") = { .pressCount = 0 };

RECT gActor503500ArmDeathVramRowSourceRect = { 0, 261, 256, 1 };

SVECTOR gActor503500ArmDeathSmokeOffsets[5] = {
    { 0, 0, 0, 0 },
    { 0, -500, 500, 0 },
    { 0, -1000, 0, 0 },
    { 500, 0, 0, 0 },
    { -500, 0, 0, 0 },
};

SVECTOR gActor503500ArmDeathFallPuffOffsetSide0 = { -1000, 0, 0, 0 };

SVECTOR gActor503500ArmDeathFallPuffOffsetSide1 = { 1000, 0, 0, 0 };

enum {
    ACTOR_503500_KNOCKBACK_PAD_HOLD_SEGMENT   = 0,
    ACTOR_503500_KNOCKBACK_PAD_FULL_SEGMENT   = 1,
    ACTOR_503500_KNOCKBACK_PAD_SEGMENT_FRAMES = 8,
    ACTOR_503500_KNOCKBACK_PAD_FULL_INTENSITY = 255,
};

const PadScriptCmd gActor503500KnockbackPadScriptCommands[] SECTION(".data") = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_503500_KNOCKBACK_PAD_HOLD_SEGMENT), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_503500_KNOCKBACK_PAD_FULL_SEGMENT) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

const PadScriptVibrationSegment gActor503500KnockbackPadScriptSegments[] SECTION(".data") = {
    { 0, 0, ACTOR_503500_KNOCKBACK_PAD_SEGMENT_FRAMES, 0 },
    { ACTOR_503500_KNOCKBACK_PAD_FULL_INTENSITY, ACTOR_503500_KNOCKBACK_PAD_FULL_INTENSITY, ACTOR_503500_KNOCKBACK_PAD_SEGMENT_FRAMES, 1 },
};

const SVECTOR gActor503500BallisticShotBodyOffset SECTION(".data") = { 0 };

const SVECTOR gActor503500LingeringShotBodyOffset SECTION(".data") = { 0 };

enum {
    ACTOR_503500_LINGERING_SHOT_ORB_FRAME_LIMIT        = 180,
    ACTOR_503500_LINGERING_SHOT_PROJECTILE_FRAME_LIMIT = 150,
};

const s32 gActor503500LingeringShotFrameLimits[ACTOR_503500_LINGERING_SHOT_KIND_COUNT] __attribute__((section(".data"))) = {
    ACTOR_503500_LINGERING_SHOT_ORB_FRAME_LIMIT,
    ACTOR_503500_LINGERING_SHOT_PROJECTILE_FRAME_LIMIT,
};

/// Pink-flash capsule body's origin in attack-root local game units.
static SVECTOR _gActor503500PinkFlashAttackBodyOffset = { 0 };

/// Unrotated far endpoint of the pink-flash capsule, in attack-root local game units.
///
/// Initialization copies it; the sweep rotates it about Y into capsule end 0.
static SVECTOR _gActor503500PinkFlashAttackFarEnd = { 0, 2000, 6000, 0 };

/// Yellow-flash attack sphere's center in player-root local game units.
static SVECTOR _gActor503500YellowFlashAttackSphereOffset = { 0 };

/// Orange-flash capsule body's origin in attack-root local game units.
static SVECTOR _gActor503500OrangeFlashAttackBodyOffset = { 0 };

/// Orange-flash capsule end 0 in attack-root local game units; end 1 is the origin.
static SVECTOR _gActor503500OrangeFlashAttackFarEnd = { 0, 500, 6000, 0 };

#include "assets/enemies/brahman/tentacle.inc"

#include "assets/enemies/brahman/clips_4.inc"

/// Scripted tentacle clips 1 and 2 for its nineteen-part rig; id 0 is NULL.
///
/// The motion handler borrows this table through bank 0 for the model's lifetime.
#include "assets/enemies/brahman/tentacle_bank.inc"

/// Carrier-local banks for nineteen-part playback: only bank 0, clips 1..2.
///
/// `AnimationPlayRequest::source.index` selects the bank; `animationId` selects
/// an entry of its clip table. Playback borrows that table and loaded clip data.
static AnimationSet** const _gActorMotionAnimBanks19[1] SECTION(".data") = {
    _gActor503500TentacleAnimationSets,
};

TaskDesc gActor503500TentacleTaskDesc = { { { TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER, TASK_PRIORITY_CONTROL } }, _actor503500TentacleTask, { .model = &_gBrahmanTentacle } };

/// Scripted tentacle messages for animation, placement, model draw mode and commands.
///
/// Four ID/callback entries followed by TASK_MESSAGE_TABLE_END; unknown IDs
/// return zero. The tentacle borrows this table and its overlay's callback code
/// for its lifetime. Each callback accepts Task*, ID and two word-sized payloads
/// interpreted by that ID; typed payload pointers retain the message ABI.
static const TaskMessageEntry _gActor503500TentacleMessageHandlers[] SECTION(".data") = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, _actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor503500SetTentacleDrawMode },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor503500ApplyTentacleCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Initializes and links the pink capsule with its borrowed contact storage.
///
/// Requires fresh work and a live root; coordinate and work remain live until
/// unlinking at exit. Radii use game units, and pair testing starts disabled.
static inline void _actor503500PinkFlashAttackLinkCapsule(_Actor503500PinkFlashAttackWork* work, GfxCoord* coord)
{
    enum {
        ACTOR_503500_PINK_FLASH_ATTACK_INITIAL_FAR_RADIUS = 2000,
    };
    WorldCollisionCapsule* capsule;
    WorldCollisionContact* contacts;

    capsule  = &work->capsule;
    contacts = work->contacts;

    work->body.coord           = coord;
    work->body.context.capsule = capsule;
    work->body.pos.vx          = _gActor503500PinkFlashAttackBodyOffset.vx;
    work->body.pos.vy          = _gActor503500PinkFlashAttackBodyOffset.vy;
    work->body.pos.vz          = _gActor503500PinkFlashAttackBodyOffset.vz;
    work->body.key             = damagePackAttackKey(gActor503500FlashAttacks[ACTOR_503500_FLASH_ATTACK_PINK], 0);
    work->body.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->body.radius          = 0;

    capsule->contacts   = contacts;
    capsule->ends[1].vx = 0;
    capsule->ends[1].vy = 0;
    capsule->ends[1].vz = 0;
    capsule->ends[0].vx = _gActor503500PinkFlashAttackFarEnd.vx;
    capsule->ends[0].vy = _gActor503500PinkFlashAttackFarEnd.vy;
    capsule->ends[0].vz = _gActor503500PinkFlashAttackFarEnd.vz;
    capsule->end1Radius = ACTOR_503500_PINK_FLASH_ATTACK_NEAR_RADIUS;
    capsule->end0Radius = ACTOR_503500_PINK_FLASH_ATTACK_INITIAL_FAR_RADIUS;

    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Initializes one of the two pink-flash sweeping attack capsules.
///
/// Requires a live coordinate body. Spawn argument 1 selects the negative
/// sweep at zero and the positive sweep otherwise; only zero spawns the charge
/// effect. The task owns its work and effect child until exit, and lends the
/// capsule/contact storage to collision. Effect-spawn failure runs exit before
/// cost acquisition, retaining the unconditional budget subtraction and wrap.
static void _actor503500PinkFlashAttackInit(Task* task)
{
    enum {
        ACTOR_503500_PINK_FLASH_ATTACK_CHARGE_SOUND = 0x0A,
    };
    _Actor503500PinkFlashAttackWork* work;
    GfxCoord*                        coord;
    EffectWork*                      effectWork;
    Task*                            effectTask;
    s32                              pan;

    coord = task->extra.coordBody->coord;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work        = work;
    work->radiusScale = ONE;

    gfxSetRotIdentity(&coord->coord);

    gfxSetRotIdentity(&work->sweepRotation);

    _actor503500PinkFlashAttackLinkCapsule(work, coord);

    // Only the negative-sweep task owns a charge effect; both own a budget share.
    if (task->spawnArg1.value == 0) {
        effectWork = effectSpawn(EFFECT_SHELTER_R48_RING_FLASH_PINK, coord, 0, NULL);
        if (effectWork == NULL) {
            _actor503500PinkFlashAttackExit(task);
            return;
        }
        effectTask       = effectWork->task;
        work->effectTask = effectTask;
        taskReparent(task, effectTask);
    }
    pan = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, ACTOR_503500_PINK_FLASH_ATTACK_CHARGE_SOUND), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    actor503500AcquireProjectileEffectCost(ACTOR_503500_PROJECTILE_EFFECT_COST_PINK_FLASH);
    task->exitCallback = _actor503500PinkFlashAttackExit;
    task->state       += 1;
}

/// Advances the pink capsule's signed 16.16 sweep angle and rebuilds its Y rotation.
///
/// Requires live attack work. Velocity has the angle's 4096-units-per-turn
/// scale; only the integer half reaches the rotation. Replaces the nine
/// rotation entries, preserving matrix translation and the coordinate stamp.
static inline void _actor503500PinkFlashAttackAdvanceSweep(_Actor503500PinkFlashAttackWork* work)
{
    work->sweepAngle.word += work->sweepAngularVelocity;
    gfxSetRotIdentity(&work->sweepRotation);
    RotMatrixY(work->sweepAngle.halves.integer, &work->sweepRotation);
}

/// Advances one pink-flash capsule through charge, hold, sweep and fade.
///
/// Requires initialized work and a live TMD root. Spawn argument 1 selects the
/// negative sweep at zero and the positive sweep otherwise. Angles use signed
/// 16.16 values with 4096 whole units per turn; radii use a Q12 scale. Completion
/// advances the task to its exit state without freeing it here.
static void _actor503500PinkFlashAttackUpdatePhase(Task* task)
{
    enum {
        /// Updating ticks before the negative-sweep capsule can begin pair testing.
        ACTOR_503500_PINK_FLASH_ATTACK_CHARGE_FRAMES = 31,
        /// Updating ticks held straight before sweeping and enabling the positive capsule.
        ACTOR_503500_PINK_FLASH_ATTACK_HOLD_FRAMES = 21,
        /// Angular acceleration magnitude in 16.16 angle units per squared updating tick.
        ///
        /// Two whole units per squared tick, with 4096 whole units per turn;
        /// the task's sweep side determines its sign.
        ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_ACCELERATION = 0x20000,
        /// Exclusive absolute sweep-angle threshold for deceleration, in 4096 units per turn.
        ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_TURNOVER_ANGLE = 0x100,
        /// Far-end radius in game units at full Q12 scale during shrinking (initially 2000).
        ACTOR_503500_PINK_FLASH_ATTACK_FAR_RADIUS = 3000,
        /// Q12 radius-scale decrement per update from entry into the sweep through exit.
        ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_SCALE_STEP = 0x50,
        /// Q12 lower clamp for shrinking radii; normal task completion precedes this floor.
        ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_SCALE_MIN = ONE / 8,
        /// Q12 scale at or below which the fade phase disables capsule pair testing.
        ///
        /// Tested before that update's decrement; earlier player contact can
        /// already have disabled testing.
        ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_SCALE_DISABLE_THRESHOLD = ONE / 4,
        // Dividing the base radii by eight leaves nine fractional scale bits.
        ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_FRACTION_BITS = 9,
    };
    _Actor503500PinkFlashAttackWork* work;
    WorldCollisionCapsule*           capsule;
    GfxCoord*                        coord;
    s32                              pan;
    s32                              angularAcceleration;
    s32                              absoluteSweepAngle;

    work    = task->work;
    capsule = &work->capsule;
    switch (work->phase) {
        case ACTOR_503500_PINK_FLASH_ATTACK_CHARGE:
            if (++work->phaseFrames >= ACTOR_503500_PINK_FLASH_ATTACK_CHARGE_FRAMES) {
                if (task->spawnArg1.value == 0) {
                    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
                coord = task->extra.tmd->coords;
                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_SOUND), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                work->phaseFrames = 0;
                work->phase++;
            }
            break;
        case ACTOR_503500_PINK_FLASH_ATTACK_HOLD:
            if (++work->phaseFrames >= ACTOR_503500_PINK_FLASH_ATTACK_HOLD_FRAMES) {
                if (task->spawnArg1.value != 0) {
                    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
                work->phaseFrames = 0;
                work->phase++;
            }
            break;
        case ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_ACCELERATE:
            angularAcceleration = -ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_ACCELERATION;
            if (task->spawnArg1.value != 0) {
                angularAcceleration = ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_ACCELERATION;
            }
            work->sweepAngularVelocity += angularAcceleration;
            _actor503500PinkFlashAttackAdvanceSweep(work);
            absoluteSweepAngle = work->sweepAngle.halves.integer;
            if (absoluteSweepAngle < 0) {
                absoluteSweepAngle = -absoluteSweepAngle;
            }
            if (absoluteSweepAngle > ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_TURNOVER_ANGLE) {
                work->phase++;
            }
            break;
        case ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_DECELERATE:
            // Bring the velocity back to zero from whichever side this task swept to.
            if (task->spawnArg1.value != 0) {
                work->sweepAngularVelocity -= ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_ACCELERATION;
                if (work->sweepAngularVelocity < 0) {
                    work->sweepAngularVelocity = 0;
                    work->phase++;
                }
            } else {
                work->sweepAngularVelocity += ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_ACCELERATION;
                if (work->sweepAngularVelocity > 0) {
                    work->sweepAngularVelocity = 0;
                    work->phase++;
                }
            }
            _actor503500PinkFlashAttackAdvanceSweep(work);
            break;
        case ACTOR_503500_PINK_FLASH_ATTACK_FADE:
            if (work->radiusScale <= ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_SCALE_DISABLE_THRESHOLD) {
                work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->phase++;
            }
            break;
        default:
            task->state++;
            break;
    }
    if (work->phase >= ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_ACCELERATE) {
        // Shrink the capsule; the radii are scaled by radiusScale / ONE, with both halves of that fraction divided by 8.
        work->radiusScale -= ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_SCALE_STEP;
        if (work->radiusScale < ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_SCALE_MIN) {
            work->radiusScale = ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_SCALE_MIN;
        }
        capsule->end0Radius = work->radiusScale * (ACTOR_503500_PINK_FLASH_ATTACK_FAR_RADIUS / 8) >> ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_FRACTION_BITS;
        capsule->end1Radius = work->radiusScale * (ACTOR_503500_PINK_FLASH_ATTACK_NEAR_RADIUS / 8) >> ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_FRACTION_BITS;
        // Turn the far end about Y by the sweep angle.
        gte_SetRotMatrix(&work->sweepRotation);
        gte_ldv0(&_gActor503500PinkFlashAttackFarEnd);
        gte_rtv0();
        gte_stsv(&capsule->ends[0]);
    }
}

/// Updates a pink-flash attack, except while scene actors are paused or hidden.
///
/// Requires a live TMD task and initialized attack work. Consumes the preceding
/// collision pass before advancing the sweep; actor-control values above the
/// defined 0..2 range retain the running behavior.
static void _actor503500PinkFlashAttackUpdate(Task* task)
{
    GfxCoord* coord;
    s32       actorControl;

    actorControl = gSceneCombatState.actorControl;
    if (actorControl <= SCENE_COMBAT_ACTORS_HIDDEN) {
        if (actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
            return;
        }
    }
    coord               = task->extra.tmd->coords;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor503500PinkFlashAttackSweepContacts(task);
    _actor503500PinkFlashAttackUpdatePhase(task);
}

/// Detaches and destroys a pink-flash attack, releasing its effect-budget cost.
///
/// Requires a live TMD root and allocated attack work. Also serves charge-effect
/// spawn failure, which takes the same unconditional budget decrement as normal
/// exit. Unlinks the capsule before task teardown releases its borrowed storage.
static void _actor503500PinkFlashAttackExit(Task* task)
{
    _Actor503500PinkFlashAttackWork* work;
    TmdObject*                       model;

    actor503500ReleaseProjectileEffectCost(ACTOR_503500_PROJECTILE_EFFECT_COST_PINK_FLASH);
    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_SOUND), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    model                 = task->extra.tmd;
    model->coords->parent = &gGfxViewCoord;
    work                  = task->work;
    worldCollisionUnlinkBody(&work->body);
    taskKill(task);
}

/// Stops a pink-flash capsule after a player-body contact, then empties its table.
///
/// Scans all four contact slots, including the final marked slot. Requires
/// initialized work; clearing preserves the table's final-entry marker. The
/// collision category includes companion bodies as well as the player.
static void _actor503500PinkFlashAttackSweepContacts(Task* task)
{
    _Actor503500PinkFlashAttackWork* work;
    WorldCollisionContact*           contacts;
    s32                              contactIndex;

    work     = task->work;
    contacts = work->contacts;
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->contacts); contactIndex++) {
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
    worldCollisionClearContacts(contacts);
}

void actor503500PinkFlashAttackTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gActor503500PinkFlashAttackStateHandlers;
    handlers.funcs[task->state](task);
}

/// Yellow-flash task handlers for states 0 initialize, 1 update and 2 exit.
///
/// Copied by value before unchecked state dispatch; every slot takes a live Task*.
static const TaskFuncTable3 _gActor503500YellowFlashAttackStateHandlers = {
    {
        _actor503500YellowFlashAttackInit,
        _actor503500YellowFlashAttackUpdate,
        _actor503500YellowFlashAttackExit,
    },
};

/// Initializes the yellow sphere on the player and links its borrowed contact slot.
///
/// Requires fresh work and a live player root; both remain live until unlinking
/// at exit. Radius uses game units, and pair testing starts disabled.
static inline void _actor503500YellowFlashAttackLinkSphere(_Actor503500YellowFlashAttackWork* work)
{
    enum {
        ACTOR_503500_YELLOW_FLASH_ATTACK_RADIUS = 300,
    };
    work->body.coord            = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    work->body.context.contacts = work->contacts;
    work->body.pos.vx           = _gActor503500YellowFlashAttackSphereOffset.vx;
    work->body.pos.vy           = _gActor503500YellowFlashAttackSphereOffset.vy;
    work->body.pos.vz           = _gActor503500YellowFlashAttackSphereOffset.vz;
    work->body.key              = damagePackAttackKey(gActor503500FlashAttacks[ACTOR_503500_FLASH_ATTACK_YELLOW], 0);
    work->body.radius           = ACTOR_503500_YELLOW_FLASH_ATTACK_RADIUS;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Initializes a yellow-flash attack sphere attached to the player's coordinate.
///
/// Requires a live coordinate body and player root. The task owns its work and
/// charge-effect child until exit; the player root must outlive the borrowed
/// collision attachment. Pair testing starts disabled. Effect-spawn failure
/// calls exit before cost acquisition, retaining the budget subtraction and wrap.
static void _actor503500YellowFlashAttackInit(Task* task)
{
    enum {
        ACTOR_503500_YELLOW_FLASH_ATTACK_CHARGE_SOUND = 0x0C,
    };
    _Actor503500YellowFlashAttackWork* work;
    GfxCoord*                          coord;
    EffectWork*                        effectWork;
    Task*                              effectTask;
    s32                                pan;

    coord = task->extra.coordBody->coord;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work = work;

    gfxSetRotIdentity(&coord->coord);

    _actor503500YellowFlashAttackLinkSphere(work);

    effectWork = effectSpawn(EFFECT_SHELTER_R48_RING_FLASH_YELLOW, coord, 0, NULL);
    if (effectWork == NULL) {
        _actor503500YellowFlashAttackExit(task);
        return;
    }
    effectTask       = effectWork->task;
    work->effectTask = effectTask;
    taskReparent(task, effectTask);
    pan = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, ACTOR_503500_YELLOW_FLASH_ATTACK_CHARGE_SOUND), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    actor503500AcquireProjectileEffectCost(ACTOR_503500_PROJECTILE_EFFECT_COST_YELLOW_FLASH);
    task->exitCallback = _actor503500YellowFlashAttackExit;
    task->state       += 1;
}

/// Plays a Brahman sound cue using the yellow flash's cached view transform.
///
/// Requires a live coordinate body with a composed local-to-view matrix. Spatial
/// offsets retain signed-byte narrowing and the depth's division by two.
/// `soundId` is a Brahman bank entry in 0..255, without bank or character bits.
static inline void _actor503500YellowFlashAttackPlayCue(Task* task, s32 soundId)
{
    GfxCoord* coord;
    s32       pan;

    coord = task->extra.coordBody->coord;
    pan   = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, soundId), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
}

/// Times the yellow flash's charge cues and height-gated four-tick strike.
///
/// Requires initialized work and a live TMD root and player matrix. The sphere
/// follows the player's coordinate and activates only below Y=-1000, with
/// negative Y upward. A boss interruption or finished phase advances the task
/// to its exit state; this routine does not clear contacts or destroy the task.
static void _actor503500YellowFlashAttackUpdatePhase(Task* task)
{
    enum {
        /// Updating ticks of charge before testing the player's height to enable the sphere.
        ACTOR_503500_YELLOW_FLASH_ATTACK_CHARGE_FRAMES = 95,
        /// Updating ticks of the strike phase before disabling the height-gated sphere.
        ///
        /// A boss interruption can end the attack sooner.
        ACTOR_503500_YELLOW_FLASH_ATTACK_STRIKE_FRAMES    = 4,
        ACTOR_503500_YELLOW_FLASH_ATTACK_FIRST_CUE_FRAME  = 62,
        ACTOR_503500_YELLOW_FLASH_ATTACK_SECOND_CUE_FRAME = 90,
        ACTOR_503500_YELLOW_FLASH_ATTACK_FIRST_CUE_SOUND  = 0x14,
        ACTOR_503500_YELLOW_FLASH_ATTACK_SECOND_CUE_SOUND = 0x0D,
        ACTOR_503500_YELLOW_FLASH_ATTACK_STRIKE_PLAYER_Y  = -1000,
    };
    _Actor503500YellowFlashAttackWork* work;

    work = task->work;
    if (actor503500ShouldInterruptAttack(task) == 0) {
        switch (work->phase) {
            case ACTOR_503500_YELLOW_FLASH_ATTACK_CHARGE:
                work->phaseFrames++;
                if (work->phaseFrames >= ACTOR_503500_YELLOW_FLASH_ATTACK_CHARGE_FRAMES) {
                    // The strike only lands on a player standing this high (Y is negative upwards).
                    if (gPlayerStatus.coordMtx->t[1] < ACTOR_503500_YELLOW_FLASH_ATTACK_STRIKE_PLAYER_Y) {
                        work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    }
                    work->phaseFrames = 0;
                    work->phase++;
                } else if (work->phaseFrames == ACTOR_503500_YELLOW_FLASH_ATTACK_FIRST_CUE_FRAME) {
                    _actor503500YellowFlashAttackPlayCue(task, ACTOR_503500_YELLOW_FLASH_ATTACK_FIRST_CUE_SOUND);
                } else if (work->phaseFrames == ACTOR_503500_YELLOW_FLASH_ATTACK_SECOND_CUE_FRAME) {
                    _actor503500YellowFlashAttackPlayCue(task, ACTOR_503500_YELLOW_FLASH_ATTACK_SECOND_CUE_SOUND);
                }
                return;
            case ACTOR_503500_YELLOW_FLASH_ATTACK_STRIKE:
                work->phaseFrames++;
                if (work->phaseFrames >= ACTOR_503500_YELLOW_FLASH_ATTACK_STRIKE_FRAMES) {
                    work->phaseFrames = 0;
                    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->phase++;
                }
                return;
        }
    }
    task->state += 1;
}

/// Updates the yellow flash, except while scene actors are paused or hidden.
///
/// Requires a live TMD task and initialized attack work. Clears the sphere's
/// previous contacts before stepping its charge/strike phases; actor-control
/// values above the defined 0..2 range retain the running behavior.
static void _actor503500YellowFlashAttackUpdate(Task* task)
{
    GfxCoord* coord;
    s32       actorControl;

    coord        = task->extra.tmd->coords;
    actorControl = gSceneCombatState.actorControl;
    if (actorControl <= SCENE_COMBAT_ACTORS_HIDDEN) {
        if (actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
            return;
        }
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor503500YellowFlashAttackClearContacts(task);
    _actor503500YellowFlashAttackUpdatePhase(task);
}

/// Stops the yellow charge sound and destroys the attack sphere's task.
///
/// Requires a live TMD root and allocated attack work. Detaches the root,
/// unlinks the sphere and unconditionally releases its effect-budget cost,
/// including when charge-effect spawning failed before acquiring that cost.
static void _actor503500YellowFlashAttackExit(Task* task)
{
    enum { ACTOR_503500_YELLOW_FLASH_ATTACK_CHARGE_SOUND = 0x0C };
    _Actor503500YellowFlashAttackWork* work;
    TmdObject*                         model;

    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, ACTOR_503500_YELLOW_FLASH_ATTACK_CHARGE_SOUND), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    actor503500ReleaseProjectileEffectCost(ACTOR_503500_PROJECTILE_EFFECT_COST_YELLOW_FLASH);
    model                 = task->extra.tmd;
    model->coords->parent = &gGfxViewCoord;
    work                  = task->work;
    worldCollisionUnlinkBody(&work->body);
    taskKill(task);
}

/// Empties the yellow strike sphere's single contact slot, retaining its end marker.
///
/// Requires initialized attack work. Contact does not shorten the strike;
/// the phase timer controls when pair testing stops.
static void _actor503500YellowFlashAttackClearContacts(Task* task)
{
    _Actor503500YellowFlashAttackWork* work;

    work = task->work;
    worldCollisionClearContacts(work->contacts);
}

void actor503500YellowFlashAttackTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gActor503500YellowFlashAttackStateHandlers;
    handlers.funcs[task->state](task);
}

/// Orange-flash task handlers for states 0 initialize, 1 update and 2 exit.
///
/// Copied by value before unchecked state dispatch; every slot takes a live Task*.
static const TaskFuncTable3 _gActor503500OrangeFlashAttackStateHandlers = {
    {
        _actor503500OrangeFlashAttackInit,
        _actor503500OrangeFlashAttackUpdate,
        _actor503500OrangeFlashAttackExit,
    },
};

/// Initializes and links the orange capsule with its borrowed contact storage.
///
/// Requires fresh work and a live root; coordinate and work remain live until
/// unlinking at exit. Radii use game units, and pair testing starts disabled.
static inline void _actor503500OrangeFlashAttackLinkCapsule(_Actor503500OrangeFlashAttackWork* work, GfxCoord* coord)
{
    enum {
        ACTOR_503500_ORANGE_FLASH_ATTACK_NEAR_RADIUS = 2000,
        ACTOR_503500_ORANGE_FLASH_ATTACK_FAR_RADIUS  = 3000,
    };
    WorldCollisionCapsule* capsule;
    WorldCollisionContact* contacts;

    capsule  = &work->capsule;
    contacts = work->contacts;

    work->body.coord           = coord;
    work->body.context.capsule = capsule;
    work->body.pos.vx          = _gActor503500OrangeFlashAttackBodyOffset.vx;
    work->body.pos.vy          = _gActor503500OrangeFlashAttackBodyOffset.vy;
    work->body.pos.vz          = _gActor503500OrangeFlashAttackBodyOffset.vz;
    work->body.key             = damagePackAttackKey(gActor503500OrangeFlashAttackTables[0], 0);
    work->body.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->body.radius          = 0;

    capsule->contacts   = contacts;
    capsule->ends[1].vx = 0;
    capsule->ends[1].vy = 0;
    capsule->ends[1].vz = 0;
    capsule->ends[0].vx = _gActor503500OrangeFlashAttackFarEnd.vx;
    capsule->ends[0].vy = _gActor503500OrangeFlashAttackFarEnd.vy;
    capsule->ends[0].vz = _gActor503500OrangeFlashAttackFarEnd.vz;
    capsule->end1Radius = ACTOR_503500_ORANGE_FLASH_ATTACK_NEAR_RADIUS;
    capsule->end0Radius = ACTOR_503500_ORANGE_FLASH_ATTACK_FAR_RADIUS;

    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Initializes the orange-flash capsule and its charge-effect child.
///
/// Requires a live coordinate body. Spawn argument 1 is a positive charge
/// countdown in updating ticks; argument 2 is the boss task used by the update's
/// interrupt test. The capsule borrows work storage until unlinking at exit. Scripted
/// events select an alternate charge sound and suppress collision activation.
/// Effect-spawn failure calls exit before cost acquisition, retaining the
/// unconditional budget subtraction and wrap.
static void _actor503500OrangeFlashAttackInit(Task* task)
{
    enum {
        ACTOR_503500_ORANGE_FLASH_ATTACK_CHARGE_SOUND       = 0x0E,
        ACTOR_503500_ORANGE_FLASH_ATTACK_EVENT_CHARGE_SOUND = 0x13,
    };
    _Actor503500OrangeFlashAttackWork* work;
    GfxCoord*                          coord;
    EffectWork*                        effectWork;
    Task*                              effectTask;
    s32                                eventPan;
    s32                                chargePan;

    coord = task->extra.coordBody->coord;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work = work;

    gfxSetRotIdentity(&coord->coord);

    _actor503500OrangeFlashAttackLinkCapsule(work, coord);

    effectWork = effectSpawn(EFFECT_SHELTER_R48_RING_FLASH, coord, task->spawnArg1.value, NULL);
    if (effectWork == NULL) {
        _actor503500OrangeFlashAttackExit(task);
        return;
    }
    effectTask       = effectWork->task;
    work->effectTask = effectTask;
    taskReparent(task, effectTask);
    if (gGameSession->eventState != 0) {
        eventPan = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, ACTOR_503500_ORANGE_FLASH_ATTACK_EVENT_CHARGE_SOUND), eventPan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    } else {
        chargePan = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, ACTOR_503500_ORANGE_FLASH_ATTACK_CHARGE_SOUND), chargePan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    }
    actor503500AcquireProjectileEffectCost(ACTOR_503500_PROJECTILE_EFFECT_COST_ORANGE_FLASH);
    task->exitCallback = _actor503500OrangeFlashAttackExit;
    task->state       += 1;
}

/// Advances the orange flash through charge, strike and linger, with pad rumble.
///
/// Requires initialized work and a live coordinate body. Counts updating ticks:
/// 91 charging, 56 striking and 36 lingering before advancing to the exit state.
/// Scripted events suppress the strike's collision, sound and full rumble.
static void _actor503500OrangeFlashAttackUpdatePhase(Task* task)
{
    enum {
        /// Updating ticks of charge before enabling the capsule outside scripted events.
        ACTOR_503500_ORANGE_FLASH_ATTACK_CHARGE_FRAMES = 91,
        /// Updating ticks of the strike phase before disabling the capsule and its sound.
        ///
        /// Player contact can disable pair testing sooner; scripted events
        /// suppress activation altogether.
        ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE_FRAMES = 56,
        /// Updating ticks after the strike before advancing the task to its exit state.
        ///
        /// A boss interruption can end the task before this count completes.
        ACTOR_503500_ORANGE_FLASH_ATTACK_LINGER_FRAMES          = 36,
        ACTOR_503500_ORANGE_FLASH_ATTACK_CHARGE_MOTOR_INTENSITY = 150,
        ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE_MOTOR_INTENSITY = 255,
        ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE_SOUND           = 0x0F,
    };
    _Actor503500OrangeFlashAttackWork* work;
    GfxCoord*                          coord;
    s32                                pan;

    work = task->work;
    switch (work->phase) {
        case ACTOR_503500_ORANGE_FLASH_ATTACK_CHARGE:
            // Refresh the light rumble on alternating display frames while charging.
            if (gDisplayState.animFrame & 1) {
                padScriptSpawnVariableMotorRamp(1, ACTOR_503500_ORANGE_FLASH_ATTACK_CHARGE_MOTOR_INTENSITY, ACTOR_503500_ORANGE_FLASH_ATTACK_CHARGE_MOTOR_INTENSITY);
            }
            if (++work->phaseFrames < ACTOR_503500_ORANGE_FLASH_ATTACK_CHARGE_FRAMES) {
                return;
            }
            if (gGameSession->eventState == 0) {
                work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                coord             = task->extra.coordBody->coord;
                pan               = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE_SOUND), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
            }
            work->phaseFrames = 0;
            work->phase++;
            return;
        case ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE:
            if (gGameSession->eventState == 0) {
                padScriptSpawnVariableMotorRamp(1, ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE_MOTOR_INTENSITY, ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE_MOTOR_INTENSITY);
            }
            if (++work->phaseFrames < ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE_FRAMES) {
                return;
            }
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE_SOUND), SOUND_SCRIPT_STOP_KEEP_RELEASE);
            work->phaseFrames = 0;
            work->phase++;
            return;
        case ACTOR_503500_ORANGE_FLASH_ATTACK_LINGER:
            if (++work->phaseFrames < ACTOR_503500_ORANGE_FLASH_ATTACK_LINGER_FRAMES) {
                return;
            }
        default:
            task->state += 1;
            return;
    }
}

/// Consumes orange-flash contacts and advances its phases while actors run.
///
/// Requires initialized work, a live coordinate body and the boss task in spawn
/// argument 2. Paused/hidden actors skip the update; values above the defined
/// 0..2 range retain the running behavior. An interrupted boss destroys the
/// attack after the contact and phase updates, through its installed exit callback.
static void _actor503500OrangeFlashAttackUpdate(Task* task)
{
    GfxCoord* coord;
    s32       actorControl;

    coord        = task->extra.coordBody->coord;
    actorControl = gSceneCombatState.actorControl;
    if (actorControl <= SCENE_COMBAT_ACTORS_HIDDEN) {
        if (actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
            return;
        }
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor503500OrangeFlashAttackSweepContacts(task);
    _actor503500OrangeFlashAttackUpdatePhase(task);
    if (actor503500ShouldInterruptAttack(task->spawnArg2.pointer)) {
        task->exitCallback(task);
    }
}

/// Stops all orange-flash sounds and destroys the attack capsule's task.
///
/// Requires a live TMD root and allocated attack work. Detaches the root,
/// unlinks the capsule and unconditionally releases its effect-budget cost,
/// including when charge-effect spawning failed before acquiring that cost.
static void _actor503500OrangeFlashAttackExit(Task* task)
{
    enum {
        ACTOR_503500_ORANGE_FLASH_ATTACK_CHARGE_SOUND       = 0x0E,
        ACTOR_503500_ORANGE_FLASH_ATTACK_EVENT_CHARGE_SOUND = 0x13,
        ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE_SOUND       = 0x0F,
    };
    _Actor503500OrangeFlashAttackWork* work;
    TmdObject*                         model;

    actor503500ReleaseProjectileEffectCost(ACTOR_503500_PROJECTILE_EFFECT_COST_ORANGE_FLASH);
    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, ACTOR_503500_ORANGE_FLASH_ATTACK_CHARGE_SOUND), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, ACTOR_503500_ORANGE_FLASH_ATTACK_EVENT_CHARGE_SOUND), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE_SOUND), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    model                 = task->extra.tmd;
    model->coords->parent = &gGfxViewCoord;
    work                  = task->work;
    worldCollisionUnlinkBody(&work->body);
    taskKill(task);
}

/// Stops an orange-flash capsule after a player-body contact, then empties its table.
///
/// Scans all four contact slots, including the final marked slot. Requires
/// initialized work; clearing preserves the table's final-entry marker. The
/// collision category includes companion bodies as well as the player.
static void _actor503500OrangeFlashAttackSweepContacts(Task* task)
{
    _Actor503500OrangeFlashAttackWork* work;
    WorldCollisionContact*             contacts;
    s32                                contactIndex;

    work     = task->work;
    contacts = work->contacts;
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->contacts); contactIndex++) {
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
    worldCollisionClearContacts(contacts);
}

void actor503500OrangeFlashAttackTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gActor503500OrangeFlashAttackStateHandlers;
    handlers.funcs[task->state](task);
}

/// Scripted tentacle handlers for states 0 initialize, 1 update and 2 exit.
///
/// Copied by value before unchecked state dispatch while actor control is running;
/// every slot takes a live Task*, and initialization and exit freeze with updates.
static const TaskFuncTable3 _gActor503500TentacleStateHandlers = {
    {
        _actor503500InitTentacle,
        _actor503500TickTentacle,
        _actor503500ExitTentacle,
    },
};

/// Adds 16.16 velocity to the tentacle root and keeps the three fractional carries.
///
/// Requires writable work and root storage. Uses parent-coordinate units and
/// marks composition dirty; velocity and all rotation entries are preserved.
static inline void _actor503500IntegrateTentacleVelocity(_Actor503500Actor361100Model06038Work* work, GfxCoord* coord)
{
    work->carry[0].word += work->velocity.vx;
    work->carry[1].word += work->velocity.vy;
    work->carry[2].word += work->velocity.vz;
    coord->coord.t[0]   += work->carry[0].halves.integer;
    coord->coord.t[1]   += work->carry[1].halves.integer;
    coord->coord.t[2]   += work->carry[2].halves.integer;
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    work->carry[0].word  = work->carry[0].halves.fraction;
    work->carry[1].word  = work->carry[1].halves.fraction;
    work->carry[2].word  = work->carry[2].halves.fraction;
}

/// Updates the scripted tentacle's motion, position, animation and lighting.
///
/// Requires a live model, initialized work and an Enemy in spawn argument 2.
/// Motion is 0 idle or 1 collapse. Integrates signed 16.16 displacement into
/// the root's parent-coordinate translation, retaining the unsigned fractional
/// halves. Animation ticks slots 1..18; hidden models skip lighting composition.
/// A nonnegative buffer countdown frees on the tick finding zero, then becomes
/// inactive at -1. Motion runs first, even when it advances the task to exit.
static void _actor503500TickTentacle(Task* task)
{
    VECTOR                                 composedPosition;
    TmdObject*                             model             = task->extra.tmd;
    _Actor503500Actor361100Model06038Work* work              = task->work;
    TaskFunc                               motionHandlers[2] = { _actor503500TentacleIdle, _actor503500CollapseTentacle };
    GfxCoord*                              coord;
    s32                                    slotIndex;

    motionHandlers[work->motion](task);
    coord = task->extra.tmd->coords;
    _actor503500IntegrateTentacleVelocity(work, coord);
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        composedPosition.vx = coord->workm.t[0];
        composedPosition.vy = coord->workm.t[1];
        composedPosition.vz = coord->workm.t[2];
        worldCoordUpdateActorColor(task->spawnArg2.pointer, &composedPosition, 0, 0);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// Collapses the scripted tentacle by squashing its saved root rotation, then burning.
///
/// Requires initialized work, a live model and an Enemy in spawn argument 2.
/// Saves only the 18-byte rotation, waits 31 updating ticks, then decreases its
/// Q12 Y scale toward ONE/8 while restoring that rotation before each scale.
/// Squash ticks 20/30/100 select translucent weighted lighting, burn and black
/// lighting; tick 150 advances to exit. Translation is left to the frame tick.
static void _actor503500CollapseTentacle(Task* task)
{
    enum {
        /// Updating ticks at full height after saving the rotation, before squashing begins.
        ACTOR_503500_TENTACLE_COLLAPSE_WAIT_FRAMES = 31,
        /// Q12 Y-scale decrement per squash update, applied to the saved root rotation.
        ACTOR_503500_TENTACLE_COLLAPSE_SCALE_STEP = 0x10,
        /// Q12 Y scale at or below which squash updates stop decrementing it.
        ///
        /// The exit cue at squash tick 150 sees scale 1696, above this guard.
        ACTOR_503500_TENTACLE_COLLAPSE_SCALE_MIN = ONE / 8,
        /// One-based squash tick enabling translucency and selecting weighted actor colour.
        ACTOR_503500_TENTACLE_COLLAPSE_FADE_FRAME = 20,
        /// One-based squash tick spawning the corpse-burn effect on the tentacle root.
        ACTOR_503500_TENTACLE_COLLAPSE_BURN_FRAME = 30,
        /// One-based squash tick selecting the black actor colour mode.
        ACTOR_503500_TENTACLE_COLLAPSE_BLACK_FRAME = 100,
        /// One-based squash tick advancing the task to its exit state.
        ACTOR_503500_TENTACLE_COLLAPSE_EXIT_FRAME = 150,
        ACTOR_503500_TENTACLE_CORPSE_BURN_BURSTS  = 2,
    };
    VECTOR                                 scale;
    GfxCoord*                              coord;
    _Actor503500Actor361100Model06038Work* work;
    TmdObject*                             model;
    Enemy*                                 enemy;

    /// Copies the tentacle's nine Q12 rotation coefficients, preserving translation.
    ///
    /// Arguments must be complete, word-aligned MATRIX::m arrays in live,
    /// disjoint matrices, with a writable destination and no side effects.
    /// Each is evaluated twice; sizeof also inspects the destination's array
    /// extent without evaluating it. Transfers four words and a final halfword,
    /// leaving the alignment bytes before translation untouched. The word casts
    /// are packed transfer views of SDK halfwords. Expands to a braced statement
    /// block with its own temporaries, captures no caller locals, and retains no
    /// pointer. The caller manages any containing coordinate's composition stamp.
    /// Arguments must not refer to destinationWords, sourceWords or wordIndex.
    /// Invoke only as a standalone statement within a braced block.
#define ACTOR_503500_TENTACLE_COPY_ROTATION(destinationRotation, sourceRotation)                                  \
    {                                                                                                             \
        s32*       destinationWords;                                                                              \
        const s32* sourceWords;                                                                                   \
        s32        wordIndex;                                                                                     \
        destinationWords = (s32*)(destinationRotation);                                                           \
        sourceWords      = (const s32*)(sourceRotation);                                                          \
        for (wordIndex = 0; wordIndex < (s32)(sizeof(destinationRotation) / sizeof(*sourceWords)); wordIndex++) { \
            *destinationWords++ = *sourceWords++;                                                                 \
        }                                                                                                         \
        (destinationRotation)[2][2] = (sourceRotation)[2][2];                                                     \
    }

    coord = task->extra.tmd->coords;
    work  = task->work;
    enemy = task->spawnArg2.pointer;
    model = task->extra.tmd;
    switch (work->motionStep) {
        case ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_STEP_SAVE:
            work->motionStepFrames = 0;
            work->collapseScaleY   = ONE;
            // Keep an unscaled rotation; the frame tick owns root translation.
            ACTOR_503500_TENTACLE_COPY_ROTATION(work->unscaledRotation.m, coord->coord.m);
            work->motionStep++;
            break;
        case ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_STEP_WAIT:
            work->motionStepFrames++;
            if (work->motionStepFrames >= ACTOR_503500_TENTACLE_COLLAPSE_WAIT_FRAMES) {
                work->motionStepFrames = 0;
                work->motionStep++;
            }
            break;
        case ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_STEP_SQUASH:
            if (work->collapseScaleY > ACTOR_503500_TENTACLE_COLLAPSE_SCALE_MIN) {
                work->collapseScaleY -= ACTOR_503500_TENTACLE_COLLAPSE_SCALE_STEP;
            }
            // Scaling compounds, so each frame starts again from the saved rotation.
            ACTOR_503500_TENTACLE_COPY_ROTATION(coord->coord.m, work->unscaledRotation.m);
            scale.vx = ONE;
            scale.vy = work->collapseScaleY;
            scale.vz = ONE;
            ScaleMatrixL(&coord->coord, &scale);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->motionStepFrames++;
            switch (work->motionStepFrames) {
                case ACTOR_503500_TENTACLE_COLLAPSE_FADE_FRAME:
                    model->flags |= TMD_OBJECT_SEMI_TRANS;
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    break;
                case ACTOR_503500_TENTACLE_COLLAPSE_BURN_FRAME:
                    effectSpawn(EFFECT_CORPSE_BURN, coord, ACTOR_503500_TENTACLE_CORPSE_BURN_BURSTS, NULL);
                    break;
                case ACTOR_503500_TENTACLE_COLLAPSE_BLACK_FRAME:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case ACTOR_503500_TENTACLE_COLLAPSE_EXIT_FRAME:
                    task->state++;
                    break;
            }
            break;
    }
#undef ACTOR_503500_TENTACLE_COPY_ROTATION
}

/// Dispatches the scripted tentacle's initialization, updating and exit states.
///
/// `state` must be 0..2. Dispatch freezes in every actor-control mode except
/// running, including initialization and exit. Spawn argument 2 is its Enemy.
static void _actor503500TentacleTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gActor503500TentacleStateHandlers;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        handlers.funcs[task->state](task);
    }
}

/// Allocates the scripted tentacle's work and installs its lighting and messages.
///
/// Requires the live nineteen-part model and an Enemy in spawn argument 2.
/// The enemy borrows the root matrix, and the model borrows the work's lighting
/// matrices until enemy-task teardown. Allocation failure exits immediately;
/// success leaves zero velocity/idle motion and no pending buffer release.
static void _actor503500InitTentacle(Task* task)
{
    enum { ACTOR_503500_TENTACLE_NO_BUFFER_RELEASE = -1 };
    _Actor503500Actor361100Model06038Work* work;
    GfxCoord*                              coord;
    Enemy*                                 enemy;

    coord = task->extra.tmd->coords;
    enemy = task->spawnArg2.pointer;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }

    task->work          = work;
    work->model.animId  = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank    = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown = ACTOR_503500_TENTACLE_NO_BUFFER_RELEASE;
    work->carry[0].word = 0;
    work->carry[1].word = 0;
    work->carry[2].word = 0;

    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    enemy->recs     = 0;

    _actor503500BindTentacleLighting(task);

    task->msgTable     = _gActor503500TentacleMessageHandlers;
    task->exitCallback = _actor503500ExitTentacle;
    task->state++;
}
/// Releases the scripted tentacle through normal enemy-task teardown.
///
/// Serves both its exit state and exit callback. The task must remain live
/// through the call; its model, primitive buffer, enemy and work are released.
static void _actor503500ExitTentacle(Task* task)
{
    enemyTaskExit(task);
}

/// Lends the tentacle model its work block's lighting and colour matrices.
///
/// Requires a live TMD model and initialized work. Stores borrowed pointers,
/// so the work must outlive the model's use of these matrices.
static void _actor503500BindTentacleLighting(Task* task)
{
    TmdObject*                             model;
    _Actor503500Actor361100Model06038Work* work;

    work            = task->work;
    model           = task->extra.tmd;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

/// Leaves the tentacle's idle motion unchanged while the frame tick moves it.
///
/// Slot zero of the motion dispatch performs no extra operation; velocity
/// integration and animation run in the surrounding tick. The task is ignored.
static void _actor503500TentacleIdle(Task* unusedTask)
{
}

#include "../../shared/actor/actor_motion_play19.inc.c"

/// Selects the private placement handler for the actor-361100 model task.
#define ACTOR_MESSAGE_PLACE_EULER_HANDLER _actorMsgPlaceEuler
#include "../../shared/actor/actor_messages_place_euler.inc.c"
#undef ACTOR_MESSAGE_PLACE_EULER_HANDLER

/// Changes tentacle visibility and primitive-buffer release policy.
///
/// Requires a live TMD model and initialized work. Modes 0/1 hide/show with
/// automatic buffer recovery; 1 also requests a primitive buffer. Mode 2 hides,
/// disables recovery and schedules release on the third updating tick. Mode 3
/// shows with recovery disabled. Showing does not cancel a pending release.
/// Message ID and second payload are ignored. Returns 0 for modes 0..3, or 1
/// without changing anything for other values.
static s32 _actor503500SetTentacleDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg)
{
    enum {
        ACTOR_503500_TENTACLE_DRAW_HIDE_RELEASE         = 2,
        ACTOR_503500_TENTACLE_DRAW_SHOW_EXISTING_BUFFER = 3,
    };
    _Actor503500Actor361100Model06038Work* work;
    TmdObject*                             model;
    s32                                    result;

    model  = task->extra.tmd;
    result = 0;
    switch (mode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags = (model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW) & ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_503500_TENTACLE_DRAW_HIDE_RELEASE:
            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work                = task->work;
            work->freeCountdown = mode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_503500_TENTACLE_DRAW_SHOW_EXISTING_BUFFER:
            model->flags = (model->flags & ~TMD_OBJECT_SKIP_ACTIVE_DRAW) | TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Stops the tentacle, starts a translation or translating collapse, or exits it.
///
/// Requires initialized work and a readable command through dispatch. Only the
/// command word is read; context, message ID and second payload are ignored.
/// Commands 0..3 stop, move, move while collapsing, and exit respectively;
/// others do nothing. Velocity uses signed 16.16 parent-coordinate units per
/// updating tick. Commands retain fractional carry and never arm a move timer;
/// the collapse command also retains its current phase. Returns 0.
static s32 _actor503500ApplyTentacleCommand(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_503500_TENTACLE_TRANSLATION_ONE = 0x10000,
        ACTOR_503500_TENTACLE_MOVE_FRAMES     = 23,
        // Fixed Q16 rates: 175/9 on X and 105/9 on Y, truncated toward zero.
        ACTOR_503500_TENTACLE_COLLAPSE_VELOCITY_X = 0x1371C7,
        ACTOR_503500_TENTACLE_COLLAPSE_VELOCITY_Y = 0xBAAAA,
    };
    _Actor503500Actor361100Model06038Work* work;

    work = task->work;
    switch (command->command) {
        case ACTOR_503500_TENTACLE_COMMAND_STOP:
            work->velocity.vx = 0;
            work->velocity.vy = 0;
            work->velocity.vz = 0;
            break;
        case ACTOR_503500_TENTACLE_COMMAND_MOVE:
            // The script stops this leg after a wait of 23 interpreter updates.
            work->velocity.vx = 5910 * ACTOR_503500_TENTACLE_TRANSLATION_ONE / ACTOR_503500_TENTACLE_MOVE_FRAMES;
            work->velocity.vy = -3360 * ACTOR_503500_TENTACLE_TRANSLATION_ONE / ACTOR_503500_TENTACLE_MOVE_FRAMES;
            work->velocity.vz = 150 * ACTOR_503500_TENTACLE_TRANSLATION_ONE / ACTOR_503500_TENTACLE_MOVE_FRAMES;
            break;
        case ACTOR_503500_TENTACLE_COMMAND_COLLAPSE:
            // Q16 rates are 175/9 on X and 105/9 on Y; the script stops them
            // after two waits of 60, rather than storing a movement duration here.
            work->velocity.vx = ACTOR_503500_TENTACLE_COLLAPSE_VELOCITY_X;
            work->velocity.vy = ACTOR_503500_TENTACLE_COLLAPSE_VELOCITY_Y;
            work->velocity.vz = 0;
            work->motion      = ACTOR_503500_ACTOR_361100_MODEL_06038_MOTION_COLLAPSE;
            break;
        case ACTOR_503500_TENTACLE_COMMAND_EXIT:
            task->exitCallback(task);
            break;
    }
    return 0;
}
