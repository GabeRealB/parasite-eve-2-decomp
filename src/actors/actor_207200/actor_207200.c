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

static TmdSource _gActor207200SkullStalkerBody;

DamageAttack D_actor_207200_8014DBB8[1] = { 0 };

EnemyParams gSkullStalkerParams = { D_actor_207200_8014DBB8, 1, 2, 32, 1, 100, 20, 100, 99 };

static TmdBone _gActor207200SkullStalkerBodySkeleton[3] = {
#include "assets/skull_stalker_body_skeleton.inc"
};

static u32 _gActor207200SkullStalkerBodyPartVerts[3] = {
#include "assets/skull_stalker_body_partVerts.inc"
};

static SVECTOR _gActor207200SkullStalkerBodyVerts[37] = {
#include "assets/skull_stalker_body_verts.inc"
};

static SVECTOR _gActor207200SkullStalkerBodyNormals[47] = {
#include "assets/skull_stalker_body_normals.inc"
};

static u32 _gActor207200SkullStalkerBodyStream[377] = {
#include "assets/skull_stalker_body_stream.inc"
};

static TmdSource _gActor207200SkullStalkerBody = {
    0,
    2184,
    332,
    3,
    _gActor207200SkullStalkerBodyPartVerts,
    _gActor207200SkullStalkerBodyVerts,
    _gActor207200SkullStalkerBodyNormals,
    _gActor207200SkullStalkerBodySkeleton,
    _gActor207200SkullStalkerBodyStream,
};

static AnimationPackedPose _gActor207200Animation04708Bank1[2] = {
#include "assets/actor_207200_animation_04708_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation04708Bank4[1] = {
#include "assets/actor_207200_animation_04708_bank4.inc"
};

static AnimationRecord _gActor207200Animation04708Records[6] = {
#include "assets/actor_207200_animation_04708_records.inc"
};

static u16 _gActor207200Animation04708Indices[4] = {
#include "assets/actor_207200_animation_04708_indices.inc"
};

static AnimationSet _gActor207200Animation04708 = {
    _gActor207200Animation04708Records,
    _gActor207200Animation04708Indices,
    { NULL, _gActor207200Animation04708Bank1, NULL, NULL, _gActor207200Animation04708Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor207200Animation0495CBank1[24] = {
#include "assets/actor_207200_animation_0495C_bank1.inc"
};

static AnimationPackedRotation _gActor207200Animation0495CBank4[10] = {
#include "assets/actor_207200_animation_0495C_bank4.inc"
};

static AnimationRecord _gActor207200Animation0495CRecords[55] = {
#include "assets/actor_207200_animation_0495C_records.inc"
};

static u16 _gActor207200Animation0495CIndices[4] = {
#include "assets/actor_207200_animation_0495C_indices.inc"
};

static AnimationSet _gActor207200Animation0495C = {
    _gActor207200Animation0495CRecords,
    _gActor207200Animation0495CIndices,
    { NULL, _gActor207200Animation0495CBank1, NULL, NULL, _gActor207200Animation0495CBank4, NULL, NULL, NULL },
};

TaskDesc D_actor_207200_8014E7A4 = { { { TASK_BODY_TMD, 96 } }, skullStalkerTask, { .model = &_gActor207200SkullStalkerBody } };

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
static const EnemyTaskFuncTable3 gSkullStalkerTaskStates = {
    { skullStalkerSpawnState, skullStalkerUpdateState, skullStalkerDeathState }
};

#include "../../shared/skull_stalker_task.inc.c"

#include "../../shared/skull_stalker_update_state.inc.c"

#include "../../shared/skull_stalker_reaction_flags.inc.c"

#include "../../shared/skull_stalker_reaction_dispatch.inc.c"

#include "../../shared/skull_stalker_animate.inc.c"

#include "../../shared/skull_stalker_colour.inc.c"

#include "../../shared/skull_stalker_light_ramp.inc.c"

#include "../../shared/skull_stalker_flatten.inc.c"

#include "../../shared/skull_stalker_exit.inc.c"
