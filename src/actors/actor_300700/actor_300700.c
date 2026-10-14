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
#include "../../shared/creatures/moth/moth.h"

static void _mothSpawn(Enemy* enemy, Task* task);
static void _mothUpdate(Enemy* enemy, Task* task);
static void _mothContacts(Task* task);
static void _mothOscillateParts(Task* task);
static void _mothSteer(Task* task);
static void _mothDrift(Task* task);
static void _mothDeath(Enemy* enemy, Task* task);
static void _mothDrawBurst(Task* task);
static void _mothTask(Task* task);
static void _mothSquash(Task* task);
static void _mothUpdateColor(Task* task);

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

/// Attack delivered by the moth's death sphere: base power 5 and darkness reaction.
static DamageAttack _gMothAttack = { .power = 5, .reaction = MOTH_BURST_REACTION_DARKNESS };

/// Shared moth health, battle rewards and hit-reaction tuning, borrowed by live enemies.
static EnemyParams _gMothParams = {
    .attacks              = &_gMothAttack,
    .hpMax                = 1,
    .exp                  = 2,
    .bp                   = 18,
    .mp                   = 1,
    .critChance           = 100,
    .buildupSteps         = 0,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 99,
};

/// Base forward speeds for alerted flight, in parent-coordinate units per update.
///
/// Indexed by placement `rowIndex` 0..7; a 0..31 draw is added before
/// narrowing to s16 and stepping along the root's Q12 forward axis.
static u16 _gMothPursuitSpeeds[MOTH_PLACEMENT_ROW_COUNT] = {
    2,
    8,
    16,
    24,
    32,
    32,
    32,
    36,
};

TaskDesc gActor300700MothTaskDesc = { { { TASK_BODY_TMD, 96 } }, _mothTask, { .model = &_gActor300700MothBody } };

/// Moth rest-pose sets: entry 0 empty, entry 1 seeds articulated slots 1..3.
///
/// The rig borrows this table while its package is loaded. Wing motion is
/// procedural; the rest-pose clip is never ticked after spawn.
static AnimationSet* _gMothAnimSets[MOTH_ANIM_SET_COUNT] = {
    NULL,
    [MOTH_ANIM_REST_POSE] = &_gActor300700Animation03D1C,
};

/// Texture origins of the eight 32-texel-square cells in the moth death burst.
///
/// Timer ticks 1..23 select `timer / MOTH_BURST_TICKS_PER_CELL`; the drawer
/// supplies the texture page and palette from the live model's offsets.
static ActorSpriteUv _gMothBurstUvs[MOTH_BURST_CELL_COUNT] = {
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

#include "../../shared/creatures/moth/moth.inc.c"
/// Task-state callbacks: 0 spawn, 1 living update, 2 death; copied before dispatch.
///
/// Indexed by `Task::state` in 0..2, with no terminator or bounds check.
static const EnemyTaskFuncTable3 _gMothStateHandlers = {
    {
        [MOTH_TASK_SPAWN]  = _mothSpawn,
        [MOTH_TASK_UPDATE] = _mothUpdate,
        [MOTH_TASK_DEATH]  = _mothDeath,
    },
};

#include "../../shared/creatures/moth/moth_task.inc.c"

/// Bite descriptor borrowed by the rat spawn fragment.
///
/// Points to the single `DamageAttack` entry also selected by the enemy parameters.
/// Spawn reads entry zero once and retains only its packed collision key.
/// Define before including the fragment and undefine afterwards; see `rat.h`.
#define RAT_BITE_ATTACK (&gRatAttack)

/// Enemy-kind parameter record supplied to the shared rat spawner.
///
/// Binds the addressable, non-const `EnemyParams` object shared by this
/// overlay's two TUs. Spawn retains its address in `Enemy::param` and copies
/// `hpMax` to the rat's hit points. Rats borrow the same record for damage
/// reactions and battle rewards; it must remain loaded until enemy teardown.
/// Both uses must select the same object without side effects. Define before
/// including `rat_spawn.inc.c` and undefine afterwards; see `rat.h`.
#define RAT_ENEMY_PARAMS (gRatParams)

/// Animation-set table supplied to the shared rat spawn fragment.
///
/// Binds the overlay's shared `AnimationSet*` array with `RAT_ANIM_SET_COUNT`
/// entries: entry 0 is NULL; `RAT_ANIM_*` entries 1..10 supply tracks for slots 1..6.
/// Evaluated once at spawn; the context and slots borrow the table and clip
/// data without modifying them until task teardown. Define before including
/// `rat_spawn.inc.c` and undefine afterwards; see `rat.h`.
#define RAT_ANIM_SETS gRatAnimSets
#include "../../shared/creatures/rat/rat_spawn.inc.c"
#undef RAT_BITE_ATTACK
#undef RAT_ENEMY_PARAMS
#undef RAT_ANIM_SETS
