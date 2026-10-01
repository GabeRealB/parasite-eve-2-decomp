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

extern AnimationSet D_actor_105300_8013C9D0;
extern AnimationSet D_actor_105300_8013CE8C;
extern AnimationSet D_actor_105300_8013D368;
extern TmdSource    D_actor_105300_8013C794;

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

SVECTOR gGeneratorSpawnOffsets[2] = {
    { 0, -500, 1000, 0 },
    { 0, -1000, 1500, 0 },
};

SVECTOR gGeneratorHitEffectOffsets[2] = {
    { 0, -500, 1400, 0 },
    { 0, -1000, 1600, 0 },
};

TmdBone D_actor_105300_80133A50[10] = {
#include "assets/beta_generator_body_skeleton.inc"
};

u32 D_actor_105300_80133BB8[10] = {
#include "assets/beta_generator_body_partVerts.inc"
};

SVECTOR D_actor_105300_80133BE0[610] = {
#include "assets/beta_generator_body_verts.inc"
};

SVECTOR D_actor_105300_80134EF0[659] = {
#include "assets/beta_generator_body_normals.inc"
};

u32 D_actor_105300_80136388[6403] = {
#include "assets/beta_generator_body_stream.inc"
};

TmdSource D_actor_105300_8013C794 = {
    0,
    37792,
    7664,
    10,
    D_actor_105300_80133BB8,
    D_actor_105300_80133BE0,
    D_actor_105300_80134EF0,
    D_actor_105300_80133A50,
    D_actor_105300_80136388,
};

AnimationPackedPose D_actor_105300_8013C7B8[5] = {
#include "assets/actor_105300_animation_0ABB0_bank1.inc"
};

AnimationPackedRotation D_actor_105300_8013C7F4[25] = {
#include "assets/actor_105300_animation_0ABB0_bank4.inc"
};

AnimationRecord D_actor_105300_8013C858[89] = {
#include "assets/actor_105300_animation_0ABB0_records.inc"
};

u16 D_actor_105300_8013C9BC[10] = {
#include "assets/actor_105300_animation_0ABB0_indices.inc"
};

AnimationSet D_actor_105300_8013C9D0 = {
    D_actor_105300_8013C858,
    D_actor_105300_8013C9BC,
    { NULL, D_actor_105300_8013C7B8, NULL, NULL, D_actor_105300_8013C7F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105300_8013C9F8[4] = {
#include "assets/actor_105300_animation_0B06C_bank1.inc"
};

AnimationPackedRotation D_actor_105300_8013CA28[120] = {
#include "assets/actor_105300_animation_0B06C_bank4.inc"
};

AnimationRecord D_actor_105300_8013CC08[156] = {
#include "assets/actor_105300_animation_0B06C_records.inc"
};

u16 D_actor_105300_8013CE78[10] = {
#include "assets/actor_105300_animation_0B06C_indices.inc"
};

AnimationSet D_actor_105300_8013CE8C = {
    D_actor_105300_8013CC08,
    D_actor_105300_8013CE78,
    { NULL, D_actor_105300_8013C9F8, NULL, NULL, D_actor_105300_8013CA28, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105300_8013CEB4[4] = {
#include "assets/actor_105300_animation_0B548_bank1.inc"
};

AnimationPackedRotation D_actor_105300_8013CEE4[124] = {
#include "assets/actor_105300_animation_0B548_bank4.inc"
};

AnimationRecord D_actor_105300_8013D0D4[160] = {
#include "assets/actor_105300_animation_0B548_records.inc"
};

u16 D_actor_105300_8013D354[10] = {
#include "assets/actor_105300_animation_0B548_indices.inc"
};

AnimationSet D_actor_105300_8013D368 = {
    D_actor_105300_8013D0D4,
    D_actor_105300_8013D354,
    { NULL, D_actor_105300_8013CEB4, NULL, NULL, D_actor_105300_8013CEE4, NULL, NULL, NULL },
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

GeneratorSndRow gGeneratorViewSound[7] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 15, 0, 76, 0 },
    { -15, -1, 70, 0 },
    { -14, -1, 64, 0 },
    { -15, -1, 38, 0 },
    { 12, 0, 38, 0 },
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
    { { { TASK_BODY_TMD, 96 } }, generatorTask, { .model = &D_actor_105300_8013C794 } },
    { { { TASK_BODY_COORD, 96 } }, generatorLifeSupportTask, { .value = 0 } },
};

AnimationSet* gGeneratorAnimSets[4] = {
    NULL,
    &D_actor_105300_8013C9D0,
    &D_actor_105300_8013CE8C,
    &D_actor_105300_8013D368,
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
