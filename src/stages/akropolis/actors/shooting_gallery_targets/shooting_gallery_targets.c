#include "stages/akropolis/actors/shooting_gallery_targets.h"

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

#include "stages/akropolis/rooms/mist_shooting_gallery.h"

#include "../../rooms/mist_shooting_gallery/config.h"

/* The package holds two tasks. The gallery's controller spawns a mount, which
 * counts as one of its live targets; the mount spawns the target it carries as
 * its child. The two signal each other through the target task's spawn
 * argument. */

/// Mount/target handshake bits in the child target's spawn argument: the mount raises MOUNT_READY once it has settled,
/// the target STANDING once it has unfolded; either may raise STOP, and FINISHED lets the mount shrink away.
enum {
    TARGET_SIGNAL_MOUNT_READY = 0x10,
    TARGET_SIGNAL_STANDING    = 0x20,
    TARGET_SIGNAL_STOP        = 0x40,
    TARGET_SIGNAL_FINISHED    = 0x80,
};

/// Fields of the mount's spawn argument used by its path callbacks.
enum {
    MOUNT_SPAWN_HOLD_SHIFT = 24,
    MOUNT_SPAWN_HOLD_MASK  = 0xF,
    MOUNT_SPAWN_ROTATING   = 0x10000000,
    FRAMES_PER_SECOND      = 30,
};

/// Fields of the child's spawn argument: kind, dead marker and mount behaviour.
enum {
    TARGET_SPAWN_KIND_MASK       = 0xF,
    TARGET_SPAWN_DEAD            = 0xFF,
    TARGET_SPAWN_BEHAVIOUR_SHIFT = 16,
    TARGET_SPAWN_BEHAVIOUR_MASK  = 0xF,
};

/// Wrapped Euler-angle units of the mount and target.
enum {
    ANGLE_TURN         = 0x1000,
    ANGLE_MASK         = ANGLE_TURN - 1,
    ANGLE_QUARTER_TURN = ANGLE_TURN / 4,
};

/// Steps of the target's damage flinch and the active step it resumes.
enum {
    TARGET_FLINCH_STEP_INIT   = 0,
    TARGET_FLINCH_STEP_SWING  = 1,
    TARGET_FLINCH_STEP_SETTLE = 2,
    TARGET_ACTIVE_STEP_STAND  = 3,
};

/// Steps of an undestroyed target's fold-away sequence.
enum {
    TARGET_FOLD_STEP_BEGIN  = 0,
    TARGET_FOLD_STEP_WAIT   = 1,
    TARGET_FOLD_STEP_FOLD   = 2,
    TARGET_FOLD_STEP_SHRINK = 3,
    TARGET_FOLD_STEP_DONE   = 4,
};

/// Steps of a destroyed target's tumble; step 2 is an unused inert handler.
enum {
    TARGET_TUMBLE_STEP_BEGIN   = 0,
    TARGET_TUMBLE_STEP_MOVE    = 1,
    TARGET_TUMBLE_STEP_INERT   = 2,
    TARGET_TUMBLE_FRAMES       = 16,
    TARGET_SHRINK_STEP_PERCENT = 32,
    TARGET_SHRINK_STOP_PERCENT = 20,
};

/// Kill cues selected by the target kind, kept separate from its fold-away cue.
enum {
    TARGET_KILL_SOUND_KIND_9       = MIST_SHOOTING_GALLERY_SOUND(MIST_SHOOTING_GALLERY_SOUND_TARGET_KILL_KIND_9),
    TARGET_KILL_SOUND_KINDS_0_TO_8 = MIST_SHOOTING_GALLERY_SOUND(MIST_SHOOTING_GALLERY_SOUND_TARGET_KILL),
    TARGET_KILL_SOUND_KINDS_10_UP  = MIST_SHOOTING_GALLERY_SOUND(MIST_SHOOTING_GALLERY_SOUND_TARGET_KILL_KIND_10_UP),
};

/// Height of a target's aim point above its root, where the player's lock-on looks.
enum { TARGET_AIM_Y = -0x244 };

/// The face effects' texture: target page 4 and its palette, row 248 loaded two rows down by the file's placement;
/// its `death_burst` and `hit_mark` cells; ordering-depth units and the hit-mark limit.
enum {
    TARGET_EFFECT_TPAGE     = getTPage(1, 0, 576, 256),
    TARGET_EFFECT_CLUT      = getClut(0, 250),
    DEATH_BURST_U           = 0x40,
    DEATH_BURST_SIZE        = 40,
    HIT_MARK_U              = 0x68,
    HIT_MARK_SIZE           = 16,
    TARGET_EFFECT_MIN_DEPTH = 0x40,
    TARGET_EFFECT_OT_SHIFT  = 4,
    TARGET_HIT_MARK_COUNT   = 8,
};

/// How a mount behaves, from bits 12-15 of its spawn argument: the value of `_MountWork::behaviour`, and of the copy
/// its target keeps in `_TargetWork::mountBehaviour`.
enum {
    MOUNT_STANDING = 0, // Rises at floor height and follows its path
    MOUNT_HANGING  = 1, // Turned half a revolution about Z and raised `HANGING_MOUNT_HEIGHT` above the floor; follows its path
    MOUNT_FIXED    = 2, // Stays where it is placed; not one of the gallery's live targets
};

/// Steps of a path-following mount, in `_MountWork::step`.
enum {
    MOUNT_STEP_RISE        = 0, // Grow to full height
    MOUNT_STEP_SETTLE      = 1, // Bob for four frames, then tell the target to unfold
    MOUNT_STEP_WAIT_TARGET = 2, // Wait for the target to stand, then pick travelling or holding from the path
    MOUNT_STEP_TRAVEL      = 3, // Close on the path's stops in turn
    MOUNT_STEP_HOLD        = 4, // Stay in place until the hold time runs out
    MOUNT_STEP_LEAVE       = 5, // Wait for the target to finish, then shrink away
};

/// Steps of a fixed mount, in `_MountWork::step`.
enum {
    MOUNT_FIXED_STEP_PLACE = 0, // Stand at full height on the floor
    MOUNT_FIXED_STEP_LEAVE = 1, // Wait for the target to finish, then shrink away
};

/// Work block of a gallery target's mount: the stand that rises where the gallery places it, carries its target along
/// a path and shrinks away once the target has gone.
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
    s16    step;             // Progress through `behaviour`: `MOUNT_STEP_*` on a path-following mount, `MOUNT_FIXED_STEP_*` on a fixed one
    byte   unknown_142[2];   // No field access established; role unproven
    s16    behaviour;        // `MOUNT_*`
    s16    path;             // Path the mount follows, from bits 16-23 of the spawn argument
    s16    waypoint;         // Stop of `path` the mount is closing on
    u8     rotating;         // Nonzero while the mount turns about Y: set once the target stands when bit 28 of the spawn argument asks for it, cleared when the path ends
    s8     heightPercent;    // Vertical scale of the model root, in percent: grows from 0 and shrinks from 100 in steps of 8, clamped the frame after it passes either end
} _MountWork;
STATIC_ASSERT_SIZEOF(_MountWork, 0x14C);

/// Behaviour states of a target, in `_TargetWork::state`.
enum {
    TARGET_STATE_ACTIVE       = 1, // Unfold, then stand - attacking if the spawn argument says so - until the mount stops
    TARGET_STATE_LIGHT_FLINCH = 2, // Swing back from a hit and recover
    TARGET_STATE_HEAVY_FLINCH = 3, // The same routine, for a hit of 20 damage or more
    TARGET_STATE_LEAVE        = 7, // Fold down and shrink away undestroyed
    TARGET_STATE_FIXED        = 8, // Target of a fixed mount: show three hit marks and leave
    TARGET_STATE_DESTROYED    = 9, // Out of hit points: score the kill, tumble away and shrink
};

/// State change a damaging hit asks for, in `_TargetWork::hitReaction`.
enum {
    HIT_REACTION_NONE  = 0,
    HIT_REACTION_LIGHT = 1, // Below 20 damage
    HIT_REACTION_HEAVY = 2,
};

/// Work block of a gallery target: the object the player shoots at, carried by its mount.
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
    s16                   state;          // `TARGET_STATE_*`
    s16                   step;           // Progress through `state`; cleared on entering one
    s16                   field_15C;      // Cleared when the target leaves or is destroyed. Nothing reads it; role unproven
    u16                   hitReaction;    // `HIT_REACTION_*`, consumed by the active state
    s16                   hitDamage;      // Damage of the last hit; sizes the flinch
    s16                   mountBehaviour; // `MOUNT_*` of the mount carrying the target
    byte                  unknown_164[2]; // No field access established; role unproven
    s16                   attackTimer;    // Frames since an attacking target last fired: it charges at 120 and fires at 210
    u8                    widthPercent;   // Scale of the model root across its width, in percent
    u8                    heightPercent;  // Scale of the model root along its height while it lies flat, in percent
    u8                    hitMarkFirst;   // First entry of the hit-mark offsets this target uses, rolled 0 to 7 at start
    u8                    hitMarkCount;   // Hit marks drawn on the target: one per damaging hit, at most 8
} _TargetWork;
STATIC_ASSERT_SIZEOF(_TargetWork, 0x16C);

/// `_Waypoint::x` of the record that closes a path.
enum { PATH_END = -1 };

/// One stop on a movement path of the gallery target's mount.
typedef struct {
    s16 x;     // Stop position, or `PATH_END`
    s16 z;
    s16 speed; // Distance per frame on each axis towards this stop; 0 in a path's first record holds the mount in place
} _Waypoint;
STATIC_ASSERT_SIZEOF(_Waypoint, 6);

/// Scratch-stack workspace for one textured quad drawn on a target's face: a hit mark, or the burst that replaces the
/// last one on a destroyed target.
typedef struct {
    s32     screenCorners[4]; // Projected corners, one GTE screen word each: X in the low half, Y in the high
    s32     depth;            // Last projected corner's SZ3 / 4, less the drawer's ordering bias
    SVECTOR vertices[4];      // Corners handed to the projection
} _QuadScratch;
STATIC_ASSERT_SIZEOF(_QuadScratch, 0x34);

