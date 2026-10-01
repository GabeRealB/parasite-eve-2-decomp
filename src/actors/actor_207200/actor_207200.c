#include "actor_207200_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/enemy.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gamemain.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/skull_stalker.h"

extern EnemyParams gSkullStalkerParams;
extern u8          gSkullStalkerAnimSets[];
extern SVECTOR     gSkullStalkerSparkOffset;
extern SVECTOR     gSkullStalkerHitFxOffset;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

extern TmdSource D_actor_207200_8014E4C8;

DamageAttack D_actor_207200_8014DBB8[1] = { 0 };

EnemyParams gSkullStalkerParams = { D_actor_207200_8014DBB8, 1, 2, 32, 1, 100, 20, 100, 99 };

TmdBone D_actor_207200_8014DBCC[3] = {
#include "assets/actor_207200_model_046A8_skeleton.inc"
};

u32 D_actor_207200_8014DC38[3] = {
#include "assets/actor_207200_model_046A8_partVerts.inc"
};

SVECTOR D_actor_207200_8014DC44[37] = {
#include "assets/actor_207200_model_046A8_verts.inc"
};

SVECTOR D_actor_207200_8014DD6C[47] = {
#include "assets/actor_207200_model_046A8_normals.inc"
};

u32 D_actor_207200_8014DEE4[377] = {
#include "assets/actor_207200_model_046A8_stream.inc"
};

TmdSource D_actor_207200_8014E4C8 = {
    0,
    2184,
    332,
    3,
    D_actor_207200_8014DC38,
    D_actor_207200_8014DC44,
    D_actor_207200_8014DD6C,
    D_actor_207200_8014DBCC,
    D_actor_207200_8014DEE4,
};

AnimationPackedPose D_actor_207200_8014E4EC[2] = {
#include "assets/actor_207200_animation_04708_bank1.inc"
};

AnimationPackedRotation D_actor_207200_8014E504[1] = {
#include "assets/actor_207200_animation_04708_bank4.inc"
};

AnimationRecord D_actor_207200_8014E508[6] = {
#include "assets/actor_207200_animation_04708_records.inc"
};

u16 D_actor_207200_8014E520[4] = {
#include "assets/actor_207200_animation_04708_indices.inc"
};

AnimationSet D_actor_207200_8014E528 = {
    D_actor_207200_8014E508,
    D_actor_207200_8014E520,
    { NULL, D_actor_207200_8014E4EC, NULL, NULL, D_actor_207200_8014E504, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_207200_8014E550[24] = {
#include "assets/actor_207200_animation_0495C_bank1.inc"
};

AnimationPackedRotation D_actor_207200_8014E670[10] = {
#include "assets/actor_207200_animation_0495C_bank4.inc"
};

AnimationRecord D_actor_207200_8014E698[55] = {
#include "assets/actor_207200_animation_0495C_records.inc"
};

u16 D_actor_207200_8014E774[4] = {
#include "assets/actor_207200_animation_0495C_indices.inc"
};

AnimationSet D_actor_207200_8014E77C = {
    D_actor_207200_8014E698,
    D_actor_207200_8014E774,
    { NULL, D_actor_207200_8014E550, NULL, NULL, D_actor_207200_8014E670, NULL, NULL, NULL },
};

TaskDesc D_actor_207200_8014E7A4 = { { { TASK_BODY_TMD, 96 } }, skullStalkerTask, { .model = &D_actor_207200_8014E4C8 } };

u8 gSkullStalkerAnimSets[12] = {
    0,
    0,
    0,
    0,
    40,
    229,
    20,
    128,
    124,
    231,
    20,
    128,
};

SVECTOR gSkullStalkerSparkOffset = { 0, -100, 0, 0 };

SVECTOR gSkullStalkerHitFxOffset = { 0, 0, 100, 0 };

DamageAttack D_actor_207200_8014E7CC[2] = { { 25, 11 }, { 10, 0 } };

#include "../../shared/skull_stalker_spawn_state.inc.c"

#include "../../shared/skull_stalker_idle_tick.inc.c"

#include "../../shared/skull_stalker_hits.inc.c"

#include "../../shared/skull_stalker_inlines.inc.c"

#include "../../shared/skull_stalker_death_state.inc.c"

/// The small enemy's state handlers - spawn, live tick and dying tick - which
/// `skullStalkerTask` dispatches through by task state.
static const GpEnemyTaskFuncTable3 gSkullStalkerTaskStates = {
    { skullStalkerSpawnState, skullStalkerUpdateState, skullStalkerDeathState }
};

#include "../../shared/skull_stalker_task.inc.c"

#include "../../shared/skull_stalker_update_state.inc.c"

#include "../../shared/skull_stalker_reaction_flags.inc.c"

#include "../../shared/skull_stalker_reaction_dispatch.inc.c"

#include "../../shared/skull_stalker_animate.inc.c"

/// The Skull Stalker's colour helper is the Sucklerceph's.
#define sucklercephColour skullStalkerColour
#include "../../shared/sucklerceph_colour.inc.c"
#undef sucklercephColour

#include "../../shared/skull_stalker_light_ramp.inc.c"

#include "../../shared/skull_stalker_flatten.inc.c"

#include "../../shared/skull_stalker_exit.inc.c"
