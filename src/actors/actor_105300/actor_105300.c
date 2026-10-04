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
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
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
#define GENERATOR_KIND GENERATOR_BETA
// Exported instance: a room spawns from this package's table by name.
#define gGeneratorTasks gActor105300GeneratorTasks
#include "../../shared/generator.h"

extern EnemyParams             gGeneratorLifeSupportParams;
extern GeneratorLifeSupportPos gGeneratorLifeSupportPos[2];
extern GeneratorPulseFrame     gGeneratorIdlePulse[];
extern GeneratorViewSound      gGeneratorViewSound[];
extern u32                     gGeneratorPulseSoundId;
extern s32                     gGeneratorSoundIds[3];
extern SVECTOR                 gGeneratorHitEffectOffsets[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern SVECTOR       gGeneratorSpawnOffsets[2];
extern EnemyParams   gGeneratorParams;
extern u32           gGeneratorSpawnSound;
extern AnimationSet* gGeneratorAnimSets[];
extern TaskDesc      gGeneratorTasks[2];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

extern GeneratorPulseFrame gGeneratorHitPulse[];
extern s16                 gGeneratorPoseStartFrames[];
extern s16                 gGeneratorReleaseIds[];

static AnimationSet _gActor105300Animation0ABB0;
static AnimationSet _gActor105300Animation0B06C;
static AnimationSet _gActor105300Animation0B548;
static TmdSource    _gActor105300BetaGeneratorBody;

TaskMessageEntry gGeneratorMessages[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, generatorSetReleaseBits },
    { ACTOR_MESSAGE_IS_PRESENT, generatorIsAlive },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s16 gGeneratorPoseStartFrames[4] = {
    0,
    0,
    4,
    4,
};

GeneratorLifeSupportPos gGeneratorLifeSupportPos[2] = {
    { 6140, -6090, -8500 },
    { 1865, -1095, -4500 },
};

s16 gGeneratorReleaseIds[2] = {
    53,
    54,
};

SVECTOR gGeneratorSpawnOffsets[2] = {
    { 0, -500, 1000, 0 },
    { 0, -1000, 1500, 0 },
};

SVECTOR gGeneratorHitEffectOffsets[2] = {
    { 0, -500, 1400, 0 },
    { 0, -1000, 1600, 0 },
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

EnemyParams gGeneratorParams = { NULL, 500, 400, 200, 100, 100, 0, 0, 0 };

EnemyParams gGeneratorLifeSupportParams = { NULL, 250, 0, 0, 0, 100, 0, 0, 0 };

s32 gGeneratorSoundIds[3] = {
    0x55100003,
    0x55100004,
    0x55100005,
};

u32 gGeneratorPulseSoundId = 0x55100008;

u32 gGeneratorSpawnSound = 0x55100009;

GeneratorViewSound gGeneratorViewSound[7] = {
    { 0, 0 },
    { 0, 0 },
    { 15, 76 },
    { -15, 70 },
    { -14, 64 },
    { -15, 38 },
    { 12, 38 },
};

GeneratorPulseFrame gGeneratorIdlePulse[3] = {
    { 0, 4032 },
    { 0, 4096 },
    { 1, 4160 },
};

GeneratorPulseFrame gGeneratorHitPulse[4] = {
    { 0, 3968 },
    { 0, 3840 },
    { 0, 4096 },
    { 1, 4224 },
};

TaskDesc gGeneratorTasks[2] = {
    { { { TASK_BODY_TMD, 96 } }, generatorTask, { .model = &_gActor105300BetaGeneratorBody } },
    { { { TASK_BODY_COORD, 96 } }, generatorLifeSupportTask, { .value = 0 } },
};

AnimationSet* gGeneratorAnimSets[4] = {
    NULL,
    &_gActor105300Animation0ABB0,
    &_gActor105300Animation0B06C,
    &_gActor105300Animation0B548,
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

/// State handlers of the part task, indexed by `Task::state`: spawn, per-frame
/// hit reaction and teardown.
static const EnemyTaskFuncTable3 gGeneratorLifeSupportStates = {
    {
        generatorLifeSupportSpawn,
        generatorLifeSupportHit,
        generatorLifeSupportTeardown,
    },
};

#include "../../shared/generator_life_support_task.inc.c"

#include "../../shared/generator_weak_point_teardown.inc.c"

#include "../../shared/generator_release_bits.inc.c"

#include "../../shared/generator_is_alive.inc.c"

/// State handlers of the main task, indexed by `Task::state`: spawn, per-frame
/// tick and death.
static const EnemyTaskFuncTable3 gGeneratorTaskStates = {
    {
        generatorSpawn,
        generatorTickState,
        generatorDeathState,
    },
};

#include "../../shared/generator_task.inc.c"
