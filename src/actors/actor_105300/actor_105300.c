#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/neo_ark_power_plant_1.h"

#include "rooms/neo_ark_power_plant_2.h"
#include "../../shared/model_placement.h"
/// Selects the Beta Generator of Power Plant 2 for the shared generator instance.
///
/// Bind to a C integer constant, `GENERATOR_BETA` (0) or `GENERATOR_PROTO` (1),
/// before `generator.h` and keep it defined through all generator fragments.
/// Selects the body target offset and collision key, and seeds the body's s16
/// kind; the Life Support child inherits it for position and room-state updates.
/// The constants are declared by the header, so use this binding in C expressions.
#define GENERATOR_KIND GENERATOR_BETA
/// Binds the shared generator implementation to this package's spawn table.
///
/// Names a `TaskDesc[GENERATOR_TASK_DESC_COUNT]` array; keep the binding defined
/// through `generator.h` and all included generator fragments.
#define GENERATOR_TASK_TABLE gActor105300GeneratorTasks
#include "../../shared/generator.h"

static void _generatorDeathState(Enemy* enemy, Task* task);

static void _generatorBodyHit(Task* task);
static s32  _generatorIsAlive(Task* task, s32 messageId, s32 unusedArg2, s32 unusedArg3);
static void _generatorLifeSupportHit(Enemy* enemy, Task* task);
static void _generatorLifeSupportTask(Task* task);
static void _generatorPulse(Task* task);
static void _generatorRegenerate(Task* task);
static s32  _generatorSetReleaseBits(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg3);
static void _generatorSpawn(Enemy* enemy, Task* task);
static void _generatorTask(Task* task);
static void _generatorUpdateAnimation(Task* task);
static void _generatorTickState(Enemy* enemy, Task* task);
static void _generatorUpdateColor(Task* task);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static AnimationSet _gActor105300Animation0ABB0;
static AnimationSet _gActor105300Animation0B06C;
static AnimationSet _gActor105300Animation0B548;
static TmdSource    _gActor105300BetaGeneratorBody;

/// Body message handlers for script release commands and combat presence.
///
/// Borrowed by the body task until teardown; the end marker rejects other IDs.
static TaskMessageEntry _gGeneratorMessages[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _generatorSetReleaseBits },
    { ACTOR_MESSAGE_IS_PRESENT, _generatorIsAlive },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Blend duration in whole frames for each body animation-set index.
///
/// Index 0 is unused; idle seeks without blending, hit and death blend for four frames.
/// Hit recovery waits another 35 frames beyond this duration.
static s16 _gGeneratorAnimationBlendFrames[4] = {
    [0]                    = 0,
    [GENERATOR_ANIM_IDLE]  = 0,
    [GENERATOR_ANIM_HIT]   = 4,
    [GENERATOR_ANIM_DEATH] = 4,
};

/// Life Support target positions in world game units, indexed by generator kind.
///
/// The coordinate-only child is placed beneath the view coordinate, independently
/// of the body model root.
static GeneratorLifeSupportPos _gGeneratorLifeSupportPositions[2] = {
    [GENERATOR_BETA]  = { 6140, -6090, -8500 },
    [GENERATOR_PROTO] = { 1865, -1095, -4500 },
};

/// Preserved per-kind arguments to the battle-reward release call.
///
/// The release routine ignores this argument; these values do not select rewards.
static s16 _gGeneratorUnusedBattleReleaseArgs[2] = {
    [GENERATOR_BETA]  = 53,
    [GENERATOR_PROTO] = 54,
};

/// Body target-sphere centres in root-local game units, indexed by generator kind.
///
/// Also used as the enemy damage/lock-on point; the model root itself stays at
/// its area placement.
static SVECTOR _gGeneratorTargetOffsets[2] = {
    [GENERATOR_BETA]  = { 0, -500, 1000, 0 },
    [GENERATOR_PROTO] = { 0, -1000, 1500, 0 },
};

/// Body hit-effect origins in root-local game units, indexed by generator kind.
///
/// Shared by critical-hit feedback and the weapon-selected hit effects.
static SVECTOR _gGeneratorHitEffectOffsets[2] = {
    [GENERATOR_BETA]  = { 0, -500, 1400, 0 },
    [GENERATOR_PROTO] = { 0, -1000, 1600, 0 },
};

static TmdBone _gActor105300BetaGeneratorBodySkeleton[10] = {
#include "assets/beta_generator_body_skeleton.inc"
};

static u32 _gActor105300BetaGeneratorBodyPartVerts[10] = {
#include "assets/beta_generator_body_partVerts.inc"
};

static SVECTOR _gActor105300BetaGeneratorBodyVerts[610] = {
#include "assets/beta_generator_body_verts.inc"
};

static SVECTOR _gActor105300BetaGeneratorBodyNormals[659] = {
#include "assets/beta_generator_body_normals.inc"
};

static u32 _gActor105300BetaGeneratorBodyStream[6403] = {
#include "assets/beta_generator_body_stream.inc"
};

