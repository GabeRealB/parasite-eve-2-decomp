#include <psyq/sys/types.h>
#include <psyq/gtemac.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/enemy.h"
#include "gameplay/lighting_work.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
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

#include "rooms/mist_shooting_gallery.h"

/* The package holds two tasks. The gallery's controller spawns a mount, which
 * counts as one of its live targets; the mount spawns the target it carries as
 * its child. The two signal each other through the target task's spawn
 * argument. */

/// Mount/target handshake bits in the child target's spawn argument.
///
/// Both tasks keep the argument for their lifetime. The mount raises READY
/// after settling; the target raises STANDING after unfolding. Either can
/// request STOP, and FINISHED lets the mount shrink after the target leaves.
enum {
    ACTOR_107600_TARGET_SIGNAL_MOUNT_READY = 0x10,
    ACTOR_107600_TARGET_SIGNAL_STANDING    = 0x20,
    ACTOR_107600_TARGET_SIGNAL_STOP        = 0x40,
    ACTOR_107600_TARGET_SIGNAL_FINISHED    = 0x80,
};

/// Fields of the mount's spawn argument used by its path callbacks.
enum {
    ACTOR_107600_MOUNT_SPAWN_HOLD_SHIFT = 24,
    ACTOR_107600_MOUNT_SPAWN_HOLD_MASK  = 0xF,
    ACTOR_107600_MOUNT_SPAWN_ROTATING   = 0x10000000,
    ACTOR_107600_FRAMES_PER_SECOND      = 30,
};

/// Fields of the child's spawn argument: kind, dead marker and mount behaviour.
enum {
    ACTOR_107600_TARGET_SPAWN_KIND_MASK       = 0xF,
    ACTOR_107600_TARGET_SPAWN_DEAD            = 0xFF,
    ACTOR_107600_TARGET_SPAWN_BEHAVIOUR_SHIFT = 16,
    ACTOR_107600_TARGET_SPAWN_BEHAVIOUR_MASK  = 0xF,
};

/// Wrapped Euler-angle units of the mount and target.
enum {
    ACTOR_107600_ANGLE_TURN         = 0x1000,
    ACTOR_107600_ANGLE_MASK         = ACTOR_107600_ANGLE_TURN - 1,
    ACTOR_107600_ANGLE_QUARTER_TURN = ACTOR_107600_ANGLE_TURN / 4,
};

/// Steps of the target's damage flinch and the active step it resumes.
enum {
    ACTOR_107600_TARGET_FLINCH_STEP_INIT   = 0,
    ACTOR_107600_TARGET_FLINCH_STEP_SWING  = 1,
    ACTOR_107600_TARGET_FLINCH_STEP_SETTLE = 2,
    ACTOR_107600_TARGET_ACTIVE_STEP_STAND  = 3,
};

/// Steps of an undestroyed target's fold-away sequence.
enum {
    ACTOR_107600_TARGET_FOLD_STEP_BEGIN  = 0,
    ACTOR_107600_TARGET_FOLD_STEP_WAIT   = 1,
    ACTOR_107600_TARGET_FOLD_STEP_FOLD   = 2,
    ACTOR_107600_TARGET_FOLD_STEP_SHRINK = 3,
    ACTOR_107600_TARGET_FOLD_STEP_DONE   = 4,
};

/// Steps of a destroyed target's tumble; step 2 is an unused inert handler.
enum {
    ACTOR_107600_TARGET_TUMBLE_STEP_BEGIN   = 0,
    ACTOR_107600_TARGET_TUMBLE_STEP_MOVE    = 1,
    ACTOR_107600_TARGET_TUMBLE_STEP_INERT   = 2,
    ACTOR_107600_TARGET_TUMBLE_FRAMES       = 16,
    ACTOR_107600_TARGET_SHRINK_STEP_PERCENT = 32,
    ACTOR_107600_TARGET_SHRINK_STOP_PERCENT = 20,
};

/// Kill cues selected by the target kind, kept separate from its fold-away cue.
enum {
    ACTOR_107600_TARGET_KILL_SOUND_KIND_9       = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0x10),
    ACTOR_107600_TARGET_KILL_SOUND_KINDS_0_TO_8 = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0x11),
    ACTOR_107600_TARGET_KILL_SOUND_KINDS_10_UP  = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0x12),
};

/// Gallery face-effect atlas bindings, ordering-depth units and hit-mark limit.
enum {
    ACTOR_107600_TARGET_EFFECT_TPAGE     = getTPage(1, 0, 576, 256),
    ACTOR_107600_TARGET_EFFECT_CLUT      = getClut(0, 250),
    ACTOR_107600_TARGET_EFFECT_MIN_DEPTH = 0x40,
    ACTOR_107600_TARGET_EFFECT_OT_SHIFT  = 4,
    ACTOR_107600_TARGET_HIT_MARK_COUNT   = 8,
};

/// How a mount behaves, from bits 12-15 of its spawn argument: the value of
/// `_Actor107600MountWork::behaviour`, and of the copy its target keeps in
/// `_Actor107600TargetWork::mountBehaviour`.
enum {
    ACTOR_107600_MOUNT_STANDING = 0, // Rises at floor height and follows its path
    ACTOR_107600_MOUNT_HANGING  = 1, // Turned half a revolution about Z and raised 0xF3C above the floor; follows its path
    ACTOR_107600_MOUNT_FIXED    = 2, // Stays where it is placed; not one of the gallery's live targets
};

/// Steps of a path-following mount, in `_Actor107600MountWork::step`.
enum {
    ACTOR_107600_MOUNT_STEP_RISE        = 0, // Grow to full height
    ACTOR_107600_MOUNT_STEP_SETTLE      = 1, // Bob for four frames, then tell the target to unfold
    ACTOR_107600_MOUNT_STEP_WAIT_TARGET = 2, // Wait for the target to stand, then pick travelling or holding from the path
    ACTOR_107600_MOUNT_STEP_TRAVEL      = 3, // Close on the path's stops in turn
    ACTOR_107600_MOUNT_STEP_HOLD        = 4, // Stay in place until the hold time runs out
    ACTOR_107600_MOUNT_STEP_LEAVE       = 5, // Wait for the target to finish, then shrink away
};

/// Steps of a fixed mount, in `_Actor107600MountWork::step`.
enum {
    ACTOR_107600_MOUNT_FIXED_STEP_PLACE = 0, // Stand at full height on the floor
    ACTOR_107600_MOUNT_FIXED_STEP_LEAVE = 1, // Wait for the target to finish, then shrink away
};

/// Work block of a gallery target's mount: the stand that rises where the
/// gallery places it, carries its target along a path and shrinks away once the
/// target has gone.
///
/// The mount's spawn state allocates it zeroed and the mount's task owns it.
/// Angles are 4096 to the turn, wrapped to one turn every frame.
typedef struct {
    MATRIX colorMatrix;      // Colour matrix the mount's model is lit through, in place of the scene's shared one
    MATRIX lightMatrix;      // Light matrix of the same
    s16    pitch;            // Rotation of the model root about X. Nothing sets it, so it stays 0
    s16    yaw;              // About Y: starts a quarter turn back and advances 0x20 a frame while `rotating`
    s16    roll;             // About Z: half a turn on a hanging mount
    byte   unknown_46[2];    // No field access established; role unproven
    s16    spawnX;           // Position of the model root when the mount spawned. Nothing reads it back
    s16    spawnY;           // The same, on Y
    s16    spawnZ;           // The same, on Z
    byte   unknown_4E[0xEC]; // No field access established; role unproven
    s16    timer;            // Frames: counts the settling bob, then counts down the hold of a stationary path
    byte   unknown_13C[2];   // No field access established; role unproven
    s16    state;            // Update routine (0 lets the spawn frame pass, 1 runs `behaviour`)
    s16    step;             // Progress through `behaviour`: `ACTOR_107600_MOUNT_STEP_*` on a path-following mount, `ACTOR_107600_MOUNT_FIXED_STEP_*` on a fixed one
    byte   unknown_142[2];   // No field access established; role unproven
    s16    behaviour;        // `ACTOR_107600_MOUNT_*`
    s16    path;             // Path the mount follows, from bits 16-23 of the spawn argument
    s16    waypoint;         // Stop of `path` the mount is closing on
    u8     rotating;         // Nonzero while the mount turns about Y: set once the target stands when bit 28 of the spawn argument asks for it, cleared when the path ends
    s8     heightPercent;    // Vertical scale of the model root, in percent: grows from 0 and shrinks from 100 in steps of 8, clamped the frame after it passes either end
} _Actor107600MountWork;
STATIC_ASSERT_SIZEOF(_Actor107600MountWork, 0x14C);

/// Behaviour states of a target, in `_Actor107600TargetWork::state`.
///
/// States 4 to 6 answer hit reactions nothing in the package requests; each
/// returns to `ACTOR_107600_TARGET_STATE_ACTIVE` at once.
enum {
    ACTOR_107600_TARGET_STATE_INIT         = 0, // Choose the first state from the mount's behaviour
    ACTOR_107600_TARGET_STATE_ACTIVE       = 1, // Unfold, then stand - attacking if the spawn argument says so - until the mount stops
    ACTOR_107600_TARGET_STATE_LIGHT_FLINCH = 2, // Swing back from a hit and recover
    ACTOR_107600_TARGET_STATE_HEAVY_FLINCH = 3, // The same routine, for a hit of 20 damage or more
    ACTOR_107600_TARGET_STATE_LEAVE        = 7, // Fold down and shrink away undestroyed
    ACTOR_107600_TARGET_STATE_FIXED        = 8, // Target of a fixed mount: show three hit marks and leave
    ACTOR_107600_TARGET_STATE_DESTROYED    = 9, // Out of hit points: score the kill, tumble away and shrink
};

/// State change a damaging hit asks for, in `_Actor107600TargetWork::hitReaction`.
enum {
    ACTOR_107600_HIT_REACTION_NONE  = 0,
    ACTOR_107600_HIT_REACTION_LIGHT = 1, // Below 20 damage
    ACTOR_107600_HIT_REACTION_HEAVY = 2,
};

/// Work block of a gallery target: the object the player shoots at, carried by
/// its mount.
///
/// The target's spawn state allocates it zeroed and the target's task owns it;
/// the collision body stays linked until the task exits. Angles are 4096 to the
/// turn, wrapped to one turn every frame. Timers count frames.
typedef struct {
    MATRIX                colorMatrix;    // Colour matrix the target's model is lit through, in place of the scene's shared one
    MATRIX                lightMatrix;    // Light matrix of the same
    VECTOR                knockback;      // Axis of the shot that last hit, 4096 per unit; a destroyed target reduces it to a decaying vertical velocity, in world units a frame
    s16                   pitch;          // Rotation of the model root about X: a quarter turn lays the target flat, 0 stands it up
    s16                   yaw;            // About Y; takes the mount's when the target is destroyed
    s16                   roll;           // About Z; takes the mount's when the target is destroyed
    byte                  unknown_56[2];  // No field access established; role unproven
    s16                   pitchSpin;      // Angle added to `pitch` each frame of a destroyed target's tumble, decaying
    s16                   yawSpin;        // The same, for `yaw`
    s16                   rollSpin;       // The same, for `roll`
    byte                  unknown_5E[2];  // No field access established; role unproven
    WorldCollisionBody    body;           // Sphere the player's shots are tested against
    WorldCollisionContact contacts[8];    // Contacts of `body`, also the enemy's contact records
    GfxCoord*             field_140;      // Set to the target's root coordinate at spawn. Nothing reads it back; role unproven
    s16                   field_144;      // Set to 0x140 at spawn. Nothing reads it back; role unproven
    s16                   field_146;      // Set to 2 at spawn. Nothing reads it back; role unproven
    byte                  unknown_148[4]; // No field access established; role unproven
    s32                   playerDistance; // Horizontal distance to the player when the last shot hit, in world units
    s16                   hitCooldown;    // Frames before another hit registers, from the weapon that last hit
    byte                  unknown_152[2]; // No field access established; role unproven
    s16                   timer;          // Frame counter of the current state's step
    s16                   hitTaken;       // 1 when the last hit check found a damaging hit, 0 otherwise
    s16                   state;          // `ACTOR_107600_TARGET_STATE_*`
    s16                   step;           // Progress through `state`; cleared on entering one
    s16                   field_15C;      // Cleared when the target leaves or is destroyed. Nothing reads it; role unproven
    u16                   hitReaction;    // `ACTOR_107600_HIT_REACTION_*`, consumed by the active state
    s16                   hitDamage;      // Damage of the last hit; sizes the flinch
    s16                   mountBehaviour; // `ACTOR_107600_MOUNT_*` of the mount carrying the target
    byte                  unknown_164[2]; // No field access established; role unproven
    s16                   attackTimer;    // Frames since an attacking target last fired: it charges at 120 and fires at 210
    u8                    widthPercent;   // Scale of the model root across its width, in percent
    u8                    heightPercent;  // Scale of the model root along its height while it lies flat, in percent
    u8                    hitMarkFirst;   // First entry of the hit-mark offsets this target uses, rolled 0 to 7 at start
    u8                    hitMarkCount;   // Hit marks drawn on the target: one per damaging hit, at most 8
} _Actor107600TargetWork;
STATIC_ASSERT_SIZEOF(_Actor107600TargetWork, 0x16C);

/// `_Actor107600Waypoint::x` of the record that closes a path.
enum { ACTOR_107600_PATH_END = -1 };

/// One stop on a movement path of the gallery target's mount.
///
/// A path is an array of these closed by a record whose `x` is
/// `ACTOR_107600_PATH_END`; a target's spawn argument selects the path. The
/// mount closes on the current stop along X and Z independently, `speed` units
/// a frame on each, and moves on to the next record whenever either axis
/// arrives. A leg that changes both coordinates therefore lists its stop
/// twice, one record per arrival. Coordinates are in the space the mount is
/// spawned in.
///
/// A path whose first record has no speed is stationary: the mount stays where
/// it was spawned for the hold time in its spawn argument, and that record's
/// coordinates are not read.
typedef struct {
    s16 x;     // Stop position, or `ACTOR_107600_PATH_END`
    s16 z;
    s16 speed; // Distance per frame on each axis towards this stop; 0 in a path's first record holds the mount in place
} _Actor107600Waypoint;
STATIC_ASSERT_SIZEOF(_Actor107600Waypoint, 6);