static void _shootingGalleryTargetsMountTask(Task* task);
static void _shootingGalleryTargetsRetireMount(Task* task);
static void _shootingGalleryTargetsDestroyMount(Task* task);
static void _shootingGalleryTargetsUpdateMountColor(Task* task);
static void _shootingGalleryTargetsUpdateMountRotation(Task* task);
static void _shootingGalleryTargetsCopyMountRotation(const MATRIX* source, MATRIX* destination);
static void _shootingGalleryTargetsBeginMountBehaviour(Task* task);
static void _shootingGalleryTargetsUpdateMountBehaviour(Task* task);
static void _shootingGalleryTargetsUpdateFixedMount(Task* task);
static void _shootingGalleryTargetsSpawnTarget(Enemy* mountEnemy, s32 targetKind, s32 targetFlags);
static void _shootingGalleryTargetsInitTarget(Task* task);
static void _shootingGalleryTargetsUpdateTarget(Task* task);
static void _shootingGalleryTargetsUpdateActiveTarget(Task* task);
static void _shootingGalleryTargetsUpdateTargetFlinch(Task* task);
static void _shootingGalleryTargetsFoldTarget(Task* task);
static void _shootingGalleryTargetsTumbleDestroyedTarget(Task* task);
static void _shootingGalleryTargetsDrawTargetDeathBurst(const GfxCoord* rootCoord, const SVECTOR* centerOffset);
static void _shootingGalleryTargetsDrawTargetHitMark(const GfxCoord* rootCoord, const SVECTOR* centerOffset);
static void _shootingGalleryTargetsUpdateTargetColor(struct Enemy* enemy, const void* worldPosition, s32 unusedArg2, s32 unusedArg3);
static void _shootingGalleryTargetsTargetTask(Task* task);
static void _shootingGalleryTargetsHideTarget(Task* task);
static void _shootingGalleryTargetsDestroyTarget(Task* task);
static void _shootingGalleryTargetsInitTargetCollision(Task* task);
static void _shootingGalleryTargetsUpdateTargetRootColor(Task* task);
static void _shootingGalleryTargetsUpdateTargetRotation(Task* task);
static void _shootingGalleryTargetsCopyTargetRotation(const MATRIX* source, MATRIX* destination);
static void _shootingGalleryTargetsSetTargetState(Task* task, s16 state);
static s16  _shootingGalleryTargetsConsumeTargetHitReaction(Task* task);
static void _shootingGalleryTargetsBeginTargetBehaviour(Task* task);
static void _shootingGalleryTargetsResumeTargetState4(Task* task);
static void _shootingGalleryTargetsResumeTargetState5(Task* task);
static void _shootingGalleryTargetsResumeTargetState6(Task* task);
static void _shootingGalleryTargetsBeginFixedTargetExit(Task* task);
static void _shootingGalleryTargetsMeasureTargetPlayerDistance(Task* task);
static void _shootingGalleryTargetsPlaceTargetOffset(GfxCoord* rootCoord);
static void _shootingGalleryTargetsApplyTargetScale(Task* task);

/* Remaining-enemy count, and the gallery controller task the room overlay
 * publishes (its `Task::work` is the `MistShootingGalleryWork`). */

static void _shootingGalleryTargetsInitMount(Task* task);
static void _shootingGalleryTargetsUpdateStandingMountPath(Task* task);
static void _shootingGalleryTargetsUpdateHangingMountPath(Task* task);
static void _shootingGalleryTargetsUpdateMount(Task* task);
static void _shootingGalleryTargetsApplyTargetHits(Task* task);

/// Gallery mount task callbacks: 0 initialize, 1 update, 2 retire, 3 destroy.
static const TaskFuncTable4 _gShootingGalleryTargetsMountTaskStates = {
    .funcs = {
        _shootingGalleryTargetsInitMount,
        _shootingGalleryTargetsUpdateMount,
        _shootingGalleryTargetsRetireMount,
        _shootingGalleryTargetsDestroyMount,
    },
};

/// Gallery mount behavior callbacks, copied before dispatch by behavior 0..2.
static const TaskFuncTable3 _gShootingGalleryTargetsMountBehaviourHandlers = { {
    [MOUNT_STANDING] = _shootingGalleryTargetsUpdateStandingMountPath,
    [MOUNT_HANGING]  = _shootingGalleryTargetsUpdateHangingMountPath,
    [MOUNT_FIXED]    = _shootingGalleryTargetsUpdateFixedMount,
} };

/// The gallery mount's single zero-power, ordinary-reaction attack row.
static DamageAttack _gShootingGalleryTargetsMountAttacks[1] = { { .power = 0, .reaction = 0 } };

/// Gallery mount enemy parameters, borrowed by each initialized mount.
static EnemyParams _gShootingGalleryTargetsMountParams = {
    .attacks    = _gShootingGalleryTargetsMountAttacks,
    .hpMax      = 50,
    .critChance = 255,
};

TaskDesc gShootingGalleryTargetsTaskDescriptors[SHOOTING_GALLERY_TARGETS_TASK_COUNT] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsMountTask, { .model = &gMistShootingGalleryMountModel } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryCreatureTarget1Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryCreatureTarget2Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryCreatureTarget3Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryCreatureTarget4Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryCreatureTarget5Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryCreatureTarget6Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryCreatureTarget7Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryCreatureTarget8Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryCreatureTarget9Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryCreatureTarget10Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryBullseye1Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryBullseye2Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryBullseye3Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryDemonstrationTargetModel } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryHalfTarget1Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryHalfTarget2Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryHalfTarget3Model } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryTargetsTargetTask, { .model = &gMistShootingGalleryHalfTarget4Model } },
};

/// Mount path 0: hold at the spawn position for the spawn argument's hold time, in seconds of 30 ticks (0 until the
/// target finishes).
static _Waypoint _gShootingGalleryTargetsMountPath0[] = {
    { 0, 3000, 0 },
    { PATH_END, 0, 0 },
};

