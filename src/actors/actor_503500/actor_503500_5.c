#include "actor_503500_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
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
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"

extern AnimationSet* D_actor_503500_8016EA54[20];

extern AnimationSet* D_actor_503500_8016EAA4[5];

/// Frames the yellow-flash attack charges before its sphere is switched on.
#define ACTOR_503500_YELLOW_FLASH_ATTACK_CHARGE_FRAMES 95
/// Frames the yellow-flash attack's sphere stays switched on.
#define ACTOR_503500_YELLOW_FLASH_ATTACK_STRIKE_FRAMES 4

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

/// Frames the pink-flash attack charges before the first of its two capsules
/// is switched on.
#define ACTOR_503500_PINK_FLASH_ATTACK_CHARGE_FRAMES 31
/// Frames the pink-flash attack then holds straight ahead before it sweeps;
/// the second capsule is switched on when they end.
#define ACTOR_503500_PINK_FLASH_ATTACK_HOLD_FRAMES 21
/// Change of `_Actor503500PinkFlashAttackWork::sweepAngularVelocity` per frame
/// of the sweep: two angle units, in 16.16.
#define ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_ACCELERATION 0x20000
/// Sweep angle past which the pink-flash attack's capsule stops speeding up
/// and slows down again (4096 to the turn, so 22.5 degrees). It is passed at
/// 272 and the slowing adds 240, so a sweep ends at 512: 45 degrees.
#define ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_TURNOVER_ANGLE 0x100
/// Radii of the pink-flash attack's capsule at its far end and at its origin
/// while `_Actor503500PinkFlashAttackWork::radiusScale` is `ONE`.
#define ACTOR_503500_PINK_FLASH_ATTACK_FAR_RADIUS  3000
#define ACTOR_503500_PINK_FLASH_ATTACK_NEAR_RADIUS 1000
/// Amount `_Actor503500PinkFlashAttackWork::radiusScale` loses each frame from
/// the sweep on, the value it stops at, and the value at or below which the
/// capsule is switched off.
#define ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_SCALE_STEP 0x50
#define ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_SCALE_MIN  (ONE / 8)
#define ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_SCALE_OFF  (ONE / 4)

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

/// Frames the orange-flash attack charges before its capsule is switched on.
#define ACTOR_503500_ORANGE_FLASH_ATTACK_CHARGE_FRAMES 91
/// Frames the orange-flash attack's capsule stays switched on, unless it
/// touches the player first.
#define ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE_FRAMES 56
/// Frames the orange-flash attack task outlives its strike by.
#define ACTOR_503500_ORANGE_FLASH_ATTACK_LINGER_FRAMES 36

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

/// Frames the collapse holds the model at full height before squashing it.
#define ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_WAIT_FRAMES 31
/// Amount `_Actor503500Actor361100Model06038Work::collapseScaleY` loses each
/// frame of the squash, and the value it stops at.
#define ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_SCALE_STEP 0x10
#define ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_SCALE_MIN  (ONE / 8)
/// Frames of the squash at which the model turns semi-transparent under
/// weighted lighting, the corpse-burn effect is spawned on it, its lighting
/// goes black, and the task moves on to its exit state.
#define ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_FADE_FRAME  20
#define ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_BURN_FRAME  30
#define ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_BLACK_FRAME 100
#define ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_EXIT_FRAME  150

/// Work block of the actor drawn with `_gActor503500Actor361100Model06038`,
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

static void func_actor_503500_801464E8(Task* arg0);
static void func_actor_503500_80146508(Task* arg0);

/// `taskMessageDispatch` handler table installed at `Task::msgTable` by
/// `func_actor_503500_8014642C`; terminator id `TASK_MESSAGE_TABLE_END`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_503500_80176530[];

/// Local offset of the display node `func_actor_503500_80144E8C` links, and the
/// offsets it seeds its `WorldCollisionCapsule` with.
extern SVECTOR D_actor_503500_801715C4;
extern SVECTOR D_actor_503500_801715CC;
/// Local offset of the display node `func_actor_503500_801455A4` links.
extern SVECTOR D_actor_503500_801715D4;
static void    func_actor_503500_80145480(Task* arg0);
static void    func_actor_503500_801450A0(Task* arg0);
static void    func_actor_503500_801454E0(Task* arg0);
static void    func_actor_503500_80145754(Task* arg0);
static void    func_actor_503500_80145950(Task* arg0);
static void    func_actor_503500_801459B0(Task* arg0);
static void    func_actor_503500_80145C50(Task* arg0);
static void    func_actor_503500_80145F18(Task* arg0);

extern SVECTOR D_actor_503500_801715DC;
extern SVECTOR D_actor_503500_801715E4;
static void    func_actor_503500_80145E98(Task* arg0);
static void    func_actor_503500_8014618C(Task* arg0);
static void    func_actor_503500_80146524(Task* arg0);

extern AnimationSet*  D_actor_503500_80176514[3];
extern AnimationSet** gActorMotionAnimBanks19[1];
static void           func_actor_503500_80144E8C(Task* arg0);
static void           func_actor_503500_80145428(Task* arg0);
static void           func_actor_503500_801455A4(Task* arg0);
static void           func_actor_503500_801458F8(Task* arg0);
static void           func_actor_503500_80145A2C(Task* arg0);
static void           func_actor_503500_80145E1C(Task* arg0);
static void           func_actor_503500_8014642C(Task* arg0);
static void           func_actor_503500_80145FDC(Task* task);
static void           func_actor_503500_801464E8(Task* arg0);

/// `Task::state` handlers `func_actor_503500_8014554C` dispatches through.
static const TaskFuncTable3 D_actor_503500_801321F4 = {
    {
        func_actor_503500_80144E8C,
        func_actor_503500_80145428,
        func_actor_503500_80145480,
    },
};

