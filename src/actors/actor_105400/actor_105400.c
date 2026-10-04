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
#define GENERATOR_KIND GENERATOR_PROTO
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

static AnimationSet _gActor105400Animation0A7C0;
static AnimationSet _gActor105400Animation0AC00;
static AnimationSet _gActor105400Animation0AFE8;
static TmdSource    _gActor105400ProtoGeneratorBody;

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

SVECTOR gGeneratorSpawnOffsets[2] = { { 0, -500, 1000, 0 }, { 0, -1000, 1500, 0 } };

SVECTOR gGeneratorHitEffectOffsets[2] = {
    { 0, -500, 1400, 0 },
    { 0, -1000, 1600, 0 },
};

static TmdBone _gActor105400ProtoGeneratorBodySkeleton[10] = {
#include "assets/proto_generator_body_skeleton.inc"
};

static u32 _gActor105400ProtoGeneratorBodyPartVerts[10] = {
#include "assets/proto_generator_body_partVerts.inc"
};

static SVECTOR _gActor105400ProtoGeneratorBodyVerts[603] = {
#include "assets/proto_generator_body_verts.inc"
};

static SVECTOR _gActor105400ProtoGeneratorBodyNormals[641] = {
#include "assets/proto_generator_body_normals.inc"
};

static u32 _gActor105400ProtoGeneratorBodyStream[6251] = {
#include "assets/proto_generator_body_stream.inc"
};

static TmdSource _gActor105400ProtoGeneratorBody = {
    0,
    37128,
    7312,
    10,
    _gActor105400ProtoGeneratorBodyPartVerts,
    _gActor105400ProtoGeneratorBodyVerts,
    _gActor105400ProtoGeneratorBodyNormals,
    _gActor105400ProtoGeneratorBodySkeleton,
    _gActor105400ProtoGeneratorBodyStream,
};

static AnimationPackedPose _gActor105400Animation0A7C0Bank1[4] = {
#include "assets/actor_105400_animation_0A7C0_bank1.inc"
};

static AnimationPackedRotation _gActor105400Animation0A7C0Bank4[9] = {
#include "assets/actor_105400_animation_0A7C0_bank4.inc"
};

static AnimationRecord _gActor105400Animation0A7C0Records[58] = {
#include "assets/actor_105400_animation_0A7C0_records.inc"
};

static u16 _gActor105400Animation0A7C0Indices[10] = {
#include "assets/actor_105400_animation_0A7C0_indices.inc"
};

static AnimationSet _gActor105400Animation0A7C0 = {
    _gActor105400Animation0A7C0Records,
    _gActor105400Animation0A7C0Indices,
    { NULL, _gActor105400Animation0A7C0Bank1, NULL, NULL, _gActor105400Animation0A7C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105400Animation0AC00Bank1[11] = {
#include "assets/actor_105400_animation_0AC00_bank1.inc"
};

static AnimationPackedRotation _gActor105400Animation0AC00Bank4[90] = {
#include "assets/actor_105400_animation_0AC00_bank4.inc"
};

static AnimationRecord _gActor105400Animation0AC00Records[134] = {
#include "assets/actor_105400_animation_0AC00_records.inc"
};

static u16 _gActor105400Animation0AC00Indices[10] = {
#include "assets/actor_105400_animation_0AC00_indices.inc"
};

static AnimationSet _gActor105400Animation0AC00 = {
    _gActor105400Animation0AC00Records,
    _gActor105400Animation0AC00Indices,
    { NULL, _gActor105400Animation0AC00Bank1, NULL, NULL, _gActor105400Animation0AC00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor105400Animation0AFE8Bank1[8] = {
#include "assets/actor_105400_animation_0AFE8_bank1.inc"
};

static AnimationPackedRotation _gActor105400Animation0AFE8Bank4[85] = {
#include "assets/actor_105400_animation_0AFE8_bank4.inc"
};

static AnimationRecord _gActor105400Animation0AFE8Records[126] = {
#include "assets/actor_105400_animation_0AFE8_records.inc"
};

static u16 _gActor105400Animation0AFE8Indices[10] = {
#include "assets/actor_105400_animation_0AFE8_indices.inc"
};

static AnimationSet _gActor105400Animation0AFE8 = {
    _gActor105400Animation0AFE8Records,
    _gActor105400Animation0AFE8Indices,
    { NULL, _gActor105400Animation0AFE8Bank1, NULL, NULL, _gActor105400Animation0AFE8Bank4, NULL, NULL, NULL },
};

EnemyParams gGeneratorParams = { NULL, 250, 200, 100, 100, 100, 0, 0, 0 };

EnemyParams gGeneratorLifeSupportParams = { NULL, 250, 0, 0, 0, 100, 0, 0, 0 };

s32 gGeneratorSoundIds[3] = {
    0x55110003,
    0x55110004,
    0x55110005,
};

u32 gGeneratorPulseSoundId = 0x55110008;

u32 gGeneratorSpawnSound = 0x55110009;

GeneratorViewSound gGeneratorViewSound[8] = {
    { 0, 0 },
    { 0, 0 },
    { -4, 64 },
    { -15, 76 },
    { 15, 76 },
    { -15, 51 },
    { 2, 64 },
    { 12, 32 },
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
    { { { TASK_BODY_TMD, 96 } }, generatorTask, { .model = &_gActor105400ProtoGeneratorBody } },
    { { { TASK_BODY_COORD, 96 } }, generatorLifeSupportTask, { .value = 0 } },
};

AnimationSet* gGeneratorAnimSets[4] = {
    NULL,
    &_gActor105400Animation0A7C0,
    &_gActor105400Animation0AC00,
    &_gActor105400Animation0AFE8,
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
