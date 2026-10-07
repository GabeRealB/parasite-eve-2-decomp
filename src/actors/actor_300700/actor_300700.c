#include "actor_300700_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
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
#include "../../shared/rat.h"
#include "../../shared/moth.h"

extern DamageAttack gMothAttack;

extern EnemyParams gMothParams;

extern u16 gMothSpeeds[8];

extern AnimationSet* gMothAnimSets[2];

extern ActorSpriteUv gMothBurstUvs[];

static TmdBone _gActor300700MothBodySkeleton[4] = {
#include "assets/moth_body_skeleton.inc"
};

static u32 _gActor300700MothBodyPartVerts[4] = {
#include "assets/moth_body_partVerts.inc"
};

static SVECTOR _gActor300700MothBodyVerts[26] = {
#include "assets/moth_body_verts.inc"
};

static SVECTOR _gActor300700MothBodyNormals[20] = {
#include "assets/moth_body_normals.inc"
};

static u32 _gActor300700MothBodyStream[265] = {
#include "assets/moth_body_stream.inc"
};

static TmdSource _gActor300700MothBody = {
    0,
    1488,
    208,
    4,
    _gActor300700MothBodyPartVerts,
    _gActor300700MothBodyVerts,
    _gActor300700MothBodyNormals,
    _gActor300700MothBodySkeleton,
    _gActor300700MothBodyStream,
};

static AnimationPackedPose _gActor300700Animation03D1CBank1[2] = {
#include "assets/actor_300700_animation_03D1C_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation03D1CBank4[1] = {
#include "assets/actor_300700_animation_03D1C_bank4.inc"
};

static AnimationRecord _gActor300700Animation03D1CRecords[12] = {
#include "assets/actor_300700_animation_03D1C_records.inc"
};

static u16 _gActor300700Animation03D1CIndices[4] = {
#include "assets/actor_300700_animation_03D1C_indices.inc"
};

static AnimationSet _gActor300700Animation03D1C = {
    _gActor300700Animation03D1CRecords,
    _gActor300700Animation03D1CIndices,
    { NULL, _gActor300700Animation03D1CBank1, NULL, NULL, _gActor300700Animation03D1CBank4, NULL, NULL, NULL },
};

DamageAttack gMothAttack = { 5, 1 };

EnemyParams gMothParams = { &gMothAttack, 1, 2, 18, 1, 100, 0, 100, 99 };

u16 gMothSpeeds[8] = {
    2,
    8,
    16,
    24,
    32,
    32,
    32,
    36,
};

TaskDesc D_actor_300700_80165B88 = { { { TASK_BODY_TMD, 96 } }, mothTask, { .model = &_gActor300700MothBody } };

AnimationSet* gMothAnimSets[2] = {
    NULL,
    &_gActor300700Animation03D1C,
};

ActorSpriteUv gMothBurstUvs[8] = {
    { 96, 0, 96, 0 },
    { 0, 0, 160, 0 },
    { 0, 0, 192, 0 },
    { 32, 0, 192, 0 },
    { 0, 0, 224, 0 },
    { 32, 0, 224, 0 },
    { 64, 0, 224, 0 },
    { 96, 0, 224, 0 },
};

static TmdBone _gActor300700RatBodySkeleton[7] = {
#include "assets/rat_body_skeleton.inc"
};

static u32 _gActor300700RatBodyPartVerts[7] = {
#include "assets/rat_body_partVerts.inc"
};

static SVECTOR _gActor300700RatBodyVerts[78] = {
#include "assets/rat_body_verts.inc"
};

static SVECTOR _gActor300700RatBodyNormals[113] = {
#include "assets/rat_body_normals.inc"
};

static u32 _gActor300700RatBodyStream[1101] = {
#include "assets/rat_body_stream.inc"
};

TmdSource gActor300700RatBody = {
    0,
    5164,
    2392,
    7,
    _gActor300700RatBodyPartVerts,
    _gActor300700RatBodyVerts,
    _gActor300700RatBodyNormals,
    _gActor300700RatBodySkeleton,
    _gActor300700RatBodyStream,
};

#include "../../shared/moth_spawn.inc.c"

#include "../../shared/moth_update.inc.c"

#include "../../shared/moth_contacts.inc.c"

#include "../../shared/moth_oscillate_parts.inc.c"

#include "../../shared/moth_steer.inc.c"

#include "../../shared/moth_drift.inc.c"

#include "../../shared/moth_death.inc.c"

#include "../../shared/moth_draw_burst.inc.c"
/// The first variant's state handlers, dispatched by `mothTask`
/// on the task's state: spawn, per-frame update, and the handler for state 2.
static const EnemyTaskFuncTable3 gMothStateHandlers = {
    {
        mothSpawn,
        mothUpdate,
        mothDeath,
    },
};

#include "../../shared/moth_task.inc.c"

#include "../../shared/moth_update_color.inc.c"

#include "../../shared/moth_squash.inc.c"

#include "../../shared/rat_spawn.inc.c"