static TmdSource _gActor105300BetaGeneratorBody = {
    0,
    37792,
    7664,
    10,
    _gActor105300BetaGeneratorBodyPartVerts,
    _gActor105300BetaGeneratorBodyVerts,
    _gActor105300BetaGeneratorBodyNormals,
    _gActor105300BetaGeneratorBodySkeleton,
    _gActor105300BetaGeneratorBodyStream,
};

static AnimationPackedPose _gActor105300Animation0ABB0Bank1[5] = {
#include "assets/actor_105300_animation_0ABB0_bank1.inc"
};

static AnimationPackedRotation _gActor105300Animation0ABB0Bank4[25] = {
#include "assets/actor_105300_animation_0ABB0_bank4.inc"
};

static AnimationRecord _gActor105300Animation0ABB0Records[89] = {
#include "assets/actor_105300_animation_0ABB0_records.inc"
};

static u16 _gActor105300Animation0ABB0Indices[10] = {
#include "assets/actor_105300_animation_0ABB0_indices.inc"
};

static AnimationSet _gActor105300Animation0ABB0 = {
    _gActor105300Animation0ABB0Records,
    _gActor105300Animation0ABB0Indices,
    { NULL, _gActor105300Animation0ABB0Bank1, NULL, NULL, _gActor105300Animation0ABB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105300Animation0B06CBank1[4] = {
#include "assets/actor_105300_animation_0B06C_bank1.inc"
};

static AnimationPackedRotation _gActor105300Animation0B06CBank4[120] = {
#include "assets/actor_105300_animation_0B06C_bank4.inc"
};

static AnimationRecord _gActor105300Animation0B06CRecords[156] = {
#include "assets/actor_105300_animation_0B06C_records.inc"
};

static u16 _gActor105300Animation0B06CIndices[10] = {
#include "assets/actor_105300_animation_0B06C_indices.inc"
};

static AnimationSet _gActor105300Animation0B06C = {
    _gActor105300Animation0B06CRecords,
    _gActor105300Animation0B06CIndices,
    { NULL, _gActor105300Animation0B06CBank1, NULL, NULL, _gActor105300Animation0B06CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105300Animation0B548Bank1[4] = {
#include "assets/actor_105300_animation_0B548_bank1.inc"
};

static AnimationPackedRotation _gActor105300Animation0B548Bank4[124] = {
#include "assets/actor_105300_animation_0B548_bank4.inc"
};

static AnimationRecord _gActor105300Animation0B548Records[160] = {
#include "assets/actor_105300_animation_0B548_records.inc"
};

static u16 _gActor105300Animation0B548Indices[10] = {
#include "assets/actor_105300_animation_0B548_indices.inc"
};

static AnimationSet _gActor105300Animation0B548 = {
    _gActor105300Animation0B548Records,
    _gActor105300Animation0B548Indices,
    { NULL, _gActor105300Animation0B548Bank1, NULL, NULL, _gActor105300Animation0B548Bank4, NULL, NULL, NULL },
};

/// Body starting HP and battle rewards for the Beta Generator.
///
/// Shared by this overlay's body enemies; the starting HP also caps regeneration.
/// No attacks are emitted by the stationary body.
static EnemyParams _gGeneratorBodyParams = {
    .attacks              = NULL,
    .hpMax                = 500,
    .exp                  = 400,
    .bp                   = 200,
    .mp                   = 100,
    .critChance           = 100,
    .buildupSteps         = 0,
    .damageOverTimeChance = 0,
    .damageOverTimeTicks  = 0,
};

/// Life Support starting HP and hit-response parameters.
///
/// The child starts at 250 HP, has no attack table and grants no battle rewards.
static EnemyParams _gGeneratorLifeSupportParams = {
    .attacks              = NULL,
    .hpMax                = 250,
    .exp                  = 0,
    .bp                   = 0,
    .mp                   = 0,
    .critChance           = 100,
    .buildupSteps         = 0,
    .damageOverTimeChance = 0,
    .damageOverTimeTicks  = 0,
};

/// Room-bank cue bases for Life Support hit, Life Support break and body hit.
///
/// Rows 0..2 follow that order; each request adds the enemy placement index in
/// the instance byte before starting the cue at the struck part.
static s32 _gGeneratorHitSoundIds[3] = {
    SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2, GENERATOR_SOUND_ENTRY_LIFE_SUPPORT_HIT),
    SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2, GENERATOR_SOUND_ENTRY_LIFE_SUPPORT_BREAK),
    SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2, GENERATOR_SOUND_ENTRY_BODY_HIT),
};

/// Room-bank cue base played on the final frame of an idle scale pulse.
///
/// The enemy placement index supplies the instance byte; mixing uses the body
/// root's current projected audio position.
static u32 _gGeneratorPulseSoundId = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2, GENERATOR_SOUND_ENTRY_IDLE_PULSE);

