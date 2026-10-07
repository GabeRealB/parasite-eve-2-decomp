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
static void func_actor_107600_80132A7C(Task* arg0);
static void func_actor_107600_80132AC0(Task* arg0);
static void func_actor_107600_80132B0C(Task* arg0);
static void func_actor_107600_80132B7C(Task* arg0);
static void func_actor_107600_80132C4C(MATRIX* src, MATRIX* dst);
static void func_actor_107600_80132CB8(Task* arg0);
static void func_actor_107600_80132CD4(Task* arg0);
static void func_actor_107600_80132D54(Task* arg0);
static void func_actor_107600_80132DF0(Enemy* arg0, s32 arg1, s32 arg2);
static void func_actor_107600_80132ED0(Task* arg0);
static void func_actor_107600_80133024(Task* arg0);
static void func_actor_107600_801332D4(Task* arg0);
static void func_actor_107600_80133668(Task* arg0);
static void func_actor_107600_801337FC(Task* arg0);
static void func_actor_107600_801339A4(Task* arg0);
static void func_actor_107600_80133FA8(GfxCoord* arg0, SVECTOR* arg1);
static void func_actor_107600_80134248(GfxCoord* arg0, SVECTOR* arg1);
static void func_actor_107600_80134608(struct Enemy* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void        func_actor_107600_801348A0(Task* arg0);
static void func_actor_107600_80134904(Task* arg0);
static void func_actor_107600_80134920(Task* arg0);
static void func_actor_107600_80134958(Task* arg0);
static void func_actor_107600_801349E0(Task* arg0);
static void func_actor_107600_80134A50(Task* arg0);
static void func_actor_107600_80134B2C(MATRIX* src, MATRIX* dst);
static void func_actor_107600_80134B98(Task* arg0, s16 arg1);
static s32  func_actor_107600_80134BAC(Task* arg0);
static void func_actor_107600_80134C54(Task* arg0);
static void func_actor_107600_80134D10(Task* arg0);
static void func_actor_107600_80134D30(Task* arg0);
static void func_actor_107600_80134D50(Task* arg0);
static void func_actor_107600_80134D70(Task* arg0);
static void func_actor_107600_80134D9C(Task* arg0);
static void func_actor_107600_80134E5C(GfxCoord* arg0);
static void func_actor_107600_80134EF4(Task* arg0);

/* Waypoint paths `func_actor_107600_80132160` walks, indexed by
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
static void func_actor_107600_80132160(Task* arg0);
static void func_actor_107600_80132514(Task* arg0);
static void func_actor_107600_80132930(Task* arg0);
static void func_actor_107600_80133DC4(Task* arg0);

/// The actor's top-level task states, which `func_actor_107600_801328CC` runs:
/// spawn, update, drop and destroy.
static const TaskFuncTable4 D_actor_107600_80131E24 = { {
    func_actor_107600_80131F10,
    func_actor_107600_80132930,
    func_actor_107600_80132A7C,
    func_actor_107600_80132AC0,
} };

/// One entry per `_Actor107600MountWork::behaviour`, run by
/// `func_actor_107600_80132CD4`.
static const TaskFuncTable3 D_actor_107600_80131E34 = { {
    func_actor_107600_80132160,
    func_actor_107600_80132514,
    func_actor_107600_80132D54,
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
    arg0->exitCallback = func_actor_107600_80132AC0;
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

/// Path-following behaviour of a standing mount, stepped through
/// `ACTOR_107600_MOUNT_STEP_*`: grows `heightPercent` to 100, bobs the model
/// root for four frames, then once the first child raises bit 0x20 steps the
/// root along path `path` of `D_actor_107600_80135624` one waypoint at a time. A path whose first waypoint
/// has no speed holds for `30 *` the spawn nibble instead; the
/// `ACTOR_107600_PATH_END` record, the countdown or the child's bit 0x40 stops
/// the path, and bit 0x80 then shrinks the scale back to 0 and advances the
/// task state.
static void func_actor_107600_80132160(Task* arg0)
{
    _Actor107600MountWork* work  = arg0->work;
    Enemy*                 enemy = arg0->spawnArg2.pointer;
    GfxCoord*              coord = arg0->extra.tmd->coords;
    _Actor107600Waypoint*  wp;
    s32                    d;

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
                coord->coord.t[1] = -0x10;
                return;
            }
            coord->coord.t[1] = 0;
            if (work->timer >= 4) {
                work->step++;
                enemy->task->firstChild->spawnArg1.value |= 0x10;
            }
            return;
        case ACTOR_107600_MOUNT_STEP_WAIT_TARGET:
            if (!(enemy->task->firstChild->spawnArg1.value & 0x20)) {
                return;
            }
            wp  = D_actor_107600_80135624[work->path];
            wp += work->waypoint;
            if (arg0->spawnArg1.value & 0x10000000) {
                work->rotating = 1;
            }
            if (wp->speed == 0) {
                work->step  = ACTOR_107600_MOUNT_STEP_HOLD;
                work->timer = (((u32)arg0->spawnArg1.value >> 24) & 0xF) * 30;
                return;
            }
            work->step++;
        case ACTOR_107600_MOUNT_STEP_TRAVEL:
            wp  = D_actor_107600_80135624[work->path];
            wp += work->waypoint;
            if (wp->x == ACTOR_107600_PATH_END) {
            stop:
                work->step                                = ACTOR_107600_MOUNT_STEP_LEAVE;
                work->rotating                            = 0;
                enemy->task->firstChild->spawnArg1.value |= 0x40;
                return;
            }
            d = (s16)(coord->coord.t[0] - wp->x);
            if (d != 0) {
                if (wp->speed >= abs(d)) {
                    if (enemy->task->firstChild->spawnArg1.value & 0x40) {
                        work->step                                = ACTOR_107600_MOUNT_STEP_LEAVE;
                        work->rotating                            = 0;
                        enemy->task->firstChild->spawnArg1.value |= 0x40;
                        return;
                    }
                    coord->coord.t[0] = wp->x;
                    work->waypoint++;
                } else if (d < 0) {
                    coord->coord.t[0] += wp->speed;
                } else {
                    coord->coord.t[0] -= wp->speed;
                }
            }
            d = (s16)(coord->coord.t[2] - wp->z);
            if (d != 0) {
                if (wp->speed >= abs(d)) {
                    if (enemy->task->firstChild->spawnArg1.value & 0x40) {
                        work->step                                = ACTOR_107600_MOUNT_STEP_LEAVE;
                        work->rotating                            = 0;
                        enemy->task->firstChild->spawnArg1.value |= 0x40;
                        return;
                    }
                    coord->coord.t[2] = wp->z;
                    work->waypoint++;
                } else if (d < 0) {
                    coord->coord.t[2] += wp->speed;
                } else {
                    coord->coord.t[2] -= wp->speed;
                }
            }
            return;
        case ACTOR_107600_MOUNT_STEP_HOLD:
            if (work->timer != 0 && --work->timer <= 0) {
                goto stop;
            }
        case ACTOR_107600_MOUNT_STEP_LEAVE:
            if (enemy->task->firstChild->spawnArg1.value & 0x80) {
                if (work->heightPercent > 0) {
                    work->heightPercent -= 8;
                    return;
                }
                work->heightPercent = 0;
                arg0->state++;
            }
            break;
    }
}

/// Twin of `func_actor_107600_80132160` that bobs the model root between
/// -0xF4C and -0xF3C instead of -0x10 and 0.
static void func_actor_107600_80132514(Task* arg0)
{
    _Actor107600MountWork* work  = arg0->work;
    Enemy*                 enemy = arg0->spawnArg2.pointer;
    GfxCoord*              coord = arg0->extra.tmd->coords;
    _Actor107600Waypoint*  wp;
    s32                    d;

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
                coord->coord.t[1] = -0xF4C;
                return;
            }
            coord->coord.t[1] = -0xF3C;
            if (work->timer >= 4) {
                work->step++;
                enemy->task->firstChild->spawnArg1.value |= 0x10;
            }
            return;
        case ACTOR_107600_MOUNT_STEP_WAIT_TARGET:
            if (!(enemy->task->firstChild->spawnArg1.value & 0x20)) {
                return;
            }
            wp  = D_actor_107600_80135624[work->path];
            wp += work->waypoint;
            if (arg0->spawnArg1.value & 0x10000000) {
                work->rotating = 1;
            }
            if (wp->speed == 0) {
                work->step  = ACTOR_107600_MOUNT_STEP_HOLD;
                work->timer = (((u32)arg0->spawnArg1.value >> 24) & 0xF) * 30;
                return;
            }
            work->step++;
        case ACTOR_107600_MOUNT_STEP_TRAVEL:
            wp  = D_actor_107600_80135624[work->path];
            wp += work->waypoint;
            if (wp->x == ACTOR_107600_PATH_END) {
            stop:
                work->step                                = ACTOR_107600_MOUNT_STEP_LEAVE;
                work->rotating                            = 0;
                enemy->task->firstChild->spawnArg1.value |= 0x40;
                return;
            }
            d = (s16)(coord->coord.t[0] - wp->x);
            if (d != 0) {
                if (wp->speed >= abs(d)) {
                    if (enemy->task->firstChild->spawnArg1.value & 0x40) {
                        work->step                                = ACTOR_107600_MOUNT_STEP_LEAVE;
                        work->rotating                            = 0;
                        enemy->task->firstChild->spawnArg1.value |= 0x40;
                        return;
                    }
                    coord->coord.t[0] = wp->x;
                    work->waypoint++;
                } else if (d < 0) {
                    coord->coord.t[0] += wp->speed;
                } else {
                    coord->coord.t[0] -= wp->speed;
                }
            }
            d = (s16)(coord->coord.t[2] - wp->z);
            if (d != 0) {
                if (wp->speed >= abs(d)) {
                    if (enemy->task->firstChild->spawnArg1.value & 0x40) {
                        work->step                                = ACTOR_107600_MOUNT_STEP_LEAVE;
                        work->rotating                            = 0;
                        enemy->task->firstChild->spawnArg1.value |= 0x40;
                        return;
                    }
                    coord->coord.t[2] = wp->z;
                    work->waypoint++;
                } else if (d < 0) {
                    coord->coord.t[2] += wp->speed;
                } else {
                    coord->coord.t[2] -= wp->speed;
                }
            }
            return;
        case ACTOR_107600_MOUNT_STEP_HOLD:
            if (work->timer != 0 && --work->timer <= 0) {
                goto stop;
            }
        case ACTOR_107600_MOUNT_STEP_LEAVE:
            if (enemy->task->firstChild->spawnArg1.value & 0x80) {
                if (work->heightPercent > 0) {
                    work->heightPercent -= 8;
                    return;
                }
                work->heightPercent = 0;
                arg0->state++;
            }
            break;
    }
}

/// Task states run by `func_actor_107600_801348A0`, indexed by its
/// `Task::state`.
static const TaskFuncTable4 D_actor_107600_80131E74 = { {
    func_actor_107600_80132ED0,
    func_actor_107600_80133024,
    func_actor_107600_80134904,
    func_actor_107600_80134920,
} };

/// Behaviour states indexed by `_Actor107600TargetWork::state`, run by
/// `func_actor_107600_80133024`.
static const TaskFuncTable10 D_actor_107600_80131E84 = { {
    func_actor_107600_80134C54,
    func_actor_107600_801332D4,
    func_actor_107600_80133668,
    func_actor_107600_80133668,
    func_actor_107600_80134D10,
    func_actor_107600_80134D30,
    func_actor_107600_80134D50,
    func_actor_107600_801337FC,
    func_actor_107600_80134D70,
    func_actor_107600_801339A4,
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

/// State 2 of the `D_actor_107600_80131E24` table: drops this instance from the
/// spawning gallery's live-target count unless it was never counted (an
/// `ACTOR_107600_MOUNT_FIXED` mount, the `0x200D` target), then advances to the
/// exit state.
static void func_actor_107600_80132A7C(Task* arg0)
{
    Task*                  parent;
    _Actor107600MountWork* work;

    parent = arg0->parent;
    work   = arg0->work;
    if (work->behaviour != ACTOR_107600_MOUNT_FIXED) {
        ((MistShootingGalleryWork*)parent->work)->liveTargets--;
    }
    arg0->state++;
}

/// Enemy exit callback: releases the shared state slot unless the mount is an
/// `ACTOR_107600_MOUNT_FIXED` one, which never took it, then hands the enemy
/// back for destruction.
static void func_actor_107600_80132AC0(Task* arg0)
{
    _Actor107600MountWork* work = arg0->work;

    if (work->behaviour != ACTOR_107600_MOUNT_FIXED) {
        sceneReleaseBattleRefWithRewards(arg0, 0);
    }
    enemyDestroy(arg0->spawnArg2.pointer, arg0);
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
/// The copy is a call to `func_actor_107600_80132C4C`.
static void func_actor_107600_80132B7C(Task* arg0)
{
    _Actor107600MountWork* work  = arg0->work;
    GfxCoord*              coord = arg0->extra.tmd->coords;
    MATRIX*                m;

    work->pitch                 &= 0xFFF;
    work->yaw                   &= 0xFFF;
    work->roll                  &= 0xFFF;
    m                            = (MATRIX*)(SCRATCH_STACK_CURSOR(u8) - 0x20);
    MATRIX_PAIR(m, 0, 0)         = 0x1000;
    MATRIX_PAIR(m, 0, 2)         = 0;
    MATRIX_PAIR(m, 1, 1)         = 0x1000;
    MATRIX_PAIR(m, 2, 0)         = 0;
    m->m[2][2]                   = 0x1000;
    SCRATCH_STACK_CURSOR(MATRIX) = m;
    RotMatrixZ(work->roll, m);
    RotMatrixX(work->pitch, m);
    RotMatrixY(work->yaw, m);
    func_actor_107600_80132C4C(m, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Copies the 3x3 rotation of the scratch matrix `func_actor_107600_80132B7C`
/// just built into the part's `GfxCoord::coord`, leaving the translation
/// row of the destination alone.
static void func_actor_107600_80132C4C(MATRIX* src, MATRIX* dst)
{
    dst->m[0][0] = src->m[0][0];
    dst->m[0][1] = src->m[0][1];
    dst->m[0][2] = src->m[0][2];
    dst->m[1][0] = src->m[1][0];
    dst->m[1][1] = src->m[1][1];
    dst->m[1][2] = src->m[1][2];
    dst->m[2][0] = src->m[2][0];
    dst->m[2][1] = src->m[2][1];
    dst->m[2][2] = src->m[2][2];
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

/// Behaviour of an `ACTOR_107600_MOUNT_FIXED` mount, which waits for its first
/// child to raise bit 0x80 of `Task::spawnArg1`: the first pass zeroes the model
/// root's Y translation and sets `heightPercent` to 100, then each frame with
/// the bit set shrinks it by 8 until it reaches 0 and the task state advances.
static void func_actor_107600_80132D54(Task* arg0)
{
    _Actor107600MountWork* work  = arg0->work;
    GfxCoord*              coord = arg0->extra.tmd->coords;
    Enemy*                 enemy = arg0->spawnArg2.pointer;

    switch (work->step) {
        case ACTOR_107600_MOUNT_FIXED_STEP_PLACE:
            work->step++;
            work->heightPercent = 100;
            coord->coord.t[1]   = 0;
        case ACTOR_107600_MOUNT_FIXED_STEP_LEAVE:
            if (enemy->task->firstChild->spawnArg1.value & 0x80) {
                if (work->heightPercent > 0) {
                    work->heightPercent -= 8;
                    return;
                }
                work->heightPercent = 0;
                arg0->state++;
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

    enemy = Gp_SpawnEnemyFromTable(D_actor_107600_80134F94, arg1 + 1, arg2, arg0);
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

/// Spawn state: allocates the work block, hangs its two matrices off the
/// display object's `colorMtx` / `lightMtx`, puts the actor's own light on the
/// enemy and links its node in. Both failure paths - the 0xFF "already dead"
/// marker in byte 0 of `Task::spawnArg1` and a failed allocation - destroy the
/// enemy and return before the exit callback is installed. The low nibble of
/// `spawnArg1` selects the HP from thirteen serialized halfwords. The gallery
/// demonstration also requests index 13, just beyond the package; the runtime
/// backing of that read remains unresolved. The high halfword's low nibble is
/// the mount's behaviour, kept in `mountBehaviour`.
static void func_actor_107600_80132ED0(Task* arg0)
{
    _Actor107600TargetWork* work;
    Enemy*                  enemy;
    TmdObject*              obj;
    GfxCoord*               coord;
    u16                     hp;
    u32                     variant;

    obj     = arg0->extra.tmd;
    variant = (u8)arg0->spawnArg1.value;
    enemy   = arg0->spawnArg2.pointer;
    coord   = obj->coords;
    if (variant == 0xFF || (work = memCalloc(sizeof(_Actor107600TargetWork), false), arg0->work = work, work == NULL)) {
        enemyDestroy(enemy, arg0);
        return;
    }
    arg0->exitCallback   = func_actor_107600_80134920;
    work->mountBehaviour = ((u32)arg0->spawnArg1.value >> 16) & 0xF;
    obj->lightMtx        = &work->lightMatrix;
    obj->colorMtx        = &work->colorMatrix;
    enemy->param         = &D_actor_107600_80135720;
    enemy->recs          = work->contacts;
    work->field_140      = arg0->extra.tmd->coords;
    work->field_144      = 0x140;
    work->field_146      = 2;
    hp                   = D_actor_107600_80135750[arg0->spawnArg1.value & 0xF];
    enemy->hpMax         = hp;
    enemy->hp            = hp;
    func_actor_107600_80134958(arg0);
    worldTargetLinkNode(&enemy->node);
    enemy->field_4                = &coord->workm;
    enemy->bodyPos.vy             = -0x244;
    enemy->field_48               = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->coord                  = coord;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    func_actor_107600_80134E5C(coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    arg0->state += 1;
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
                    func_actor_107600_80134B98(arg0, ACTOR_107600_TARGET_STATE_DESTROYED);
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
    func_actor_107600_80134A50(arg0);
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
                func_actor_107600_80134B98(arg0, ACTOR_107600_TARGET_STATE_LEAVE);
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

/// The two flinch states, stepped through `step`: swings `pitch` for six
/// frames with a step scaled by `hitDamage` (capped at 0x200), then flickers it
/// on odd frames and hands off to step 3 of `ACTOR_107600_TARGET_STATE_ACTIVE`.
static void func_actor_107600_80133668(Task* arg0)
{
    _Actor107600TargetWork* work = arg0->work;
    s32                     step;
    s16                     count;

    switch (work->step) {
        case 0:
            work->step++;
            work->timer = 6;
        case 1:
            step = work->hitDamage * 6;
            if (step > 0x200) {
                step = 0x200;
            }
            count = work->timer;
            step /= 3;
            if (count >= 4) {
                if (work->pitch < 0x200) {
                    work->pitch += step - step / 3 * (6 - count);
                }
            } else if (count <= 0) {
                work->pitch = 0;
                work->timer = 0;
                work->step++;
            } else if (work->pitch > 0) {
                work->pitch -= step + step / 3 * (3 - count);
            }
            work->timer--;
            break;
        case 2:
            work->timer++;
            if (work->timer & 1) {
                work->pitch = (work->timer - 7) * 8;
                return;
            }
            work->pitch     = 0;
            work->hitDamage = 0;
            if (work->timer >= 4) {
                work->state = ACTOR_107600_TARGET_STATE_ACTIVE;
                work->step  = 3;
            }
            break;
    }
}

/// `ACTOR_107600_TARGET_STATE_LEAVE`, stepped through `step`: unlinks the enemy
/// node and waits seven frames, plays the death cue, ramps `pitch` up to 0x400
/// to lay the target flat, then shrinks `heightPercent` and `widthPercent` by
/// 0x20 until both are <= 20 and
/// raises bit 0x80 of `Task::spawnArg1`.
static void func_actor_107600_801337FC(Task* arg0)
{
    _Actor107600TargetWork* work  = arg0->work;
    Enemy*                  enemy = arg0->spawnArg2.pointer;
    GfxCoord*               obj;
    s32                     pan;

    switch (work->step) {
        case 0:
            work->step++;
            arg0->spawnArg1.value |= 0x40;
            work->timer            = 7;
            worldTargetUnlinkNode(&enemy->node);
            enemy->recs       = 0;
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        case 1:
            work->timer--;
            if (work->timer <= 0) {
                obj = arg0->extra.tmd->coords;
                work->step++;
                work->hitMarkCount = 0;
                work->field_15C    = 0;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                pan = (s8)worldCoordGetOriginAudioPan(obj);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_TARGET_DEATH, pan, (s8)worldCoordGetOriginAudioDepth(obj));
            }
            break;
        case 2:
            if (work->pitch < 0x400) {
                work->pitch += 0x80;
                return;
            }
            work->pitch = 0x400;
            work->step++;
        case 3:
            if (work->heightPercent > 20) {
                work->heightPercent -= 0x20;
                return;
            }
            if (work->widthPercent > 20) {
                work->widthPercent -= 0x20;
                return;
            }
            work->step++;
            arg0->spawnArg1.value |= 0x80;
            break;
        case 4:
            break;
    }
}

/// `ACTOR_107600_TARGET_STATE_DESTROYED`, stepped through `step`: bumps the
/// gallery's kill count for the target's kind, plays the kill cue, rolls random
/// spins into `pitchSpin`/`yawSpin`/`rollSpin`, detaches the model to world
/// space and turns the hit direction in `knockback` into a velocity, then tumbles and shrinks it for 16 frames before
/// advancing `Task::state` and raising bit 0x80 of `Task::spawnArg1`.
static void func_actor_107600_801339A4(Task* arg0)
{
    _Actor107600TargetWork*  work  = arg0->work;
    Enemy*                   enemy = arg0->spawnArg2.pointer;
    TmdObject*               tmd   = arg0->extra.tmd;
    GfxCoord*                obj   = tmd->coords;
    MistShootingGalleryWork* gal   = D_mist_shooting_gallery_8018E0C4->work;
    VECTOR*                  pos;
    s32                      id;
    s32                      pan;
    s16                      x;
    s16                      y;
    s16                      z;
    s32                      v;

    switch (work->step) {
        case 0:
            work->step++;
            tmd->flags            |= TMD_OBJECT_SEMI_TRANS;
            arg0->spawnArg1.value |= 0x40;
            work->hitMarkCount     = 0;
            work->field_15C        = 0;
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
            work->timer = 0;
            id          = arg0->spawnArg1.value & 0xF;
            gal->kills[id]++;
            if (id < 9) {
                id = 0x51140011;
            } else if (id == 9) {
                id = 0x51140010;
            } else {
                id = 0x51140012;
            }
            pan = (s8)worldCoordGetOriginAudioPan(obj);
            sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(obj));
            work->yaw       = (obj->parent)->param.rot.vy;
            work->roll      = (obj->parent)->param.rot.vz;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            x               = (gRandomLcgState >> 16) & 0x7F;
            work->pitchSpin = x;
            if (!((gRandomLcgState >> 16) & 1)) {
                x = -x;
            }
            work->pitchSpin = x;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            y               = (gRandomLcgState >> 16) & 0x7F;
            work->yawSpin   = y;
            if (!((gRandomLcgState >> 16) & 1)) {
                y = -y;
            }
            work->yawSpin   = y;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            z               = (gRandomLcgState >> 16) & 0x7F;
            work->rollSpin  = z;
            if (!((gRandomLcgState >> 16) & 1)) {
                z = -z;
            }
            work->rollSpin = z;
            func_actor_107600_80134B2C(&obj->parent->coord, &obj->coord);
            obj->coord.t[0] += obj->parent->coord.t[0];
            obj->coord.t[1] += obj->parent->coord.t[1];
            obj->coord.t[2] += obj->parent->coord.t[2];
            obj->parent      = &gGfxViewCoord;
            pos              = &work->knockback;
            VectorNormal(pos, pos);
            ApplyMatrixLV(&obj->coord, pos, pos);
            work->knockback.vx = 0;
            if (obj->coord.t[1] < -2000) {
                v = work->knockback.vy >> 4;
            } else {
                v = work->knockback.vy >> 2;
            }
            work->knockback.vy = v = -v;
            work->knockback.vz     = 0;
            if (v < -220) {
                work->knockback.vy = -220;
            }
            worldTargetUnlinkNode(&enemy->node);
            enemy->recs       = 0;
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        case 1:
            work->timer++;
            if (work->timer < 0x10) {
                work->knockback.vx -= work->knockback.vx >> 4;
                work->knockback.vy -= work->knockback.vy >> 4;
                work->knockback.vz -= work->knockback.vz >> 4;
                work->pitchSpin    -= work->pitchSpin >> 6;
                work->pitch        += work->pitchSpin;
                work->yawSpin      -= work->yawSpin >> 6;
                work->yaw          += work->yawSpin;
                work->rollSpin     -= work->rollSpin >> 6;
                work->roll         += work->rollSpin;
                obj->coord.t[0]    += work->knockback.vx;
                obj->coord.t[1]    += work->knockback.vy;
                obj->coord.t[2]    += work->knockback.vz;
                if (obj->coord.t[1] < -0x40) {
                    obj->coord.t[1] += 0x40;
                }
                if (work->heightPercent > 20) {
                    work->heightPercent -= 0x20;
                    return;
                }
                if (work->widthPercent > 20) {
                    work->widthPercent -= 0x20;
                    return;
                }
            } else {
                arg0->state++;
                arg0->spawnArg1.value |= 0x80;
            }
            break;
        case 2:
            break;
    }
}

/// Hit handler: for each contact of category 2, stores the shot's direction in
/// `knockback`, applies `Gp_ComputeDamage` to the enemy's HP, starts
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
                damage            = Gp_ComputeDamage(work->contacts[i].key.value, work->playerDistance, 0, 0);
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

static void func_actor_107600_80134920(Task* arg0)
{
    worldCollisionUnlinkBody(&((_Actor107600TargetWork*)arg0->work)->body);
    enemyDestroy(arg0->spawnArg2.pointer, arg0);
}

/// Links this actor's display node the way `func_8010C980` does for the
/// gameplay objects: the node's collision table is the `WorldCollisionContact` run at
/// `work->contacts`, and its radius is 0x220 on an
/// `ACTOR_107600_MOUNT_HANGING` mount and 0x190 otherwise.
static void func_actor_107600_80134958(Task* arg0)
{
    _Actor107600TargetWork* work  = arg0->work;
    GfxCoord*               coord = arg0->extra.tmd->coords;
    WorldCollisionContact*  rec   = work->contacts;

    work->body.coord            = coord;
    work->body.context.contacts = rec;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = -0x250;
    work->body.pos.vz           = 0;
    work->body.key              = 0x3004C;
    work->body.radius           = (work->mountBehaviour == ACTOR_107600_MOUNT_HANGING) ? 0x220 : 0x190;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(rec, ARRAY_SIZE(work->contacts), 0);
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

/// Builds the target's rotation from its `pitch`, `yaw` and `roll`: wrap
/// each to 12 bits, lay an unscaled `MATRIX` down at the scratchpad head, apply
/// `gfxRotMatrixZ`, `gfxRotMatrixX` and `gfxRotMatrixY` in that order, copy its 3x3 into the model's
/// `GfxCoord::coord` through `func_actor_107600_80134B2C`, and hand the
/// scratch block back.
static void func_actor_107600_80134A50(Task* arg0)
{
    _Actor107600TargetWork* work  = arg0->work;
    GfxCoord*               coord = arg0->extra.tmd->coords;
    MATRIX*                 m;

    work->pitch                 &= 0xFFF;
    work->yaw                   &= 0xFFF;
    work->roll                  &= 0xFFF;
    m                            = (MATRIX*)(SCRATCH_STACK_CURSOR(u8) - 0x20);
    MATRIX_PAIR(m, 0, 0)         = 0x1000;
    MATRIX_PAIR(m, 0, 2)         = 0;
    MATRIX_PAIR(m, 1, 1)         = 0x1000;
    MATRIX_PAIR(m, 2, 0)         = 0;
    m->m[2][2]                   = 0x1000;
    SCRATCH_STACK_CURSOR(MATRIX) = m;
    gfxRotMatrixZ(m, work->roll, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(m, work->pitch, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixY(m, work->yaw, 0);
    func_actor_107600_80134B2C(m, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Copies the 3x3 rotation of `src` into `dst`, leaving `dst`'s translation row
/// alone. The actor carries this body twice; the other copy is
/// `func_actor_107600_80132C4C`.
static void func_actor_107600_80134B2C(MATRIX* src, MATRIX* dst)
{
    dst->m[0][0] = src->m[0][0];
    dst->m[0][1] = src->m[0][1];
    dst->m[0][2] = src->m[0][2];
    dst->m[1][0] = src->m[1][0];
    dst->m[1][1] = src->m[1][1];
    dst->m[1][2] = src->m[1][2];
    dst->m[2][0] = src->m[2][0];
    dst->m[2][1] = src->m[2][1];
    dst->m[2][2] = src->m[2][2];
}

static void func_actor_107600_80134B98(Task* arg0, s16 arg1)
{
    _Actor107600TargetWork* work = arg0->work;

    work->state = arg1;
    work->step  = 0;
}

/// Applies the transition `work->hitReaction` queues once `hitTaken` is 1:
/// requests 1..5 open states 2, 3, 4, 6 and 5 through the same stores as
/// `func_actor_107600_80134B98` (written out, since the setter is not
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
/// that rotation through `func_actor_107600_80134A50`, and put the spawned
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
            func_actor_107600_80134A50(arg0);
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
    func_actor_107600_80134B98(arg0, ACTOR_107600_TARGET_STATE_ACTIVE);
}

static void func_actor_107600_80134D30(Task* arg0)
{
    func_actor_107600_80134B98(arg0, ACTOR_107600_TARGET_STATE_ACTIVE);
}

static void func_actor_107600_80134D50(Task* arg0)
{
    func_actor_107600_80134B98(arg0, ACTOR_107600_TARGET_STATE_ACTIVE);
}

static void func_actor_107600_80134D70(Task* arg0)
{
    ((_Actor107600TargetWork*)arg0->work)->hitMarkCount = 3;
    func_actor_107600_80134B98(arg0, ACTOR_107600_TARGET_STATE_LEAVE);
}

/// Measures the XZ offset from this model's own attach coordinate to the one on
/// the `gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]` actor's model, in a 0x10-byte `VECTOR` carved off
/// the scratch stack the way `func_actor_107600_80134E5C` carves its block, and
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

/// Rotates a fixed 0x10-byte offset by the coordinate's own `coord` matrix and
/// leaves the result in that matrix's translation row. The offset is carved off
/// the scratch stack the way `func_actor_107600_80132B0C` carves its VECTOR, but
/// is filled with (0, -0x180, 0) and rotated in place by `ApplyMatrixLV`, which
/// also folds in the matrix's existing translation. `func_actor_107600_80132ED0`
/// calls this on the coordinate it then hands to `actorRenderComposeCoord`.
static void func_actor_107600_80134E5C(GfxCoord* arg0)
{
    void**  scratch;
    u8*     head;
    VECTOR* block;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    SCRATCH_HEAD_AT(scratch, void) = block;
    block->vx                      = 0;
    block->vy                      = -0x180;
    block->vz                      = 0;
    ApplyMatrixLV(&arg0->coord, block, block);
    arg0->coord.t[0] = block->vx;
    arg0->coord.t[1] = block->vy;
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
    arg0->coord.t[2] = block->vz;
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