static AnimationSet _gActor503500Animation444F4;
static AnimationSet _gActor503500Animation446CC;
static TmdSource    _gActor503500Actor361100Model06038;
s32                 func_actor_503500_80146664(Task* task, s32 msgId, ActorTransform* args, s32 arg3);
s32                 func_actor_503500_801466E0(Task*, s32, s32, s32);
s32                 func_actor_503500_801467C0(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
void                func_actor_503500_801463C0(Task*);

static AnimationSet _gActor503500Animation3DE60;
static AnimationSet _gActor503500Animation3E5FC;
static AnimationSet _gActor503500Animation3EE08;
static AnimationSet _gActor503500Animation3F61C;

TaskMessageEntry D_actor_503500_8016EA2C[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_503500_80135950 },
    { ACTOR_MESSAGE_PLACE, func_actor_503500_80137088 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_503500_80137158 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_503500_80135B74 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_actor_503500_8016EA54[20] = {
    NULL,
    &gActor503500Animation2DB14,
    &gActor503500Animation2E4DC,
    &gActor503500Animation2EF88,
    &gActor503500Animation2FC70,
    &gActor503500Animation306E0,
    &gActor503500Animation30F1C,
    &gActor503500Animation31788,
    &gActor503500Animation31D8C,
    &gActor503500Animation32E24,
    &gActor503500Animation333DC,
    &gActor503500Animation33C14,
    &gActor503500Animation33EBC,
    &gActor503500Animation341D8,
    &gActor503500Animation350C8,
    &gActor503500Animation35390,
    &gActor503500Animation356DC,
    &gActor503500Animation2DB14,
    &gActor503500Animation2DB14,
    &gActor503500Animation38AE0,
};

AnimationSet* D_actor_503500_8016EAA4[5] = {
    NULL,
    &gActor503500Animation38AE0,
    &gActor503500Animation3A190,
    &gActor503500Animation350C8,
    &gActor503500Animation3C968,
};

AnimationSet** D_actor_503500_8016EAB8[2] = {
    D_actor_503500_8016EA54,
    D_actor_503500_8016EAA4,
};

AnimationPlayRequest D_actor_503500_8016EAC0[1] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_503500_8016EAD4 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_503500_8016EAE8[18] = {
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

SVECTOR D_actor_503500_8016EC50 = { 0, -500, 1600, 0 };

Actor503500AttackChoice D_actor_503500_8016EC58[4] = {
    { func_actor_503500_8013667C, 127 },
    { func_actor_503500_80133BF4, 79 },
    { func_actor_503500_8013680C, 47 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016EC78[4] = {
    { func_actor_503500_80133BF4, 127 },
    { func_actor_503500_80136770, 79 },
    { func_actor_503500_8013680C, 47 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016EC98[5] = {
    { func_actor_503500_80136770, 79 },
    { func_actor_503500_80134284, 63 },
    { func_actor_503500_8013656C, 47 },
    { func_actor_503500_8013680C, 31 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016ECC0[3] = {
    { func_actor_503500_8013667C, 159 },
    { func_actor_503500_8013680C, 95 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016ECD8[3] = {
    { func_actor_503500_8013680C, 159 },
    { func_actor_503500_80136770, 95 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016ECF0[4] = {
    { func_actor_503500_8013680C, 111 },
    { func_actor_503500_80134284, 79 },
    { func_actor_503500_80136770, 63 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016ED10[4] = {
    { func_actor_503500_80134284, 159 },
    { func_actor_503500_8013656C, 63 },
    { func_actor_503500_80136770, 31 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016ED30[4] = {
    { func_actor_503500_80136948, 95 },
    { func_actor_503500_8013667C, 95 },
    { func_actor_503500_8013680C, 63 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016ED50[3] = {
    { func_actor_503500_8013680C, 159 },
    { func_actor_503500_80134284, 95 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016ED68[3] = {
    { func_actor_503500_80134284, 223 },
    { func_actor_503500_8013680C, 31 },
    { NULL, 0 },
};

Actor503500AttackChoice* D_actor_503500_8016ED80[4] = {
    D_actor_503500_8016EC58,
    D_actor_503500_8016EC78,
    D_actor_503500_8016EC78,
    D_actor_503500_8016EC98,
};

s16 D_actor_503500_8016ED90[4] = {
    700,
    1000,
    1300,
    2048,
};

Actor503500AttackChoice* D_actor_503500_8016ED98[4] = {
    D_actor_503500_8016ECC0,
    D_actor_503500_8016ECD8,
    D_actor_503500_8016ECF0,
    D_actor_503500_8016ED10,
};

s16 D_actor_503500_8016EDA8[4] = {
    700,
    1000,
    1300,
    2048,
};

Actor503500AttackChoice* D_actor_503500_8016EDB0[4] = {
    D_actor_503500_8016ED30,
    D_actor_503500_8016ED50,
    D_actor_503500_8016ED50,
    D_actor_503500_8016ED68,
};

s16 D_actor_503500_8016EDC0[4] = {
    300,
    1000,
    1300,
    2048,
};

Actor503500AttackChoice D_actor_503500_8016EDC8[4] = {
    { func_actor_503500_801364D0, 111 },
    { func_actor_503500_8013667C, 111 },
    { func_actor_503500_80133BF4, 31 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016EDE8[3] = {
    { func_actor_503500_8013667C, 159 },
    { func_actor_503500_80133BF4, 95 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016EE00[2] = {
    { func_actor_503500_8013656C, 255 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016EE10[3] = {
    { func_actor_503500_801364D0, 127 },
    { func_actor_503500_8013667C, 127 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016EE28[3] = {
    { func_actor_503500_8013680C, 159 },
    { func_actor_503500_80136770, 95 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016EE40[4] = {
    { func_actor_503500_8013680C, 95 },
    { func_actor_503500_80134284, 79 },
    { func_actor_503500_80136770, 63 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016EE60[4] = {
    { func_actor_503500_80134284, 159 },
    { func_actor_503500_8013656C, 63 },
    { func_actor_503500_80136770, 31 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016EE80[3] = {
    { func_actor_503500_801364D0, 127 },
    { func_actor_503500_80136948, 127 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016EE98[3] = {
    { func_actor_503500_8013680C, 159 },
    { func_actor_503500_80134284, 95 },
    { NULL, 0 },
};

Actor503500AttackChoice D_actor_503500_8016EEB0[3] = {
    { func_actor_503500_80134284, 127 },
    { func_actor_503500_8013656C, 127 },
    { NULL, 0 },
};

Actor503500AttackChoice* D_actor_503500_8016EEC8[4] = {
    D_actor_503500_8016EDC8,
    D_actor_503500_8016EDE8,
    D_actor_503500_8016EDE8,
    D_actor_503500_8016EE00,
};

s16 D_actor_503500_8016EED8[4] = {
    700,
    1000,
    1300,
    2048,
};

Actor503500AttackChoice* D_actor_503500_8016EEE0[4] = {
    D_actor_503500_8016EE10,
    D_actor_503500_8016EE28,
    D_actor_503500_8016EE40,
    D_actor_503500_8016EE60,
};

s16 D_actor_503500_8016EEF0[4] = {
    700,
    1000,
    1300,
    2048,
};

Actor503500AttackChoice* D_actor_503500_8016EEF8[4] = {
    D_actor_503500_8016EE80,
    D_actor_503500_8016EE98,
    D_actor_503500_8016EE98,
    D_actor_503500_8016EEB0,
};

s16 D_actor_503500_8016EF08[4] = {
    500,
    1000,
    1300,
    2048,
};

Actor503500AttackChoice** D_actor_503500_8016EF10[2][3] = {
    { D_actor_503500_8016ED80, D_actor_503500_8016ED98, D_actor_503500_8016EDB0 },
    { D_actor_503500_8016EEC8, D_actor_503500_8016EEE0, D_actor_503500_8016EEF8 },
};

s16* D_actor_503500_8016EF28[2][3] = {
    { D_actor_503500_8016ED90, D_actor_503500_8016EDA8, D_actor_503500_8016EDC0 },
    { D_actor_503500_8016EED8, D_actor_503500_8016EEF0, D_actor_503500_8016EF08 },
};

s16 D_actor_503500_8016EF40[4] = {
    1,
    1,
    1,
    100,
};

s16 D_actor_503500_8016EF48[4] = {
    2,
    13,
    14,
    7,
};

s16 D_actor_503500_8016EF50[4] = {
    3,
    16,
    15,
    8,
};

SVECTOR D_actor_503500_8016EF58[7] = {
    { 0, -1000, 2000, 0 },
    { 0, -500, 2500, 0 },
    { 0, 0, 3000, 0 },
    { 200, -700, 2500, 0 },
    { -300, -200, 2200, 0 },
    { -200, -300, 2700, 0 },
    { 300, -800, 2500, 0 },
};

static SVECTOR _gActor503500Collision3D21CNormals[4] = {
#include "assets/actor_503500_collision_3D21C_normals.inc"
};

static SVECTOR _gActor503500Collision3D21CVerts[8] = {
#include "assets/actor_503500_collision_3D21C_verts.inc"
};

static WorldCollisionGridFace _gActor503500Collision3D21CFaces[4] = {
#include "assets/actor_503500_collision_3D21C_faces.inc"
};

static s16 _gActor503500Collision3D21CCells[10] = {
#include "assets/actor_503500_collision_3D21C_cells.inc"
};

#define GRID_CELL(i) (&_gActor503500Collision3D21CCells[i])
static s16* _gActor503500Collision3D21CTable[2] = {
#include "assets/actor_503500_collision_3D21C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_actor_503500_8016F03C = { NULL, _gActor503500Collision3D21CNormals, _gActor503500Collision3D21CVerts, _gActor503500Collision3D21CFaces, _gActor503500Collision3D21CTable, 2393, 1502, 2, 1, 4000, 4 };

SVECTOR D_actor_503500_8016F060 = { 0, -80, 596, 0 };

SVECTOR D_actor_503500_8016F068 = { 0, -80, 1000, 0 };

SVECTOR D_actor_503500_8016F070 = { 0, -200, 800, 0 };

SVECTOR D_actor_503500_8016F078[3] = {
    { 0, -300, -300, 0 },
    { 0, 300, 300, 0 },
    { 0, 300, -300, 0 },
};

SVECTOR D_actor_503500_8016F090[2] = {
    { 1300, -1468, -2700, 0 },
    { -1300, -1468, -2700, 0 },
};

SVECTOR D_actor_503500_8016F0A0[1] = {
    { -364, 1479, 0, 0 },
};

SVECTOR D_actor_503500_8016F0A8[1] = {
    { -364, -1479, 0, 0 },
};

SVECTOR D_actor_503500_8016F0B0 = { 0, 0, 400, 0 };

SVECTOR D_actor_503500_8016F0B8[2] = {
    { 3500, -1500, -5200, 0 },
    { -3500, -1500, -5200, 0 },
};

SVECTOR D_actor_503500_8016F0C8 = { 0, 0, 1000, 0 };

SVECTOR D_actor_503500_8016F0D0[3] = {
    { 0, -600, 0, 0 },
    { -600, 0, 0, 0 },
    { 600, 0, 0, 0 },
};

s32 D_actor_503500_8016F0E8[2] = {
    11,
    5,
};

SVECTOR D_actor_503500_8016F0F0[2] = {
    { 300, 0, 0, 0 },
    { -300, 0, 0, 0 },
};

RECT D_actor_503500_8016F100 = { 0, 253, 256, 1 };

SVECTOR D_actor_503500_8016F108[4] = {
    { 2000, 0, 1000, 0 },
    { 2000, 0, -1000, 0 },
    { -750, 300, 0, 0 },
    { -750, 600, 0, 0 },
};

SVECTOR D_actor_503500_8016F128[1][3] = {
    { { -750, 900, 0, 0 }, { -750, 1700, 0, 0 }, { -750, 1400, 0, 0 } },
};

SVECTOR D_actor_503500_8016F140 = { -750, 1100, 0, 0 };

RECT D_actor_503500_8016F148[2][2] = {
    { { 640, 256, 64, 256 }, { 0, 252, 256, 1 } },
    { { 704, 256, 64, 256 }, { 0, 261, 256, 1 } },
};

SVECTOR D_actor_503500_8016F168[9] = {
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

SVECTOR D_actor_503500_8016F1B0 = { 0, 500, -1500, 0 };

SVECTOR D_actor_503500_8016F1B8[18] = {
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

SVECTOR D_actor_503500_8016F248[2] = {
    { 1300, -1468, -2800, 0 },
    { -1300, -1468, -2800, 0 },
};

SVECTOR D_actor_503500_8016F258 = { 1300, -1468, -2700, 0 };

SVECTOR D_actor_503500_8016F260 = { 500, 1700, 0, 0 };

SVECTOR D_actor_503500_8016F268[2] = {
    { 500, 1400, 0, 0 },
    { 500, 1100, 0, 0 },
};

SVECTOR D_actor_503500_8016F278[3] = {
    { 1300, -1468, -2700, 0 },
    { 1700, -1668, -2900, 0 },
    { 2100, -1868, -3100, 0 },
};

SVECTOR D_actor_503500_8016F290[9] = {
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

SVECTOR D_actor_503500_8016F2D8 = { 0, 800, 2000, 0 };

s16 D_actor_503500_8016F2E0[6] = {
    3584,
    3328,
    3712,
    3456,
    3584,
    3712,
};

SVECTOR D_actor_503500_8016F2EC[6] = {
    { 712, 0, 0, 0 },
    { 688, 24, 0, 0 },
    { 780, 88, 0, 0 },
    { 745, -46, 0, 0 },
    { 656, -20, 0, 0 },
    { 780, -80, 0, 0 },
};

SVECTOR D_actor_503500_8016F31C[9] = {
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

RECT D_actor_503500_8016F364 = { 358, 480, 25, 31 };

SVECTOR D_actor_503500_8016F36C = { 0, 0, 800, 0 };

SVECTOR D_actor_503500_8016F374[6] = {
    { 0, 250, 800, 0 },
    { 400, 300, 1000, 0 },
    { -400, 350, 1100, 0 },
    { 400, -50, 1400, 0 },
    { -400, -100, 1200, 0 },
    { 0, -150, 1000, 0 },
};

RECT D_actor_503500_8016F3A4 = { 352, 256, 31, 40 };

SVECTOR D_actor_503500_8016F3AC[4] = {
    { 1300, -1468, -2700, 0 },
    { 1300, -1468, -2700, 0 },
    { -1300, -1468, -2700, 0 },
    { -1300, -1468, -2700, 0 },
};

SVECTOR D_actor_503500_8016F3CC[4] = {
    { -364, 1479, 0, 0 },
    { -364, 1479, 0, 0 },
    { -364, -1479, 0, 0 },
    { -364, -1479, 0, 0 },
};

SVECTOR D_actor_503500_8016F3EC = { 0, 0, 400, 0 };

SVECTOR D_actor_503500_8016F3F4[4] = {
    { -200, 0, 400, 0 },
    { 200, 0, 400, 0 },
    { -200, 0, 400, 0 },
    { 200, 0, 400, 0 },
};

SVECTOR D_actor_503500_8016F414[4] = {
    { 3500, -3000, -5200, 0 },
    { 1800, -2000, -5200, 0 },
    { -1800, -2000, -5200, 0 },
    { -3500, -3000, -5200, 0 },
};

s16 D_actor_503500_8016F434[10] = {
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

SVECTOR D_actor_503500_8016F448[3] = {
    { 0, -600, 0, 0 },
    { -400, 0, 0, 0 },
    { 400, 0, 0, 0 },
};

static AnimationPackedPose _gActor503500Animation3DE60Bank1[21] = {
#include "assets/actor_503500_animation_3DE60_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation3DE60Bank4[193] = {
#include "assets/actor_503500_animation_3DE60_bank4.inc"
};

static AnimationRecord _gActor503500Animation3DE60Records[254] = {
#include "assets/actor_503500_animation_3DE60_records.inc"
};

static u16 _gActor503500Animation3DE60Indices[20] = {
#include "assets/actor_503500_animation_3DE60_indices.inc"
};

static AnimationSet _gActor503500Animation3DE60 = {
    _gActor503500Animation3DE60Records,
    _gActor503500Animation3DE60Indices,
    { NULL, _gActor503500Animation3DE60Bank1, NULL, NULL, _gActor503500Animation3DE60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation3E5FCBank1[20] = {
#include "assets/actor_503500_animation_3E5FC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation3E5FCBank4[172] = {
#include "assets/actor_503500_animation_3E5FC_bank4.inc"
};

static AnimationRecord _gActor503500Animation3E5FCRecords[235] = {
#include "assets/actor_503500_animation_3E5FC_records.inc"
};

static u16 _gActor503500Animation3E5FCIndices[20] = {
#include "assets/actor_503500_animation_3E5FC_indices.inc"
};

static AnimationSet _gActor503500Animation3E5FC = {
    _gActor503500Animation3E5FCRecords,
    _gActor503500Animation3E5FCIndices,
    { NULL, _gActor503500Animation3E5FCBank1, NULL, NULL, _gActor503500Animation3E5FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation3EE08Bank1[15] = {
#include "assets/actor_503500_animation_3EE08_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation3EE08Bank4[206] = {
#include "assets/actor_503500_animation_3EE08_bank4.inc"
};

static AnimationRecord _gActor503500Animation3EE08Records[244] = {
#include "assets/actor_503500_animation_3EE08_records.inc"
};

static u16 _gActor503500Animation3EE08Indices[20] = {
#include "assets/actor_503500_animation_3EE08_indices.inc"
};

static AnimationSet _gActor503500Animation3EE08 = {
    _gActor503500Animation3EE08Records,
    _gActor503500Animation3EE08Indices,
    { NULL, _gActor503500Animation3EE08Bank1, NULL, NULL, _gActor503500Animation3EE08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation3F61CBank1[14] = {
#include "assets/actor_503500_animation_3F61C_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation3F61CBank4[207] = {
#include "assets/actor_503500_animation_3F61C_bank4.inc"
};

static AnimationRecord _gActor503500Animation3F61CRecords[248] = {
#include "assets/actor_503500_animation_3F61C_records.inc"
};

static u16 _gActor503500Animation3F61CIndices[20] = {
#include "assets/actor_503500_animation_3F61C_indices.inc"
};

static AnimationSet _gActor503500Animation3F61C = {
    _gActor503500Animation3F61CRecords,
    _gActor503500Animation3F61CIndices,
    { NULL, _gActor503500Animation3F61CBank1, NULL, NULL, _gActor503500Animation3F61CBank4, NULL, NULL, NULL },
};

s32 D_actor_503500_80171464[2] = {
    11,
    5,
};

TaskDesc D_actor_503500_8017146C = { { { TASK_BODY_NONE, 192 } }, func_actor_503500_80143AC0, { .value = 0 } };

SVECTOR D_actor_503500_80171478 = { 0 };

SVECTOR D_actor_503500_80171480[2] = {
    { 500, 0, 0, 0 },
    { -500, 0, 0, 0 },
};

s8 D_actor_503500_80171490[56] = {
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

AnimationSet* D_actor_503500_801714C8[5] = {
    NULL,
    &_gActor503500Animation3DE60,
    &_gActor503500Animation3E5FC,
    &_gActor503500Animation3EE08,
    &_gActor503500Animation3F61C,
};

s32 D_actor_503500_801714DC = 0;

AnimationPlayRequest D_actor_503500_801714E0[2] = {
    { { .sets = D_actor_503500_801714C8 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .sets = D_actor_503500_801714C8 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_actor_503500_80171508[2] = {
    { { .sets = D_actor_503500_801714C8 }, 3, ANIMATION_BLEND_INTERPOLATE, 3, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .sets = D_actor_503500_801714C8 }, 4, ANIMATION_BLEND_INTERPOLATE, 3, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_actor_503500_80171530 = { { .sets = D_actor_503500_801714C8 }, 5, ANIMATION_BLEND_INTERPOLATE, 3, ANIMATION_WORLD_COLLISION_ENABLE };

GameActorButtonPressHold D_actor_503500_80171544 = { 0 };

RECT D_actor_503500_8017155C = { 0, 261, 256, 1 };

SVECTOR D_actor_503500_80171564[5] = {
    { 0, 0, 0, 0 },
    { 0, -500, 500, 0 },
    { 0, -1000, 0, 0 },
    { 500, 0, 0, 0 },
    { -500, 0, 0, 0 },
};

SVECTOR D_actor_503500_8017158C = { -1000, 0, 0, 0 };

SVECTOR D_actor_503500_80171594 = { 1000, 0, 0, 0 };

PadScriptCmd D_actor_503500_8017159C[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_503500_801715A4[2] = { { 0, 0, 8, 0 }, { 255, 255, 8, 1 } };

SVECTOR D_actor_503500_801715AC = { 0 };

SVECTOR D_actor_503500_801715B4 = { 0 };

s32 D_actor_503500_801715BC[2] = {
    180,
    150,
};

SVECTOR D_actor_503500_801715C4 = { 0 };

SVECTOR D_actor_503500_801715CC = { 0, 2000, 6000, 0 };

SVECTOR D_actor_503500_801715D4 = { 0 };

SVECTOR D_actor_503500_801715DC = { 0 };

SVECTOR D_actor_503500_801715E4 = { 0, 500, 6000, 0 };

static TmdBone _gActor503500Actor361100Model06038Skeleton[19] = {
#include "assets/actor_361100_model_06038_skeleton.inc"
};

static u32 _gActor503500Actor361100Model06038PartVerts[19] = {
#include "assets/actor_361100_model_06038_partVerts.inc"
};

static SVECTOR _gActor503500Actor361100Model06038Verts[258] = {
#include "assets/actor_361100_model_06038_verts.inc"
};

static SVECTOR _gActor503500Actor361100Model06038Normals[270] = {
#include "assets/actor_361100_model_06038_normals.inc"
};

static u32 _gActor503500Actor361100Model06038Stream[3353] = {
#include "assets/actor_361100_model_06038_stream.inc"
};

static TmdSource _gActor503500Actor361100Model06038 = {
    0,
    15104,
    9712,
    19,
    _gActor503500Actor361100Model06038PartVerts,
    _gActor503500Actor361100Model06038Verts,
    _gActor503500Actor361100Model06038Normals,
    _gActor503500Actor361100Model06038Skeleton,
    _gActor503500Actor361100Model06038Stream,
};

static AnimationPackedPose _gActor503500Animation444F4Bank1[15] = {
#include "assets/actor_503500_animation_444F4_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation444F4Bank4[49] = {
#include "assets/actor_503500_animation_444F4_bank4.inc"
};

static AnimationRecord _gActor503500Animation444F4Records[226] = {
#include "assets/actor_503500_animation_444F4_records.inc"
};

static u16 _gActor503500Animation444F4Indices[20] = {
#include "assets/actor_503500_animation_444F4_indices.inc"
};

static AnimationSet _gActor503500Animation444F4 = {
    _gActor503500Animation444F4Records,
    _gActor503500Animation444F4Indices,
    { NULL, _gActor503500Animation444F4Bank1, NULL, NULL, _gActor503500Animation444F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation446CCBank1[6] = {
#include "assets/actor_503500_animation_446CC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation446CCBank4[18] = {
#include "assets/actor_503500_animation_446CC_bank4.inc"
};

static AnimationRecord _gActor503500Animation446CCRecords[62] = {
#include "assets/actor_503500_animation_446CC_records.inc"
};

static u16 _gActor503500Animation446CCIndices[20] = {
#include "assets/actor_503500_animation_446CC_indices.inc"
};

static AnimationSet _gActor503500Animation446CC = {
    _gActor503500Animation446CCRecords,
    _gActor503500Animation446CCIndices,
    { NULL, _gActor503500Animation446CCBank1, NULL, NULL, _gActor503500Animation446CCBank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_503500_80176514[3] = {
    NULL,
    &_gActor503500Animation444F4,
    &_gActor503500Animation446CC,
};

AnimationSet** gActorMotionAnimBanks19[1] = {
    D_actor_503500_80176514,
};

TaskDesc D_actor_503500_80176524 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_503500_801463C0, { .model = &_gActor503500Actor361100Model06038 } };

TaskMessageEntry D_actor_503500_80176530[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, func_actor_503500_80146664 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_503500_801466E0 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_503500_801467C0 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_actor_503500_80144E8C(Task* arg0)
{
    _Actor503500PinkFlashAttackWork* work;
    GfxCoord*                        coord;
    WorldCollisionCapsule*           capsule;
    WorldCollisionContact*           contacts;
    EffectWork*                      eff;
    Task*                            child;
    GfxRotationWords*                m1;
    GfxRotationWords*                m2;
    s32                              pan;

    coord = arg0->extra.tmd->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work        = work;
    work->radiusScale = ONE;

    m1         = (GfxRotationWords*)&coord->coord;
    m1->m00M01 = ONE;
    m1->m02M10 = 0;
    m1->m11M12 = ONE;
    m1->m20M21 = 0;
    m1->m22    = ONE;

    m2         = (GfxRotationWords*)&work->sweepRotation;
    m2->m00M01 = ONE;
    m2->m02M10 = 0;
    m2->m11M12 = ONE;
    m2->m20M21 = 0;
    m2->m22    = ONE;

    capsule  = &work->capsule;
    contacts = work->contacts;

    work->body.coord           = coord;
    work->body.context.capsule = capsule;
    work->body.pos.vx          = D_actor_503500_801715C4.vx;
    work->body.pos.vy          = D_actor_503500_801715C4.vy;
    work->body.pos.vz          = D_actor_503500_801715C4.vz;
    work->body.key             = Gp_PackPair(D_actor_503500_8016E7D4[0], 0);
    work->body.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->body.radius          = 0;

    capsule->contacts   = contacts;
    capsule->ends[1].vx = 0;
    capsule->ends[1].vy = 0;
    capsule->ends[1].vz = 0;
    capsule->ends[0].vx = D_actor_503500_801715CC.vx;
    capsule->ends[0].vy = D_actor_503500_801715CC.vy;
    capsule->ends[0].vz = D_actor_503500_801715CC.vz;
    capsule->end1Radius = 0x3E8;
    capsule->end0Radius = 0x7D0;

    Gp_LinkObj(3, &work->body);
    Gp_InitRec18Table(contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    if (arg0->spawnArg1.value == 0) {
        eff = Gp_SpawnEff(EFFECT_SHELTER_R48_RING_FLASH_PINK, coord, 0, NULL);
        if (eff == NULL) {
            func_actor_503500_80145480(arg0);
            return;
        }
        child            = eff->task;
        work->effectTask = child;
        taskReparent(arg0, child);
    }
    pan = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0A), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    func_actor_503500_80137290(6);
    arg0->exitCallback = func_actor_503500_80145480;
    arg0->state       += 1;
}

static void func_actor_503500_801450A0(Task* arg0)
{
    _Actor503500PinkFlashAttackWork* work;
    WorldCollisionCapsule*           capsule;
    GfxCoord*                        coord;
    s32                              pan;
    s32                              step;
    s32                              ang;

    work    = arg0->work;
    capsule = &work->capsule;
    switch (work->phase) {
        case ACTOR_503500_PINK_FLASH_ATTACK_CHARGE:
            if (++work->phaseFrames >= ACTOR_503500_PINK_FLASH_ATTACK_CHARGE_FRAMES) {
                if (arg0->spawnArg1.value == 0) {
                    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
                coord = arg0->extra.tmd->coords;
                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0B), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                work->phaseFrames = 0;
                work->phase++;
            }
            break;
        case ACTOR_503500_PINK_FLASH_ATTACK_HOLD:
            if (++work->phaseFrames >= ACTOR_503500_PINK_FLASH_ATTACK_HOLD_FRAMES) {
                if (arg0->spawnArg1.value != 0) {
                    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
                work->phaseFrames = 0;
                work->phase++;
            }
            break;
        case ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_ACCELERATE:
            step = -ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_ACCELERATION;
            if (arg0->spawnArg1.value != 0) {
                step = ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_ACCELERATION;
            }
            {
                GfxRotationWords* m;

                m                           = (GfxRotationWords*)&work->sweepRotation;
                m->m00M01                   = ONE;
                work->sweepAngularVelocity += step;
                work->sweepAngle.word      += work->sweepAngularVelocity;
                m->m02M10                   = 0;
                m->m11M12                   = ONE;
                m->m20M21                   = 0;
                m->m22                      = ONE;
            }
            RotMatrixY(work->sweepAngle.halves.integer, &work->sweepRotation);
            ang = work->sweepAngle.halves.integer;
            if (ang < 0) {
                ang = -ang;
            }
            if (ang > ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_TURNOVER_ANGLE) {
                work->phase++;
            }
            break;
        case ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_DECELERATE:
            // Bring the velocity back to zero from whichever side this task swept to.
            if (arg0->spawnArg1.value != 0) {
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
            {
                GfxRotationWords* m;

                m                      = (GfxRotationWords*)&work->sweepRotation;
                m->m00M01              = ONE;
                work->sweepAngle.word += work->sweepAngularVelocity;
                m->m02M10              = 0;
                m->m11M12              = ONE;
                m->m20M21              = 0;
                m->m22                 = ONE;
            }
            RotMatrixY(work->sweepAngle.halves.integer, &work->sweepRotation);
            break;
        case ACTOR_503500_PINK_FLASH_ATTACK_FADE:
            if (work->radiusScale <= ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_SCALE_OFF) {
                work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->phase++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
    if (work->phase >= ACTOR_503500_PINK_FLASH_ATTACK_SWEEP_ACCELERATE) {
        // Shrink the capsule; the radii are scaled by radiusScale / ONE, with both halves of that fraction divided by 8.
        work->radiusScale -= ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_SCALE_STEP;
        if (work->radiusScale < ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_SCALE_MIN) {
            work->radiusScale = ACTOR_503500_PINK_FLASH_ATTACK_RADIUS_SCALE_MIN;
        }
        capsule->end0Radius = work->radiusScale * (ACTOR_503500_PINK_FLASH_ATTACK_FAR_RADIUS / 8) >> 9;
        capsule->end1Radius = work->radiusScale * (ACTOR_503500_PINK_FLASH_ATTACK_NEAR_RADIUS / 8) >> 9;
        // Turn the far end about Y by the sweep angle.
        gte_SetRotMatrix(&work->sweepRotation);
        gte_ldv0(&D_actor_503500_801715CC);
        gte_rtv0();
        gte_stsv(&capsule->ends[0]);
    }
}

static void func_actor_503500_80145428(Task* arg0)
{
    GfxCoord* coord;
    s32       state;

    state = gSceneCombatState.actorControl;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord               = arg0->extra.tmd->coords;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_503500_801454E0(arg0);
    func_actor_503500_801450A0(arg0);
}

static void func_actor_503500_80145480(Task* arg0)
{
    _Actor503500PinkFlashAttackWork* work;
    TmdObject*                       ext;

    func_actor_503500_801372AC(6);
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0B), 1);
    ext                   = arg0->extra.tmd;
    (ext->coords)->parent = &gGfxViewCoord;
    work                  = arg0->work;
    Gp_UnlinkObj(&work->body);
    taskKill(arg0);
}

static void func_actor_503500_801454E0(Task* arg0)
{
    _Actor503500PinkFlashAttackWork* work;
    WorldCollisionContact*           contacts;
    s32                              i;

    work     = arg0->work;
    contacts = work->contacts;
    for (i = 0; i < ARRAY_SIZE(work->contacts); i++) {
        if ((contacts[i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == 0x10000) {
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
    Gp_ClearRec18Occupied(contacts);
}

void func_actor_503500_8014554C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_801321F4;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_801459D4` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132218 = {
    {
        func_actor_503500_801455A4,
        func_actor_503500_801458F8,
        func_actor_503500_80145950,
    },
};

static void func_actor_503500_801455A4(Task* arg0)
{
    _Actor503500YellowFlashAttackWork* work;
    GfxCoord*                          coord;
    GfxRotationWords*                  m;
    EffectWork*                        eff;
    Task*                              child;
    s32                                pan;

    coord = arg0->extra.tmd->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work = work;

    m         = (GfxRotationWords*)&coord->coord;
    m->m00M01 = ONE;
    m->m02M10 = 0;
    m->m11M12 = ONE;
    m->m20M21 = 0;
    m->m22    = ONE;

    work->body.coord            = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    work->body.context.contacts = work->contacts;
    work->body.pos.vx           = D_actor_503500_801715D4.vx;
    work->body.pos.vy           = D_actor_503500_801715D4.vy;
    work->body.pos.vz           = D_actor_503500_801715D4.vz;
    work->body.key              = Gp_PackPair(D_actor_503500_8016E7D4[1], 0);
    work->body.radius           = 0x12C;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->body);
    Gp_InitRec18Table(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    eff = Gp_SpawnEff(EFFECT_SHELTER_R48_RING_FLASH_YELLOW, coord, 0, NULL);
    if (eff == NULL) {
        func_actor_503500_80145950(arg0);
        return;
    }
    child            = eff->task;
    work->effectTask = child;
    taskReparent(arg0, child);
    pan = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0C), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    func_actor_503500_80137290(6);
    arg0->exitCallback = func_actor_503500_80145950;
    arg0->state       += 1;
}

static void func_actor_503500_80145754(Task* arg0)
{
    _Actor503500YellowFlashAttackWork* work;
    GfxCoord*                          coord;
    GfxCoord*                          coord2;
    s32                                pan;
    s32                                pan2;

    work = arg0->work;
    if (func_actor_503500_8013608C(arg0) == 0) {
        switch (work->phase) {
            case ACTOR_503500_YELLOW_FLASH_ATTACK_CHARGE:
                work->phaseFrames++;
                if (work->phaseFrames >= ACTOR_503500_YELLOW_FLASH_ATTACK_CHARGE_FRAMES) {
                    // The strike only lands on a player standing this high (Y is negative upwards).
                    if (gPlayerStatus.coordMtx->t[1] < -1000) {
                        work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    }
                    work->phaseFrames = 0;
                    work->phase++;
                } else if (work->phaseFrames == 0x3E) {
                    coord = arg0->extra.tmd->coords;
                    pan   = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x14), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                } else if (work->phaseFrames == 0x5A) {
                    coord2 = arg0->extra.tmd->coords;
                    pan2   = (s8)worldCoordGetOriginAudioPan(coord2);
                    sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0D), pan2, (s8)(worldCoordGetOriginAudioDepth(coord2) / 2));
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
    arg0->state += 1;
}

static void func_actor_503500_801458F8(Task* arg0)
{
    GfxCoord* coord;
    s32       state;

    coord = arg0->extra.tmd->coords;
    state = gSceneCombatState.actorControl;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_503500_801459B0(arg0);
    func_actor_503500_80145754(arg0);
}

static void func_actor_503500_80145950(Task* arg0)
{
    _Actor503500YellowFlashAttackWork* work;
    TmdObject*                         ext;

    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0C), 1);
    func_actor_503500_801372AC(6);
    ext                   = arg0->extra.tmd;
    (ext->coords)->parent = &gGfxViewCoord;
    work                  = arg0->work;
    Gp_UnlinkObj(&work->body);
    taskKill(arg0);
}

static void func_actor_503500_801459B0(Task* arg0)
{
    _Actor503500YellowFlashAttackWork* work;

    work = arg0->work;
    Gp_ClearRec18Occupied(work->contacts);
}

void func_actor_503500_801459D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132218;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_80145F84` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132224 = {
    {
        func_actor_503500_80145A2C,
        func_actor_503500_80145E1C,
        func_actor_503500_80145E98,
    },
};

static void func_actor_503500_80145A2C(Task* arg0)
{
    _Actor503500OrangeFlashAttackWork* work;
    GfxCoord*                          coord;
    WorldCollisionCapsule*             capsule;
    WorldCollisionContact*             contacts;
    EffectWork*                        eff;
    Task*                              child;
    GfxRotationWords*                  m;
    s32                                pan;
    s32                                pan2;

    coord = arg0->extra.tmd->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work = work;

    m         = (GfxRotationWords*)&coord->coord;
    m->m00M01 = ONE;
    m->m02M10 = 0;
    m->m11M12 = ONE;
    m->m20M21 = 0;
    m->m22    = ONE;

    capsule  = &work->capsule;
    contacts = work->contacts;

    work->body.coord           = coord;
    work->body.context.capsule = capsule;
    work->body.pos.vx          = D_actor_503500_801715DC.vx;
    work->body.pos.vy          = D_actor_503500_801715DC.vy;
    work->body.pos.vz          = D_actor_503500_801715DC.vz;
    work->body.key             = Gp_PackPair(D_actor_503500_8016E7DC[0], 0);
    work->body.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->body.radius          = 0;

    capsule->contacts   = contacts;
    capsule->ends[1].vx = 0;
    capsule->ends[1].vy = 0;
    capsule->ends[1].vz = 0;
    capsule->ends[0].vx = D_actor_503500_801715E4.vx;
    capsule->ends[0].vy = D_actor_503500_801715E4.vy;
    capsule->ends[0].vz = D_actor_503500_801715E4.vz;
    capsule->end1Radius = 0x7D0;
    capsule->end0Radius = 0xBB8;

    Gp_LinkObj(3, &work->body);
    Gp_InitRec18Table(contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    eff = Gp_SpawnEff(EFFECT_SHELTER_R48_RING_FLASH, coord, arg0->spawnArg1.value, NULL);
    if (eff == NULL) {
        func_actor_503500_80145E98(arg0);
        return;
    }
    child            = eff->task;
    work->effectTask = child;
    taskReparent(arg0, child);
    if (gGameSession->eventState != 0) {
        pan = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x13), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    } else {
        pan2 = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0E), pan2, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    }
    func_actor_503500_80137290(8);
    arg0->exitCallback = func_actor_503500_80145E98;
    arg0->state       += 1;
}

static void func_actor_503500_80145C50(Task* arg0)
{
    _Actor503500OrangeFlashAttackWork* work;
    GfxCoord*                          coord;
    s32                                pan;

    work = arg0->work;
    switch (work->phase) {
        case ACTOR_503500_ORANGE_FLASH_ATTACK_CHARGE:
            if (gDisplayState.animFrame & 1) {
                Gp_SpawnPadLerp(1, 0x96, 0x96);
            }
            if (++work->phaseFrames < ACTOR_503500_ORANGE_FLASH_ATTACK_CHARGE_FRAMES) {
                return;
            }
            if (gGameSession->eventState == 0) {
                work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                coord             = arg0->extra.tmd->coords;
                pan               = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0F), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
            }
            goto next;
        case ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE:
            if (gGameSession->eventState == 0) {
                Gp_SpawnPadLerp(1, 0xFF, 0xFF);
            }
            if (++work->phaseFrames < ACTOR_503500_ORANGE_FLASH_ATTACK_STRIKE_FRAMES) {
                return;
            }
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0F), 1);
        next:
            work->phaseFrames = 0;
            work->phase++;
            return;
        case ACTOR_503500_ORANGE_FLASH_ATTACK_LINGER:
            if (++work->phaseFrames < ACTOR_503500_ORANGE_FLASH_ATTACK_LINGER_FRAMES) {
                return;
            }
        default:
            arg0->state += 1;
            return;
    }
}

static void func_actor_503500_80145E1C(Task* arg0)
{
    GfxCoord* coord;
    s32       state;

    coord = arg0->extra.tmd->coords;
    state = gSceneCombatState.actorControl;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_503500_80145F18(arg0);
    func_actor_503500_80145C50(arg0);
    if (func_actor_503500_8013608C(arg0->spawnArg2.pointer)) {
        arg0->exitCallback(arg0);
    }
}

static void func_actor_503500_80145E98(Task* arg0)
{
    _Actor503500OrangeFlashAttackWork* work;
    TmdObject*                         ext;

    func_actor_503500_801372AC(8);
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0E), 1);
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x13), 1);
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0F), 1);
    ext                   = arg0->extra.tmd;
    (ext->coords)->parent = &gGfxViewCoord;
    work                  = arg0->work;
    Gp_UnlinkObj(&work->body);
    taskKill(arg0);
}

static void func_actor_503500_80145F18(Task* arg0)
{
    _Actor503500OrangeFlashAttackWork* work;
    WorldCollisionContact*             contacts;
    s32                                i;

    work     = arg0->work;
    contacts = work->contacts;
    for (i = 0; i < ARRAY_SIZE(work->contacts); i++) {
        if ((contacts[i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == 0x10000) {
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
    Gp_ClearRec18Occupied(contacts);
}

void func_actor_503500_80145F84(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132224;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_801463C0` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132230 = {
    {
        func_actor_503500_8014642C,
        func_actor_503500_80145FDC,
        func_actor_503500_801464E8,
    },
};

/// Per-frame tick of the `_Actor503500Actor361100Model06038Work` actor: runs the
/// handler `motion` selects, adds `velocity` onto `carry`, moves the
/// coordinate by the integer halves and keeps only the fractions, then ticks
/// the animation slots and the actor colour. `freeCountdown` counts the
/// model's buffers down to the free.
static void func_actor_503500_80145FDC(Task* task)
{
    VECTOR                                 pos;
    TmdObject*                             ext      = task->extra.tmd;
    _Actor503500Actor361100Model06038Work* work     = task->work;
    TaskFunc                               funcs[2] = { func_actor_503500_80146524, func_actor_503500_8014618C };
    GfxCoord*                              coord;
    s32                                    i;

    funcs[work->motion](task);
    coord                = task->extra.tmd->coords;
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
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        pos.vx = coord->workm.t[0];
        pos.vy = coord->workm.t[1];
        pos.vz = coord->workm.t[2];
        Gp_UpdateActorColor(task->spawnArg2.pointer, &pos, 0, 0);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->freeCountdown--;
    }
}

/// The collapse, the handler `_Actor503500Actor361100Model06038Work::motion`
/// selects once it is set, stepped by `motionStep`: the first step saves the
/// coordinate's rotation into `unscaledRotation`, the second waits, and the
/// third restores that rotation every frame while squashing it by
/// `collapseScaleY`, from `ONE` down to an eighth, firing the light and burn
/// cues on the way before advancing the task to its exit state.
static void func_actor_503500_8014618C(Task* arg0)
{
    VECTOR                                 scale;
    GfxCoord*                              coord;
    _Actor503500Actor361100Model06038Work* work;
    TmdObject*                             ext;
    void*                                  enemy;
    s32*                                   src;
    s32*                                   dst;
    s32                                    i;

    // `extra` is read twice on purpose: the second read is what leaves the
    // target's `move s2, v0` copy.
    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    ext   = arg0->extra.tmd;
    switch (work->motionStep) {
        case ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_STEP_SAVE:
            work->motionStepFrames = 0;
            work->collapseScaleY   = ONE;
            // The rotation is nine halfwords, copied as four words and one more halfword.
            dst = (s32*)work->unscaledRotation.m;
            src = (s32*)coord->coord.m;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            work->unscaledRotation.m[2][2] = coord->coord.m[2][2];
            work->motionStep++;
            break;
        case ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_STEP_WAIT:
            work->motionStepFrames++;
            if (work->motionStepFrames >= ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_WAIT_FRAMES) {
                work->motionStepFrames = 0;
                work->motionStep++;
            }
            break;
        case ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_STEP_SQUASH:
            if (work->collapseScaleY > ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_SCALE_MIN) {
                work->collapseScaleY -= ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_SCALE_STEP;
            }
            // Scaling compounds, so each frame starts again from the saved rotation.
            dst = (s32*)coord->coord.m;
            src = (s32*)work->unscaledRotation.m;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            coord->coord.m[2][2] = work->unscaledRotation.m[2][2];
            scale.vx             = ONE;
            scale.vy             = work->collapseScaleY;
            scale.vz             = ONE;
            ScaleMatrixL(&coord->coord, &scale);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->motionStepFrames++;
            switch (work->motionStepFrames) {
                case ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_FADE_FRAME:
                    ext->flags |= TMD_OBJECT_SEMI_TRANS;
                    Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                    break;
                case ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_BURN_FRAME:
                    Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 2, NULL);
                    break;
                case ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_BLACK_FRAME:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case ACTOR_503500_ACTOR_361100_MODEL_06038_COLLAPSE_EXIT_FRAME:
                    arg0->state++;
                    break;
            }
            break;
    }
}

void func_actor_503500_801463C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132230;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

static void func_actor_503500_8014642C(Task* arg0)
{
    _Actor503500Actor361100Model06038Work* work;
    GfxCoord*                              coord;
    Enemy*                                 enemy;

    coord = arg0->extra.tmd->coords;
    enemy = arg0->spawnArg2.pointer;

    work = memCalloc(sizeof(_Actor503500Actor361100Model06038Work), false);
    if (work == NULL) {
        enemyTaskExit(arg0);
        return;
    }

    arg0->work          = work;
    work->model.animId  = ACTOR_MODEL_STATE_NONE;
    work->model.bank    = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown = -1;
    work->carry[0].word = 0;
    work->carry[1].word = 0;
    work->carry[2].word = 0;

    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    enemy->recs     = 0;

    func_actor_503500_80146508(arg0);

    arg0->msgTable     = D_actor_503500_80176530;
    arg0->exitCallback = func_actor_503500_801464E8;
    arg0->state++;
}
/// `Task::exitCallback` of the effect task `func_actor_503500_8014642C`
/// initialises, and the third entry of its state table: hands the task to
/// `enemyTaskExit`.
static void func_actor_503500_801464E8(Task* arg0)
{
    enemyTaskExit(arg0);
}

static void func_actor_503500_80146508(Task* arg0)
{
    TmdObject*                             ext;
    _Actor503500Actor361100Model06038Work* work;

    work          = arg0->work;
    ext           = arg0->extra.tmd;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

static void func_actor_503500_80146524(Task* arg0)
{
}

#include "../../shared/actor_motion_play19.inc.c"

/// A second copy of the handler, under this file's own name.
#define actorMsgPlaceEuler func_actor_503500_80146664
#include "../../shared/actor_messages_place_euler.inc.c"
#undef actorMsgPlaceEuler

s32 func_actor_503500_801466E0(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject* ext;
    s32        ret;

    ext = task->extra.tmd;
    ret = 0;
    switch (mode) {
        case 0:
            ext->flags = (ext->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW) & ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            ext->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(ext);
            ext->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            ext->flags                                                         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((_Actor503500Actor361100Model06038Work*)task->work)->freeCountdown = mode;
            ext->flags                                                         |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            ext->flags = (ext->flags & ~TMD_OBJECT_SKIP_ACTIVE_DRAW) | TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_503500_801467C0(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    _Actor503500Actor361100Model06038Work* work;

    work = task->work;
    switch (msg->command) {
        case 0:
            work->velocity.vx = 0;
            work->velocity.vy = 0;
            work->velocity.vz = 0;
            break;
        case 1:
            work->velocity.vx = 0x0100F4DE;
            work->velocity.vy = 0xFF6DE9BE;
            work->velocity.vz = 0x68590;
            break;
        case 2:
            work->velocity.vx = 0x1371C7;
            work->velocity.vy = 0xBAAAA;
            work->velocity.vz = 0;
            work->motion      = ACTOR_503500_ACTOR_361100_MODEL_06038_MOTION_COLLAPSE;
            break;
        case 3:
            task->exitCallback(task);
            break;
    }
    return 0;
}