/// Mount path 1: decreasing X at Z 6000, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath1[] = {
    { 1500, 6000, 40 },
    { -1500, 6000, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 2: decreasing X at Z 4500, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath2[] = {
    { 1500, 4500, 40 },
    { -1500, 4500, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 3: decreasing X at Z 3000, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath3[] = {
    { 1500, 3000, 40 },
    { -1500, 3000, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 4: decreasing X at Z 1500, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath4[] = {
    { 1500, 1500, 40 },
    { -1500, 1500, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 5: decreasing X at Z 0, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath5[] = {
    { 1500, 0, 40 },
    { -1500, 0, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 6: reach XZ (-1400, 5800), at 40 world units per axis per frame.
static _Waypoint _gShootingGalleryTargetsMountPath6[] = {
    { -1400, 5800, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 7: reach XZ (-1400, 3000), at 40 world units per axis per frame.
static _Waypoint _gShootingGalleryTargetsMountPath7[] = {
    { -1400, 3000, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 8: reach XZ (-1400, 200), at 40 world units per axis per frame.
static _Waypoint _gShootingGalleryTargetsMountPath8[] = {
    { -1400, 200, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 9: increasing X at Z 6000, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath9[] = {
    { 1500, 6000, 40 },
    { 4500, 6000, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 10: increasing X at Z 4500, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath10[] = {
    { 1500, 4500, 40 },
    { 6000, 4500, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 11: increasing X at Z 3000, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath11[] = {
    { 1500, 3000, 40 },
    { 6000, 3000, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 12: increasing X at Z 1500, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath12[] = {
    { 1500, 1500, 40 },
    { 6000, 1500, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 13: increasing X at Z 0, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath13[] = {
    { 1500, 0, 40 },
    { 4500, 0, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 14: reach XZ (4600, 5800), at 40 world units per axis per frame.
static _Waypoint _gShootingGalleryTargetsMountPath14[] = {
    { 4600, 5800, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 15: reach XZ (4600, 3000), at 40 world units per axis per frame.
static _Waypoint _gShootingGalleryTargetsMountPath15[] = {
    { 4600, 3000, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 16: reach XZ (4600, 200), at 40 world units per axis per frame.
static _Waypoint _gShootingGalleryTargetsMountPath16[] = {
    { 4600, 200, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 17: Z stops 3000 then 0 at X 6000, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath17[] = {
    { 6000, 3000, 40 },
    { 6000, 0, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 18: decreasing Z at X 4500, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath18[] = {
    { 4500, 3000, 40 },
    { 4500, 0, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 19: decreasing Z at X 3000, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath19[] = {
    { 3000, 3000, 40 },
    { 3000, 0, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 20: decreasing Z at X 1500, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath20[] = {
    { 1500, 3000, 40 },
    { 1500, 0, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 21: decreasing Z at X 0, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath21[] = {
    { 0, 3000, 40 },
    { 0, 0, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 22: decreasing Z at X -1500, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath22[] = {
    { -1500, 3000, 40 },
    { -1500, 0, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 23: reach XZ (4600, 200), at 40 world units per axis per frame.
static _Waypoint _gShootingGalleryTargetsMountPath23[] = {
    { 4600, 200, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 24: reach XZ (1600, 200), at 40 world units per axis per frame.
static _Waypoint _gShootingGalleryTargetsMountPath24[] = {
    { 1600, 200, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 25: reach XZ (-1400, 200), at 40 world units per axis per frame.
static _Waypoint _gShootingGalleryTargetsMountPath25[] = {
    { -1400, 200, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 26: Z stops 3000 then 6000 at X 6000, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath26[] = {
    { 6000, 3000, 40 },
    { 6000, 6000, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 27: increasing Z at X 4500, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath27[] = {
    { 4500, 3000, 40 },
    { 4500, 6000, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 28: increasing Z at X 3000, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath28[] = {
    { 3000, 3000, 40 },
    { 3000, 6000, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 29: increasing Z at X 1500, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath29[] = {
    { 1500, 3000, 40 },
    { 1500, 6000, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 30: increasing Z at X 0, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath30[] = {
    { 0, 3000, 40 },
    { 0, 6000, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 31: increasing Z at X -1500, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath31[] = {
    { -1500, 3000, 40 },
    { -1500, 6000, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 32: reach XZ (4600, 5800), at 40 world units per axis per frame.
static _Waypoint _gShootingGalleryTargetsMountPath32[] = {
    { 4600, 5800, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 33: reach XZ (1600, 5800), at 40 world units per axis per frame.
static _Waypoint _gShootingGalleryTargetsMountPath33[] = {
    { 1600, 5800, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 34: reach XZ (-1400, 5800), at 40 world units per axis per frame.
static _Waypoint _gShootingGalleryTargetsMountPath34[] = {
    { -1400, 5800, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 35: Z 6000 to 0 via X 1500 then 4500, at 50 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath35[] = {
    { 1500, 6000, 50 },
    { 1500, 3000, 50 },
    { 4500, 3000, 50 },
    { 4500, 0, 50 },
    { PATH_END, 0, 0 },
};

/// Mount path 36: Z 0 to 6000 via X 1500 then 4500, at 50 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath36[] = {
    { 1500, 0, 50 },
    { 1500, 3000, 50 },
    { 4500, 3000, 50 },
    { 4500, 6000, 50 },
    { PATH_END, 0, 0 },
};

/// Mount path 37: increasing X through Z 4500/6000 steps, at 30, 40, 50 then 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath37[] = {
    { 0, 6000, 30 },
    { 0, 4500, 30 },
    { 1500, 4500, 40 },
    { 1500, 6000, 40 },
    { 4500, 6000, 50 },
    { 4500, 4500, 50 },
    { 6000, 4500, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 38: increasing X through Z 0/1500 steps, at 30, 40, 50 then 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath38[] = {
    { 0, 0, 30 },
    { 0, 1500, 30 },
    { 1500, 1500, 40 },
    { 1500, 0, 40 },
    { 4500, 0, 50 },
    { 4500, 1500, 50 },
    { 6000, 1500, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 39: increasing X with alternating Z 0/6000 crossings, at 40, 50, 70 then 100 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath39[] = {
    { -1500, 0, 40 },
    { 0, 0, 40 },
    { 0, 6000, 50 },
    { 1500, 6000, 50 },
    { 1500, 0, 70 },
    { 3000, 0, 70 },
    { 3000, 6000, 100 },
    { 4500, 6000, 100 },
    { 4500, 0, 100 },
    { PATH_END, 0, 0 },
};

/// Mount path 40: decreasing X from 9000 to -3000 through Z 4500/6000 steps, at 40, 50 then 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath40[] = {
    { 9000, 6000, 40 },
    { 9000, 4500, 40 },
    { 6000, 4500, 50 },
    { 3000, 4500, 50 },
    { 3000, 6000, 50 },
    { -3000, 6000, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 41: decreasing X from 9000 to -3000 through Z 0/1500 steps, at 40, 50 then 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath41[] = {
    { 9000, 0, 40 },
    { 9000, 1500, 40 },
    { 6000, 1500, 50 },
    { 3000, 1500, 50 },
    { 3000, 0, 50 },
    { -3000, 0, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 42: decreasing Z at X 9000, then returning to Z 3000 at X 12000, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath42[] = {
    { 9000, 6000, 40 },
    { 9000, 3000, 40 },
    { 9000, 0, 40 },
    { 12000, 0, 40 },
    { 12000, 3000, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 43: reach XZ (7500, 4500) at 40 world units per frame, then detour through Z 6000 to (12000, 4500) at
/// 70.
static _Waypoint _gShootingGalleryTargetsMountPath43[] = {
    { 7500, 4500, 40 },
    { 7500, 6000, 70 },
    { 12000, 6000, 70 },
    { 12000, 4500, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 44: reach XZ (7500, 1500) at 40 world units per frame, then detour through Z 0 to (12000, 1500) at 70.
static _Waypoint _gShootingGalleryTargetsMountPath44[] = {
    { 7500, 1500, 40 },
    { 7500, 0, 70 },
    { 12000, 0, 70 },
    { 12000, 1500, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 45: cross between X -3000/7500 and Z 0/6000, ending at XZ (7500, 3000), at 140 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath45[] = {
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
    { PATH_END, 0, 0 },
};

/// Mount path 46: increasing X from -3000 to 7500 through Z 1500/4500 steps, at 50 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath46[] = {
    { -3000, 1500, 50 },
    { 0, 1500, 50 },
    { 0, 4500, 50 },
    { 3000, 4500, 50 },
    { 3000, 1500, 50 },
    { 7500, 1500, 50 },
    { PATH_END, 0, 0 },
};

/// Mount path 47: increasing X from 1500 to 7500 with a Z 6000-to-4500 step at X 4500, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath47[] = {
    { 1500, 6000, 40 },
    { 4500, 6000, 40 },
    { 4500, 4500, 40 },
    { 7500, 4500, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 48: XZ (1500, 0) to (4500, 1500), then (7500, 1500), at 40 world units per axis per frame.
static _Waypoint _gShootingGalleryTargetsMountPath48[] = {
    { 1500, 0, 40 },
    { 4500, 1500, 40 },
    { 4500, 1500, 40 },
    { 7500, 1500, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 49: Z stops 0, 6000, 0 then 6000 at X 12000, at 40 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath49[] = {
    { 12000, 0, 40 },
    { 12000, 6000, 40 },
    { 12000, 0, 40 },
    { 12000, 6000, 40 },
    { PATH_END, 0, 0 },
};

/// Mount path 50: Z stops 3000 then 0 at X 6000, at 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath50[] = {
    { 6000, 3000, 70 },
    { 6000, 0, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 51: Z stops 3000 then 0 at X 4500, at 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath51[] = {
    { 4500, 3000, 70 },
    { 4500, 0, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 52: decreasing Z at X 3000, at 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath52[] = {
    { 3000, 3000, 70 },
    { 3000, 0, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 53: decreasing Z at X 1500, at 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath53[] = {
    { 1500, 3000, 70 },
    { 1500, 0, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 54: decreasing Z at X 0, at 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath54[] = {
    { 0, 3000, 70 },
    { 0, 0, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 55: decreasing Z at X -1500, at 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath55[] = {
    { -1500, 3000, 70 },
    { -1500, 0, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 56: Z stops 3000 then 6000 at X 6000, at 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath56[] = {
    { 6000, 3000, 70 },
    { 6000, 6000, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 57: Z stops 3000 then 6000 at X 4500, at 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath57[] = {
    { 4500, 3000, 70 },
    { 4500, 6000, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 58: increasing Z at X 3000, at 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath58[] = {
    { 3000, 3000, 70 },
    { 3000, 6000, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 59: increasing Z at X 1500, at 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath59[] = {
    { 1500, 3000, 70 },
    { 1500, 6000, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 60: increasing Z at X 0, at 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath60[] = {
    { 0, 3000, 70 },
    { 0, 6000, 70 },
    { PATH_END, 0, 0 },
};

/// Mount path 61: increasing Z at X -1500, at 70 world units per frame.
static _Waypoint _gShootingGalleryTargetsMountPath61[] = {
    { -1500, 3000, 70 },
    { -1500, 6000, 70 },
    { PATH_END, 0, 0 },
};

/// Sentinel-terminated routes selected by the gallery mount's spawn path byte (bits 16-23), which is not range-checked.
static const _Waypoint* _gShootingGalleryTargetsMountPaths[] = {
    _gShootingGalleryTargetsMountPath0,
    _gShootingGalleryTargetsMountPath1,
    _gShootingGalleryTargetsMountPath2,
    _gShootingGalleryTargetsMountPath3,
    _gShootingGalleryTargetsMountPath4,
    _gShootingGalleryTargetsMountPath5,
    _gShootingGalleryTargetsMountPath6,
    _gShootingGalleryTargetsMountPath7,
    _gShootingGalleryTargetsMountPath8,
    _gShootingGalleryTargetsMountPath9,
    _gShootingGalleryTargetsMountPath10,
    _gShootingGalleryTargetsMountPath11,
    _gShootingGalleryTargetsMountPath12,
    _gShootingGalleryTargetsMountPath13,
    _gShootingGalleryTargetsMountPath14,
    _gShootingGalleryTargetsMountPath15,
    _gShootingGalleryTargetsMountPath16,
    _gShootingGalleryTargetsMountPath17,
    _gShootingGalleryTargetsMountPath18,
    _gShootingGalleryTargetsMountPath19,
    _gShootingGalleryTargetsMountPath20,
    _gShootingGalleryTargetsMountPath21,
    _gShootingGalleryTargetsMountPath22,
    _gShootingGalleryTargetsMountPath23,
    _gShootingGalleryTargetsMountPath24,
    _gShootingGalleryTargetsMountPath25,
    _gShootingGalleryTargetsMountPath26,
    _gShootingGalleryTargetsMountPath27,
    _gShootingGalleryTargetsMountPath28,
    _gShootingGalleryTargetsMountPath29,
    _gShootingGalleryTargetsMountPath30,
    _gShootingGalleryTargetsMountPath31,
    _gShootingGalleryTargetsMountPath32,
    _gShootingGalleryTargetsMountPath33,
    _gShootingGalleryTargetsMountPath34,
    _gShootingGalleryTargetsMountPath35,
    _gShootingGalleryTargetsMountPath36,
    _gShootingGalleryTargetsMountPath37,
    _gShootingGalleryTargetsMountPath38,
    _gShootingGalleryTargetsMountPath39,
    _gShootingGalleryTargetsMountPath40,
    _gShootingGalleryTargetsMountPath41,
    _gShootingGalleryTargetsMountPath42,
    _gShootingGalleryTargetsMountPath43,
    _gShootingGalleryTargetsMountPath44,
    _gShootingGalleryTargetsMountPath45,
    _gShootingGalleryTargetsMountPath46,
    _gShootingGalleryTargetsMountPath47,
    _gShootingGalleryTargetsMountPath48,
    _gShootingGalleryTargetsMountPath49,
    _gShootingGalleryTargetsMountPath50,
    _gShootingGalleryTargetsMountPath51,
    _gShootingGalleryTargetsMountPath52,
    _gShootingGalleryTargetsMountPath53,
    _gShootingGalleryTargetsMountPath54,
    _gShootingGalleryTargetsMountPath55,
    _gShootingGalleryTargetsMountPath56,
    _gShootingGalleryTargetsMountPath57,
    _gShootingGalleryTargetsMountPath58,
    _gShootingGalleryTargetsMountPath59,
    _gShootingGalleryTargetsMountPath60,
    _gShootingGalleryTargetsMountPath61,
};

/// Retained zero-power, ordinary-reaction attack row in the gallery target parameters.
static DamageAttack _gShootingGalleryTargetsTargetAttacks[1] = { { 0, GAME_ACTOR_REACTION_ORDINARY } };

/// Gallery target parameters, borrowed by each live target enemy.
static EnemyParams _gShootingGalleryTargetsTargetParams = {
    .attacks    = _gShootingGalleryTargetsTargetAttacks,
    .hpMax      = 50,
    .critChance = 255,
};

/// Eight cyclic hit-mark offsets on a gallery target's face.
static DVECTOR _gShootingGalleryTargetsTargetHitMarkOffsets[TARGET_HIT_MARK_COUNT] = {
    { 0, -0x120 },
    { 0x60, -0xA0 },
    { -0x70, -0x60 },
    { 0xC0, -0x140 },
    { -0x20, -0x1C0 },
    { -0xA0, -0x160 },
    { 0xE0, -0x30 },
    { 0, 0 },
};

/// Starting hit points indexed by the gallery target's spawn-kind nibble, for kinds 0..12. The demonstration's kind 13
/// reads one entry past the table, the original program's overrun; what health it gets is unproven.
static u16 _gShootingGalleryTargetsTargetHitPoints[13] = { 32, 24, 16, 12, 60, 36, 28, 20, 12, 1, 12, 20, 28 };

static void _shootingGalleryTargetsRemapTargetColor(Enemy* unusedEnemy, MATRIX* colorMatrix, s32 colorMode);

/// Initializes a gallery mount and spawns the target it carries.
static void _shootingGalleryTargetsInitMount(Task* task)
{
    enum {
        SPAWN_BEHAVIOUR_SHIFT = 12,
        SPAWN_BEHAVIOUR_MASK  = 0xF000,
        SPAWN_PATH_SHIFT      = 16,
        SPAWN_ATTACKING       = 0x20000000,
        SPAWN_FORCE           = 0x40000000,
        PLAYER_EXCLUSION      = 0x400,
    };

    TmdObject*  model;
    Enemy*      enemy;
    GfxCoord*   rootCoord;
    GfxCoord*   playerCoord;
    void**      scratchSlot;
    VECTOR*     savedCursor;
    VECTOR*     playerOffset;
    _MountWork* work;

    model                                      = task->extra.tmd;
    enemy                                      = task->spawnArg2.pointer;
    rootCoord                                  = model->coords;
    playerCoord                                = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    scratchSlot                                = SCRATCH_STACK_CURSOR_SLOT;
    savedCursor                                = SCRATCH_STACK_CURSOR_AT(scratchSlot, VECTOR);
    playerOffset                               = savedCursor - 1;
    playerOffset->vx                           = playerCoord->coord.t[0] - rootCoord->coord.t[0];
    SCRATCH_STACK_CURSOR_AT(scratchSlot, void) = playerOffset;
    playerOffset->vz                           = playerCoord->coord.t[2] - rootCoord->coord.t[2];
    // Reject before acquiring work, child ownership or a battle reference.
    if ((!(task->spawnArg1.value & SPAWN_FORCE) &&
         playerActorPlanarLength(playerOffset->vx, playerOffset->vz) < PLAYER_EXCLUSION + 1) ||
        (u8)task->spawnArg1.value == TARGET_SPAWN_DEAD) {
    fail:
        if ((task->spawnArg1.value & SPAWN_BEHAVIOUR_MASK) !=
            MOUNT_FIXED << SPAWN_BEHAVIOUR_SHIFT) {
            MistShootingGalleryWork* gallery = task->parent->work;

            gallery->liveTargets--;
        }
        SCRATCH_STACK_RELEASE_BYTES(sizeof(*playerOffset));
        enemyDestroy(enemy, task);
        return;
    }
    work       = memCalloc(sizeof(_MountWork), false);
    task->work = work;
    if (work == NULL) {
        goto fail;
    }
    task->exitCallback = _shootingGalleryTargetsDestroyMount;
    work->path         = (u8)((u32)task->spawnArg1.value >> SPAWN_PATH_SHIFT);
    work->behaviour    = (s32)(task->spawnArg1.value & SPAWN_BEHAVIOUR_MASK) >> SPAWN_BEHAVIOUR_SHIFT;
    model->lightMtx    = &work->lightMatrix;
    model->colorMtx    = &work->colorMatrix;
    enemy->param       = &_gShootingGalleryTargetsMountParams;
    rootCoord->parent  = &gGfxViewCoord;
    enemy->field_4     = &task->extra.tmd->coords->coord;
    enemy->field_48    = 0;
    if (work->behaviour != MOUNT_FIXED) {
        sceneAcquireBattleRef(0);
    }
    work->spawnX = rootCoord->coord.t[0];
    work->spawnY = rootCoord->coord.t[1];
    work->spawnZ = rootCoord->coord.t[2];
    work->yaw    = -ANGLE_QUARTER_TURN;
    if (work->behaviour == MOUNT_HANGING) {
        work->roll += ANGLE_TURN / 2;
    }
    task->state++;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    // The child's behaviour moves from bits 12..15 to 16..19; attack stays at 29.
    _shootingGalleryTargetsSpawnTarget(enemy, task->spawnArg1.value & TARGET_SPAWN_KIND_MASK,
                                       work->behaviour | (((u32)task->spawnArg1.value >> TARGET_SPAWN_BEHAVIOUR_SHIFT) &
                                                          (SPAWN_ATTACKING >> TARGET_SPAWN_BEHAVIOUR_SHIFT)));
    SCRATCH_STACK_RELEASE_BYTES_AT(scratchSlot, sizeof(*playerOffset));
}

/// Stops a mount path and asks its live child target to leave.
static inline void _shootingGalleryTargetsStopMountPath(_MountWork* work, Enemy* enemy)
{
    work->step                                = MOUNT_STEP_LEAVE;
    work->rotating                            = 0;
    enemy->task->firstChild->spawnArg1.value |= TARGET_SIGNAL_STOP;
}

/// Advances a path-following mount at the supplied bob and resting Y heights.
static inline void _shootingGalleryTargetsUpdateMountPath(Task* task, s32 bobY, s32 restingY)
{
    enum {
        MOUNT_FULL_HEIGHT_PERCENT = 100,
        HEIGHT_STEP_PERCENT       = 8,
        SETTLE_FRAMES             = 4,
    };

    _MountWork*      work      = task->work;
    Enemy*           enemy     = task->spawnArg2.pointer;
    GfxCoord*        rootCoord = task->extra.tmd->coords;
    const _Waypoint* waypoint;
    s32              axisDelta;

    switch (work->step) {
        case MOUNT_STEP_RISE:
            if (work->heightPercent < MOUNT_FULL_HEIGHT_PERCENT) {
                work->heightPercent += HEIGHT_STEP_PERCENT;
                return;
            }
            work->heightPercent = MOUNT_FULL_HEIGHT_PERCENT;
            work->timer         = 0;
            work->step++;
        case MOUNT_STEP_SETTLE:
            if (++work->timer & 1) {
                rootCoord->coord.t[1] = bobY;
                return;
            }
            rootCoord->coord.t[1] = restingY;
            if (work->timer >= SETTLE_FRAMES) {
                work->step++;
                enemy->task->firstChild->spawnArg1.value |= TARGET_SIGNAL_MOUNT_READY;
            }
            return;
        case MOUNT_STEP_WAIT_TARGET:
            if (!(enemy->task->firstChild->spawnArg1.value & TARGET_SIGNAL_STANDING)) {
                return;
            }
            waypoint  = _gShootingGalleryTargetsMountPaths[work->path];
            waypoint += work->waypoint;
            if (task->spawnArg1.value & MOUNT_SPAWN_ROTATING) {
                work->rotating = 1;
            }
            if (waypoint->speed == 0) {
                work->step  = MOUNT_STEP_HOLD;
                work->timer = (((u32)task->spawnArg1.value >> MOUNT_SPAWN_HOLD_SHIFT) & MOUNT_SPAWN_HOLD_MASK) * FRAMES_PER_SECOND;
                return;
            }
            work->step++;
        case MOUNT_STEP_TRAVEL:
            waypoint  = _gShootingGalleryTargetsMountPaths[work->path];
            waypoint += work->waypoint;
            if (waypoint->x == PATH_END) {
            stop:
                _shootingGalleryTargetsStopMountPath(work, enemy);
                return;
            }
            // Each axis arrival advances the waypoint; diagonal legs repeat their stop.
            axisDelta = (s16)(rootCoord->coord.t[0] - waypoint->x);
            if (axisDelta != 0) {
                if (waypoint->speed >= abs(axisDelta)) {
                    if (enemy->task->firstChild->spawnArg1.value & TARGET_SIGNAL_STOP) {
                        _shootingGalleryTargetsStopMountPath(work, enemy);
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
                    if (enemy->task->firstChild->spawnArg1.value & TARGET_SIGNAL_STOP) {
                        _shootingGalleryTargetsStopMountPath(work, enemy);
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
        case MOUNT_STEP_HOLD:
            if (work->timer != 0 && --work->timer <= 0) {
                goto stop;
            }
        case MOUNT_STEP_LEAVE:
            if (enemy->task->firstChild->spawnArg1.value & TARGET_SIGNAL_FINISHED) {
                if (work->heightPercent > 0) {
                    work->heightPercent -= HEIGHT_STEP_PERCENT;
                    return;
                }
                work->heightPercent = 0;
                task->state++;
            }
            break;
    }
}

/// How far a hanging mount hangs above the floor, how high every mount bobs above where it rests, and how fast a
/// rotating mount turns.
enum {
    HANGING_MOUNT_HEIGHT = 0xF3C,
    MOUNT_BOB_HEIGHT     = 0x10,
    MOUNT_TURN_STEP      = 0x20,
};

/// Runs a standing gallery mount along its selected path, then shrinks it away.
static void _shootingGalleryTargetsUpdateStandingMountPath(Task* task)
{
    _shootingGalleryTargetsUpdateMountPath(task, -MOUNT_BOB_HEIGHT, 0);
}

/// Runs a hanging gallery mount along its selected path, then shrinks it away.
static void _shootingGalleryTargetsUpdateHangingMountPath(Task* task)
{
    _shootingGalleryTargetsUpdateMountPath(task, -HANGING_MOUNT_HEIGHT - MOUNT_BOB_HEIGHT, -HANGING_MOUNT_HEIGHT);
}

/// Gallery target task callbacks: 0 initialize, 1 update, 2 hide, 3 destroy.
static const TaskFuncTable4 _gShootingGalleryTargetsTargetTaskStates = {
    .funcs = {
        _shootingGalleryTargetsInitTarget,
        _shootingGalleryTargetsUpdateTarget,
        _shootingGalleryTargetsHideTarget,
        _shootingGalleryTargetsDestroyTarget,
    },
};

/// Gallery target behavior callbacks indexed by `TARGET_STATE_*`.
static const TaskFuncTable10 _gShootingGalleryTargetsTargetStateHandlers = {
    .funcs = {
        _shootingGalleryTargetsBeginTargetBehaviour,
        _shootingGalleryTargetsUpdateActiveTarget,
        _shootingGalleryTargetsUpdateTargetFlinch,
        _shootingGalleryTargetsUpdateTargetFlinch,
        _shootingGalleryTargetsResumeTargetState4,
        _shootingGalleryTargetsResumeTargetState5,
        _shootingGalleryTargetsResumeTargetState6,
        _shootingGalleryTargetsFoldTarget,
        _shootingGalleryTargetsBeginFixedTargetExit,
        _shootingGalleryTargetsTumbleDestroyedTarget,
    },
};

/// Dispatches one gallery mount tick through its four task lifecycle states.
static void _shootingGalleryTargetsMountTask(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = _gShootingGalleryTargetsMountTaskStates;
    handlers.funcs[task->state](task);
}

/// Updates a gallery mount's behaviour, transform, lighting and draw visibility.
static void _shootingGalleryTargetsUpdateMount(Task* task)
{
    enum { MOUNT_FULL_HEIGHT_PERCENT = 100 };

    TmdObject*  model       = task->extra.tmd;
    GfxCoord*   rootCoord   = model->coords;
    _MountWork* work        = task->work;
    TaskFunc    handlers[2] = { _shootingGalleryTargetsBeginMountBehaviour, _shootingGalleryTargetsUpdateMountBehaviour };
    TmdObject*  visibleModel;

    visibleModel = model;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            handlers[work->state](task);
            rootCoord->param.rot.vy = work->yaw;
            rootCoord->param.rot.vz = work->roll;
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            _shootingGalleryTargetsUpdateMountRotation(task);
            rootCoord->coord.m[1][1] = work->heightPercent * (rootCoord->coord.m[1][1] / MOUNT_FULL_HEIGHT_PERCENT);
            // Paused mounts also refresh their lighting and remain drawable.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _shootingGalleryTargetsUpdateMountColor(task);
            visibleModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
}

/// Removes a finished gallery mount from its controller's live-target count.
static void _shootingGalleryTargetsRetireMount(Task* task)
{
    Task*       controllerTask;
    _MountWork* work;

    controllerTask = task->parent;
    work           = task->work;
    if (work->behaviour != MOUNT_FIXED) {
        MistShootingGalleryWork* gallery = controllerTask->work;

        gallery->liveTargets--;
    }
    task->state++;
}

/// Releases a gallery mount's battle reference and destroys its enemy and task.
static void _shootingGalleryTargetsDestroyMount(Task* task)
{
    _MountWork* work = task->work;

    if (work->behaviour != MOUNT_FIXED) {
        sceneReleaseBattleRefWithRewards(task, 0);
    }
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Refreshes the mount's lighting and colour from its model root position.
static void _shootingGalleryTargetsUpdateMountColor(Task* task)
{
    GfxCoord* rootCoord;
    void**    scratchSlot;
    VECTOR*   savedCursor;
    VECTOR*   worldPosition;
    Enemy*    enemy;

    enemy                                      = task->spawnArg2.pointer;
    rootCoord                                  = task->extra.tmd->coords;
    scratchSlot                                = SCRATCH_STACK_CURSOR_SLOT;
    savedCursor                                = SCRATCH_STACK_CURSOR_AT(scratchSlot, VECTOR);
    worldPosition                              = savedCursor - 1;
    worldPosition->vx                          = rootCoord->workm.t[0];
    worldPosition->vy                          = rootCoord->workm.t[1];
    worldPosition->vz                          = rootCoord->workm.t[2];
    SCRATCH_STACK_CURSOR_AT(scratchSlot, void) = worldPosition;
    worldCoordUpdateActorColor(enemy, worldPosition, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES_AT(scratchSlot, sizeof(*worldPosition));
}

/// Rebuilds a gallery mount's local rotation from its wrapped Euler angles.
static void _shootingGalleryTargetsUpdateMountRotation(Task* task)
{
    _MountWork* work      = task->work;
    GfxCoord*   rootCoord = task->extra.tmd->coords;
    MATRIX*     rotation;

    work->pitch &= ANGLE_MASK;
    work->yaw   &= ANGLE_MASK;
    work->roll  &= ANGLE_MASK;
    rotation     = SCRATCH_STACK_CURSOR(MATRIX) - 1;
    // Initialize before publishing the reservation for the axis helpers.
    gfxSetRotIdentity(rotation);
    SCRATCH_STACK_CURSOR(MATRIX) = rotation;
    RotMatrixZ(work->roll, rotation);
    RotMatrixX(work->pitch, rotation);
    RotMatrixY(work->yaw, rotation);
    _shootingGalleryTargetsCopyMountRotation(rotation, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Copies the nine rotation elements used by the gallery mount model.
static void _shootingGalleryTargetsCopyMountRotation(const MATRIX* source, MATRIX* destination)
{
    gfxCopyRotation(destination, source);
}

/// Advances a newly spawned mount to its behaviour dispatcher after one tick.
static void _shootingGalleryTargetsBeginMountBehaviour(Task* task)
{
    _MountWork* work = task->work;

    work->state++;
}

/// Updates the gallery mount's selected behaviour and optional yaw spin.
static void _shootingGalleryTargetsUpdateMountBehaviour(Task* task)
{
    TaskFuncTable3 handlers;
    _MountWork*    work = task->work;

    handlers = _gShootingGalleryTargetsMountBehaviourHandlers;
    handlers.funcs[work->behaviour](task);
    if (work->rotating != 0) {
        work->yaw += MOUNT_TURN_STEP;
    }
}

/// Places a fixed demonstration mount and shrinks it after its target finishes.
static void _shootingGalleryTargetsUpdateFixedMount(Task* task)
{
    _MountWork* work      = task->work;
    GfxCoord*   rootCoord = task->extra.tmd->coords;
    Enemy*      enemy     = task->spawnArg2.pointer;

    switch (work->step) {
        case MOUNT_FIXED_STEP_PLACE:
            work->step++;
            work->heightPercent   = 100;
            rootCoord->coord.t[1] = 0;
        case MOUNT_FIXED_STEP_LEAVE:
            if (enemy->task->firstChild->spawnArg1.value & TARGET_SIGNAL_FINISHED) {
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
static void _shootingGalleryTargetsSpawnTarget(Enemy* mountEnemy, s32 targetKind, s32 targetFlags)
{
    enum {
        FIRST_ZERO_CLUT_KIND = 10,
        EARLY_CLUT_ROW       = 2,
    };

    Enemy*     targetEnemy;
    GfxCoord*  targetCoord;
    TmdObject* targetModel;

    targetEnemy = enemySpawnFromTable(gShootingGalleryTargetsTaskDescriptors, targetKind + SHOOTING_GALLERY_TARGETS_TASK_TARGET_BASE, targetFlags, mountEnemy);
    if (targetEnemy != NULL) {
        taskReparent(mountEnemy->task, targetEnemy->task);
        targetCoord                        = targetEnemy->task->extra.tmd->coords;
        targetCoord->parent                = mountEnemy->task->extra.tmd->coords;
        targetEnemy->task->spawnArg1.value = targetKind | (targetFlags << TARGET_SPAWN_BEHAVIOUR_SHIFT);
        targetModel                        = targetEnemy->task->extra.tmd;
        targetModel->texturePageOffset     = 0;
        if (targetKind < FIRST_ZERO_CLUT_KIND) {
            targetModel->clutRowOffset = EARLY_CLUT_ROW;
        } else {
            targetModel->clutRowOffset = 0;
        }
        tmdBuildBufferHalf(targetModel);
        tmdBuildBufferHalf(targetModel);
        targetEnemy->workType = ENEMY_WORK_ACTOR;
    }
}

/// Allocates and registers the gallery target carried by a mount.
static void _shootingGalleryTargetsInitTarget(Task* task)
{
    _TargetWork* work;
    Enemy*       enemy;
    TmdObject*   model;
    GfxCoord*    rootCoord;
    u16          hitPoints;
    u32          spawnByte;

    model     = task->extra.tmd;
    spawnByte = (u8)task->spawnArg1.value;
    enemy     = task->spawnArg2.pointer;
    rootCoord = model->coords;
    if (spawnByte == TARGET_SPAWN_DEAD) {
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
    task->exitCallback   = _shootingGalleryTargetsDestroyTarget;
    work->mountBehaviour = ((u32)task->spawnArg1.value >> TARGET_SPAWN_BEHAVIOUR_SHIFT) & TARGET_SPAWN_BEHAVIOUR_MASK;
    model->lightMtx      = &work->lightMatrix;
    model->colorMtx      = &work->colorMatrix;
    enemy->param         = &_gShootingGalleryTargetsTargetParams;
    enemy->recs          = work->contacts;
    work->field_140      = task->extra.tmd->coords;
    work->field_144      = 0x140;
    work->field_146      = 2;
    hitPoints            = _gShootingGalleryTargetsTargetHitPoints[task->spawnArg1.value & TARGET_SPAWN_KIND_MASK];
    enemy->hpMax         = hitPoints;
    enemy->hp            = hitPoints;
    _shootingGalleryTargetsInitTargetCollision(task);
    worldTargetLinkNode(&enemy->node);
    enemy->field_4                = &rootCoord->workm;
    enemy->bodyPos.vy             = TARGET_AIM_Y;
    enemy->field_48               = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->coord                  = rootCoord;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    _shootingGalleryTargetsPlaceTargetOffset(rootCoord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    task->state += 1;
}

/// Advances a gallery target's behaviour, hit handling and face effects.
static void _shootingGalleryTargetsUpdateTarget(Task* task)
{
    TaskFuncTable10 states;
    Enemy*          enemy;
    TmdObject*      model;
    TmdObject*      drawModel;

    _TargetWork* work;
    GfxCoord*    rootCoord;
    SVECTOR*     hitMarkOffset;
    s32          hitMarkIndex;

    enemy     = task->spawnArg2.pointer;
    model     = task->extra.tmd;
    work      = task->work;
    rootCoord = model->coords;
    drawModel = model;
    states    = _gShootingGalleryTargetsTargetStateHandlers;
    SCRATCH_STACK_RESERVE_BYTES(sizeof(*hitMarkOffset));
    hitMarkOffset = SCRATCH_STACK_CURSOR(SVECTOR);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            states.funcs[work->state](task);
            if (work->state < TARGET_STATE_LEAVE) {
                if (work->hitCooldown == 0) {
                    _shootingGalleryTargetsApplyTargetHits(task);
                } else {
                    work->hitCooldown--;
                }
                worldCollisionClearContacts(work->contacts);
                if (enemy->hp <= 0) {
                    _shootingGalleryTargetsSetTargetState(task, TARGET_STATE_DESTROYED);
                }
            }
            // Running targets also use the paused mode's colour/draw refresh.
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            _shootingGalleryTargetsUpdateTargetRootColor(task);
            drawModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            drawModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    if (work->mountBehaviour != MOUNT_FIXED) {
        ((MistShootingGalleryWork*)gMistShootingGalleryControllerTask->work)->targetLockMask = worldTargetGetActorLockMask(&enemy->node);
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    _shootingGalleryTargetsUpdateTargetRotation(task);
    _shootingGalleryTargetsApplyTargetScale(task);
    for (hitMarkIndex = 0; hitMarkIndex < work->hitMarkCount; hitMarkIndex++) {
        if (hitMarkIndex == work->hitMarkCount - 1 && enemy->hp <= 0) {
            hitMarkOffset->vx = 0;
            hitMarkOffset->vy = -0xE0;
            hitMarkOffset->vz = 0;
            _shootingGalleryTargetsDrawTargetDeathBurst(rootCoord, hitMarkOffset);
        } else {
            hitMarkOffset->vx = _gShootingGalleryTargetsTargetHitMarkOffsets[(work->hitMarkFirst + hitMarkIndex) & (TARGET_HIT_MARK_COUNT - 1)].vx;
            hitMarkOffset->vy = _gShootingGalleryTargetsTargetHitMarkOffsets[(work->hitMarkFirst + hitMarkIndex) & (TARGET_HIT_MARK_COUNT - 1)].vy;
            hitMarkOffset->vz = 0;
            _shootingGalleryTargetsDrawTargetHitMark(rootCoord, hitMarkOffset);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(*hitMarkOffset));
}

/// Unfolds a gallery target and runs its optional repeating player attack.
static void _shootingGalleryTargetsUpdateActiveTarget(Task* task)
{
    enum {
        ACTIVE_STEP_WAIT_MOUNT        = 0,
        ACTIVE_STEP_GROW_WIDTH        = 1,
        ACTIVE_STEP_GROW_HEIGHT       = 2,
        ACTIVE_STEP_BOB               = 4,
        ACTIVE_STEP_ATTACK            = 5,
        ATTACK_ENABLED                = 0x20000000,
        FULL_PERCENT                  = 100,
        GROWTH_STEP                   = 32,
        STAND_EASE_BIAS               = 32,
        CHARGE_TICK                   = 120,
        FIRE_TICK                     = 210,
        PLAYER_DAMAGE                 = 10,
        PLAYER_HIT_PART               = 4,
        PLAYER_BODY_HIT               = 1,
        PLAYER_DESCENDING_HIT_FLASHES = 5
    };

    _TargetWork* work  = task->work;
    Enemy*       enemy = task->spawnArg2.pointer;
    Task*        player;
    GameActor*   playerActor;
    s32          hitPan;
    s32          spawnFlags;
    s16          bobTick;

    if (_shootingGalleryTargetsConsumeTargetHitReaction(task) != 0) {
        return;
    }
    // Entry phases fall through until a growth or settling tick must wait.
    switch (work->step) {
        case ACTIVE_STEP_WAIT_MOUNT:
            if (!(task->spawnArg1.value & TARGET_SIGNAL_MOUNT_READY)) {
                return;
            }
            work->step++;
        case ACTIVE_STEP_GROW_WIDTH:
            if (work->widthPercent < FULL_PERCENT) {
                work->widthPercent += GROWTH_STEP;
                return;
            }
            work->step++;
        case ACTIVE_STEP_GROW_HEIGHT:
            if (work->heightPercent < FULL_PERCENT) {
                work->heightPercent += GROWTH_STEP;
                return;
            }
            {
                GfxCoord* soundCoord = task->extra.tmd->coords;
                s32       soundPan;
                work->step++;
                soundPan = (s8)worldCoordGetOriginAudioPan(soundCoord);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_TARGET_APPEAR, soundPan, (s8)worldCoordGetOriginAudioDepth(soundCoord));
            }
        case TARGET_ACTIVE_STEP_STAND: {
            s16 pitch = work->pitch;
            if ((u16)(pitch - 1) < ANGLE_QUARTER_TURN) {
                work->pitch = pitch - ((ANGLE_QUARTER_TURN + STAND_EASE_BIAS - pitch) >> 2);
                return;
            }
        }
            work->timer = 0;
            work->step++;
            return;
        case ACTIVE_STEP_BOB:
            bobTick     = work->timer + 1;
            work->timer = bobTick;
            if (bobTick & 1) {
                work->pitch = ((bobTick << 16) >> 13) - 0x38;
            } else {
                work->pitch = 0;
                if (work->timer >= 4) {
                    work->step++;
                    task->spawnArg1.value |= TARGET_SIGNAL_STANDING;
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                    enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
                    work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
            }
        case ACTIVE_STEP_ATTACK:
            spawnFlags = task->spawnArg1.value;
            if (spawnFlags & TARGET_SIGNAL_STOP) {
                _shootingGalleryTargetsSetTargetState(task, TARGET_STATE_LEAVE);
                return;
            }
            if (!(spawnFlags & ATTACK_ENABLED)) {
                return;
            }
            work->attackTimer++;
            if (work->attackTimer == CHARGE_TICK) {
                GfxCoord* soundCoord = task->extra.tmd->coords;
                s32       soundPan;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                soundPan = (s8)worldCoordGetOriginAudioPan(soundCoord);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_TARGET_CHARGE, soundPan, (s8)worldCoordGetOriginAudioDepth(soundCoord));
            } else if (work->attackTimer == FIRE_TICK) {
                GfxCoord* playerHitCoord;
                s32       soundPan;
                player            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                playerHitCoord    = &player->extra.tmd->coords[PLAYER_HIT_PART];
                playerActor       = player->work;
                work->attackTimer = 0;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                effectSpawn(EFFECT_MIST_GALLERY_TRACER, playerHitCoord, 0, NULL);
                soundPan = (s8)worldCoordGetOriginAudioPan(playerHitCoord);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_TARGET_ATTACK, soundPan, (s8)worldCoordGetOriginAudioDepth(playerHitCoord));
                if (playerActor->mode != GAME_ACTOR_MODE_DAMAGE) {
                    if (gPlayerStatus.hp <= PLAYER_DAMAGE) {
                        ((MistShootingGalleryWork*)gMistShootingGalleryControllerTask->work)->lethalHit = 1;
                        playerActor->pendingDamage                                                      = 0;
                    } else {
                        playerActor->pendingDamage = PLAYER_DAMAGE;
                    }
                    playerActor->hitRegion      = PLAYER_BODY_HIT;
                    playerActor->damageReaction = PLAYER_DESCENDING_HIT_FLASHES;
                    playerActorEnterPendingHit(player);
                    hitPan = (s8)worldCoordGetOriginAudioPan(playerHitCoord);
                    sndEvtRequestScriptStart(SOUND_PLAYER_STRUCK, hitPan, (s8)worldCoordGetOriginAudioDepth(playerHitCoord));
                }
            }
            break;
    }
}

/// Swings a hit gallery target back, settles it and resumes its standing motion.
static void _shootingGalleryTargetsUpdateTargetFlinch(Task* task)
{
    enum { TARGET_FLINCH_ANGLE_LIMIT = ANGLE_TURN / 8 };

    _TargetWork* work = task->work;
    s32          swingAngleStep;
    s16          framesLeft;

    switch (work->step) {
        case TARGET_FLINCH_STEP_INIT:
            work->step++;
            work->timer = 6;
        case TARGET_FLINCH_STEP_SWING:
            swingAngleStep = work->hitDamage * 6;
            if (swingAngleStep > TARGET_FLINCH_ANGLE_LIMIT) {
                swingAngleStep = TARGET_FLINCH_ANGLE_LIMIT;
            }
            framesLeft      = work->timer;
            swingAngleStep /= 3;
            if (framesLeft >= 4) {
                if (work->pitch < TARGET_FLINCH_ANGLE_LIMIT) {
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
        case TARGET_FLINCH_STEP_SETTLE:
            work->timer++;
            if (work->timer & 1) {
                work->pitch = (work->timer - 7) * 8;
                return;
            }
            work->pitch     = 0;
            work->hitDamage = 0;
            if (work->timer >= 4) {
                work->state = TARGET_STATE_ACTIVE;
                work->step  = TARGET_ACTIVE_STEP_STAND;
            }
            break;
    }
}

/// Folds an undestroyed gallery target away and releases its mount to shrink.
static void _shootingGalleryTargetsFoldTarget(Task* task)
{
    _TargetWork* work  = task->work;
    Enemy*       enemy = task->spawnArg2.pointer;
    GfxCoord*    rootCoord;
    s32          pan;

    switch (work->step) {
        case TARGET_FOLD_STEP_BEGIN:
            work->step++;
            task->spawnArg1.value |= TARGET_SIGNAL_STOP;
            work->timer            = 7;
            // Stop accepting shots before folding the target back onto its mount.
            worldTargetUnlinkNode(&enemy->node);
            enemy->recs       = NULL;
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        case TARGET_FOLD_STEP_WAIT:
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
        case TARGET_FOLD_STEP_FOLD:
            if (work->pitch < ANGLE_QUARTER_TURN) {
                work->pitch += 0x80;
                return;
            }
            work->pitch = ANGLE_QUARTER_TURN;
            work->step++;
        case TARGET_FOLD_STEP_SHRINK:
            if (work->heightPercent > TARGET_SHRINK_STOP_PERCENT) {
                work->heightPercent -= TARGET_SHRINK_STEP_PERCENT;
                return;
            }
            if (work->widthPercent > TARGET_SHRINK_STOP_PERCENT) {
                work->widthPercent -= TARGET_SHRINK_STEP_PERCENT;
                return;
            }
            work->step++;
            task->spawnArg1.value |= TARGET_SIGNAL_FINISHED;
            break;
        case TARGET_FOLD_STEP_DONE:
            break;
    }
}

/// Draws one target tumble spin in 4096-per-turn angle units per tick.
static inline void _shootingGalleryTargetsRollTargetSpin(s16* spinAngle)
{
    enum { TARGET_SPIN_MAGNITUDE_MASK = 0x7F };

    s16 spin;

    RANDOM_LCG_ADVANCE();
    spin = RANDOM_LCG_DRAW() & TARGET_SPIN_MAGNITUDE_MASK;
    // Publish the magnitude before choosing its sign.
    *spinAngle = spin;
    if (!(RANDOM_LCG_DRAW() & 1)) {
        spin = -spin;
    }
    *spinAngle = spin;
}

/// Scores a destroyed gallery target, detaches it and tumbles it out of play.
static void _shootingGalleryTargetsTumbleDestroyedTarget(Task* task)
{
    _TargetWork*             work      = task->work;
    Enemy*                   enemy     = task->spawnArg2.pointer;
    TmdObject*               model     = task->extra.tmd;
    GfxCoord*                rootCoord = model->coords;
    MistShootingGalleryWork* gallery   = gMistShootingGalleryControllerTask->work;
    VECTOR*                  hitDirection;
    s32                      targetKind;
    s32                      soundId;
    s32                      pan;
    s32                      verticalVelocity;

    switch (work->step) {
        case TARGET_TUMBLE_STEP_BEGIN:
            work->step++;
            model->flags          |= TMD_OBJECT_SEMI_TRANS;
            task->spawnArg1.value |= TARGET_SIGNAL_STOP;
            work->hitMarkCount     = 0;
            work->field_15C        = 0;
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
            work->timer = 0;
            targetKind  = task->spawnArg1.value & TARGET_SPAWN_KIND_MASK;
            soundId     = targetKind;
            gallery->kills[targetKind]++;
            if (soundId < 9) {
                soundId = TARGET_KILL_SOUND_KINDS_0_TO_8;
            } else if (soundId == 9) {
                soundId = TARGET_KILL_SOUND_KIND_9;
            } else {
                soundId = TARGET_KILL_SOUND_KINDS_10_UP;
            }
            pan = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            work->yaw  = (rootCoord->parent)->param.rot.vy;
            work->roll = (rootCoord->parent)->param.rot.vz;
            _shootingGalleryTargetsRollTargetSpin(&work->pitchSpin);
            _shootingGalleryTargetsRollTargetSpin(&work->yawSpin);
            _shootingGalleryTargetsRollTargetSpin(&work->rollSpin);
            // Detach into the mount's parent frame before applying the shot direction.
            _shootingGalleryTargetsCopyTargetRotation(&rootCoord->parent->coord, &rootCoord->coord);
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
        case TARGET_TUMBLE_STEP_MOVE:
            // Decay spin and velocity while the target shrinks; tick 16 ends its task state.
            work->timer++;
            if (work->timer < TARGET_TUMBLE_FRAMES) {
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
                if (work->heightPercent > TARGET_SHRINK_STOP_PERCENT) {
                    work->heightPercent -= TARGET_SHRINK_STEP_PERCENT;
                    return;
                }
                if (work->widthPercent > TARGET_SHRINK_STOP_PERCENT) {
                    work->widthPercent -= TARGET_SHRINK_STEP_PERCENT;
                    return;
                }
            } else {
                task->state++;
                task->spawnArg1.value |= TARGET_SIGNAL_FINISHED;
            }
            break;
        case TARGET_TUMBLE_STEP_INERT:
            break;
    }
}

/// Adds a hit mark and its sound cue until the target's eight marks are filled.
static inline void _shootingGalleryTargetsAddTargetHitMark(Task* task, _TargetWork* work)
{
    GfxCoord* rootCoord;
    s32       pan;

    if (work->hitMarkCount < TARGET_HIT_MARK_COUNT) {
        rootCoord = task->extra.tmd->coords;
        work->hitMarkCount++;
        pan = (s8)worldCoordGetOriginAudioPan(rootCoord);
        sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_TARGET_HIT, pan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
    }
}

/// Applies attack contacts to a gallery target and queues its hit reaction.
static void _shootingGalleryTargetsApplyTargetHits(Task* task)
{
    enum {
        HIT_SCRATCH_BYTES = 8,
        HEAVY_HIT_HP      = 20,
    };

    _TargetWork* work;
    Enemy*       enemy;
    s32          contactIndex;
    s16          damage;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    // Retain the eight-byte scratch frame; this routine never accesses its payload.
    SCRATCH_STACK_RESERVE_BYTES(HIT_SCRATCH_BYTES);
    work->hitTaken = 0;
    if (worldCollisionFindContactIndex(work->body.context.contacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->contacts); contactIndex++) {
            if ((work->contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
                // Contact axes become the destroyed target's later knockback direction.
                work->hitTaken     = 1;
                work->knockback.vx = work->contacts[contactIndex].response.direction.vx;
                work->knockback.vy = work->contacts[contactIndex].response.direction.vy;
                work->knockback.vz = work->contacts[contactIndex].response.direction.vz;
                _shootingGalleryTargetsMeasureTargetPlayerDistance(task);
                damage            = damageComputePlayerAttack(work->contacts[contactIndex].key.value, work->playerDistance, 0, 0);
                work->hitCooldown = damageGetPlayerAttackHitCooldown(work->contacts[contactIndex].key.value);
                work->hitDamage   = damage;
                worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                enemy->hp -= damage;
                if (enemy->hp <= 0) {
                    enemy->hp = 0;
                }
                if (damage > 0) {
                    _shootingGalleryTargetsAddTargetHitMark(task, work);
                    if (damage >= HEAVY_HIT_HP) {
                        work->hitReaction = HIT_REACTION_HEAVY;
                    } else {
                        work->hitReaction = HIT_REACTION_LIGHT;
                    }
                } else {
                    work->hitTaken = 0;
                }
            }
        }
    }
    worldCollisionClearContacts(work->contacts);
    SCRATCH_STACK_RELEASE_BYTES(HIT_SCRATCH_BYTES);
}

enum { TARGET_DEATH_BURST_HALF_EXTENT = 256 };

/// Four X/Y offsets of the target death-burst quad, in game-coordinate units.
static const DVECTOR _gShootingGalleryTargetsTargetDeathBurstCorners[4] = {
    { -TARGET_DEATH_BURST_HALF_EXTENT, -TARGET_DEATH_BURST_HALF_EXTENT },
    { -TARGET_DEATH_BURST_HALF_EXTENT, TARGET_DEATH_BURST_HALF_EXTENT },
    { TARGET_DEATH_BURST_HALF_EXTENT, -TARGET_DEATH_BURST_HALF_EXTENT },
    { TARGET_DEATH_BURST_HALF_EXTENT, TARGET_DEATH_BURST_HALF_EXTENT },
};

/// Draws the 512-unit burst replacing the final hit mark on a dead target.
static void _shootingGalleryTargetsDrawTargetDeathBurst(const GfxCoord* rootCoord, const SVECTOR* centerOffset)
{
    enum { TARGET_DEATH_BURST_DEPTH_BIAS = 0xA0 };

    _QuadScratch* projection;
    POLY_FT4*     packet;
    s32           cornerIndex;

    projection = SCRATCH_STACK_RESERVE_BLOCK(_QuadScratch);
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(projection->vertices); cornerIndex++) {
        projection->vertices[cornerIndex].vx = centerOffset->vx + (_gShootingGalleryTargetsTargetDeathBurstCorners[cornerIndex].vx + rootCoord->coord.t[0]);
        projection->vertices[cornerIndex].vy = centerOffset->vy + (_gShootingGalleryTargetsTargetDeathBurstCorners[cornerIndex].vy + rootCoord->coord.t[1]);
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
    packet->tpage = TARGET_EFFECT_TPAGE;
    packet->clut  = TARGET_EFFECT_CLUT;
    setUV4(packet, DEATH_BURST_U, 0, DEATH_BURST_U + DEATH_BURST_SIZE - 1, 0, DEATH_BURST_U, DEATH_BURST_SIZE - 1,
           DEATH_BURST_U + DEATH_BURST_SIZE - 1, DEATH_BURST_SIZE - 1);
    setShadeTex(packet, 1);
    gte_stsxy3(&projection->screenCorners[1], &projection->screenCorners[2], &projection->screenCorners[3]);
    gte_stszotz(&projection->depth);
    projection->depth -= TARGET_DEATH_BURST_DEPTH_BIAS;
    if (projection->depth < TARGET_EFFECT_MIN_DEPTH) {
        SCRATCH_STACK_RELEASE_BLOCK(_QuadScratch);
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
    addPrim(&gGpuCurrentOt[projection->depth >> TARGET_EFFECT_OT_SHIFT], packet);
    SCRATCH_STACK_RELEASE_BLOCK(_QuadScratch);
}

enum { TARGET_HIT_MARK_HALF_EXTENT = 96 };

/// Four X/Y offsets of a target hit-mark quad, followed by an unused zero record.
static const DVECTOR _gShootingGalleryTargetsTargetHitMarkCorners[5] = {
    { -TARGET_HIT_MARK_HALF_EXTENT, -TARGET_HIT_MARK_HALF_EXTENT },
    { -TARGET_HIT_MARK_HALF_EXTENT, TARGET_HIT_MARK_HALF_EXTENT },
    { TARGET_HIT_MARK_HALF_EXTENT, -TARGET_HIT_MARK_HALF_EXTENT },
    { TARGET_HIT_MARK_HALF_EXTENT, TARGET_HIT_MARK_HALF_EXTENT },
    { 0, 0 },
};

/// Draws one 192-unit hit mark on a gallery target's face.
static void _shootingGalleryTargetsDrawTargetHitMark(const GfxCoord* rootCoord, const SVECTOR* centerOffset)
{
    enum { TARGET_HIT_MARK_DEPTH_BIAS = 0x40 };

    _QuadScratch* projection;
    POLY_FT4*     packet;
    s32           cornerIndex;

    projection = SCRATCH_STACK_RESERVE_BLOCK(_QuadScratch);
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(projection->vertices); cornerIndex++) {
        projection->vertices[cornerIndex].vx = centerOffset->vx + (_gShootingGalleryTargetsTargetHitMarkCorners[cornerIndex].vx + rootCoord->coord.t[0]);
        projection->vertices[cornerIndex].vy = centerOffset->vy + (_gShootingGalleryTargetsTargetHitMarkCorners[cornerIndex].vy + rootCoord->coord.t[1]);
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
    packet->tpage = TARGET_EFFECT_TPAGE;
    packet->clut  = TARGET_EFFECT_CLUT;
    setUV4(packet, HIT_MARK_U, 0, HIT_MARK_U + HIT_MARK_SIZE - 1, 0, HIT_MARK_U, HIT_MARK_SIZE - 1, HIT_MARK_U + HIT_MARK_SIZE - 1,
           HIT_MARK_SIZE - 1);
    setShadeTex(packet, 1);
    gte_stsxy3(&projection->screenCorners[1], &projection->screenCorners[2], &projection->screenCorners[3]);
    gte_stszotz(&projection->depth);
    projection->depth -= TARGET_HIT_MARK_DEPTH_BIAS;
    if (projection->depth < TARGET_EFFECT_MIN_DEPTH) {
        SCRATCH_STACK_RELEASE_BLOCK(_QuadScratch);
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
    addPrim(&gGpuCurrentOt[projection->depth >> TARGET_EFFECT_OT_SHIFT], packet);
    SCRATCH_STACK_RELEASE_BLOCK(_QuadScratch);
}

/// Remaps a gallery target's lighting color matrix, including its sine pulse.
static void _shootingGalleryTargetsRemapTargetColor(Enemy* unusedEnemy, MATRIX* colorMatrix, s32 colorMode)
{
    enum { TARGET_COLOR_PULSE_ANGLE_STEP = 198 };

    s32 columnIndex;
    s16 columnValue;

    switch (colorMode) {
        case ENEMY_COLOR_DEFAULT:
            break;
        case ENEMY_COLOR_WEIGHTED:
            for (columnIndex = 0; columnIndex < ARRAY_SIZE(colorMatrix->m[0]); columnIndex++) {
                // Narrow the weighted column before adding the halfword sine pulse.
                columnValue                    = (colorMatrix->m[0][columnIndex] * 7 + colorMatrix->m[1][columnIndex] * 6 + colorMatrix->m[2][columnIndex] * 3) / 33;
                columnValue                   += (s16)(rsin(gDisplayState.loopCount * TARGET_COLOR_PULSE_ANGLE_STEP) + ONE);
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
static inline void _shootingGalleryTargetsCopyTargetLightColor(s16 destination[3][3], const s16 source[3][3])
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
static void _shootingGalleryTargetsUpdateTargetColor(Enemy* enemy, const void* worldPosition, s32 unusedArg2, s32 unusedArg3)
{
    enum {
        UPDATE_PAUSED      = 1,
        BLEND_WEIGHT_SHIFT = 8,
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
    if ((!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && (model->buffer != NULL)) || (gGameSession->sceneUpdatesPaused != UPDATE_PAUSED)) {
        blendScratch = SCRATCH_STACK_RESERVE_BLOCK(WorldCoordActorColorScratch);
        // Sample the three lights before remapping their colour coefficients.
        worldCoordSetModelLighting(model, worldPosition, 0, ARRAY_SIZE(colorMtx->m[0]));
        if ((s8)enemy->colorBlend <= 0) {
            _shootingGalleryTargetsRemapTargetColor(enemy, colorMtx, currentMode);
        } else {
            // Remap two copies of the same sample; ambient is never blended.
            _shootingGalleryTargetsCopyTargetLightColor(blendScratch->previousColor.m, colorMtx->m);
            _shootingGalleryTargetsRemapTargetColor(enemy, colorMtx, currentMode);
            _shootingGalleryTargetsRemapTargetColor(enemy, &blendScratch->previousColor, (enemy->colorMode >> ENEMY_COLOR_PREVIOUS_SHIFT) & ENEMY_COLOR_MODE_MASK);
            previousWeight = (s8)enemy->colorBlend << BLEND_WEIGHT_SHIFT;
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
static void _shootingGalleryTargetsTargetTask(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = _gShootingGalleryTargetsTargetTaskStates;
    handlers.funcs[task->state](task);
}

/// Hides a finished target while its mount keeps ownership of the task.
static void _shootingGalleryTargetsHideTarget(Task* task)
{
    TmdObject* model = task->extra.tmd;

    model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
}

/// Unlinks a gallery target's collision body and destroys its enemy and task.
static void _shootingGalleryTargetsDestroyTarget(Task* task)
{
    _TargetWork* work = task->work;

    worldCollisionUnlinkBody(&work->body);
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Links the gallery target's shot-contact sphere and initializes its eight contacts.
static void _shootingGalleryTargetsInitTargetCollision(Task* task)
{
    enum {
        TARGET_COLLISION_KEY = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x4C,
        SPHERE_CENTER_Y      = -0x250, // Above the root, at the face's centre
        HANGING_RADIUS       = 0x220,
        STANDING_RADIUS      = 0x190,
    };

    _TargetWork*           work      = task->work;
    GfxCoord*              rootCoord = task->extra.tmd->coords;
    WorldCollisionContact* contacts  = work->contacts;

    work->body.coord            = rootCoord;
    work->body.context.contacts = contacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = SPHERE_CENTER_Y;
    work->body.pos.vz           = 0;
    work->body.key              = TARGET_COLLISION_KEY;
    work->body.radius           = (work->mountBehaviour == MOUNT_HANGING) ? HANGING_RADIUS : STANDING_RADIUS;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(work->contacts), 0);
}

/// Refreshes a gallery target's lighting and hit tint from its composed root.
static void _shootingGalleryTargetsUpdateTargetRootColor(Task* task)
{
    GfxCoord* rootCoord;
    void**    cursorSlot;
    VECTOR*   savedCursor;
    VECTOR*   worldPosition;
    Enemy*    enemy;

    enemy                                     = task->spawnArg2.pointer;
    rootCoord                                 = task->extra.tmd->coords;
    cursorSlot                                = SCRATCH_STACK_CURSOR_SLOT;
    savedCursor                               = SCRATCH_STACK_CURSOR_AT(cursorSlot, VECTOR);
    worldPosition                             = savedCursor - 1;
    worldPosition->vx                         = rootCoord->workm.t[0];
    worldPosition->vy                         = rootCoord->workm.t[1];
    worldPosition->vz                         = rootCoord->workm.t[2];
    SCRATCH_STACK_CURSOR_AT(cursorSlot, void) = worldPosition;
    _shootingGalleryTargetsUpdateTargetColor(enemy, worldPosition, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES_AT(cursorSlot, sizeof(*worldPosition));
}

/// Rebuilds a gallery target's local rotation from its wrapped Euler angles.
static void _shootingGalleryTargetsUpdateTargetRotation(Task* task)
{
    _TargetWork* work      = task->work;
    GfxCoord*    rootCoord = task->extra.tmd->coords;
    MATRIX*      rotation;

    work->pitch &= ANGLE_MASK;
    work->yaw   &= ANGLE_MASK;
    work->roll  &= ANGLE_MASK;
    rotation     = SCRATCH_STACK_CURSOR(MATRIX) - 1;
    // Initialize the unpublished rotation before making room for the axis helpers.
    gfxSetRotIdentity(rotation);
    SCRATCH_STACK_CURSOR(MATRIX) = rotation;
    gfxRotMatrixZ(rotation, work->roll, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(rotation, work->pitch, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixY(rotation, work->yaw, GRAPHICS_ROTATION_COMPOSE);
    _shootingGalleryTargetsCopyTargetRotation(rotation, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Copies the nine rotation elements used by the gallery target model.
static void _shootingGalleryTargetsCopyTargetRotation(const MATRIX* source, MATRIX* destination)
{
    gfxCopyRotation(destination, source);
}

/// Enters a gallery target behaviour state and restarts it at step zero.
static void _shootingGalleryTargetsSetTargetState(Task* task, s16 state)
{
    _TargetWork* work = task->work;

    work->state = state;
    work->step  = 0;
}

/// Enters a hit-selected target state and restarts its step sequence.
static inline void _shootingGalleryTargetsEnterTargetHitState(Task* task, s16 state)
{
    _TargetWork* reactionWork = task->work;

    reactionWork->state = state;
    reactionWork->step  = 0;
}

/// Consumes a gallery target's queued reaction when its last hit was damaging.
static s16 _shootingGalleryTargetsConsumeTargetHitReaction(Task* task)
{
    enum {
        HIT_REACTION_RESUME_STATE_4 = 3,
        HIT_REACTION_RESUME_STATE_6 = 4,
        HIT_REACTION_RESUME_STATE_5 = 5,
        TARGET_STATE_RESUME_4       = 4,
        TARGET_STATE_RESUME_5       = 5,
        TARGET_STATE_RESUME_6       = 6,
    };

    _TargetWork* work = task->work;

    if (work->hitTaken == 1) {
        // Preserve the signed halfword dispatch, including wraparound requests.
        switch ((s16)(work->hitReaction - 1)) {
            case HIT_REACTION_LIGHT - 1:
                _shootingGalleryTargetsEnterTargetHitState(task, TARGET_STATE_LIGHT_FLINCH);
                break;
            case HIT_REACTION_HEAVY - 1:
                _shootingGalleryTargetsEnterTargetHitState(task, TARGET_STATE_HEAVY_FLINCH);
                break;
            case HIT_REACTION_RESUME_STATE_4 - 1:
                _shootingGalleryTargetsEnterTargetHitState(task, TARGET_STATE_RESUME_4);
                break;
            case HIT_REACTION_RESUME_STATE_6 - 1:
                _shootingGalleryTargetsEnterTargetHitState(task, TARGET_STATE_RESUME_6);
                break;
            case HIT_REACTION_RESUME_STATE_5 - 1:
                _shootingGalleryTargetsEnterTargetHitState(task, TARGET_STATE_RESUME_5);
                break;
        }
        work->hitReaction = HIT_REACTION_NONE;
        return 1;
    }
    return 0;
}

/// Starts the gallery target's behaviour according to its mount kind.
static void _shootingGalleryTargetsBeginTargetBehaviour(Task* task)
{
    enum {
        INITIAL_SCALE_PERCENT     = 10,
        TARGET_FULL_SCALE_PERCENT = 100,
    };

    _TargetWork* work  = task->work;
    Enemy*       enemy = task->spawnArg2.pointer;

    switch (work->mountBehaviour) {
        case MOUNT_STANDING:
        case MOUNT_HANGING:
            work->state = TARGET_STATE_ACTIVE;
            work->pitch = ANGLE_QUARTER_TURN;
            RANDOM_LCG_ADVANCE();
            work->hitMarkFirst  = RANDOM_LCG_DRAW() & (TARGET_HIT_MARK_COUNT - 1);
            work->widthPercent  = INITIAL_SCALE_PERCENT;
            work->heightPercent = INITIAL_SCALE_PERCENT;
            _shootingGalleryTargetsUpdateTargetRotation(task);
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
            enemy->colorBlend = 0;
            break;
        case MOUNT_FIXED:
            work->state         = TARGET_STATE_FIXED;
            work->widthPercent  = TARGET_FULL_SCALE_PERCENT;
            work->heightPercent = TARGET_FULL_SCALE_PERCENT;
            break;
    }
}

/// Resumes active target behaviour from state-table slot 4.
static void _shootingGalleryTargetsResumeTargetState4(Task* task)
{
    _shootingGalleryTargetsSetTargetState(task, TARGET_STATE_ACTIVE);
}

/// Resumes active target behaviour from state-table slot 5.
static void _shootingGalleryTargetsResumeTargetState5(Task* task)
{
    _shootingGalleryTargetsSetTargetState(task, TARGET_STATE_ACTIVE);
}

/// Resumes active target behaviour from state-table slot 6.
static void _shootingGalleryTargetsResumeTargetState6(Task* task)
{
    _shootingGalleryTargetsSetTargetState(task, TARGET_STATE_ACTIVE);
}

/// Shows three hit marks on a fixed demonstration target and starts its exit.
static void _shootingGalleryTargetsBeginFixedTargetExit(Task* task)
{
    enum { FIXED_TARGET_HIT_MARKS = 3 };

    _TargetWork* work = task->work;

    work->hitMarkCount = FIXED_TARGET_HIT_MARKS;
    _shootingGalleryTargetsSetTargetState(task, TARGET_STATE_LEAVE);
}

/// Stores the XZ distance between the target and player root translations.
static void _shootingGalleryTargetsMeasureTargetPlayerDistance(Task* task)
{
    _TargetWork* work;
    GfxCoord*    rootCoord;
    GfxCoord*    playerCoord;
    void**       scratchSlot;
    VECTOR*      savedCursor;
    VECTOR*      offset;
    s32          distance;

    work                                       = task->work;
    rootCoord                                  = task->extra.tmd->coords;
    scratchSlot                                = SCRATCH_STACK_CURSOR_SLOT;
    savedCursor                                = SCRATCH_STACK_CURSOR_AT(scratchSlot, VECTOR);
    offset                                     = savedCursor - 1;
    SCRATCH_STACK_CURSOR_AT(scratchSlot, void) = offset;
    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] == NULL) {
        SCRATCH_STACK_CURSOR_AT(scratchSlot, void) = savedCursor;
        return;
    }
    // Read local translations directly; the parent hierarchy is not composed here.
    playerCoord = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    offset->vx  = playerCoord->coord.t[0] - rootCoord->coord.t[0];
    offset->vy  = playerCoord->coord.t[1] - rootCoord->coord.t[1];
    offset->vz  = playerCoord->coord.t[2] - rootCoord->coord.t[2];
    distance    = playerActorPlanarLength(offset->vx, offset->vz);
    SCRATCH_STACK_RELEASE_BYTES_AT(scratchSlot, sizeof(*offset));
    // Keep the call result live until the scratch cursor is restored.
    work->playerDistance = distance;
}

/// Places a target root at its rotated local offset (0, -384, 0).
static void _shootingGalleryTargetsPlaceTargetOffset(GfxCoord* rootCoord)
{
    void**  scratchSlot;
    VECTOR* savedCursor;
    VECTOR* offset;

    scratchSlot                                = SCRATCH_STACK_CURSOR_SLOT;
    savedCursor                                = SCRATCH_STACK_CURSOR_AT(scratchSlot, VECTOR);
    offset                                     = savedCursor - 1;
    SCRATCH_STACK_CURSOR_AT(scratchSlot, void) = offset;
    offset->vx                                 = 0;
    offset->vy                                 = -0x180;
    offset->vz                                 = 0;
    ApplyMatrixLV(&rootCoord->coord, offset, offset);
    rootCoord->coord.t[0] = offset->vx;
    rootCoord->coord.t[1] = offset->vy;
    // Cursor restoration leaves these bytes intact for the final Z read.
    SCRATCH_STACK_RELEASE_BYTES_AT(scratchSlot, sizeof(*offset));
    rootCoord->coord.t[2] = offset->vz;
}

/// Applies the target's width and folded-height percentages to its root matrix.
static void _shootingGalleryTargetsApplyTargetScale(Task* task)
{
    enum { TARGET_FULL_SCALE_PERCENT = 100 };

    _TargetWork* work                    = task->work;
    GfxCoord*    rootCoord               = task->extra.tmd->coords;
    s16          widthCoefficient        = rootCoord->coord.m[0][0];
    s16          foldedHeightCoefficient = rootCoord->coord.m[2][1];

    rootCoord->coord.m[0][0] = widthCoefficient / TARGET_FULL_SCALE_PERCENT * work->widthPercent;
    rootCoord->coord.m[2][1] = foldedHeightCoefficient / TARGET_FULL_SCALE_PERCENT * work->heightPercent;
}
