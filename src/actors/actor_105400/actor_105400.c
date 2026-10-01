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

extern EnemyParams       gGeneratorLifeSupportParams;
extern GeneratorSpawnPos gGeneratorLifeSupportPos[2];
extern GeneratorClip     gGeneratorIdlePulse[];
extern GeneratorSndRow   gGeneratorViewSound[];
extern u32               gGeneratorPulseSoundId;
extern s32               gGeneratorSoundIds[3];
extern SVECTOR           gGeneratorHitEffectOffsets[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
STATIC_ASSERT_SIZEOF(GeneratorMsgEntry, 8);

extern SVECTOR       gGeneratorSpawnOffsets[2];
extern EnemyParams   gGeneratorParams;
extern u32           gGeneratorSpawnSound;
extern AnimationSet* gGeneratorAnimSets[];
extern TaskDesc      gGeneratorTasks[2];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

extern GeneratorClip gGeneratorHitPulse[];
extern s16           gGeneratorPoseStartFrames[];
extern s16           gGeneratorReleaseIds[];

extern AnimationSet D_actor_105400_8013C5E0;
extern AnimationSet D_actor_105400_8013CA20;
extern AnimationSet D_actor_105400_8013CE08;
extern TmdSource    D_actor_105400_8013C46C;

GeneratorMsgEntry gGeneratorMessages[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = generatorSetReleaseBits } },
    { 2006, { .call0 = generatorIsAlive } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

s16 gGeneratorPoseStartFrames[4] = {
    0,
    0,
    4,
    4,
};

GeneratorSpawnPos gGeneratorLifeSupportPos[2] = {
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

TmdBone D_actor_105400_80133A50[10] = {
#include "assets/actor_105400_model_0A64C_skeleton.inc"
};

u32 D_actor_105400_80133BB8[10] = {
#include "assets/actor_105400_model_0A64C_partVerts.inc"
};

SVECTOR D_actor_105400_80133BE0[603] = {
#include "assets/actor_105400_model_0A64C_verts.inc"
};

SVECTOR D_actor_105400_80134EB8[641] = {
#include "assets/actor_105400_model_0A64C_normals.inc"
};

u32 D_actor_105400_801362C0[6251] = {
#include "assets/actor_105400_model_0A64C_stream.inc"
};

TmdSource D_actor_105400_8013C46C = {
    0,
    37128,
    7312,
    10,
    D_actor_105400_80133BB8,
    D_actor_105400_80133BE0,
    D_actor_105400_80134EB8,
    D_actor_105400_80133A50,
    D_actor_105400_801362C0,
};

AnimationPackedPose D_actor_105400_8013C490[4] = {
#include "assets/actor_105400_animation_0A7C0_bank1.inc"
};

AnimationPackedRotation D_actor_105400_8013C4C0[9] = {
#include "assets/actor_105400_animation_0A7C0_bank4.inc"
};

AnimationRecord D_actor_105400_8013C4E4[58] = {
#include "assets/actor_105400_animation_0A7C0_records.inc"
};

u16 D_actor_105400_8013C5CC[10] = {
#include "assets/actor_105400_animation_0A7C0_indices.inc"
};

AnimationSet D_actor_105400_8013C5E0 = {
    D_actor_105400_8013C4E4,
    D_actor_105400_8013C5CC,
    { NULL, D_actor_105400_8013C490, NULL, NULL, D_actor_105400_8013C4C0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105400_8013C608[11] = {
#include "assets/actor_105400_animation_0AC00_bank1.inc"
};

AnimationPackedRotation D_actor_105400_8013C68C[90] = {
#include "assets/actor_105400_animation_0AC00_bank4.inc"
};

AnimationRecord D_actor_105400_8013C7F4[134] = {
#include "assets/actor_105400_animation_0AC00_records.inc"
};

u16 D_actor_105400_8013CA0C[10] = {
#include "assets/actor_105400_animation_0AC00_indices.inc"
};

AnimationSet D_actor_105400_8013CA20 = {
    D_actor_105400_8013C7F4,
    D_actor_105400_8013CA0C,
    { NULL, D_actor_105400_8013C608, NULL, NULL, D_actor_105400_8013C68C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105400_8013CA48[8] = {
#include "assets/actor_105400_animation_0AFE8_bank1.inc"
};

AnimationPackedRotation D_actor_105400_8013CAA8[85] = {
#include "assets/actor_105400_animation_0AFE8_bank4.inc"
};

AnimationRecord D_actor_105400_8013CBFC[126] = {
#include "assets/actor_105400_animation_0AFE8_records.inc"
};

u16 D_actor_105400_8013CDF4[10] = {
#include "assets/actor_105400_animation_0AFE8_indices.inc"
};

AnimationSet D_actor_105400_8013CE08 = {
    D_actor_105400_8013CBFC,
    D_actor_105400_8013CDF4,
    { NULL, D_actor_105400_8013CA48, NULL, NULL, D_actor_105400_8013CAA8, NULL, NULL, NULL },
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

GeneratorSndRow gGeneratorViewSound[8] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { -4, -1, 64, 0 },
    { -15, -1, 76, 0 },
    { 15, 0, 76, 0 },
    { -15, -1, 51, 0 },
    { 2, 0, 64, 0 },
    { 12, 0, 32, 0 },
};

GeneratorClip gGeneratorIdlePulse[3] = {
    { 0, 4032 },
    { 0, 4096 },
    { 1, 4160 },
};

GeneratorClip gGeneratorHitPulse[4] = {
    { 0, 3968 },
    { 0, 3840 },
    { 0, 4096 },
    { 1, 4224 },
};

TaskDesc gGeneratorTasks[2] = {
    { { { TASK_BODY_TMD, 96 } }, generatorTask, { .model = &D_actor_105400_8013C46C } },
    { { { TASK_BODY_COORD, 96 } }, generatorLifeSupportTask, { .value = 0 } },
};

AnimationSet* gGeneratorAnimSets[4] = {
    NULL,
    &D_actor_105400_8013C5E0,
    &D_actor_105400_8013CA20,
    &D_actor_105400_8013CE08,
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
static const GpEnemyTaskFuncTable3 gGeneratorLifeSupportStates = {
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
static const GpEnemyTaskFuncTable3 gGeneratorTaskStates = {
    {
        generatorSpawn,
        generatorTickState,
        generatorDeathState,
    },
};

#include "../../shared/generator_task.inc.c"