/// Scratch-stack workspace for one textured quad drawn on a target's face: a
/// hit mark, or the burst that replaces the last one on a destroyed target.
///
/// `vertices` holds the four corners in the space of the target's root
/// coordinate. One RTPS projects corner 0 and one RTPT corners 1..3 through
/// that coordinate's composed matrix, and the drawer splits each resulting
/// screen word onto the packet. A quad whose `depth` falls below the nearest
/// ordering bucket it may use is dropped.
///
/// Reserve the whole block and release it before the drawer returns. Pointers
/// into the block must not survive release.
typedef struct {
    s32     screenCorners[4]; // Projected corners, one GTE screen word each: X in the low half, Y in the high
    s32     depth;            // Last projected corner's SZ3 / 4, less the drawer's ordering bias
    SVECTOR vertices[4];      // Corners handed to the projection
} _Actor107600QuadScratch;
STATIC_ASSERT_SIZEOF(_Actor107600QuadScratch, 0x34);

static void _actor107600MountTask(Task* task);
static void _actor107600RetireMount(Task* task);
static void _actor107600DestroyMount(Task* task);
static void _actor107600UpdateMountColor(Task* task);
static void _actor107600UpdateMountRotation(Task* task);
static void _actor107600CopyMountRotation(const MATRIX* source, MATRIX* destination);
static void _actor107600BeginMountBehaviour(Task* task);
static void _actor107600UpdateMountBehaviour(Task* task);
static void _actor107600UpdateFixedMount(Task* task);
static void _actor107600SpawnTarget(Enemy* mountEnemy, s32 targetKind, s32 targetFlags);
static void _actor107600InitTarget(Task* task);
static void _actor107600UpdateTarget(Task* task);
static void _actor107600UpdateActiveTarget(Task* task);
static void _actor107600UpdateTargetFlinch(Task* task);
static void _actor107600FoldTarget(Task* task);
static void _actor107600TumbleDestroyedTarget(Task* task);
static void _actor107600DrawTargetDeathBurst(const GfxCoord* rootCoord, const SVECTOR* centerOffset);
static void _actor107600DrawTargetHitMark(const GfxCoord* rootCoord, const SVECTOR* centerOffset);
static void _actor107600UpdateTargetColor(struct Enemy* enemy, const void* worldPosition, s32 unusedArg2, s32 unusedArg3);
static void _actor107600TargetTask(Task* task);
static void _actor107600HideTarget(Task* task);
static void _actor107600DestroyTarget(Task* task);
static void _actor107600InitTargetCollision(Task* task);
static void _actor107600UpdateTargetRootColor(Task* task);
static void _actor107600UpdateTargetRotation(Task* task);
static void _actor107600CopyTargetRotation(const MATRIX* source, MATRIX* destination);
static void _actor107600SetTargetState(Task* task, s16 state);
static s16  _actor107600ConsumeTargetHitReaction(Task* task);
static void _actor107600BeginTargetBehaviour(Task* task);
static void _actor107600ResumeTargetState4(Task* task);
static void _actor107600ResumeTargetState5(Task* task);
static void _actor107600ResumeTargetState6(Task* task);
static void _actor107600BeginFixedTargetExit(Task* task);
static void _actor107600MeasureTargetPlayerDistance(Task* task);
static void _actor107600PlaceTargetOffset(GfxCoord* rootCoord);
static void _actor107600ApplyTargetScale(Task* task);

/* Waypoint paths `_actor107600UpdateStandingMountPath` walks, indexed by
 * `_Actor107600MountWork::path`; trailing-blob data. */
extern _Actor107600Waypoint* D_actor_107600_80135624[];

/* Eight effect offsets `_actor107600UpdateTarget` cycles through from
 * `_Actor107600TargetWork::hitMarkFirst`. */
extern DVECTOR D_actor_107600_80135730[];

/* Table `_actor107600SpawnTarget` spawns from, indexed with `targetKind + 1`; it
 * is the trailing animation/data blob, not the leading rodata. */
extern TaskDesc D_actor_107600_80134F94[];

/* The pair-source record the spawn state hangs off the enemy's `Enemy.param`
 * (a zeroed pointer to `D_actor_107600_8013571C`, 0x32 and 0xFF000000) and the
 * 16-entry HP table it indexes with the target kind. Both are trailing-blob
 * data, after the collision tables. */
/* Pair-source record the spawn state hangs off `Enemy.param`. */
extern EnemyParams D_actor_107600_80134F84;
extern EnemyParams D_actor_107600_80135720;
extern u16         D_actor_107600_80135750[];

/* Remaining-enemy count, and the gallery controller task the room overlay
 * publishes (its `Task::work` is the `MistShootingGalleryWork`). */

static void _actor107600InitMount(Task* task);
static void _actor107600UpdateStandingMountPath(Task* task);
static void _actor107600UpdateHangingMountPath(Task* task);
static void _actor107600UpdateMount(Task* task);
static void _actor107600ApplyTargetHits(Task* task);

/// The actor's top-level task states, which `_actor107600MountTask` runs:
/// spawn, update, drop and destroy.
static const TaskFuncTable4 D_actor_107600_80131E24 = { {
    _actor107600InitMount,
    _actor107600UpdateMount,
    _actor107600RetireMount,
    _actor107600DestroyMount,
} };

/// One entry per `_Actor107600MountWork::behaviour`, run by
/// `_actor107600UpdateMountBehaviour`.
static const TaskFuncTable3 D_actor_107600_80131E34 = { {
    _actor107600UpdateStandingMountPath,
    _actor107600UpdateHangingMountPath,
    _actor107600UpdateFixedMount,
} };

DamageAttack D_actor_107600_80134F80[1] = { 0 };

EnemyParams D_actor_107600_80134F84 = { D_actor_107600_80134F80, 50, 0, 0, 0, 255, 0, 0, 0 };

TaskDesc D_actor_107600_80134F94[19] = {
    { { { TASK_BODY_TMD, 96 } }, _actor107600MountTask, { .model = &gMistShootingGalleryModel0A81C } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel093FC } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel095EC } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel097DC } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel099CC } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel09BBC } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel09DAC } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel09F9C } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel0A18C } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel0A37C } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel0A56C } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel0B0D0 } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel0B2C0 } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel0B4B0 } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel0B6A0 } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel0AB30 } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel0AC94 } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel0ADF8 } },
    { { { TASK_BODY_TMD, 96 } }, _actor107600TargetTask, { .model = &gMistShootingGalleryModel0AF5C } },
};