/// Room-bank cue base for the running sound started with the body.
///
/// The placement index supplies the instance byte. The task retains the full ID
/// for per-view mixing and stops it when Life Support tears down.
static u32 _gGeneratorRunningSoundId = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2, GENERATOR_SOUND_ENTRY_RUNNING);

/// Running-sound pan and attenuation indexed by the one-based logical room view.
///
/// Ordinary active-body views are 2..6; rows 0 and 1 are zero. Higher
/// cinematic views are selected after body death, which does not read this table.
/// Spawn and active updates require a view in the ordinary range.
/// Entries are signed halfwords narrowed to signed bytes by the sound API.
static GeneratorViewSound _gGeneratorViewSound[7] = {
    { .panOffset = 0, .attenuation = 0 },
    { .panOffset = 0, .attenuation = 0 },
    { .panOffset = 15, .attenuation = 76 },
    { .panOffset = -15, .attenuation = 70 },
    { .panOffset = -14, .attenuation = 64 },
    { .panOffset = -15, .attenuation = 38 },
    { .panOffset = 12, .attenuation = 38 },
};

/// Three-frame uniform idle scale pulse, with twelve fractional bits.
///
/// Played one row per active frame after the idle delay; the last row is applied
/// before restarting the delay and emitting the pulse sound.
static GeneratorPulseFrame _gGeneratorIdlePulse[3] = {
    { .last = 0, .scale = ONE - 64 },
    { .last = 0, .scale = ONE },
    { .last = 1, .scale = ONE + 64 },
};

/// Four-frame hit scale pulse, with twelve fractional bits.
///
/// Applied uniformly for a surviving hit and multiplied into the Y shrink during
/// death flicker. Both players apply the last row before leaving the clip.
static GeneratorPulseFrame _gGeneratorHitPulse[4] = {
    { .last = 0, .scale = ONE - 128 },
    { .last = 0, .scale = ONE - 256 },
    { .last = 0, .scale = ONE },
    { .last = 1, .scale = ONE + 128 },
};

TaskDesc GENERATOR_TASK_TABLE[GENERATOR_TASK_DESC_COUNT] = {
    [GENERATOR_BODY_TASK_INDEX]         = { { { TASK_BODY_TMD, GENERATOR_TASK_PRIORITY } }, _generatorTask, { .model = &_gActor105300BetaGeneratorBody } },
    [GENERATOR_LIFE_SUPPORT_TASK_INDEX] = { { { TASK_BODY_COORD, GENERATOR_TASK_PRIORITY } }, _generatorLifeSupportTask, { .value = 0 } },
};

/// Body animation sets indexed by `GENERATOR_ANIM_*`, with an empty entry 0.
///
/// Borrowed by the task's animation context; idle, hit and death drive model
/// slots 1..9 for the lifetime of this overlay.
static AnimationSet* _gGeneratorAnimSets[4] = {
    [0]                    = NULL,
    [GENERATOR_ANIM_IDLE]  = &_gActor105300Animation0ABB0,
    [GENERATOR_ANIM_HIT]   = &_gActor105300Animation0B06C,
    [GENERATOR_ANIM_DEATH] = &_gActor105300Animation0B548,
};

#include "../../shared/generator_body_hit.inc.c"

#include "../../shared/generator_pulse.inc.c"

#include "../../shared/generator_inlines.inc.c"

#include "../../shared/generator_death.inc.c"

#include "../../shared/generator_weak_point_spawn.inc.c"

#include "../../shared/generator_weak_point_hit.inc.c"

#include "../../shared/generator_spawn.inc.c"

#include "../../shared/generator_tick.inc.c"

#include "../../shared/generator_regenerate.inc.c"

#include "../../shared/generator_update_color.inc.c"

#include "../../shared/generator_tick_pose.inc.c"

#include "../../shared/model_placement_scale.inc.c"

/// Life Support handlers indexed by `Task::state`: spawn, active hit processing and teardown.
///
/// The dispatcher copies all three typed callbacks before selecting one.
static const EnemyTaskFuncTable3 _gGeneratorLifeSupportStates = {
    {
        [GENERATOR_TASK_SPAWN]    = _generatorLifeSupportSpawn,
        [GENERATOR_TASK_ACTIVE]   = _generatorLifeSupportHit,
        [GENERATOR_TASK_TEARDOWN] = _generatorLifeSupportTeardown,
    },
};

#include "../../shared/generator_life_support_task.inc.c"

#include "../../shared/generator_weak_point_teardown.inc.c"

#include "../../shared/generator_release_bits.inc.c"

#include "../../shared/generator_is_alive.inc.c"

/// Body handlers indexed by `Task::state`: spawn, active update and held death.
///
/// The dispatcher copies all three typed callbacks before selecting one.
static const EnemyTaskFuncTable3 _gGeneratorTaskStates = {
    {
        [GENERATOR_TASK_SPAWN]    = _generatorSpawn,
        [GENERATOR_TASK_ACTIVE]   = _generatorTickState,
        [GENERATOR_TASK_TEARDOWN] = _generatorDeathState,
    },
};

#include "../../shared/generator_task.inc.c"
