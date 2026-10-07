#include <psyq/sys/types.h>
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

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

void        func_actor_107600_801328CC(Task* arg0);
static void _actor107600RetireMount(Task* task);
static void _actor107600DestroyMount(Task* task);
static void func_actor_107600_80132B0C(Task* arg0);
static void func_actor_107600_80132B7C(Task* arg0);
static void _actor107600CopyMountRotation(const MATRIX* source, MATRIX* destination);
static void func_actor_107600_80132CB8(Task* arg0);
static void func_actor_107600_80132CD4(Task* arg0);
static void _actor107600UpdateFixedMount(Task* task);
static void func_actor_107600_80132DF0(Enemy* arg0, s32 arg1, s32 arg2);
static void _actor107600InitTarget(Task* task);
static void func_actor_107600_80133024(Task* arg0);
static void func_actor_107600_801332D4(Task* arg0);
static void _actor107600UpdateTargetFlinch(Task* task);
static void _actor107600FoldTarget(Task* task);
static void _actor107600TumbleDestroyedTarget(Task* task);
static void func_actor_107600_80133FA8(GfxCoord* arg0, SVECTOR* arg1);
static void func_actor_107600_80134248(GfxCoord* arg0, SVECTOR* arg1);
static void func_actor_107600_80134608(struct Enemy* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void        func_actor_107600_801348A0(Task* arg0);
static void func_actor_107600_80134904(Task* arg0);
static void _actor107600DestroyTarget(Task* task);
static void _actor107600InitTargetCollision(Task* task);
static void func_actor_107600_801349E0(Task* arg0);
static void _actor107600UpdateTargetRotation(Task* task);
static void _actor107600CopyTargetRotation(const MATRIX* source, MATRIX* destination);
static void _actor107600SetTargetState(Task* task, s16 state);
static s32  func_actor_107600_80134BAC(Task* arg0);
static void func_actor_107600_80134C54(Task* arg0);
static void func_actor_107600_80134D10(Task* arg0);
static void func_actor_107600_80134D30(Task* arg0);
static void func_actor_107600_80134D50(Task* arg0);
static void func_actor_107600_80134D70(Task* arg0);
static void func_actor_107600_80134D9C(Task* arg0);
static void _actor107600PlaceTargetOffset(GfxCoord* rootCoord);
static void func_actor_107600_80134EF4(Task* arg0);

/* Waypoint paths `_actor107600UpdateStandingMountPath` walks, indexed by
 * `_Actor107600MountWork::path`; trailing-blob data. */
extern _Actor107600Waypoint* D_actor_107600_80135624[];

/* Eight effect offsets `func_actor_107600_80133024` cycles through from
 * `_Actor107600TargetWork::hitMarkFirst`. */
extern DVECTOR D_actor_107600_80135730[];

/* Table `func_actor_107600_80132DF0` spawns from, indexed with `arg1 + 1`; it
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

static void func_actor_107600_80131F10(Task* arg0);
static void _actor107600UpdateStandingMountPath(Task* task);
static void _actor107600UpdateHangingMountPath(Task* task);
static void func_actor_107600_80132930(Task* arg0);
static void func_actor_107600_80133DC4(Task* arg0);

/// The actor's top-level task states, which `func_actor_107600_801328CC` runs:
/// spawn, update, drop and destroy.
static const TaskFuncTable4 D_actor_107600_80131E24 = { {
    func_actor_107600_80131F10,
    func_actor_107600_80132930,
    _actor107600RetireMount,
    _actor107600DestroyMount,
} };

/// One entry per `_Actor107600MountWork::behaviour`, run by
/// `func_actor_107600_80132CD4`.
static const TaskFuncTable3 D_actor_107600_80131E34 = { {
    _actor107600UpdateStandingMountPath,
    _actor107600UpdateHangingMountPath,
    _actor107600UpdateFixedMount,
} };

void func_actor_107600_801328CC(Task*);
void func_actor_107600_801348A0(Task*);

DamageAttack D_actor_107600_80134F80[1] = { 0 };

EnemyParams D_actor_107600_80134F84 = { D_actor_107600_80134F80, 50, 0, 0, 0, 255, 0, 0, 0 };

TaskDesc D_actor_107600_80134F94[19] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801328CC, { .model = &gMistShootingGalleryModel0A81C } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel093FC } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel095EC } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel097DC } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel099CC } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel09BBC } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel09DAC } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel09F9C } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel0A18C } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel0A37C } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel0A56C } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel0B0D0 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel0B2C0 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel0B4B0 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel0B6A0 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel0AB30 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel0AC94 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel0ADF8 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_107600_801348A0, { .model = &gMistShootingGalleryModel0AF5C } },
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

static void func_actor_107600_801344E8(void* arg0, MATRIX* m, s32 mode);

/// Spawn state of the `D_actor_107600_80131E24` table. The target is dropped
/// (and the gallery's live count given back) when the player is within 0x400 on
/// XZ unless `Task::spawnArg1` bit 0x40000000 forces it, when byte 0 of
/// `spawnArg1` is the 0xFF marker, or when the work block cannot be allocated.
/// Otherwise binds the work block's matrices, records the spawn position, and
/// spawns the child from `func_actor_107600_80132DF0`.
static void func_actor_107600_80131F10(Task* arg0)
{
    TmdObject*             obj;
    Enemy*                 enemy;
    GfxCoord*              coord;
    GfxCoord*              target;
    void**                 scratch;
    u8*                    head;
    VECTOR*                block;
    _Actor107600MountWork* work;

    obj                            = arg0->extra.tmd;
    enemy                          = arg0->spawnArg2.pointer;
    coord                          = obj->coords;
    target                         = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = target->coord.t[0] - coord->coord.t[0];
    SCRATCH_HEAD_AT(scratch, void) = block;
    block->vz                      = target->coord.t[2] - coord->coord.t[2];
    if ((!(arg0->spawnArg1.value & 0x40000000) && playerActorPlanarLength(block->vx, block->vz) < 0x401) || (u8)arg0->spawnArg1.value == 0xFF) {
    fail:
        if ((arg0->spawnArg1.value & 0xF000) != 0x2000) {
            ((MistShootingGalleryWork*)arg0->parent->work)->liveTargets--;
        }
        SCRATCH_STACK_RELEASE_BYTES(0x10);
        enemyDestroy(enemy, arg0);
        return;
    }
    work       = memCalloc(sizeof(_Actor107600MountWork), false);
    arg0->work = work;
    if (work == NULL) {
        goto fail;
    }
    arg0->exitCallback = _actor107600DestroyMount;
    work->path         = (u8)((u32)arg0->spawnArg1.value >> 16);
    work->behaviour    = (s32)(arg0->spawnArg1.value & 0xF000) >> 12;
    obj->lightMtx      = &work->lightMatrix;
    obj->colorMtx      = &work->colorMatrix;
    enemy->param       = &D_actor_107600_80134F84;
    coord->parent      = &gGfxViewCoord;
    enemy->field_4     = &arg0->extra.tmd->coords->coord;
    enemy->field_48    = 0;
    if (work->behaviour != ACTOR_107600_MOUNT_FIXED) {
        /* retail passes a 0 the resident definition ignores */
        (sceneAcquireBattleRef)(0);
    }
    work->spawnX = coord->coord.t[0];
    work->spawnY = coord->coord.t[1];
    work->spawnZ = coord->coord.t[2];
    work->yaw    = -0x400;
    if (work->behaviour == ACTOR_107600_MOUNT_HANGING) {
        work->roll += 0x800;
    }
    arg0->state++;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    func_actor_107600_80132DF0(enemy, arg0->spawnArg1.value & 0xF,
                               work->behaviour | (((u32)arg0->spawnArg1.value >> 16) & 0x2000));
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Advances a path-following mount at the supplied bob and resting Y heights.
///
/// Heights and path coordinates use the root's parent frame. The task owns
/// mount work and requires a live first-child target throughout this tick.
static inline void _actor107600UpdateMountPath(Task* task, s32 bobY, s32 restingY)
{
    _Actor107600MountWork* work      = task->work;
    Enemy*                 enemy     = task->spawnArg2.pointer;
    GfxCoord*              rootCoord = task->extra.tmd->coords;
    _Actor107600Waypoint*  waypoint;
    s32                    axisDelta;

    switch (work->step) {
        case ACTOR_107600_MOUNT_STEP_RISE:
            if (work->heightPercent < 100) {
                work->heightPercent += 8;
                return;
            }
            work->heightPercent = 100;
            work->timer         = 0;
            work->step++;
        case ACTOR_107600_MOUNT_STEP_SETTLE:
            if (++work->timer & 1) {
                rootCoord->coord.t[1] = bobY;
                return;
            }
            rootCoord->coord.t[1] = restingY;
            if (work->timer >= 4) {
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
                work->step                                = ACTOR_107600_MOUNT_STEP_LEAVE;
                work->rotating                            = 0;
                enemy->task->firstChild->spawnArg1.value |= ACTOR_107600_TARGET_SIGNAL_STOP;
                return;
            }
            // Each axis arrival advances the waypoint; diagonal legs repeat their stop.
            axisDelta = (s16)(rootCoord->coord.t[0] - waypoint->x);
            if (axisDelta != 0) {
                if (waypoint->speed >= abs(axisDelta)) {
                    if (enemy->task->firstChild->spawnArg1.value & ACTOR_107600_TARGET_SIGNAL_STOP) {
                        work->step                                = ACTOR_107600_MOUNT_STEP_LEAVE;
                        work->rotating                            = 0;
                        enemy->task->firstChild->spawnArg1.value |= ACTOR_107600_TARGET_SIGNAL_STOP;
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
                        work->step                                = ACTOR_107600_MOUNT_STEP_LEAVE;
                        work->rotating                            = 0;
                        enemy->task->firstChild->spawnArg1.value |= ACTOR_107600_TARGET_SIGNAL_STOP;
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
                    work->heightPercent -= 8;
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

/// Task states run by `func_actor_107600_801348A0`, indexed by its
/// `Task::state`.
static const TaskFuncTable4 D_actor_107600_80131E74 = { {
    _actor107600InitTarget,
    func_actor_107600_80133024,
    func_actor_107600_80134904,
    _actor107600DestroyTarget,
} };

/// Behaviour states indexed by `_Actor107600TargetWork::state`, run by
/// `func_actor_107600_80133024`.
static const TaskFuncTable10 D_actor_107600_80131E84 = { {
    func_actor_107600_80134C54,
    func_actor_107600_801332D4,
    _actor107600UpdateTargetFlinch,
    _actor107600UpdateTargetFlinch,
    func_actor_107600_80134D10,
    func_actor_107600_80134D30,
    func_actor_107600_80134D50,
    _actor107600FoldTarget,
    func_actor_107600_80134D70,
    _actor107600TumbleDestroyedTarget,
} };

void func_actor_107600_801328CC(Task* arg0)
{
    TaskFuncTable4 handlers;

    handlers = D_actor_107600_80131E24;
    handlers.funcs[arg0->state](arg0);
}

/// Update state of the `D_actor_107600_80131E24` table, switched on the scene
/// mode `gSceneCombatState.actorControl`. Mode 0 runs the `state` sub-state, copies the yaw and
/// roll onto the model root, rebuilds its rotation and scales `coord.m[1][1]`
/// by the `heightPercent` percent; modes 0 and 1 then refresh the colour and show
/// the model, and mode 2 hides it.
static void func_actor_107600_80132930(Task* arg0)
{
    TmdObject*             ext      = arg0->extra.tmd;
    GfxCoord*              coord    = ext->coords;
    _Actor107600MountWork* work     = arg0->work;
    TaskFunc               funcs[2] = { func_actor_107600_80132CB8, func_actor_107600_80132CD4 };
    TmdObject*             obj;

    obj = ext;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            funcs[work->state](arg0);
            coord->param.rot.vy = work->yaw;
            coord->param.rot.vz = work->roll;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            func_actor_107600_80132B7C(arg0);
            coord->coord.m[1][1] = work->heightPercent * (coord->coord.m[1][1] / 100);
        case SCENE_COMBAT_ACTORS_PAUSED:
            func_actor_107600_80132B0C(arg0);
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            ext->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
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

/// Copies the world position of the model's first attach coordinate onto a
/// 0x10-byte `VECTOR` carved off the scratch stack and hands it to
/// `worldCoordUpdateActorColor` for the enemy in `Task::spawnArg2` with zero for the unused
/// arguments. Same shape as `func_actor_107600_801349E0`, a different callee.
static void func_actor_107600_80132B0C(Task* arg0)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;
    void*     obj;

    obj                            = arg0->spawnArg2.pointer;
    coord                          = arg0->extra.tmd->coords;
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    worldCoordUpdateActorColor(obj, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Rebuilds the model root's rotation from the work block's three angles: wrap
/// each to 12 bits, build the rotation in a scratch matrix carved off
/// the scratch stack, then copy its 3x3 into the part's `GfxCoord::coord`.
/// The copy is a call to `_actor107600CopyMountRotation`.
static void func_actor_107600_80132B7C(Task* arg0)
{
    _Actor107600MountWork* work  = arg0->work;
    GfxCoord*              coord = arg0->extra.tmd->coords;
    MATRIX*                m;

    work->pitch &= 0xFFF;
    work->yaw   &= 0xFFF;
    work->roll  &= 0xFFF;
    m            = (MATRIX*)(SCRATCH_STACK_CURSOR(u8) - 0x20);
    gfxSetRotIdentity(m);
    SCRATCH_STACK_CURSOR(MATRIX) = m;
    RotMatrixZ(work->roll, m);
    RotMatrixX(work->pitch, m);
    RotMatrixY(work->yaw, m);
    _actor107600CopyMountRotation(m, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(0x20);
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

static void func_actor_107600_80132CB8(Task* arg0)
{
    _Actor107600MountWork* work = arg0->work;

    work->state++;
}

/// Runs the `D_actor_107600_80131E34` entry for the mount's `behaviour`
/// through the same stack-copied table idiom as
/// `func_actor_107600_801328CC`, then advances the model's yaw by 0x20 while
/// `rotating` is set.
static void func_actor_107600_80132CD4(Task* arg0)
{
    TaskFuncTable3         sp;
    _Actor107600MountWork* work = arg0->work;

    sp = D_actor_107600_80131E34;
    sp.funcs[work->behaviour](arg0);
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

/// Spawns the next instance of this actor's own `D_actor_107600_80134F94`
/// table (`arg1 + 1` is the index) and adopts it as a child of `arg0`: the new
/// task is reparented and its root coordinate's `parent` link is pointed at
/// `arg0`'s own root coordinate, `Task::spawnArg1` is packed from the two
/// arguments, and the spawned model takes `arg1`'s texture page - dropping the
/// CLUT row to 0 once `arg1` reaches 10 - before the stream is processed twice
/// (one half-buffer per call) and the enemy's light is set to 0x900.
static void func_actor_107600_80132DF0(Enemy* arg0, s32 arg1, s32 arg2)
{
    Enemy*     enemy;
    GfxCoord*  coord;
    TmdObject* obj;

    enemy = enemySpawnFromTable(D_actor_107600_80134F94, arg1 + 1, arg2, arg0);
    if (enemy != NULL) {
        taskReparent(arg0->task, enemy->task);
        coord                        = enemy->task->extra.tmd->coords;
        coord->parent                = arg0->task->extra.tmd->coords;
        enemy->task->spawnArg1.value = arg1 | (arg2 << 16);
        obj                          = enemy->task->extra.tmd;
        obj->texturePageOffset       = 0;
        if (arg1 < 10) {
            obj->clutRowOffset = 2;
        } else {
            obj->clutRowOffset = 0;
        }
        tmdBuildBufferHalf(obj);
        tmdBuildBufferHalf(obj);
        enemy->workType = ENEMY_WORK_PLAIN;
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

/// Per-frame update switched on the scene mode `gSceneCombatState.actorControl`, like
/// `func_actor_107600_80132930`. Mode 0 runs the `state` entry of
/// `D_actor_107600_80131E84`, then (before `ACTOR_107600_TARGET_STATE_LEAVE`)
/// takes hits unless `hitCooldown` is still running, clears the contacts and
/// enters `ACTOR_107600_TARGET_STATE_DESTROYED` once the enemy's HP is gone.
/// Afterwards publishes the enemy's slot mask to the gallery and draws one hit
/// mark per `hitMarkCount`, the last one a larger burst when dead.
static void func_actor_107600_80133024(Task* arg0)
{
    TaskFuncTable10         sp;
    Enemy*                  enemy;
    TmdObject*              ext;
    TmdObject*              obj;
    _Actor107600TargetWork* work;
    GfxCoord*               coord;
    SVECTOR*                v;
    s32                     i;

    enemy = arg0->spawnArg2.pointer;
    ext   = arg0->extra.tmd;
    work  = arg0->work;
    coord = ext->coords;
    obj   = ext;
    sp    = D_actor_107600_80131E84;
    SCRATCH_STACK_RESERVE_BYTES(8);
    v = SCRATCH_STACK_CURSOR(SVECTOR);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            sp.funcs[work->state](arg0);
            if (work->state < ACTOR_107600_TARGET_STATE_LEAVE) {
                if (work->hitCooldown == 0) {
                    func_actor_107600_80133DC4(arg0);
                } else {
                    work->hitCooldown--;
                }
                worldCollisionClearContacts(work->contacts);
                if (enemy->hp <= 0) {
                    _actor107600SetTargetState(arg0, ACTOR_107600_TARGET_STATE_DESTROYED);
                }
            }
        case SCENE_COMBAT_ACTORS_PAUSED:
            func_actor_107600_801349E0(arg0);
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    if (work->mountBehaviour != ACTOR_107600_MOUNT_FIXED) {
        ((MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->work)->targetLockMask = worldTargetGetActorLockMask(&enemy->node);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor107600UpdateTargetRotation(arg0);
    func_actor_107600_80134EF4(arg0);
    for (i = 0; i < work->hitMarkCount; i++) {
        if (i == work->hitMarkCount - 1 && enemy->hp <= 0) {
            v->vx = 0;
            v->vy = -0xE0;
            v->vz = 0;
            func_actor_107600_80133FA8(coord, v);
        } else {
            v->vx = D_actor_107600_80135730[(work->hitMarkFirst + i) & 7].vx;
            v->vy = D_actor_107600_80135730[(work->hitMarkFirst + i) & 7].vy;
            v->vz = 0;
            func_actor_107600_80134248(coord, v);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// `ACTOR_107600_TARGET_STATE_ACTIVE`, stepped through `step`: once
/// `Task::spawnArg1` bit 0x10 is set, grows `widthPercent` then `heightPercent`
/// by 0x20 up to 100, plays a cue and eases `pitch` down to stand the target
/// up, alternates `pitch` for four frames and raises bit 0x20. From then on,
/// while bit 0x20000000 is set, `attackTimer` counts frames:
/// at 120 it switches the light mode, at 210 it spawns an effect on the
/// `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` actor's fifth coordinate and updates that actor.
static void func_actor_107600_801332D4(Task* arg0)
{
    _Actor107600TargetWork* work  = arg0->work;
    Enemy*                  enemy = arg0->spawnArg2.pointer;
    Task*                   player;
    GameActor*              actor;
    s32                     pan;
    s32                     flags;
    s16                     v;

    if ((s16)func_actor_107600_80134BAC(arg0) != 0) {
        return;
    }
    switch (work->step) {
        case 0:
            if (!(arg0->spawnArg1.value & 0x10)) {
                return;
            }
            work->step++;
        case 1:
            if (work->widthPercent < 100) {
                work->widthPercent += 0x20;
                return;
            }
            work->step++;
        case 2:
            if (work->heightPercent < 100) {
                work->heightPercent += 0x20;
                return;
            }
            {
                GfxCoord* o = arg0->extra.tmd->coords;
                s32       p;
                work->step++;
                p = (s8)worldCoordGetOriginAudioPan(o);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_TARGET_APPEAR, p, (s8)worldCoordGetOriginAudioDepth(o));
            }
        case 3: {
            u16 w = work->pitch;
            if ((u16)(w - 1) < 0x400) {
                work->pitch = w - ((0x420 - (s16)w) >> 2);
                return;
            }
        }
            work->timer = 0;
            work->step++;
            return;
        case 4:
            v           = work->timer + 1;
            work->timer = v;
            if (v & 1) {
                work->pitch = ((v << 16) >> 13) - 0x38;
            } else {
                work->pitch = 0;
                if (work->timer >= 4) {
                    work->step++;
                    arg0->spawnArg1.value |= 0x20;
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                    enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
                    work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
            }
        case 5:
            flags = arg0->spawnArg1.value;
            if (flags & 0x40) {
                _actor107600SetTargetState(arg0, ACTOR_107600_TARGET_STATE_LEAVE);
                return;
            }
            if (!(flags & 0x20000000)) {
                return;
            }
            work->attackTimer++;
            if (work->attackTimer == 120) {
                GfxCoord* o = arg0->extra.tmd->coords;
                s32       p;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                p = (s8)worldCoordGetOriginAudioPan(o);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_TARGET_CHARGE, p, (s8)worldCoordGetOriginAudioDepth(o));
            } else if (work->attackTimer == 210) {
                GfxCoord* c;
                s32       p;
                player            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                c                 = &player->extra.tmd->coords[4];
                actor             = player->work;
                work->attackTimer = 0;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                effectSpawn(EFFECT_MIST_GALLERY_TRACER, c, 0, NULL);
                p = (s8)worldCoordGetOriginAudioPan(c);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_TARGET_ATTACK, p, (s8)worldCoordGetOriginAudioDepth(c));
                if (actor->mode != GAME_ACTOR_MODE_DAMAGE) {
                    if (gPlayerStatus.hp < 11) {
                        ((MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->work)->lethalHit = 1;
                        actor->pendingDamage                                                          = 0;
                    } else {
                        actor->pendingDamage = 10;
                    }
                    actor->hitRegion      = 1;
                    actor->damageReaction = 5;
                    func_8010A9D0(player);
                    pan = (s8)worldCoordGetOriginAudioPan(c);
                    sndEvtRequestScriptStart(SOUND_PLAYER_STRUCK, pan, (s8)worldCoordGetOriginAudioDepth(c));
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

/// Draws one tumble spin in angle units per tick, from -126 to 127.
///
/// Advances the shared LCG once. Even magnitudes take the negative sign;
/// odd magnitudes stay positive. The output must be a live target spin field.
static inline void _actor107600RollTargetSpin(s16* spinAngle)
{
    enum { ACTOR_107600_TARGET_SPIN_MAGNITUDE_MASK = 0x7F };

    s16 spin;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    spin            = (gRandomLcgState >> 16) & ACTOR_107600_TARGET_SPIN_MAGNITUDE_MASK;
    *spinAngle      = spin;
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

/// Hit handler: for each contact of category 2, stores the shot's direction in
/// `knockback`, applies `damageComputePlayerAttack` to the enemy's HP, starts
/// `hitCooldown`, adds a hit mark and plays the hit sound for the first eight
/// damaging hits, and asks for a light or heavy flinch in `hitReaction`.
static void func_actor_107600_80133DC4(Task* arg0)
{
    _Actor107600TargetWork* work;
    Enemy*                  enemy;
    GfxCoord*               obj;
    s32                     i;
    s16                     damage;
    s32                     pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    SCRATCH_STACK_RESERVE_BYTES(8);
    work->hitTaken = 0;
    if (worldCollisionFindContactIndex(work->body.context.contacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        for (i = 0; i < ARRAY_SIZE(work->contacts); i++) {
            if ((work->contacts[i].key.value & 0xFFFF0000) == 0x20000) {
                work->hitTaken     = 1;
                work->knockback.vx = work->contacts[i].response.direction.vx;
                work->knockback.vy = work->contacts[i].response.direction.vy;
                work->knockback.vz = work->contacts[i].response.direction.vz;
                func_actor_107600_80134D9C(arg0);
                damage            = damageComputePlayerAttack(work->contacts[i].key.value, work->playerDistance, 0, 0);
                work->hitCooldown = damageGetPlayerAttackHitCooldown(work->contacts[i].key.value);
                work->hitDamage   = damage;
                worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                enemy->hp -= damage;
                if (enemy->hp <= 0) {
                    enemy->hp = 0;
                }
                if (damage > 0) {
                    if (work->hitMarkCount < 8) {
                        obj = arg0->extra.tmd->coords;
                        work->hitMarkCount++;
                        pan = (s8)worldCoordGetOriginAudioPan(obj);
                        sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_TARGET_HIT, pan, (s8)worldCoordGetOriginAudioDepth(obj));
                    }
                    if (damage >= 0x14) {
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
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Corner offsets of the quad `func_actor_107600_80133FA8` draws.
static const DVECTOR D_actor_107600_80131ED8[] = {
    { -0x100, -0x100 },
    { -0x100, 0x100 },
    { 0x100, -0x100 },
    { 0x100, 0x100 },
};

/// Same as `func_actor_107600_80134248` with a 0x200-wide square, UVs
/// 0x40..0x67 x 0..0x27 and a 0xA0 depth bias.
static void func_actor_107600_80133FA8(GfxCoord* coord, SVECTOR* pos)
{
    _Actor107600QuadScratch* s;
    POLY_FT4*                p;
    s32                      i;

    s = SCRATCH_STACK_RESERVE_BLOCK(_Actor107600QuadScratch);
    for (i = 0; i < 4; i++) {
        s->vertices[i].vx = pos->vx + (D_actor_107600_80131ED8[i].vx + coord->coord.t[0]);
        s->vertices[i].vy = pos->vy + (D_actor_107600_80131ED8[i].vy + coord->coord.t[1]);
        s->vertices[i].vz = coord->coord.t[2] + pos->vz;
    }
    gte_SetRotMatrix(&coord->workm);
    gte_SetTransMatrix(&coord->workm);
    gte_ldv0(&s->vertices[0]);
    gte_rtps();
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setPolyFT4(p);
    gte_stsxy(&s->screenCorners[0]);
    gte_ldv3(&s->vertices[1], &s->vertices[2], &s->vertices[3]);
    gte_rtpt();
    p->tpage = 0x99;
    p->clut  = 0x3E80;
    setUV4(p, 0x40, 0, 0x67, 0, 0x40, 0x27, 0x67, 0x27);
    setShadeTex(p, 1);
    gte_stsxy3(&s->screenCorners[1], &s->screenCorners[2], &s->screenCorners[3]);
    gte_stszotz(&s->depth);
    s->depth -= 0xA0;
    if (s->depth < 0x40) {
        SCRATCH_STACK_RELEASE_BLOCK(_Actor107600QuadScratch);
        return;
    }
    p->x0 = s->screenCorners[0];
    p->y0 = s->screenCorners[0] >> 16;
    p->x1 = s->screenCorners[1];
    p->y1 = s->screenCorners[1] >> 16;
    p->x2 = s->screenCorners[2];
    p->y2 = s->screenCorners[2] >> 16;
    p->x3 = s->screenCorners[3];
    p->y3 = s->screenCorners[3] >> 16;
    addPrim(&gGpuCurrentOt[s->depth >> 4], p);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor107600QuadScratch);
}

/// Corner offsets of the quad `func_actor_107600_80134248` draws; the
/// zero fifth entry is never read.
static const DVECTOR D_actor_107600_80131EE8[] = {
    { -0x60, -0x60 },
    { -0x60, 0x60 },
    { 0x60, -0x60 },
    { 0x60, 0x60 },
    { 0, 0 },
};

/// Projects a 0xC0-wide square centred on `pos` (relative to `coord`'s
/// translation) and links it as an unshaded `POLY_FT4` on tpage 0x99,
/// dropping it when its OT depth lands too close.
static void func_actor_107600_80134248(GfxCoord* coord, SVECTOR* pos)
{
    _Actor107600QuadScratch* s;
    POLY_FT4*                p;
    s32                      i;

    s = SCRATCH_STACK_RESERVE_BLOCK(_Actor107600QuadScratch);
    for (i = 0; i < 4; i++) {
        s->vertices[i].vx = pos->vx + (D_actor_107600_80131EE8[i].vx + coord->coord.t[0]);
        s->vertices[i].vy = pos->vy + (D_actor_107600_80131EE8[i].vy + coord->coord.t[1]);
        s->vertices[i].vz = coord->coord.t[2] + pos->vz;
    }
    gte_SetRotMatrix(&coord->workm);
    gte_SetTransMatrix(&coord->workm);
    gte_ldv0(&s->vertices[0]);
    gte_rtps();
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setPolyFT4(p);
    gte_stsxy(&s->screenCorners[0]);
    gte_ldv3(&s->vertices[1], &s->vertices[2], &s->vertices[3]);
    gte_rtpt();
    p->tpage = 0x99;
    p->clut  = 0x3E80;
    setUV4(p, 0x68, 0, 0x77, 0, 0x68, 0xF, 0x77, 0xF);
    setShadeTex(p, 1);
    gte_stsxy3(&s->screenCorners[1], &s->screenCorners[2], &s->screenCorners[3]);
    gte_stszotz(&s->depth);
    s->depth -= 0x40;
    if (s->depth < 0x40) {
        SCRATCH_STACK_RELEASE_BLOCK(_Actor107600QuadScratch);
        return;
    }
    p->x0 = s->screenCorners[0];
    p->y0 = s->screenCorners[0] >> 16;
    p->x1 = s->screenCorners[1];
    p->y1 = s->screenCorners[1] >> 16;
    p->x2 = s->screenCorners[2];
    p->y2 = s->screenCorners[2] >> 16;
    p->x3 = s->screenCorners[3];
    p->y3 = s->screenCorners[3] >> 16;
    addPrim(&gGpuCurrentOt[s->depth >> 4], p);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor107600QuadScratch);
}

/// Weighted mode collapses each column of `m` to one value plus a
/// `gDisplayState.loopCount`-driven sine pulse; black clears the 3x3 part.
static void func_actor_107600_801344E8(void* arg0, MATRIX* m, s32 mode)
{
    s32 i;
    s16 v;

    switch (mode) {
        case ENEMY_COLOR_DEFAULT:
            break;
        case ENEMY_COLOR_WEIGHTED:
            for (i = 0; i < 3; i++) {
                v          = (m->m[0][i] * 7 + m->m[1][i] * 6 + m->m[2][i] * 3) / 33;
                v         += (s16)(rsin(gDisplayState.loopCount * 198) + 0x1000);
                m->m[0][i] = v;
                m->m[1][i] = v;
                m->m[2][i] = v;
            }
            break;
        case ENEMY_COLOR_BLACK:
            m->m[0][0] = 0;
            m->m[0][1] = 0;
            m->m[0][2] = 0;
            m->m[1][0] = 0;
            m->m[1][1] = 0;
            m->m[1][2] = 0;
            m->m[2][0] = 0;
            m->m[2][1] = 0;
            m->m[2][2] = 0;
            break;
    }
}

/// Recolours the model's colour matrix from the `colorMode` pair, blending
/// the two remaps by `colorBlend` while it counts down; a copy of
/// `worldCoordUpdateActorColor`.
static void func_actor_107600_80134608(Enemy* arg0, VECTOR* arg1, s32 arg2, s32 arg3)
{
    TmdObject*                   extra;
    MATRIX*                      colorMtx;
    s32                          mode;
    WorldCoordActorColorScratch* block;
    s32                          i;
    s32                          w0;
    s32                          w1;

    extra    = arg0->task->extra.tmd;
    colorMtx = extra->colorMtx;
    mode     = arg0->colorMode & ENEMY_COLOR_MODE_MASK;
    if ((!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && (extra->buffer != NULL)) || (gGameSession->sceneUpdatesPaused != 1)) {
        block = SCRATCH_STACK_RESERVE_BLOCK(WorldCoordActorColorScratch);
        worldCoordSetModelLighting(extra, arg1, 0, 3);
        if ((s8)arg0->colorBlend <= 0) {
            func_actor_107600_801344E8(arg0, colorMtx, mode);
        } else {
            block->previousColor.m[0][0] = colorMtx->m[0][0];
            block->previousColor.m[0][1] = colorMtx->m[0][1];
            block->previousColor.m[0][2] = colorMtx->m[0][2];
            block->previousColor.m[1][0] = colorMtx->m[1][0];
            block->previousColor.m[1][1] = colorMtx->m[1][1];
            block->previousColor.m[1][2] = colorMtx->m[1][2];
            block->previousColor.m[2][0] = colorMtx->m[2][0];
            block->previousColor.m[2][1] = colorMtx->m[2][1];
            block->previousColor.m[2][2] = colorMtx->m[2][2];
            func_actor_107600_801344E8(arg0, colorMtx, mode);
            func_actor_107600_801344E8(arg0, &block->previousColor, (arg0->colorMode >> ENEMY_COLOR_PREVIOUS_SHIFT) & ENEMY_COLOR_MODE_MASK);
            w0 = (s8)arg0->colorBlend << 8;
            w1 = 0x1000 - w0;
            for (i = 0; i < 3; i++) {
                block->currentColumn.vx  = colorMtx->m[0][i];
                block->currentColumn.vy  = colorMtx->m[1][i];
                block->currentColumn.vz  = colorMtx->m[2][i];
                block->previousColumn.vx = block->previousColor.m[0][i];
                block->previousColumn.vy = block->previousColor.m[1][i];
                block->previousColumn.vz = block->previousColor.m[2][i];
                gte_lddp(w1);
                gte_ldsv(&block->currentColumn);
                gte_gpf12();
                gte_lddp(w0);
                gte_ldsv(&block->previousColumn);
                gte_gpl12();
                gte_stsv(&block->currentColumn);
                colorMtx->m[0][i] = block->currentColumn.vx;
                colorMtx->m[1][i] = block->currentColumn.vy;
                colorMtx->m[2][i] = block->currentColumn.vz;
            }
            if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                arg0->colorBlend--;
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(WorldCoordActorColorScratch);
    }
}

void func_actor_107600_801348A0(Task* arg0)
{
    TaskFuncTable4 handlers;

    handlers = D_actor_107600_80131E74;
    handlers.funcs[arg0->state](arg0);
}

static void func_actor_107600_80134904(Task* arg0)
{
    TmdObject* obj = arg0->extra.tmd;

    obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
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

/// Copies the world position of the model's first attach coordinate onto a
/// 0x10-byte `VECTOR` carved off the scratch stack and hands it to
/// `func_actor_107600_80134608` with no blend parameters.
static void func_actor_107600_801349E0(Task* arg0)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;
    void*     obj;

    obj                            = arg0->spawnArg2.pointer;
    coord                          = arg0->extra.tmd->coords;
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    func_actor_107600_80134608(obj, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
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

/// Applies the transition `work->hitReaction` queues once `hitTaken` is 1:
/// requests 1..5 open states 2, 3, 4, 6 and 5 through the same stores as
/// `_actor107600SetTargetState` (written out, since the setter is not
/// inlined). The request is always consumed; returns whether one was pending.
static s32 func_actor_107600_80134BAC(Task* arg0)
{
    _Actor107600TargetWork* work = arg0->work;

    if (work->hitTaken == 1) {
        switch ((s16)(work->hitReaction - 1)) {
            case 0: {
                _Actor107600TargetWork* w = arg0->work;

                w->state = ACTOR_107600_TARGET_STATE_LIGHT_FLINCH;
                w->step  = 0;
                break;
            }
            case 1: {
                _Actor107600TargetWork* w = arg0->work;

                w->state = ACTOR_107600_TARGET_STATE_HEAVY_FLINCH;
                w->step  = 0;
                break;
            }
            case 2: {
                _Actor107600TargetWork* w = arg0->work;

                w->state = 4;
                w->step  = 0;
                break;
            }
            case 3: {
                _Actor107600TargetWork* w = arg0->work;

                w->state = 6;
                w->step  = 0;
                break;
            }
            case 4: {
                _Actor107600TargetWork* w = arg0->work;

                w->state = 5;
                w->step  = 0;
                break;
            }
        }
        work->hitReaction = ACTOR_107600_HIT_REACTION_NONE;
        return 1;
    }
    return 0;
}

/// First entry of the `D_actor_107600_80131E84` state table. Targets of
/// path-following mounts start `ACTOR_107600_TARGET_STATE_ACTIVE`, lie flat
/// with `pitch` at 0x400,
/// roll `gRandomLcgState` into `hitMarkFirst` beside the 10 percent scale pair
/// `func_actor_107600_80134EF4` divides the model root's rotation by, rebuild
/// that rotation through `_actor107600UpdateTargetRotation`, and put the spawned
/// object's light into mode 2 with its blend timer cleared. The target of an
/// `ACTOR_107600_MOUNT_FIXED` mount only starts
/// `ACTOR_107600_TARGET_STATE_FIXED` and leaves the scale pair at 100 percent.
static void func_actor_107600_80134C54(Task* arg0)
{
    _Actor107600TargetWork* work = arg0->work;
    Enemy*                  obj  = arg0->spawnArg2.pointer;

    switch (work->mountBehaviour) {
        case ACTOR_107600_MOUNT_STANDING:
        case ACTOR_107600_MOUNT_HANGING:
            work->state         = ACTOR_107600_TARGET_STATE_ACTIVE;
            work->pitch         = 0x400;
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->hitMarkFirst  = (gRandomLcgState >> 16) & 7;
            work->widthPercent  = 10;
            work->heightPercent = 10;
            _actor107600UpdateTargetRotation(arg0);
            worldCoordSetActorColorMode(obj, ENEMY_COLOR_BLACK);
            obj->colorBlend = 0;
            break;
        case ACTOR_107600_MOUNT_FIXED:
            work->state         = ACTOR_107600_TARGET_STATE_FIXED;
            work->widthPercent  = 100;
            work->heightPercent = 100;
            break;
    }
}

static void func_actor_107600_80134D10(Task* arg0)
{
    _actor107600SetTargetState(arg0, ACTOR_107600_TARGET_STATE_ACTIVE);
}

static void func_actor_107600_80134D30(Task* arg0)
{
    _actor107600SetTargetState(arg0, ACTOR_107600_TARGET_STATE_ACTIVE);
}

static void func_actor_107600_80134D50(Task* arg0)
{
    _actor107600SetTargetState(arg0, ACTOR_107600_TARGET_STATE_ACTIVE);
}

static void func_actor_107600_80134D70(Task* arg0)
{
    ((_Actor107600TargetWork*)arg0->work)->hitMarkCount = 3;
    _actor107600SetTargetState(arg0, ACTOR_107600_TARGET_STATE_LEAVE);
}

/// Measures the XZ offset from this model's own attach coordinate to the one on
/// the `gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]` actor's model, in a 0x10-byte `VECTOR` carved off
/// the scratch stack the way `_actor107600PlaceTargetOffset` carves its block, and
/// leaves the distance in `_Actor107600TargetWork::playerDistance`. With no slot-0 actor the
/// carve is undone and nothing is measured. The distance is only stored once the
/// scratch block has been handed back, which is the order the original compiled
/// in - moving the store up costs a nop after the reload.
static void func_actor_107600_80134D9C(Task* arg0)
{
    _Actor107600TargetWork* work;
    GfxCoord*               self;
    GfxCoord*               target;
    void**                  scratch;
    u8*                     head;
    VECTOR*                 block;
    s32                     dist;

    work                           = arg0->work;
    self                           = arg0->extra.tmd->coords;
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    SCRATCH_HEAD_AT(scratch, void) = block;
    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] == NULL) {
        SCRATCH_HEAD_AT(scratch, void) = head;
        return;
    }
    target    = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    block->vx = target->coord.t[0] - self->coord.t[0];
    block->vy = target->coord.t[1] - self->coord.t[1];
    block->vz = target->coord.t[2] - self->coord.t[2];
    dist      = playerActorPlanarLength(block->vx, block->vz);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
    work->playerDistance = dist;
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

/// Scales the model root's rotation by `widthPercent` and `heightPercent`: the
/// `coord.m[0][0]` and `coord.m[2][1]` halves, each read as a raw 16-bit value
/// and re-signed before the divide so the scale stays signed.
static void func_actor_107600_80134EF4(Task* arg0)
{
    _Actor107600TargetWork* work  = arg0->work;
    GfxCoord*               coord = arg0->extra.tmd->coords;
    u16                     x     = coord->coord.m[0][0];
    u16                     y     = coord->coord.m[2][1];

    coord->coord.m[0][0] = (s16)x / 100 * work->widthPercent;
    coord->coord.m[2][1] = (s16)y / 100 * work->heightPercent;
}