_Actor107600Waypoint D_actor_107600_80135078[2] = {
    { 0, 3000, 0 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135084[3] = {
    { 1500, 6000, 40 },
    { -1500, 6000, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135098[3] = {
    { 1500, 4500, 40 },
    { -1500, 4500, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801350AC[3] = {
    { 1500, 3000, 40 },
    { -1500, 3000, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801350C0[3] = {
    { 1500, 1500, 40 },
    { -1500, 1500, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801350D4[3] = {
    { 1500, 0, 40 },
    { -1500, 0, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801350E8[2] = {
    { -1400, 5800, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801350F4[2] = {
    { -1400, 3000, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135100[2] = {
    { -1400, 200, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_8013510C[3] = {
    { 1500, 6000, 40 },
    { 4500, 6000, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135120[3] = {
    { 1500, 4500, 40 },
    { 6000, 4500, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135134[3] = {
    { 1500, 3000, 40 },
    { 6000, 3000, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135148[3] = {
    { 1500, 1500, 40 },
    { 6000, 1500, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_8013515C[3] = {
    { 1500, 0, 40 },
    { 4500, 0, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135170[2] = {
    { 4600, 5800, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_8013517C[2] = {
    { 4600, 3000, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135188[2] = {
    { 4600, 200, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135194[3] = {
    { 6000, 3000, 40 },
    { 6000, 0, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801351A8[3] = {
    { 4500, 3000, 40 },
    { 4500, 0, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801351BC[3] = {
    { 3000, 3000, 40 },
    { 3000, 0, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801351D0[3] = {
    { 1500, 3000, 40 },
    { 1500, 0, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801351E4[3] = {
    { 0, 3000, 40 },
    { 0, 0, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801351F8[3] = {
    { -1500, 3000, 40 },
    { -1500, 0, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_8013520C[2] = {
    { 4600, 200, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135218[2] = {
    { 1600, 200, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135224[2] = {
    { -1400, 200, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135230[3] = {
    { 6000, 3000, 40 },
    { 6000, 6000, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135244[3] = {
    { 4500, 3000, 40 },
    { 4500, 6000, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135258[3] = {
    { 3000, 3000, 40 },
    { 3000, 6000, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_8013526C[3] = {
    { 1500, 3000, 40 },
    { 1500, 6000, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135280[3] = {
    { 0, 3000, 40 },
    { 0, 6000, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135294[3] = {
    { -1500, 3000, 40 },
    { -1500, 6000, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801352A8[2] = {
    { 4600, 5800, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801352B4[2] = {
    { 1600, 5800, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801352C0[2] = {
    { -1400, 5800, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801352CC[5] = {
    { 1500, 6000, 50 },
    { 1500, 3000, 50 },
    { 4500, 3000, 50 },
    { 4500, 0, 50 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801352EC[5] = {
    { 1500, 0, 50 },
    { 1500, 3000, 50 },
    { 4500, 3000, 50 },
    { 4500, 6000, 50 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_8013530C[8] = {
    { 0, 6000, 30 },
    { 0, 4500, 30 },
    { 1500, 4500, 40 },
    { 1500, 6000, 40 },
    { 4500, 6000, 50 },
    { 4500, 4500, 50 },
    { 6000, 4500, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_8013533C[8] = {
    { 0, 0, 30 },
    { 0, 1500, 30 },
    { 1500, 1500, 40 },
    { 1500, 0, 40 },
    { 4500, 0, 50 },
    { 4500, 1500, 50 },
    { 6000, 1500, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_8013536C[10] = {
    { -1500, 0, 40 },
    { 0, 0, 40 },
    { 0, 6000, 50 },
    { 1500, 6000, 50 },
    { 1500, 0, 70 },
    { 3000, 0, 70 },
    { 3000, 6000, 100 },
    { 4500, 6000, 100 },
    { 4500, 0, 100 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801353A8[7] = {
    { 9000, 6000, 40 },
    { 9000, 4500, 40 },
    { 6000, 4500, 50 },
    { 3000, 4500, 50 },
    { 3000, 6000, 50 },
    { -3000, 6000, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801353D4[7] = {
    { 9000, 0, 40 },
    { 9000, 1500, 40 },
    { 6000, 1500, 50 },
    { 3000, 1500, 50 },
    { 3000, 0, 50 },
    { -3000, 0, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135400[6] = {
    { 9000, 6000, 40 },
    { 9000, 3000, 40 },
    { 9000, 0, 40 },
    { 12000, 0, 40 },
    { 12000, 3000, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135424[5] = {
    { 7500, 4500, 40 },
    { 7500, 6000, 70 },
    { 12000, 6000, 70 },
    { 12000, 4500, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135444[5] = {
    { 7500, 1500, 40 },
    { 7500, 0, 70 },
    { 12000, 0, 70 },
    { 12000, 1500, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135464[11] = {
    { 4500, 6000, 140 },
    { 4500, 4500, 140 },
    { 7500, 4500, 140 },
    { 7500, 1500, 140 },
    { 4500, 1500, 140 },
    { 4500, 0, 140 },
    { -3000, 0, 140 },
    { -3000, 3000, 140 },
    { 4500, 3000, 140 },
    { 7500, 3000, 140 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801354A8[7] = {
    { -3000, 1500, 50 },
    { 0, 1500, 50 },
    { 0, 4500, 50 },
    { 3000, 4500, 50 },
    { 3000, 1500, 50 },
    { 7500, 1500, 50 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801354D4[5] = {
    { 1500, 6000, 40 },
    { 4500, 6000, 40 },
    { 4500, 4500, 40 },
    { 7500, 4500, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801354F4[5] = {
    { 1500, 0, 40 },
    { 4500, 1500, 40 },
    { 4500, 1500, 40 },
    { 7500, 1500, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135514[5] = {
    { 12000, 0, 40 },
    { 12000, 6000, 40 },
    { 12000, 0, 40 },
    { 12000, 6000, 40 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135534[3] = {
    { 6000, 3000, 70 },
    { 6000, 0, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135548[3] = {
    { 4500, 3000, 70 },
    { 4500, 0, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_8013555C[3] = {
    { 3000, 3000, 70 },
    { 3000, 0, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135570[3] = {
    { 1500, 3000, 70 },
    { 1500, 0, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135584[3] = {
    { 0, 3000, 70 },
    { 0, 0, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135598[3] = {
    { -1500, 3000, 70 },
    { -1500, 0, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801355AC[3] = {
    { 6000, 3000, 70 },
    { 6000, 6000, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801355C0[3] = {
    { 4500, 3000, 70 },
    { 4500, 6000, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801355D4[3] = {
    { 3000, 3000, 70 },
    { 3000, 6000, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801355E8[3] = {
    { 1500, 3000, 70 },
    { 1500, 6000, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_801355FC[3] = {
    { 0, 3000, 70 },
    { 0, 6000, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint D_actor_107600_80135610[3] = {
    { -1500, 3000, 70 },
    { -1500, 6000, 70 },
    { ACTOR_107600_PATH_END, 0, 0 },
};

_Actor107600Waypoint* D_actor_107600_80135624[62] = {
    D_actor_107600_80135078,
    D_actor_107600_80135084,
    D_actor_107600_80135098,
    D_actor_107600_801350AC,
    D_actor_107600_801350C0,
    D_actor_107600_801350D4,
    D_actor_107600_801350E8,
    D_actor_107600_801350F4,
    D_actor_107600_80135100,
    D_actor_107600_8013510C,
    D_actor_107600_80135120,
    D_actor_107600_80135134,
    D_actor_107600_80135148,
    D_actor_107600_8013515C,
    D_actor_107600_80135170,
    D_actor_107600_8013517C,
    D_actor_107600_80135188,
    D_actor_107600_80135194,
    D_actor_107600_801351A8,
    D_actor_107600_801351BC,
    D_actor_107600_801351D0,
    D_actor_107600_801351E4,
    D_actor_107600_801351F8,
    D_actor_107600_8013520C,
    D_actor_107600_80135218,
    D_actor_107600_80135224,
    D_actor_107600_80135230,
    D_actor_107600_80135244,
    D_actor_107600_80135258,
    D_actor_107600_8013526C,
    D_actor_107600_80135280,
    D_actor_107600_80135294,
    D_actor_107600_801352A8,
    D_actor_107600_801352B4,
    D_actor_107600_801352C0,
    D_actor_107600_801352CC,
    D_actor_107600_801352EC,
    D_actor_107600_8013530C,
    D_actor_107600_8013533C,
    D_actor_107600_8013536C,
    D_actor_107600_801353A8,
    D_actor_107600_801353D4,
    D_actor_107600_80135400,
    D_actor_107600_80135424,
    D_actor_107600_80135444,
    D_actor_107600_80135464,
    D_actor_107600_801354A8,
    D_actor_107600_801354D4,
    D_actor_107600_801354F4,
    D_actor_107600_80135514,
    D_actor_107600_80135534,
    D_actor_107600_80135548,
    D_actor_107600_8013555C,
    D_actor_107600_80135570,
    D_actor_107600_80135584,
    D_actor_107600_80135598,
    D_actor_107600_801355AC,
    D_actor_107600_801355C0,
    D_actor_107600_801355D4,
    D_actor_107600_801355E8,
    D_actor_107600_801355FC,
    D_actor_107600_80135610,
};

DamageAttack D_actor_107600_8013571C[1] = { 0 };

EnemyParams D_actor_107600_80135720 = { D_actor_107600_8013571C, 50, 0, 0, 0, 255, 0, 0, 0 };

DVECTOR D_actor_107600_80135730[8] = {
    { 0, -0x120 },
    { 0x60, -0xA0 },
    { -0x70, -0x60 },
    { 0xC0, -0x140 },
    { -0x20, -0x1C0 },
    { -0xA0, -0x160 },
    { 0xE0, -0x30 },
    { 0, 0 },
};

u16 D_actor_107600_80135750[13] = { 32, 24, 16, 12, 60, 36, 28, 20, 12, 1, 12, 20, 28 };

static void _actor107600RemapTargetColor(Enemy* unusedEnemy, MATRIX* colorMatrix, s32 colorMode);

/// Initializes a gallery mount and spawns the target it carries.
///
/// The kind nibble selects target descriptor kind + 1 (0..15); behaviour must
/// be standing, hanging or fixed (0..2), and the path byte must be in 0..61.
/// Bits 24..27 hold a stationary path in seconds; bit 28 requests rotation,
/// bit 29 lets the child attack, and bit 30 bypasses the 1024-unit XZ exclusion.
/// A low byte of 0xFF or failed work allocation also rejects the spawn.
/// Counted mounts require a live controller parent; rejection returns its count.
/// The task owns its zeroed work and child, and nonfixed mounts own a battle
/// reference until exit. Requires initialized scratch storage for one VECTOR.
static void _actor107600InitMount(Task* task)
{
    enum {
        ACTOR_107600_MOUNT_SPAWN_BEHAVIOUR_SHIFT = 12,
        ACTOR_107600_MOUNT_SPAWN_BEHAVIOUR_MASK  = 0xF000,
        ACTOR_107600_MOUNT_SPAWN_PATH_SHIFT      = 16,
        ACTOR_107600_MOUNT_SPAWN_ATTACKING       = 0x20000000,
        ACTOR_107600_MOUNT_SPAWN_FORCE           = 0x40000000,
        ACTOR_107600_MOUNT_PLAYER_EXCLUSION      = 0x400,
    };

    TmdObject*             model;
    Enemy*                 enemy;
    GfxCoord*              rootCoord;
    GfxCoord*              playerCoord;
    void**                 scratchSlot;
    VECTOR*                savedCursor;
    VECTOR*                playerOffset;
    _Actor107600MountWork* work;

    model                              = task->extra.tmd;
    enemy                              = task->spawnArg2.pointer;
    rootCoord                          = model->coords;
    playerCoord                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    scratchSlot                        = SCRATCH_HEAD_ADDR;
    savedCursor                        = SCRATCH_HEAD_AT(scratchSlot, VECTOR);
    playerOffset                       = savedCursor - 1;
    playerOffset->vx                   = playerCoord->coord.t[0] - rootCoord->coord.t[0];
    SCRATCH_HEAD_AT(scratchSlot, void) = playerOffset;
    playerOffset->vz                   = playerCoord->coord.t[2] - rootCoord->coord.t[2];
    // Reject before acquiring work, child ownership or a battle reference.
    if ((!(task->spawnArg1.value & ACTOR_107600_MOUNT_SPAWN_FORCE) &&
         playerActorPlanarLength(playerOffset->vx, playerOffset->vz) < ACTOR_107600_MOUNT_PLAYER_EXCLUSION + 1) ||
        (u8)task->spawnArg1.value == ACTOR_107600_TARGET_SPAWN_DEAD) {
    fail:
        if ((task->spawnArg1.value & ACTOR_107600_MOUNT_SPAWN_BEHAVIOUR_MASK) !=
            ACTOR_107600_MOUNT_FIXED << ACTOR_107600_MOUNT_SPAWN_BEHAVIOUR_SHIFT) {
            MistShootingGalleryWork* gallery = task->parent->work;

            gallery->liveTargets--;
        }
        SCRATCH_STACK_RELEASE_BYTES(sizeof(*playerOffset));
        enemyDestroy(enemy, task);
        return;
    }
    work       = memCalloc(sizeof(_Actor107600MountWork), false);
    task->work = work;
    if (work == NULL) {
        goto fail;
    }
    task->exitCallback = _actor107600DestroyMount;
    work->path         = (u8)((u32)task->spawnArg1.value >> ACTOR_107600_MOUNT_SPAWN_PATH_SHIFT);
    work->behaviour    = (s32)(task->spawnArg1.value & ACTOR_107600_MOUNT_SPAWN_BEHAVIOUR_MASK) >> ACTOR_107600_MOUNT_SPAWN_BEHAVIOUR_SHIFT;
    model->lightMtx    = &work->lightMatrix;
    model->colorMtx    = &work->colorMatrix;
    enemy->param       = &D_actor_107600_80134F84;
    rootCoord->parent  = &gGfxViewCoord;
    enemy->field_4     = &task->extra.tmd->coords->coord;
    enemy->field_48    = 0;
    if (work->behaviour != ACTOR_107600_MOUNT_FIXED) {
        sceneAcquireBattleRef(0);
    }
    work->spawnX = rootCoord->coord.t[0];
    work->spawnY = rootCoord->coord.t[1];
    work->spawnZ = rootCoord->coord.t[2];
    work->yaw    = -ACTOR_107600_ANGLE_QUARTER_TURN;
    if (work->behaviour == ACTOR_107600_MOUNT_HANGING) {
        work->roll += ACTOR_107600_ANGLE_TURN / 2;
    }
    task->state++;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    // The child's behaviour moves from bits 12..15 to 16..19; attack stays at 29.
    _actor107600SpawnTarget(enemy, task->spawnArg1.value & ACTOR_107600_TARGET_SPAWN_KIND_MASK,
                            work->behaviour | (((u32)task->spawnArg1.value >> ACTOR_107600_TARGET_SPAWN_BEHAVIOUR_SHIFT) &
                                               (ACTOR_107600_MOUNT_SPAWN_ATTACKING >> ACTOR_107600_TARGET_SPAWN_BEHAVIOUR_SHIFT)));
    SCRATCH_POP_BYTES_AT(scratchSlot, sizeof(*playerOffset));
}

/// Stops a mount path and asks its live child target to leave.
///
/// Enemy must belong to the work-owning mount and keep its first child alive.
/// Moves the mount to its leave step and clears rotation. The stop signal does
/// not finish the child; the mount waits for its separate FINISHED signal.
static inline void _actor107600StopMountPath(_Actor107600MountWork* work, Enemy* enemy)
{
    work->step                                = ACTOR_107600_MOUNT_STEP_LEAVE;
    work->rotating                            = 0;
    enemy->task->firstChild->spawnArg1.value |= ACTOR_107600_TARGET_SIGNAL_STOP;
}

/// Advances a path-following mount at the supplied bob and resting Y heights.
///
/// Heights and path coordinates use the root's parent frame. The task owns
/// mount work and requires a live enemy, model and first-child target. Its path
/// index must select one of 62 sentinel-terminated paths. Axis differences
/// narrow to s16; either axis arrival advances the waypoint, so diagonal legs
/// repeat their destination. A zero-speed first waypoint holds for the spawn
/// nibble in seconds at 30 ticks per second; zero waits until the target ends.
/// The target finishes before the mount shrinks and advances its task state.
static inline void _actor107600UpdateMountPath(Task* task, s32 bobY, s32 restingY)
{
    enum {
        ACTOR_107600_MOUNT_FULL_HEIGHT_PERCENT = 100,
        ACTOR_107600_MOUNT_HEIGHT_STEP_PERCENT = 8,
        ACTOR_107600_MOUNT_SETTLE_FRAMES       = 4,
    };

    _Actor107600MountWork* work      = task->work;
    Enemy*                 enemy     = task->spawnArg2.pointer;
    GfxCoord*              rootCoord = task->extra.tmd->coords;
    _Actor107600Waypoint*  waypoint;
    s32                    axisDelta;

    switch (work->step) {
        case ACTOR_107600_MOUNT_STEP_RISE:
            if (work->heightPercent < ACTOR_107600_MOUNT_FULL_HEIGHT_PERCENT) {
                work->heightPercent += ACTOR_107600_MOUNT_HEIGHT_STEP_PERCENT;
                return;
            }
            work->heightPercent = ACTOR_107600_MOUNT_FULL_HEIGHT_PERCENT;
            work->timer         = 0;
            work->step++;
        case ACTOR_107600_MOUNT_STEP_SETTLE:
            if (++work->timer & 1) {
                rootCoord->coord.t[1] = bobY;
                return;
            }
            rootCoord->coord.t[1] = restingY;
            if (work->timer >= ACTOR_107600_MOUNT_SETTLE_FRAMES) {
                work->step++;
                enemy->task->firstChild->spawnArg1.value |= ACTOR_107600_TARGET_SIGNAL_MOUNT_READY;
            }
            return;
        case ACTOR_107600_MOUNT_STEP_WAIT_TARGET:
            if (!(enemy->task->firstChild->spawnArg1.value & ACTOR_107600_TARGET_SIGNAL_STANDING)) {
                return;
            }
            waypoint  = D_actor_107600_80135624[work->path];
            waypoint += work->waypoint;
            if (task->spawnArg1.value & ACTOR_107600_MOUNT_SPAWN_ROTATING) {
                work->rotating = 1;
            }
            if (waypoint->speed == 0) {
                work->step  = ACTOR_107600_MOUNT_STEP_HOLD;
                work->timer = (((u32)task->spawnArg1.value >> ACTOR_107600_MOUNT_SPAWN_HOLD_SHIFT) & ACTOR_107600_MOUNT_SPAWN_HOLD_MASK) * ACTOR_107600_FRAMES_PER_SECOND;
                return;
            }
            work->step++;
        case ACTOR_107600_MOUNT_STEP_TRAVEL:
            waypoint  = D_actor_107600_80135624[work->path];
            waypoint += work->waypoint;
            if (waypoint->x == ACTOR_107600_PATH_END) {
            stop:
                _actor107600StopMountPath(work, enemy);
                return;
            }
            // Each axis arrival advances the waypoint; diagonal legs repeat their stop.
            axisDelta = (s16)(rootCoord->coord.t[0] - waypoint->x);
            if (axisDelta != 0) {
                if (waypoint->speed >= abs(axisDelta)) {
                    if (enemy->task->firstChild->spawnArg1.value & ACTOR_107600_TARGET_SIGNAL_STOP) {
                        _actor107600StopMountPath(work, enemy);
                        return;
                    }
                    rootCoord->coord.t[0] = waypoint->x;
                    work->waypoint++;
                } else if (axisDelta < 0) {
                    rootCoord->coord.t[0] += waypoint->speed;
                } else {
                    rootCoord->coord.t[0] -= waypoint->speed;
                }
            }
            axisDelta = (s16)(rootCoord->coord.t[2] - waypoint->z);
            if (axisDelta != 0) {
                if (waypoint->speed >= abs(axisDelta)) {
                    if (enemy->task->firstChild->spawnArg1.value & ACTOR_107600_TARGET_SIGNAL_STOP) {
                        _actor107600StopMountPath(work, enemy);
                        return;
                    }
                    rootCoord->coord.t[2] = waypoint->z;
                    work->waypoint++;
                } else if (axisDelta < 0) {
                    rootCoord->coord.t[2] += waypoint->speed;
                } else {
                    rootCoord->coord.t[2] -= waypoint->speed;
                }
            }
            return;
        case ACTOR_107600_MOUNT_STEP_HOLD:
            if (work->timer != 0 && --work->timer <= 0) {
                goto stop;
            }
        case ACTOR_107600_MOUNT_STEP_LEAVE:
            if (enemy->task->firstChild->spawnArg1.value & ACTOR_107600_TARGET_SIGNAL_FINISHED) {
                if (work->heightPercent > 0) {
                    work->heightPercent -= ACTOR_107600_MOUNT_HEIGHT_STEP_PERCENT;
                    return;
                }
                work->heightPercent = 0;
                task->state++;
            }
            break;
    }
}

/// Runs a standing gallery mount along its selected path, then shrinks it away.
///
/// The mount settles at floor height before its child target unfolds.
/// Requires a live first-child target and a spawn-selected path in 0..61.
/// A zero-speed first stop holds for the spawn nibble in seconds (30 ticks
/// per second); zero seconds holds until the target finishes. Coordinates
/// and speeds use the mount root's parent frame; axis deltas narrow to s16.
static void _actor107600UpdateStandingMountPath(Task* task)
{
    _actor107600UpdateMountPath(task, -0x10, 0);
}

/// Runs a hanging gallery mount along its selected path, then shrinks it away.
///
/// The mount settles at ceiling height before its child target unfolds.
/// Requires a live first-child target and a spawn-selected path in 0..61.
/// A zero-speed first stop holds for the spawn nibble in seconds (30 ticks
/// per second); zero seconds holds until the target finishes. Coordinates
/// and speeds use the mount root's parent frame; axis deltas narrow to s16.
static void _actor107600UpdateHangingMountPath(Task* task)
{
    _actor107600UpdateMountPath(task, -0xF4C, -0xF3C);
}

/// Task states run by `_actor107600TargetTask`, indexed by its
/// `Task::state`.
static const TaskFuncTable4 D_actor_107600_80131E74 = { {
    _actor107600InitTarget,
    _actor107600UpdateTarget,
    _actor107600HideTarget,
    _actor107600DestroyTarget,
} };

/// Behaviour states indexed by `_Actor107600TargetWork::state`, run by
/// `_actor107600UpdateTarget`.
static const TaskFuncTable10 D_actor_107600_80131E84 = { {
    _actor107600BeginTargetBehaviour,
    _actor107600UpdateActiveTarget,
    _actor107600UpdateTargetFlinch,
    _actor107600UpdateTargetFlinch,
    _actor107600ResumeTargetState4,
    _actor107600ResumeTargetState5,
    _actor107600ResumeTargetState6,
    _actor107600FoldTarget,
    _actor107600BeginFixedTargetExit,
    _actor107600TumbleDestroyedTarget,
} };

/// Dispatches one gallery mount tick through its four task lifecycle states.
///
/// Task state must be in 0..3: initialize, update, retire, destroy. The exported
/// actor descriptor selects this private callback; initialization owns work
/// allocation and installs teardown only after that allocation succeeds.
static void _actor107600MountTask(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_actor_107600_80131E24;
    handlers.funcs[task->state](task);
}

/// Updates a gallery mount's behaviour, transform, lighting and draw visibility.
///
/// Requires live mount work, enemy and model. Running ticks dispatch work
/// state 0..1, rebuild the rotation and scale its vertical coefficient by
/// heightPercent. Running and paused ticks refresh lighting and enable drawing;
/// hidden ticks disable drawing. Angles use 4096 units per turn. Scaling divides
/// the coefficient by 100 before multiplication, retaining integer truncation.
static void _actor107600UpdateMount(Task* task)
{
    enum { ACTOR_107600_MOUNT_FULL_HEIGHT_PERCENT = 100 };

    TmdObject*             model       = task->extra.tmd;
    GfxCoord*              rootCoord   = model->coords;
    _Actor107600MountWork* work        = task->work;
    TaskFunc               handlers[2] = { _actor107600BeginMountBehaviour, _actor107600UpdateMountBehaviour };
    TmdObject*             visibleModel;

    visibleModel = model;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            handlers[work->state](task);
            rootCoord->param.rot.vy = work->yaw;
            rootCoord->param.rot.vz = work->roll;
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            _actor107600UpdateMountRotation(task);
            rootCoord->coord.m[1][1] = work->heightPercent * (rootCoord->coord.m[1][1] / ACTOR_107600_MOUNT_FULL_HEIGHT_PERCENT);
            // Paused mounts also refresh their lighting and remain drawable.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor107600UpdateMountColor(task);
            visibleModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
}

/// Removes a finished gallery mount from its controller's live-target count.
///
/// Fixed demonstration mounts were never counted and need no controller.
/// Other mounts require the spawning controller to remain alive; the task
/// advances to its destruction state after releasing the count.
static void _actor107600RetireMount(Task* task)
{
    Task*                  controllerTask;
    _Actor107600MountWork* work;

    controllerTask = task->parent;
    work           = task->work;
    if (work->behaviour != ACTOR_107600_MOUNT_FIXED) {
        MistShootingGalleryWork* gallery = controllerTask->work;

        gallery->liveTargets--;
    }
    task->state++;
}

/// Releases a gallery mount's battle reference and destroys its enemy and task.
///
/// Installed only after mount work allocation succeeds. Fixed mounts do not
/// own a battle reference. Task teardown releases the work and child target.
static void _actor107600DestroyMount(Task* task)
{
    _Actor107600MountWork* work = task->work;

    if (work->behaviour != ACTOR_107600_MOUNT_FIXED) {
        sceneReleaseBattleRefWithRewards(task, 0);
    }
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Refreshes the mount's lighting and colour from its model root position.
///
/// Requires a live enemy/model and a current composed root matrix. Its cached
/// translation follows the resident lighting query's coordinate-space contract.
/// One scratch VECTOR survives the nested query and is released before return;
/// only XYZ are initialized or read, and no pointer to it is retained.
static void _actor107600UpdateMountColor(Task* task)
{
    GfxCoord* rootCoord;
    void**    scratchSlot;
    VECTOR*   savedCursor;
    VECTOR*   worldPosition;
    Enemy*    enemy;

    enemy                              = task->spawnArg2.pointer;
    rootCoord                          = task->extra.tmd->coords;
    scratchSlot                        = SCRATCH_HEAD_ADDR;
    savedCursor                        = SCRATCH_HEAD_AT(scratchSlot, VECTOR);
    worldPosition                      = savedCursor - 1;
    worldPosition->vx                  = rootCoord->workm.t[0];
    worldPosition->vy                  = rootCoord->workm.t[1];
    worldPosition->vz                  = rootCoord->workm.t[2];
    SCRATCH_HEAD_AT(scratchSlot, void) = worldPosition;
    worldCoordUpdateActorColor(enemy, worldPosition, 0, 0);
    SCRATCH_POP_BYTES_AT(scratchSlot, sizeof(*worldPosition));
}

/// Rebuilds a gallery mount's local rotation from its wrapped Euler angles.
///
/// Angles use 4096 units per turn. Composes Z, X, then Y on the fixed-point
/// identity and copies only the nine rotation elements, preserving translation.
/// Requires initialized scratch storage for one MATRIX and nested axis-helper
/// reservations. The caller dirties coordinate composition.
static void _actor107600UpdateMountRotation(Task* task)
{
    _Actor107600MountWork* work      = task->work;
    GfxCoord*              rootCoord = task->extra.tmd->coords;
    MATRIX*                rotation;

    work->pitch &= ACTOR_107600_ANGLE_MASK;
    work->yaw   &= ACTOR_107600_ANGLE_MASK;
    work->roll  &= ACTOR_107600_ANGLE_MASK;
    rotation     = SCRATCH_STACK_CURSOR(MATRIX) - 1;
    // Initialize before publishing the reservation for the axis helpers.
    gfxSetRotIdentity(rotation);
    SCRATCH_STACK_CURSOR(MATRIX) = rotation;
    RotMatrixZ(work->roll, rotation);
    RotMatrixX(work->pitch, rotation);
    RotMatrixY(work->yaw, rotation);
    _actor107600CopyMountRotation(rotation, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Copies the nine rotation elements used by the gallery mount model.
///
/// Source and destination must provide live `MATRIX` objects. Translation
/// and alignment bytes are preserved; copying a matrix onto itself is valid.
static void _actor107600CopyMountRotation(const MATRIX* source, MATRIX* destination)
{
    destination->m[0][0] = source->m[0][0];
    destination->m[0][1] = source->m[0][1];
    destination->m[0][2] = source->m[0][2];
    destination->m[1][0] = source->m[1][0];
    destination->m[1][1] = source->m[1][1];
    destination->m[1][2] = source->m[1][2];
    destination->m[2][0] = source->m[2][0];
    destination->m[2][1] = source->m[2][1];
    destination->m[2][2] = source->m[2][2];
}

/// Advances a newly spawned mount to its behaviour dispatcher after one tick.
///
/// Requires live mount work in state 0; movement begins on the following tick.
static void _actor107600BeginMountBehaviour(Task* task)
{
    _Actor107600MountWork* work = task->work;

    work->state++;
}

/// Updates the gallery mount's selected behaviour and optional yaw spin.
///
/// The live work selects standing, hanging or fixed behaviour (0..2). Path
/// behaviours require a live first-child target. While rotating, yaw advances
/// 32 angle units per tick, with 4096 units per turn; the rotation builder wraps it.
static void _actor107600UpdateMountBehaviour(Task* task)
{
    TaskFuncTable3         handlers;
    _Actor107600MountWork* work = task->work;

    handlers = D_actor_107600_80131E34;
    handlers.funcs[work->behaviour](task);
    if (work->rotating != 0) {
        work->yaw += 0x20;
    }
}

/// Places a fixed demonstration mount and shrinks it after its target finishes.
///
/// Requires its live first-child target. The first tick places the root at
/// Y = 0 and full height; completion advances the mount's task state.
static void _actor107600UpdateFixedMount(Task* task)
{
    _Actor107600MountWork* work      = task->work;
    GfxCoord*              rootCoord = task->extra.tmd->coords;
    Enemy*                 enemy     = task->spawnArg2.pointer;

    switch (work->step) {
        case ACTOR_107600_MOUNT_FIXED_STEP_PLACE:
            work->step++;
            work->heightPercent   = 100;
            rootCoord->coord.t[1] = 0;
        case ACTOR_107600_MOUNT_FIXED_STEP_LEAVE:
            if (enemy->task->firstChild->spawnArg1.value & ACTOR_107600_TARGET_SIGNAL_FINISHED) {
                if (work->heightPercent > 0) {
                    work->heightPercent -= 8;
                    return;
                }
                work->heightPercent = 0;
                task->state++;
            }
            break;
    }
}

/// Spawns a target as both a task child and a coordinate child of its mount.
///
/// `mountEnemy` must own a live task/model and outlive the child. `targetKind`
/// selects descriptor kind + 1; the sole caller supplies a nibble (0..15).
/// `targetFlags` carries mount behaviour in bits 0..3 and attack enable in bit 13;
/// shifting it into the high half leaves the child's low byte for kind/signals.
/// Kinds 0..9 use CLUT-row offset 2, later kinds use 0. Both primitive-buffer
/// halves are rebuilt. Spawn failure leaves the mount unchanged, without retry.
static void _actor107600SpawnTarget(Enemy* mountEnemy, s32 targetKind, s32 targetFlags)
{
    enum {
        ACTOR_107600_TARGET_FIRST_ZERO_CLUT_KIND = 10,
        ACTOR_107600_TARGET_EARLY_CLUT_ROW       = 2,
    };

    Enemy*     targetEnemy;
    GfxCoord*  targetCoord;
    TmdObject* targetModel;

    targetEnemy = enemySpawnFromTable(D_actor_107600_80134F94, targetKind + 1, targetFlags, mountEnemy);
    if (targetEnemy != NULL) {
        taskReparent(mountEnemy->task, targetEnemy->task);
        targetCoord                        = targetEnemy->task->extra.tmd->coords;
        targetCoord->parent                = mountEnemy->task->extra.tmd->coords;
        targetEnemy->task->spawnArg1.value = targetKind | (targetFlags << ACTOR_107600_TARGET_SPAWN_BEHAVIOUR_SHIFT);
        targetModel                        = targetEnemy->task->extra.tmd;
        targetModel->texturePageOffset     = 0;
        if (targetKind < ACTOR_107600_TARGET_FIRST_ZERO_CLUT_KIND) {
            targetModel->clutRowOffset = ACTOR_107600_TARGET_EARLY_CLUT_ROW;
        } else {
            targetModel->clutRowOffset = 0;
        }
        tmdBuildBufferHalf(targetModel);
        tmdBuildBufferHalf(targetModel);
        targetEnemy->workType = ENEMY_WORK_PLAIN;
    }
}

/// Allocates and registers the gallery target carried by a mount.
///
/// The task owns the zeroed work; the enemy and collision body borrow its
/// matrices and contacts until teardown. A low spawn byte of 0xFF or failed
/// allocation destroys the enemy before installing the exit callback. The
/// kind nibble indexes 13 HP entries; the demonstration kind 13 also reads
/// past that serialized table, whose runtime backing is unproven.
static void _actor107600InitTarget(Task* task)
{
    _Actor107600TargetWork* work;
    Enemy*                  enemy;
    TmdObject*              model;
    GfxCoord*               rootCoord;
    u16                     hitPoints;
    u32                     spawnByte;

    model     = task->extra.tmd;
    spawnByte = (u8)task->spawnArg1.value;
    enemy     = task->spawnArg2.pointer;
    rootCoord = model->coords;
    if (spawnByte == ACTOR_107600_TARGET_SPAWN_DEAD) {
        enemyDestroy(enemy, task);
        return;
    }
    work       = memCalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // Install cleanup before linking collision and target tracking.
    task->exitCallback   = _actor107600DestroyTarget;
    work->mountBehaviour = ((u32)task->spawnArg1.value >> ACTOR_107600_TARGET_SPAWN_BEHAVIOUR_SHIFT) & ACTOR_107600_TARGET_SPAWN_BEHAVIOUR_MASK;
    model->lightMtx      = &work->lightMatrix;
    model->colorMtx      = &work->colorMatrix;
    enemy->param         = &D_actor_107600_80135720;
    enemy->recs          = work->contacts;
    work->field_140      = task->extra.tmd->coords;
    work->field_144      = 0x140;
    work->field_146      = 2;
    hitPoints            = D_actor_107600_80135750[task->spawnArg1.value & ACTOR_107600_TARGET_SPAWN_KIND_MASK];
    enemy->hpMax         = hitPoints;
    enemy->hp            = hitPoints;
    _actor107600InitTargetCollision(task);
    worldTargetLinkNode(&enemy->node);
    enemy->field_4                = &rootCoord->workm;
    enemy->bodyPos.vy             = -0x244;
    enemy->field_48               = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->coord                  = rootCoord;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    _actor107600PlaceTargetOffset(rootCoord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    task->state += 1;
}

/// Advances a gallery target's behaviour, hit handling and face effects.
///
/// Requires live target work/Enemy/model and a state-table index in 0..9.
/// Running combat dispatches behaviour and accepts hits before the leave state;
/// paused combat still refreshes colour, while hidden combat suppresses drawing.
/// Every mode updates rotation/scale and face effects. Non-fixed targets publish
/// their lock mask to the live gallery controller. Hit offsets cycle through
/// eight entries; a dead target replaces its last mark with a death burst.
/// Borrows one scratch SVECTOR across state dispatch and releases it on return.
static void _actor107600UpdateTarget(Task* task)
{
    TaskFuncTable10 states;
    Enemy*          enemy;
    TmdObject*      model;
    TmdObject*      drawModel;

    _Actor107600TargetWork* work;
    GfxCoord*               rootCoord;
    SVECTOR*                hitMarkOffset;
    s32                     hitMarkIndex;

    enemy     = task->spawnArg2.pointer;
    model     = task->extra.tmd;
    work      = task->work;
    rootCoord = model->coords;
    drawModel = model;
    states    = D_actor_107600_80131E84;
    SCRATCH_STACK_RESERVE_BYTES(sizeof(*hitMarkOffset));
    hitMarkOffset = SCRATCH_STACK_CURSOR(SVECTOR);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            states.funcs[work->state](task);
            if (work->state < ACTOR_107600_TARGET_STATE_LEAVE) {
                if (work->hitCooldown == 0) {
                    _actor107600ApplyTargetHits(task);
                } else {
                    work->hitCooldown--;
                }
                worldCollisionClearContacts(work->contacts);
                if (enemy->hp <= 0) {
                    _actor107600SetTargetState(task, ACTOR_107600_TARGET_STATE_DESTROYED);
                }
            }
            // Running targets also use the paused mode's colour/draw refresh.
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor107600UpdateTargetRootColor(task);
            drawModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            drawModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    if (work->mountBehaviour != ACTOR_107600_MOUNT_FIXED) {
        ((MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->work)->targetLockMask = worldTargetGetActorLockMask(&enemy->node);
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor107600UpdateTargetRotation(task);
    _actor107600ApplyTargetScale(task);
    for (hitMarkIndex = 0; hitMarkIndex < work->hitMarkCount; hitMarkIndex++) {
        if (hitMarkIndex == work->hitMarkCount - 1 && enemy->hp <= 0) {
            hitMarkOffset->vx = 0;
            hitMarkOffset->vy = -0xE0;
            hitMarkOffset->vz = 0;
            _actor107600DrawTargetDeathBurst(rootCoord, hitMarkOffset);
        } else {
            hitMarkOffset->vx = D_actor_107600_80135730[(work->hitMarkFirst + hitMarkIndex) & (ACTOR_107600_TARGET_HIT_MARK_COUNT - 1)].vx;
            hitMarkOffset->vy = D_actor_107600_80135730[(work->hitMarkFirst + hitMarkIndex) & (ACTOR_107600_TARGET_HIT_MARK_COUNT - 1)].vy;
            hitMarkOffset->vz = 0;
            _actor107600DrawTargetHitMark(rootCoord, hitMarkOffset);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(*hitMarkOffset));
}

/// Unfolds a gallery target and runs its optional repeating player attack.
///
/// Requires initialized active-target work and the mount/target handshake in
/// spawnArg1. Width grows before height by 32 percentage points while below 100,
/// so the final step can overshoot; pitch uses 4096 units per turn. Four bob ticks
/// enable locking/pair collision and signal STANDING. STOP requests folding.
/// Attack-enabled ticks charge at 120 and fire at 210, then restart the timer.
/// Requires a live player model through part 4. A hit outside damage mode requests
/// ten HP and descending-part hit flashes; at ten HP or less the gallery instead
/// receives a lethal-hit latch and pending damage is zero. Hit reactions preempt
/// these phases, and disabled attacks retain their elapsed timer.
static void _actor107600UpdateActiveTarget(Task* task)
{
    enum {
        ACTOR_107600_TARGET_ACTIVE_STEP_WAIT_MOUNT        = 0,
        ACTOR_107600_TARGET_ACTIVE_STEP_GROW_WIDTH        = 1,
        ACTOR_107600_TARGET_ACTIVE_STEP_GROW_HEIGHT       = 2,
        ACTOR_107600_TARGET_ACTIVE_STEP_BOB               = 4,
        ACTOR_107600_TARGET_ACTIVE_STEP_ATTACK            = 5,
        ACTOR_107600_TARGET_ATTACK_ENABLED                = 0x20000000,
        ACTOR_107600_TARGET_FULL_PERCENT                  = 100,
        ACTOR_107600_TARGET_GROWTH_STEP                   = 32,
        ACTOR_107600_TARGET_STAND_EASE_BIAS               = 32,
        ACTOR_107600_TARGET_CHARGE_TICK                   = 120,
        ACTOR_107600_TARGET_FIRE_TICK                     = 210,
        ACTOR_107600_TARGET_PLAYER_DAMAGE                 = 10,
        ACTOR_107600_TARGET_PLAYER_HIT_PART               = 4,
        ACTOR_107600_TARGET_PLAYER_BODY_HIT               = 1,
        ACTOR_107600_TARGET_PLAYER_DESCENDING_HIT_FLASHES = 5
    };

    _Actor107600TargetWork* work  = task->work;
    Enemy*                  enemy = task->spawnArg2.pointer;
    Task*                   player;
    GameActor*              playerActor;
    s32                     hitPan;
    s32                     spawnFlags;
    s16                     bobTick;

    if (_actor107600ConsumeTargetHitReaction(task) != 0) {
        return;
    }
    // Entry phases fall through until a growth or settling tick must wait.
    switch (work->step) {
        case ACTOR_107600_TARGET_ACTIVE_STEP_WAIT_MOUNT:
            if (!(task->spawnArg1.value & ACTOR_107600_TARGET_SIGNAL_MOUNT_READY)) {
                return;
            }
            work->step++;
        case ACTOR_107600_TARGET_ACTIVE_STEP_GROW_WIDTH:
            if (work->widthPercent < ACTOR_107600_TARGET_FULL_PERCENT) {
                work->widthPercent += ACTOR_107600_TARGET_GROWTH_STEP;
                return;
            }
            work->step++;
        case ACTOR_107600_TARGET_ACTIVE_STEP_GROW_HEIGHT:
            if (work->heightPercent < ACTOR_107600_TARGET_FULL_PERCENT) {
                work->heightPercent += ACTOR_107600_TARGET_GROWTH_STEP;
                return;
            }
            {
                GfxCoord* soundCoord = task->extra.tmd->coords;
                s32       soundPan;
                work->step++;
                soundPan = (s8)worldCoordGetOriginAudioPan(soundCoord);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_TARGET_APPEAR, soundPan, (s8)worldCoordGetOriginAudioDepth(soundCoord));
            }
        case ACTOR_107600_TARGET_ACTIVE_STEP_STAND: {
            s16 pitch = work->pitch;
            if ((u16)(pitch - 1) < ACTOR_107600_ANGLE_QUARTER_TURN) {
                work->pitch = pitch - ((ACTOR_107600_ANGLE_QUARTER_TURN + ACTOR_107600_TARGET_STAND_EASE_BIAS - pitch) >> 2);
                return;
            }
        }
            work->timer = 0;
            work->step++;
            return;
        case ACTOR_107600_TARGET_ACTIVE_STEP_BOB:
            bobTick     = work->timer + 1;
            work->timer = bobTick;
            if (bobTick & 1) {
                work->pitch = ((bobTick << 16) >> 13) - 0x38;
            } else {
                work->pitch = 0;
                if (work->timer >= 4) {
                    work->step++;
                    task->spawnArg1.value |= ACTOR_107600_TARGET_SIGNAL_STANDING;
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                    enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
                    work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
            }
        case ACTOR_107600_TARGET_ACTIVE_STEP_ATTACK:
            spawnFlags = task->spawnArg1.value;
            if (spawnFlags & ACTOR_107600_TARGET_SIGNAL_STOP) {
                _actor107600SetTargetState(task, ACTOR_107600_TARGET_STATE_LEAVE);
                return;
            }
            if (!(spawnFlags & ACTOR_107600_TARGET_ATTACK_ENABLED)) {
                return;
            }
            work->attackTimer++;
            if (work->attackTimer == ACTOR_107600_TARGET_CHARGE_TICK) {
                GfxCoord* soundCoord = task->extra.tmd->coords;
                s32       soundPan;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                soundPan = (s8)worldCoordGetOriginAudioPan(soundCoord);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_TARGET_CHARGE, soundPan, (s8)worldCoordGetOriginAudioDepth(soundCoord));
            } else if (work->attackTimer == ACTOR_107600_TARGET_FIRE_TICK) {
                GfxCoord* playerHitCoord;
                s32       soundPan;
                player            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                playerHitCoord    = &player->extra.tmd->coords[ACTOR_107600_TARGET_PLAYER_HIT_PART];
                playerActor       = player->work;
                work->attackTimer = 0;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                effectSpawn(EFFECT_MIST_GALLERY_TRACER, playerHitCoord, 0, NULL);
                soundPan = (s8)worldCoordGetOriginAudioPan(playerHitCoord);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_TARGET_ATTACK, soundPan, (s8)worldCoordGetOriginAudioDepth(playerHitCoord));
                if (playerActor->mode != GAME_ACTOR_MODE_DAMAGE) {
                    if (gPlayerStatus.hp <= ACTOR_107600_TARGET_PLAYER_DAMAGE) {
                        ((MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->work)->lethalHit = 1;
                        playerActor->pendingDamage                                                    = 0;
                    } else {
                        playerActor->pendingDamage = ACTOR_107600_TARGET_PLAYER_DAMAGE;
                    }
                    playerActor->hitRegion      = ACTOR_107600_TARGET_PLAYER_BODY_HIT;
                    playerActor->damageReaction = ACTOR_107600_TARGET_PLAYER_DESCENDING_HIT_FLASHES;
                    playerActorEnterPendingHit(player);
                    hitPan = (s8)worldCoordGetOriginAudioPan(playerHitCoord);
                    sndEvtRequestScriptStart(SOUND_PLAYER_STRUCK, hitPan, (s8)worldCoordGetOriginAudioDepth(playerHitCoord));
                }
            }
            break;
    }
}

/// Swings a hit gallery target back, settles it and resumes its standing motion.
///
/// Both light and heavy hit states use this body. Damage scales the six-tick
/// pitch swing in 4096 units per turn; the settling phase counts timer values
/// 0..4 before resuming the active stand-up step.
static void _actor107600UpdateTargetFlinch(Task* task)
{
    enum { ACTOR_107600_TARGET_FLINCH_ANGLE_LIMIT = ACTOR_107600_ANGLE_TURN / 8 };

    _Actor107600TargetWork* work = task->work;
    s32                     swingAngleStep;
    s16                     framesLeft;

    switch (work->step) {
        case ACTOR_107600_TARGET_FLINCH_STEP_INIT:
            work->step++;
            work->timer = 6;
        case ACTOR_107600_TARGET_FLINCH_STEP_SWING:
            swingAngleStep = work->hitDamage * 6;
            if (swingAngleStep > ACTOR_107600_TARGET_FLINCH_ANGLE_LIMIT) {
                swingAngleStep = ACTOR_107600_TARGET_FLINCH_ANGLE_LIMIT;
            }
            framesLeft      = work->timer;
            swingAngleStep /= 3;
            if (framesLeft >= 4) {
                if (work->pitch < ACTOR_107600_TARGET_FLINCH_ANGLE_LIMIT) {
                    work->pitch += swingAngleStep - swingAngleStep / 3 * (6 - framesLeft);
                }
            } else if (framesLeft <= 0) {
                // The shared decrement below starts settling at timer -1.
                work->pitch = 0;
                work->timer = 0;
                work->step++;
            } else if (work->pitch > 0) {
                work->pitch -= swingAngleStep + swingAngleStep / 3 * (3 - framesLeft);
            }
            work->timer--;
            break;
        case ACTOR_107600_TARGET_FLINCH_STEP_SETTLE:
            work->timer++;
            if (work->timer & 1) {
                work->pitch = (work->timer - 7) * 8;
                return;
            }
            work->pitch     = 0;
            work->hitDamage = 0;
            if (work->timer >= 4) {
                work->state = ACTOR_107600_TARGET_STATE_ACTIVE;
                work->step  = ACTOR_107600_TARGET_ACTIVE_STEP_STAND;
            }
            break;
    }
}

/// Folds an undestroyed gallery target away and releases its mount to shrink.
///
/// Disables target tracking and pair collision first, then waits seven ticks,
/// folds through a quarter turn and shrinks height before width. The collision
/// body stays linked until the task exits. The finished signal is raised only
/// after both scales reach at most 20 percent.
static void _actor107600FoldTarget(Task* task)
{
    _Actor107600TargetWork* work  = task->work;
    Enemy*                  enemy = task->spawnArg2.pointer;
    GfxCoord*               rootCoord;
    s32                     pan;

    switch (work->step) {
        case ACTOR_107600_TARGET_FOLD_STEP_BEGIN:
            work->step++;
            task->spawnArg1.value |= ACTOR_107600_TARGET_SIGNAL_STOP;
            work->timer            = 7;
            // Stop accepting shots before folding the target back onto its mount.
            worldTargetUnlinkNode(&enemy->node);
            enemy->recs       = NULL;
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        case ACTOR_107600_TARGET_FOLD_STEP_WAIT:
            work->timer--;
            if (work->timer <= 0) {
                rootCoord = task->extra.tmd->coords;
                work->step++;
                work->hitMarkCount = 0;
                work->field_15C    = 0;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                pan = (s8)worldCoordGetOriginAudioPan(rootCoord);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_TARGET_DEATH, pan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            }
            break;
        case ACTOR_107600_TARGET_FOLD_STEP_FOLD:
            if (work->pitch < ACTOR_107600_ANGLE_QUARTER_TURN) {
                work->pitch += 0x80;
                return;
            }
            work->pitch = ACTOR_107600_ANGLE_QUARTER_TURN;
            work->step++;
        case ACTOR_107600_TARGET_FOLD_STEP_SHRINK:
            if (work->heightPercent > ACTOR_107600_TARGET_SHRINK_STOP_PERCENT) {
                work->heightPercent -= ACTOR_107600_TARGET_SHRINK_STEP_PERCENT;
                return;
            }
            if (work->widthPercent > ACTOR_107600_TARGET_SHRINK_STOP_PERCENT) {
                work->widthPercent -= ACTOR_107600_TARGET_SHRINK_STEP_PERCENT;
                return;
            }
            work->step++;
            task->spawnArg1.value |= ACTOR_107600_TARGET_SIGNAL_FINISHED;
            break;
        case ACTOR_107600_TARGET_FOLD_STEP_DONE:
            break;
    }
}

/// Draws one target tumble spin in 4096-per-turn angle units per tick.
///
/// Advances the shared LCG once. Even magnitudes are negated and odd ones stay
/// positive, producing -126..127. The output is a live target work spin field,
/// separate from the shared generator state.
static inline void _actor107600RollTargetSpin(s16* spinAngle)
{
    enum { ACTOR_107600_TARGET_SPIN_MAGNITUDE_MASK = 0x7F };

    s16 spin;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    spin            = (gRandomLcgState >> 16) & ACTOR_107600_TARGET_SPIN_MAGNITUDE_MASK;
    // Publish the magnitude before choosing its sign.
    *spinAngle = spin;
    if (!((gRandomLcgState >> 16) & 1)) {
        spin = -spin;
    }
    *spinAngle = spin;
}

/// Scores a destroyed gallery target, detaches it and tumbles it out of play.
///
/// Requires the live gallery controller and mounted root. Kind 0..12 selects
/// the kill counter and cue. Random spin is in 4096 units per turn; the last
/// shot direction becomes a clamped vertical velocity in parent units per
/// tick. At tick 16 the target signals completion and enters the hidden task
/// state; its mount's teardown later destroys it.
static void _actor107600TumbleDestroyedTarget(Task* task)
{
    _Actor107600TargetWork*  work      = task->work;
    Enemy*                   enemy     = task->spawnArg2.pointer;
    TmdObject*               model     = task->extra.tmd;
    GfxCoord*                rootCoord = model->coords;
    MistShootingGalleryWork* gallery   = D_mist_shooting_gallery_8018E0C4->work;
    VECTOR*                  hitDirection;
    s32                      targetKind;
    s32                      soundId;
    s32                      pan;
    s32                      verticalVelocity;

    switch (work->step) {
        case ACTOR_107600_TARGET_TUMBLE_STEP_BEGIN:
            work->step++;
            model->flags          |= TMD_OBJECT_SEMI_TRANS;
            task->spawnArg1.value |= ACTOR_107600_TARGET_SIGNAL_STOP;
            work->hitMarkCount     = 0;
            work->field_15C        = 0;
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
            work->timer = 0;
            targetKind  = task->spawnArg1.value & ACTOR_107600_TARGET_SPAWN_KIND_MASK;
            soundId     = targetKind;
            gallery->kills[targetKind]++;
            if (soundId < 9) {
                soundId = ACTOR_107600_TARGET_KILL_SOUND_KINDS_0_TO_8;
            } else if (soundId == 9) {
                soundId = ACTOR_107600_TARGET_KILL_SOUND_KIND_9;
            } else {
                soundId = ACTOR_107600_TARGET_KILL_SOUND_KINDS_10_UP;
            }
            pan = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            work->yaw  = (rootCoord->parent)->param.rot.vy;
            work->roll = (rootCoord->parent)->param.rot.vz;
            _actor107600RollTargetSpin(&work->pitchSpin);
            _actor107600RollTargetSpin(&work->yawSpin);
            _actor107600RollTargetSpin(&work->rollSpin);
            // Detach into the mount's parent frame before applying the shot direction.
            _actor107600CopyTargetRotation(&rootCoord->parent->coord, &rootCoord->coord);
            rootCoord->coord.t[0] += rootCoord->parent->coord.t[0];
            rootCoord->coord.t[1] += rootCoord->parent->coord.t[1];
            rootCoord->coord.t[2] += rootCoord->parent->coord.t[2];
            rootCoord->parent      = &gGfxViewCoord;
            hitDirection           = &work->knockback;
            VectorNormal(hitDirection, hitDirection);
            ApplyMatrixLV(&rootCoord->coord, hitDirection, hitDirection);
            work->knockback.vx = 0;
            if (rootCoord->coord.t[1] < -2000) {
                verticalVelocity = work->knockback.vy >> 4;
            } else {
                verticalVelocity = work->knockback.vy >> 2;
            }
            work->knockback.vy = verticalVelocity = -verticalVelocity;
            work->knockback.vz                    = 0;
            if (verticalVelocity < -220) {
                work->knockback.vy = -220;
            }
            worldTargetUnlinkNode(&enemy->node);
            enemy->recs       = NULL;
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        case ACTOR_107600_TARGET_TUMBLE_STEP_MOVE:
            // Decay spin and velocity while the target shrinks; tick 16 ends its task state.
            work->timer++;
            if (work->timer < ACTOR_107600_TARGET_TUMBLE_FRAMES) {
                work->knockback.vx    -= work->knockback.vx >> 4;
                work->knockback.vy    -= work->knockback.vy >> 4;
                work->knockback.vz    -= work->knockback.vz >> 4;
                work->pitchSpin       -= work->pitchSpin >> 6;
                work->pitch           += work->pitchSpin;
                work->yawSpin         -= work->yawSpin >> 6;
                work->yaw             += work->yawSpin;
                work->rollSpin        -= work->rollSpin >> 6;
                work->roll            += work->rollSpin;
                rootCoord->coord.t[0] += work->knockback.vx;
                rootCoord->coord.t[1] += work->knockback.vy;
                rootCoord->coord.t[2] += work->knockback.vz;
                if (rootCoord->coord.t[1] < -0x40) {
                    rootCoord->coord.t[1] += 0x40;
                }
                if (work->heightPercent > ACTOR_107600_TARGET_SHRINK_STOP_PERCENT) {
                    work->heightPercent -= ACTOR_107600_TARGET_SHRINK_STEP_PERCENT;
                    return;
                }
                if (work->widthPercent > ACTOR_107600_TARGET_SHRINK_STOP_PERCENT) {
                    work->widthPercent -= ACTOR_107600_TARGET_SHRINK_STEP_PERCENT;
                    return;
                }
            } else {
                task->state++;
                task->spawnArg1.value |= ACTOR_107600_TARGET_SIGNAL_FINISHED;
            }
            break;
        case ACTOR_107600_TARGET_TUMBLE_STEP_INERT:
            break;
    }
}

/// Adds a hit mark and its sound cue until the target's eight marks are filled.
///
/// Task must own the supplied live target work and model. At the eight-mark
/// limit neither the count nor the cue changes. Each accepted mark sounds at
/// the root's composed origin; pan and attenuation narrow to signed bytes.
static inline void _actor107600AddTargetHitMark(Task* task, _Actor107600TargetWork* work)
{
    GfxCoord* rootCoord;
    s32       pan;

    if (work->hitMarkCount < ACTOR_107600_TARGET_HIT_MARK_COUNT) {
        rootCoord = task->extra.tmd->coords;
        work->hitMarkCount++;
        pan = (s8)worldCoordGetOriginAudioPan(rootCoord);
        sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_TARGET_HIT, pan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
    }
}

/// Applies attack contacts to a gallery target and queues its hit reaction.
///
/// Requires live target work, enemy, model and initialized eight-entry contacts.
/// Each category-2 contact copies a 4096-per-unit direction, measures the player
/// distance and computes damage, narrowed to signed 16-bit HP. Cooldowns narrow
/// to s16 frames; a cooldown started here does not stop subsequent contacts in
/// this scan. HP clamps at zero. Positive damage adds at most eight marks and
/// requests a heavy flinch at 20 HP, otherwise a light one.
///
/// Each attack overwrites hitTaken and hitDamage; nonpositive damage clears hitTaken
/// without clearing an earlier queued reaction. All contacts are cleared before
/// returning.
static void _actor107600ApplyTargetHits(Task* task)
{
    enum {
        ACTOR_107600_TARGET_HIT_SCRATCH_BYTES = 8,
        ACTOR_107600_TARGET_HEAVY_HIT_HP      = 20,
    };

    _Actor107600TargetWork* work;
    Enemy*                  enemy;
    s32                     contactIndex;
    s16                     damage;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    // Retain the eight-byte scratch frame; this routine never accesses its payload.
    SCRATCH_STACK_RESERVE_BYTES(ACTOR_107600_TARGET_HIT_SCRATCH_BYTES);
    work->hitTaken = 0;
    if (worldCollisionFindContactIndex(work->body.context.contacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->contacts); contactIndex++) {
            if ((work->contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
                // Contact axes become the destroyed target's later knockback direction.
                work->hitTaken     = 1;
                work->knockback.vx = work->contacts[contactIndex].response.direction.vx;
                work->knockback.vy = work->contacts[contactIndex].response.direction.vy;
                work->knockback.vz = work->contacts[contactIndex].response.direction.vz;
                _actor107600MeasureTargetPlayerDistance(task);
                damage            = damageComputePlayerAttack(work->contacts[contactIndex].key.value, work->playerDistance, 0, 0);
                work->hitCooldown = damageGetPlayerAttackHitCooldown(work->contacts[contactIndex].key.value);
                work->hitDamage   = damage;
                worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                enemy->hp -= damage;
                if (enemy->hp <= 0) {
                    enemy->hp = 0;
                }
                if (damage > 0) {
                    _actor107600AddTargetHitMark(task, work);
                    if (damage >= ACTOR_107600_TARGET_HEAVY_HIT_HP) {
                        work->hitReaction = ACTOR_107600_HIT_REACTION_HEAVY;
                    } else {
                        work->hitReaction = ACTOR_107600_HIT_REACTION_LIGHT;
                    }
                } else {
                    work->hitTaken = 0;
                }
            }
        }
    }
    worldCollisionClearContacts(work->contacts);
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_107600_TARGET_HIT_SCRATCH_BYTES);
}

/// Corner offsets of the quad `_actor107600DrawTargetDeathBurst` draws.
static const DVECTOR D_actor_107600_80131ED8[] = {
    { -0x100, -0x100 },
    { -0x100, 0x100 },
    { 0x100, -0x100 },
    { 0x100, 0x100 },
};

/// Draws the 512-unit burst replacing the final hit mark on a dead target.
///
/// Borrows the root and centerOffset; offsets are added to the local root
/// translation, narrowed to s16, then projected through its cached workm. The
/// caller must refresh that cache, initialize scratch storage and provide space
/// for one POLY_FT4 in the primitive arena. Accepted buckets are 4..1013
/// in the normal 1024-tag ordering table. A packet is consumed even when its
/// biased depth is too near to queue; no arena bounds check runs.
/// Depth is the last corner's SZ3/4, biased by 160, and shifted four more bits for the ordering bucket.
static void _actor107600DrawTargetDeathBurst(const GfxCoord* rootCoord, const SVECTOR* centerOffset)
{
    enum { ACTOR_107600_TARGET_DEATH_BURST_DEPTH_BIAS = 0xA0 };

    _Actor107600QuadScratch* projection;
    POLY_FT4*                packet;
    s32                      cornerIndex;

    projection = SCRATCH_STACK_RESERVE_BLOCK(_Actor107600QuadScratch);
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(projection->vertices); cornerIndex++) {
        projection->vertices[cornerIndex].vx = centerOffset->vx + (D_actor_107600_80131ED8[cornerIndex].vx + rootCoord->coord.t[0]);
        projection->vertices[cornerIndex].vy = centerOffset->vy + (D_actor_107600_80131ED8[cornerIndex].vy + rootCoord->coord.t[1]);
        projection->vertices[cornerIndex].vz = rootCoord->coord.t[2] + centerOffset->vz;
    }
    // Project four signed-halfword corners through the cached root transform.
    gte_SetRotMatrix(&rootCoord->workm);
    gte_SetTransMatrix(&rootCoord->workm);
    gte_ldv0(&projection->vertices[0]);
    gte_rtps();
    packet         = gGpuPrimCursor;
    gGpuPrimCursor = packet + 1;
    setPolyFT4(packet);
    gte_stsxy(&projection->screenCorners[0]);
    gte_ldv3(&projection->vertices[1], &projection->vertices[2], &projection->vertices[3]);
    gte_rtpt();
    packet->tpage = ACTOR_107600_TARGET_EFFECT_TPAGE;
    packet->clut  = ACTOR_107600_TARGET_EFFECT_CLUT;
    setUV4(packet, 0x40, 0, 0x67, 0, 0x40, 0x27, 0x67, 0x27);
    setShadeTex(packet, 1);
    gte_stsxy3(&projection->screenCorners[1], &projection->screenCorners[2], &projection->screenCorners[3]);
    gte_stszotz(&projection->depth);
    projection->depth -= ACTOR_107600_TARGET_DEATH_BURST_DEPTH_BIAS;
    if (projection->depth < ACTOR_107600_TARGET_EFFECT_MIN_DEPTH) {
        SCRATCH_STACK_RELEASE_BLOCK(_Actor107600QuadScratch);
        return;
    }
    // Split the GTE screen words into the packet's signed X/Y halves.
    packet->x0 = projection->screenCorners[0];
    packet->y0 = projection->screenCorners[0] >> 16;
    packet->x1 = projection->screenCorners[1];
    packet->y1 = projection->screenCorners[1] >> 16;
    packet->x2 = projection->screenCorners[2];
    packet->y2 = projection->screenCorners[2] >> 16;
    packet->x3 = projection->screenCorners[3];
    packet->y3 = projection->screenCorners[3] >> 16;
    addPrim(&gGpuCurrentOt[projection->depth >> ACTOR_107600_TARGET_EFFECT_OT_SHIFT], packet);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor107600QuadScratch);
}

/// Corner offsets of the quad `_actor107600DrawTargetHitMark` draws; the
/// zero fifth entry is never read.
static const DVECTOR D_actor_107600_80131EE8[] = {
    { -0x60, -0x60 },
    { -0x60, 0x60 },
    { 0x60, -0x60 },
    { 0x60, 0x60 },
    { 0, 0 },
};

/// Draws one 192-unit hit mark on a gallery target's face.
///
/// Borrows the root and centerOffset; offsets are added to the local root
/// translation, narrowed to s16, then projected through its cached workm. The
/// caller must refresh that cache, initialize scratch storage and provide space
/// for one POLY_FT4 in the primitive arena. Accepted buckets are 4..1019
/// in the normal 1024-tag ordering table. A packet is consumed even when its
/// biased depth is too near to queue; no arena bounds check runs.
/// Depth is the last corner's SZ3/4, biased by 64, and shifted four more bits for the ordering bucket.
static void _actor107600DrawTargetHitMark(const GfxCoord* rootCoord, const SVECTOR* centerOffset)
{
    enum { ACTOR_107600_TARGET_HIT_MARK_DEPTH_BIAS = 0x40 };

    _Actor107600QuadScratch* projection;
    POLY_FT4*                packet;
    s32                      cornerIndex;

    projection = SCRATCH_STACK_RESERVE_BLOCK(_Actor107600QuadScratch);
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(projection->vertices); cornerIndex++) {
        projection->vertices[cornerIndex].vx = centerOffset->vx + (D_actor_107600_80131EE8[cornerIndex].vx + rootCoord->coord.t[0]);
        projection->vertices[cornerIndex].vy = centerOffset->vy + (D_actor_107600_80131EE8[cornerIndex].vy + rootCoord->coord.t[1]);
        projection->vertices[cornerIndex].vz = rootCoord->coord.t[2] + centerOffset->vz;
    }
    // Project four signed-halfword corners through the cached root transform.
    gte_SetRotMatrix(&rootCoord->workm);
    gte_SetTransMatrix(&rootCoord->workm);
    gte_ldv0(&projection->vertices[0]);
    gte_rtps();
    packet         = gGpuPrimCursor;
    gGpuPrimCursor = packet + 1;
    setPolyFT4(packet);
    gte_stsxy(&projection->screenCorners[0]);
    gte_ldv3(&projection->vertices[1], &projection->vertices[2], &projection->vertices[3]);
    gte_rtpt();
    packet->tpage = ACTOR_107600_TARGET_EFFECT_TPAGE;
    packet->clut  = ACTOR_107600_TARGET_EFFECT_CLUT;
    setUV4(packet, 0x68, 0, 0x77, 0, 0x68, 0xF, 0x77, 0xF);
    setShadeTex(packet, 1);
    gte_stsxy3(&projection->screenCorners[1], &projection->screenCorners[2], &projection->screenCorners[3]);
    gte_stszotz(&projection->depth);
    projection->depth -= ACTOR_107600_TARGET_HIT_MARK_DEPTH_BIAS;
    if (projection->depth < ACTOR_107600_TARGET_EFFECT_MIN_DEPTH) {
        SCRATCH_STACK_RELEASE_BLOCK(_Actor107600QuadScratch);
        return;
    }
    // Split the GTE screen words into the packet's signed X/Y halves.
    packet->x0 = projection->screenCorners[0];
    packet->y0 = projection->screenCorners[0] >> 16;
    packet->x1 = projection->screenCorners[1];
    packet->y1 = projection->screenCorners[1] >> 16;
    packet->x2 = projection->screenCorners[2];
    packet->y2 = projection->screenCorners[2] >> 16;
    packet->x3 = projection->screenCorners[3];
    packet->y3 = projection->screenCorners[3] >> 16;
    addPrim(&gGpuCurrentOt[projection->depth >> ACTOR_107600_TARGET_EFFECT_OT_SHIFT], packet);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor107600QuadScratch);
}

/// Remaps a gallery target's lighting color matrix, including its sine pulse.
///
/// Default and unhandled modes preserve the matrix; black clears only its 3x3
/// coefficients. Weighted mode replicates (7R + 6G + 3B)/33 down each column,
/// then adds rsin(loopCount * 198) + ONE. Both stages narrow to s16, so the
/// intermediate truncation is retained. Translation and alignment bytes stay
/// intact. unusedEnemy is ignored; callers pass the target's live Enemy.
static void _actor107600RemapTargetColor(Enemy* unusedEnemy, MATRIX* colorMatrix, s32 colorMode)
{
    enum { ACTOR_107600_TARGET_COLOR_PULSE_ANGLE_STEP = 198 };

    s32 columnIndex;
    s16 columnValue;

    switch (colorMode) {
        case ENEMY_COLOR_DEFAULT:
            break;
        case ENEMY_COLOR_WEIGHTED:
            for (columnIndex = 0; columnIndex < ARRAY_SIZE(colorMatrix->m[0]); columnIndex++) {
                // Narrow the weighted column before adding the halfword sine pulse.
                columnValue                    = (colorMatrix->m[0][columnIndex] * 7 + colorMatrix->m[1][columnIndex] * 6 + colorMatrix->m[2][columnIndex] * 3) / 33;
                columnValue                   += (s16)(rsin(gDisplayState.loopCount * ACTOR_107600_TARGET_COLOR_PULSE_ANGLE_STEP) + ONE);
                colorMatrix->m[0][columnIndex] = columnValue;
                colorMatrix->m[1][columnIndex] = columnValue;
                colorMatrix->m[2][columnIndex] = columnValue;
            }
            break;
        case ENEMY_COLOR_BLACK:
            colorMatrix->m[0][0] = 0;
            colorMatrix->m[0][1] = 0;
            colorMatrix->m[0][2] = 0;
            colorMatrix->m[1][0] = 0;
            colorMatrix->m[1][1] = 0;
            colorMatrix->m[1][2] = 0;
            colorMatrix->m[2][0] = 0;
            colorMatrix->m[2][1] = 0;
            colorMatrix->m[2][2] = 0;
            break;
    }
}

/// Copies the nine directional-light colour coefficients for a target's blend.
///
/// Both arrays must be disjoint. Copies signed Q12 coefficients without the
/// enclosing matrices' alignment bytes or ambient translation.
static inline void _actor107600CopyTargetLightColor(s16 destination[3][3], const s16 source[3][3])
{
    destination[0][0] = source[0][0];
    destination[0][1] = source[0][1];
    destination[0][2] = source[0][2];
    destination[1][0] = source[1][0];
    destination[1][1] = source[1][1];
    destination[1][2] = source[1][2];
    destination[2][0] = source[2][0];
    destination[2][1] = source[2][1];
    destination[2][2] = source[2][2];
}

/// Rebuilds a gallery target's lighting and applies or blends its colour modes.
///
/// Requires a live enemy/model with writable light and colour matrices.
/// `worldPosition` supplies three word-aligned signed 32-bit world coordinates;
/// only those 12 bytes are read, following `worldCoordSetModelLighting`'s frame
/// contract. Neither it nor the ignored trailing arguments are retained.
/// With scene updates paused exactly at 1, only actively drawn models with a
/// buffer update. A positive signed-byte countdown weights the previous mode
/// by countdown/16; weighted mode includes the target's sine pulse. Only the
/// nine light coefficients are blended, preserving the sampled ambient term.
/// The countdown decreases only while actors run. Borrows and releases one
/// `WorldCoordActorColorScratch` plus the query's nested scratch; changes GTE
/// state. Input and output storage must be disjoint from those reservations.
static void _actor107600UpdateTargetColor(Enemy* enemy, const void* worldPosition, s32 unusedArg2, s32 unusedArg3)
{
    enum {
        ACTOR_107600_COLOR_UPDATE_PAUSED      = 1,
        ACTOR_107600_COLOR_BLEND_WEIGHT_SHIFT = 8,
    };
    TmdObject*                   model;
    MATRIX*                      colorMtx;
    s32                          currentMode;
    WorldCoordActorColorScratch* blendScratch;
    s32                          lightIndex;
    s32                          previousWeight;
    s32                          currentWeight;

    model       = enemy->task->extra.tmd;
    colorMtx    = model->colorMtx;
    currentMode = enemy->colorMode & ENEMY_COLOR_MODE_MASK;
    if ((!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && (model->buffer != NULL)) || (gGameSession->sceneUpdatesPaused != ACTOR_107600_COLOR_UPDATE_PAUSED)) {
        blendScratch = SCRATCH_STACK_RESERVE_BLOCK(WorldCoordActorColorScratch);
        // Sample the three lights before remapping their colour coefficients.
        worldCoordSetModelLighting(model, worldPosition, 0, ARRAY_SIZE(colorMtx->m[0]));
        if ((s8)enemy->colorBlend <= 0) {
            _actor107600RemapTargetColor(enemy, colorMtx, currentMode);
        } else {
            // Remap two copies of the same sample; ambient is never blended.
            _actor107600CopyTargetLightColor(blendScratch->previousColor.m, colorMtx->m);
            _actor107600RemapTargetColor(enemy, colorMtx, currentMode);
            _actor107600RemapTargetColor(enemy, &blendScratch->previousColor, (enemy->colorMode >> ENEMY_COLOR_PREVIOUS_SHIFT) & ENEMY_COLOR_MODE_MASK);
            previousWeight = (s8)enemy->colorBlend << ACTOR_107600_COLOR_BLEND_WEIGHT_SHIFT;
            currentWeight  = ONE - previousWeight;
            for (lightIndex = 0; lightIndex < (s32)ARRAY_SIZE(colorMtx->m[0]); lightIndex++) {
                blendScratch->currentColumn.vx  = colorMtx->m[0][lightIndex];
                blendScratch->currentColumn.vy  = colorMtx->m[1][lightIndex];
                blendScratch->currentColumn.vz  = colorMtx->m[2][lightIndex];
                blendScratch->previousColumn.vx = blendScratch->previousColor.m[0][lightIndex];
                blendScratch->previousColumn.vy = blendScratch->previousColor.m[1][lightIndex];
                blendScratch->previousColumn.vz = blendScratch->previousColor.m[2][lightIndex];
                gte_LoadAverageShort12(&blendScratch->currentColumn, &blendScratch->previousColumn, currentWeight, previousWeight, &blendScratch->currentColumn);
                colorMtx->m[0][lightIndex] = blendScratch->currentColumn.vx;
                colorMtx->m[1][lightIndex] = blendScratch->currentColumn.vy;
                colorMtx->m[2][lightIndex] = blendScratch->currentColumn.vz;
            }
            if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                enemy->colorBlend--;
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(WorldCoordActorColorScratch);
    }
}

/// Dispatches one gallery target tick through its four task lifecycle states.
///
/// Task state must be in 0..3: initialize, update, hide, destroy. The exported
/// actor descriptors select this private callback. The hide state waits for
/// mount teardown; the installed exit callback unlinks collision before the
/// owned work is freed.
static void _actor107600TargetTask(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_actor_107600_80131E74;
    handlers.funcs[task->state](task);
}

/// Hides a finished target while its mount keeps ownership of the task.
///
/// Requires a live model; this state waits for parent teardown without advancing.
static void _actor107600HideTarget(Task* task)
{
    TmdObject* model = task->extra.tmd;

    model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
}

/// Unlinks a gallery target's collision body and destroys its enemy and task.
///
/// Installed after work allocation succeeds. Unlinking precedes task teardown,
/// which releases the work containing the body and its contact table.
static void _actor107600DestroyTarget(Task* task)
{
    _Actor107600TargetWork* work = task->work;

    worldCollisionUnlinkBody(&work->body);
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Links the gallery target's shot-contact sphere and initializes its eight contacts.
///
/// The sphere borrows the live root coordinate and work-owned contact table,
/// which also supplies the enemy's contacts. Its local Y is -592 parent units;
/// radius is 544 for hanging targets and 400 otherwise. Linking alone leaves
/// pair testing disabled until the target stands.
static void _actor107600InitTargetCollision(Task* task)
{
    enum { ACTOR_107600_TARGET_COLLISION_KEY = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x4C };

    _Actor107600TargetWork* work      = task->work;
    GfxCoord*               rootCoord = task->extra.tmd->coords;
    WorldCollisionContact*  contacts  = work->contacts;

    work->body.coord            = rootCoord;
    work->body.context.contacts = contacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = -0x250;
    work->body.pos.vz           = 0;
    work->body.key              = ACTOR_107600_TARGET_COLLISION_KEY;
    work->body.radius           = (work->mountBehaviour == ACTOR_107600_MOUNT_HANGING) ? 0x220 : 0x190;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(work->contacts), 0);
}

/// Refreshes a gallery target's lighting and hit tint from its composed root.
///
/// Requires live target work, Enemy and model with a current root matrix.
/// Borrows one scratch VECTOR across the colour query; only XYZ are initialized,
/// in the query's world-coordinate units, and no pointer is retained.
static void _actor107600UpdateTargetRootColor(Task* task)
{
    GfxCoord* rootCoord;
    void**    cursorSlot;
    VECTOR*   savedCursor;
    VECTOR*   worldPosition;
    Enemy*    enemy;

    enemy                             = task->spawnArg2.pointer;
    rootCoord                         = task->extra.tmd->coords;
    cursorSlot                        = SCRATCH_HEAD_ADDR;
    savedCursor                       = SCRATCH_HEAD_AT(cursorSlot, VECTOR);
    worldPosition                     = savedCursor - 1;
    worldPosition->vx                 = rootCoord->workm.t[0];
    worldPosition->vy                 = rootCoord->workm.t[1];
    worldPosition->vz                 = rootCoord->workm.t[2];
    SCRATCH_HEAD_AT(cursorSlot, void) = worldPosition;
    _actor107600UpdateTargetColor(enemy, worldPosition, 0, 0);
    SCRATCH_POP_BYTES_AT(cursorSlot, sizeof(*worldPosition));
}

/// Rebuilds a gallery target's local rotation from its wrapped Euler angles.
///
/// Angles use 4096 units per turn. Compose Z, X, then Y rotations on the
/// fixed-point identity and copy only the nine matrix elements, preserving
/// root translation. Requires initialized scratch storage for one MATRIX
/// plus the axis helpers' nested reservations. The caller dirties composition.
static void _actor107600UpdateTargetRotation(Task* task)
{
    _Actor107600TargetWork* work      = task->work;
    GfxCoord*               rootCoord = task->extra.tmd->coords;
    MATRIX*                 rotation;

    work->pitch &= ACTOR_107600_ANGLE_MASK;
    work->yaw   &= ACTOR_107600_ANGLE_MASK;
    work->roll  &= ACTOR_107600_ANGLE_MASK;
    rotation     = SCRATCH_STACK_CURSOR(MATRIX) - 1;
    // Initialize the unpublished rotation before making room for the axis helpers.
    gfxSetRotIdentity(rotation);
    SCRATCH_STACK_CURSOR(MATRIX) = rotation;
    gfxRotMatrixZ(rotation, work->roll, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(rotation, work->pitch, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixY(rotation, work->yaw, GRAPHICS_ROTATION_COMPOSE);
    _actor107600CopyTargetRotation(rotation, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Copies the nine rotation elements used by the gallery target model.
///
/// Source and destination must provide live `MATRIX` objects. Translation
/// and alignment bytes are preserved; copying a matrix onto itself is valid.
static void _actor107600CopyTargetRotation(const MATRIX* source, MATRIX* destination)
{
    destination->m[0][0] = source->m[0][0];
    destination->m[0][1] = source->m[0][1];
    destination->m[0][2] = source->m[0][2];
    destination->m[1][0] = source->m[1][0];
    destination->m[1][1] = source->m[1][1];
    destination->m[1][2] = source->m[1][2];
    destination->m[2][0] = source->m[2][0];
    destination->m[2][1] = source->m[2][1];
    destination->m[2][2] = source->m[2][2];
}

/// Enters a gallery target behaviour state and restarts it at step zero.
///
/// State is an `ACTOR_107600_TARGET_STATE_*` dispatch value stored as s16.
/// The live task must own a target work block; this does not change `Task::state`.
static void _actor107600SetTargetState(Task* task, s16 state)
{
    _Actor107600TargetWork* work = task->work;

    work->state = state;
    work->step  = 0;
}

/// Enters a hit-selected target state and restarts its step sequence.
///
/// Requires live gallery-target work in task. state is a signed-halfword
/// ACTOR_107600_TARGET_STATE_* behaviour index; callers select flinch or resume
/// states. Resets only the behaviour step, leaving Task::state and the queued
/// hit request unchanged. Retains no pointer.
static inline void _actor107600EnterTargetHitState(Task* task, s16 state)
{
    _Actor107600TargetWork* reactionWork = task->work;

    reactionWork->state = state;
    reactionWork->step  = 0;
}

/// Consumes a gallery target's queued reaction when its last hit was damaging.
///
/// Returns 1 exactly when hitTaken is 1, including a zero or unrecognized
/// request; in that case hitReaction is cleared. Other hitTaken values return
/// 0 and preserve the request. Requests 1 and 2 restart light/heavy flinch;
/// retained requests 3..5 select immediate-resume slots 4, 6 and 5. The package
/// never produces those three requests, whose intended effects are unproven.
/// Requires live target work; it changes work state and step, not task state.
static s16 _actor107600ConsumeTargetHitReaction(Task* task)
{
    enum {
        ACTOR_107600_HIT_REACTION_RESUME_STATE_4 = 3,
        ACTOR_107600_HIT_REACTION_RESUME_STATE_6 = 4,
        ACTOR_107600_HIT_REACTION_RESUME_STATE_5 = 5,
        ACTOR_107600_TARGET_STATE_RESUME_4       = 4,
        ACTOR_107600_TARGET_STATE_RESUME_5       = 5,
        ACTOR_107600_TARGET_STATE_RESUME_6       = 6,
    };

    _Actor107600TargetWork* work = task->work;

    if (work->hitTaken == 1) {
        // Preserve the signed halfword dispatch, including wraparound requests.
        switch ((s16)(work->hitReaction - 1)) {
            case ACTOR_107600_HIT_REACTION_LIGHT - 1:
                _actor107600EnterTargetHitState(task, ACTOR_107600_TARGET_STATE_LIGHT_FLINCH);
                break;
            case ACTOR_107600_HIT_REACTION_HEAVY - 1:
                _actor107600EnterTargetHitState(task, ACTOR_107600_TARGET_STATE_HEAVY_FLINCH);
                break;
            case ACTOR_107600_HIT_REACTION_RESUME_STATE_4 - 1:
                _actor107600EnterTargetHitState(task, ACTOR_107600_TARGET_STATE_RESUME_4);
                break;
            case ACTOR_107600_HIT_REACTION_RESUME_STATE_6 - 1:
                _actor107600EnterTargetHitState(task, ACTOR_107600_TARGET_STATE_RESUME_6);
                break;
            case ACTOR_107600_HIT_REACTION_RESUME_STATE_5 - 1:
                _actor107600EnterTargetHitState(task, ACTOR_107600_TARGET_STATE_RESUME_5);
                break;
        }
        work->hitReaction = ACTOR_107600_HIT_REACTION_NONE;
        return 1;
    }
    return 0;
}

/// Starts the gallery target's behaviour according to its mount kind.
///
/// Standing and hanging targets start active, folded flat at 10 percent scale
/// and black lighting; one random draw chooses the first of eight hit marks.
/// Fixed demonstration targets start their exit behaviour at full scale.
/// Requires live target work, enemy and model; no other work fields are reset.
static void _actor107600BeginTargetBehaviour(Task* task)
{
    enum {
        ACTOR_107600_TARGET_INITIAL_SCALE_PERCENT = 10,
        ACTOR_107600_TARGET_FULL_SCALE_PERCENT    = 100,
    };

    _Actor107600TargetWork* work  = task->work;
    Enemy*                  enemy = task->spawnArg2.pointer;

    switch (work->mountBehaviour) {
        case ACTOR_107600_MOUNT_STANDING:
        case ACTOR_107600_MOUNT_HANGING:
            work->state         = ACTOR_107600_TARGET_STATE_ACTIVE;
            work->pitch         = ACTOR_107600_ANGLE_QUARTER_TURN;
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->hitMarkFirst  = (gRandomLcgState >> 16) & (ACTOR_107600_TARGET_HIT_MARK_COUNT - 1);
            work->widthPercent  = ACTOR_107600_TARGET_INITIAL_SCALE_PERCENT;
            work->heightPercent = ACTOR_107600_TARGET_INITIAL_SCALE_PERCENT;
            _actor107600UpdateTargetRotation(task);
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
            enemy->colorBlend = 0;
            break;
        case ACTOR_107600_MOUNT_FIXED:
            work->state         = ACTOR_107600_TARGET_STATE_FIXED;
            work->widthPercent  = ACTOR_107600_TARGET_FULL_SCALE_PERCENT;
            work->heightPercent = ACTOR_107600_TARGET_FULL_SCALE_PERCENT;
            break;
    }
}

/// Resumes active target behaviour from state-table slot 4.
///
/// Restarts at step zero. The package's hit handler does not request this state;
/// its original reaction meaning is unproven.
static void _actor107600ResumeTargetState4(Task* task)
{
    _actor107600SetTargetState(task, ACTOR_107600_TARGET_STATE_ACTIVE);
}

/// Resumes active target behaviour from state-table slot 5.
///
/// Restarts at step zero. The package's hit handler does not request this state;
/// its original reaction meaning is unproven.
static void _actor107600ResumeTargetState5(Task* task)
{
    _actor107600SetTargetState(task, ACTOR_107600_TARGET_STATE_ACTIVE);
}

/// Resumes active target behaviour from state-table slot 6.
///
/// Restarts at step zero. The package's hit handler does not request this state;
/// its original reaction meaning is unproven.
static void _actor107600ResumeTargetState6(Task* task)
{
    _actor107600SetTargetState(task, ACTOR_107600_TARGET_STATE_ACTIVE);
}

/// Shows three hit marks on a fixed demonstration target and starts its exit.
///
/// Requires live target work. The fold-away state starts at step zero; these
/// marks are set without applying damage or registering a player hit.
static void _actor107600BeginFixedTargetExit(Task* task)
{
    enum { ACTOR_107600_FIXED_TARGET_HIT_MARKS = 3 };

    _Actor107600TargetWork* work = task->work;

    work->hitMarkCount = ACTOR_107600_FIXED_TARGET_HIT_MARKS;
    _actor107600SetTargetState(task, ACTOR_107600_TARGET_STATE_LEAVE);
}

/// Stores the XZ distance between the target and player root translations.
///
/// Reads coord.t directly, without transforming differing parent frames or
/// refreshing composition. The stored value is in coordinate units and supplies
/// the weapon damage distance scale. A missing player leaves the cache intact.
/// Requires live target work/model and initialized scratch storage for one VECTOR;
/// Y is staged but does not enter the length. XZ differences and their sum of
/// squares must fit s32.
static void _actor107600MeasureTargetPlayerDistance(Task* task)
{
    _Actor107600TargetWork* work;
    GfxCoord*               rootCoord;
    GfxCoord*               playerCoord;
    void**                  scratchSlot;
    VECTOR*                 savedCursor;
    VECTOR*                 offset;
    s32                     distance;

    work                               = task->work;
    rootCoord                          = task->extra.tmd->coords;
    scratchSlot                        = SCRATCH_HEAD_ADDR;
    savedCursor                        = SCRATCH_HEAD_AT(scratchSlot, VECTOR);
    offset                             = savedCursor - 1;
    SCRATCH_HEAD_AT(scratchSlot, void) = offset;
    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] == NULL) {
        SCRATCH_HEAD_AT(scratchSlot, void) = savedCursor;
        return;
    }
    // Read local translations directly; the parent hierarchy is not composed here.
    playerCoord = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    offset->vx  = playerCoord->coord.t[0] - rootCoord->coord.t[0];
    offset->vy  = playerCoord->coord.t[1] - rootCoord->coord.t[1];
    offset->vz  = playerCoord->coord.t[2] - rootCoord->coord.t[2];
    distance    = playerActorPlanarLength(offset->vx, offset->vz);
    SCRATCH_POP_BYTES_AT(scratchSlot, sizeof(*offset));
    // Keep the call result live until the scratch cursor is restored.
    work->playerDistance = distance;
}

/// Places a target root at its rotated local offset (0, -384, 0).
///
/// Uses the root's rotation, without adding its previous translation; the
/// result replaces all three parent-frame translation components. Requires
/// initialized scratch storage for one VECTOR. The caller dirties composition.
static void _actor107600PlaceTargetOffset(GfxCoord* rootCoord)
{
    void**  scratchSlot;
    VECTOR* savedCursor;
    VECTOR* offset;

    scratchSlot                        = SCRATCH_HEAD_ADDR;
    savedCursor                        = SCRATCH_HEAD_AT(scratchSlot, VECTOR);
    offset                             = savedCursor - 1;
    SCRATCH_HEAD_AT(scratchSlot, void) = offset;
    offset->vx                         = 0;
    offset->vy                         = -0x180;
    offset->vz                         = 0;
    ApplyMatrixLV(&rootCoord->coord, offset, offset);
    rootCoord->coord.t[0] = offset->vx;
    rootCoord->coord.t[1] = offset->vy;
    // Cursor restoration leaves these bytes intact for the final Z read.
    SCRATCH_POP_BYTES_AT(scratchSlot, sizeof(*offset));
    rootCoord->coord.t[2] = offset->vz;
}

/// Applies the target's width and folded-height percentages to its root matrix.
///
/// Requires live target work/model and a freshly rebuilt root rotation: repeated
/// application compounds the scale. Only m[0][0] and m[2][1] change. Coefficients
/// are signed Q12; division by 100 precedes multiplication and truncates towards
/// zero, then the result narrows back to s16. Translation is preserved.
static void _actor107600ApplyTargetScale(Task* task)
{
    enum { ACTOR_107600_TARGET_FULL_SCALE_PERCENT = 100 };

    _Actor107600TargetWork* work                    = task->work;
    GfxCoord*               rootCoord               = task->extra.tmd->coords;
    s16                     widthCoefficient        = rootCoord->coord.m[0][0];
    s16                     foldedHeightCoefficient = rootCoord->coord.m[2][1];

    rootCoord->coord.m[0][0] = widthCoefficient / ACTOR_107600_TARGET_FULL_SCALE_PERCENT * work->widthPercent;
    rootCoord->coord.m[2][1] = foldedHeightCoefficient / ACTOR_107600_TARGET_FULL_SCALE_PERCENT * work->heightPercent;
}
