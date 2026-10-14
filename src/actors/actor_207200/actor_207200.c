#include "actor_207200_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"
#include "actors/task_tables.h"

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
#include "../../shared/creatures/skull_stalker/skull_stalker.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static TmdSource _gActor207200SkullStalkerBody;

/// Skull Stalker's single zero-power, ordinary-reaction parameter row.
///
/// The enemy parameters borrow this row while the package is loaded. Its
/// collision bodies do not pack an attack key from it; a stored zero row
/// would still pack a category-4 key if passed to the damage API.
static DamageAttack _gActor207200SkullStalkerAttacks[1] = { { 0, GAME_ACTOR_REACTION_ORDINARY } };

/// Skull Stalker combat, reaction and release-reward parameters.
///
/// Spawn starts each enemy with one HP and borrows this record until release.
/// The retained zero-power attack row is not selected by its handlers. The
/// damage-over-time fields store a 100-percent base chance and 99 base pulses.
static EnemyParams _gSkullStalkerParams = {
    .attacks              = _gActor207200SkullStalkerAttacks,
    .hpMax                = 1,
    .exp                  = 2,
    .bp                   = 32,
    .mp                   = 1,
    .critChance           = 100,
    .buildupSteps         = 20,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 99,
};

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

enum { ACTOR_207200_TASK_PRIORITY = 96 };

TaskDesc gActor207200SkullStalkerTaskDesc = { { { TASK_BODY_TMD, ACTOR_207200_TASK_PRIORITY } }, _skullStalkerTask, { .model = &_gActor207200SkullStalkerBody } };

/// Skull Stalker animation sets: 0 empty, 1 idle, 2 alert.
///
/// Contexts borrow this table and its sets for the model's lifetime.
static AnimationSet* _gSkullStalkerAnimSets[3] = {
    NULL,
    &_gActor207200Animation04708,
    &_gActor207200Animation0495C,
};

/// Both death particles' offset in signed game-coordinate units relative to the model root.
static SVECTOR _gSkullStalkerDeathParticleOffset = { 0, -100, 0, 0 };

/// Red death glow's offset in signed game-coordinate units relative to the model root.
static SVECTOR _gSkullStalkerDeathGlowOffset = { 0, 0, 100, 0 };

DamageAttack gActor207200CreepingStrangerAttacks[ACTOR_207200_CREEPING_STRANGER_ATTACK_COUNT] = {
    [ACTOR_207200_CREEPING_STRANGER_ATTACK_FRONT] = { 25, GAME_ACTOR_REACTION_BERSERKER },
    [ACTOR_207200_CREEPING_STRANGER_ATTACK_SIDE]  = { 10, GAME_ACTOR_REACTION_ORDINARY },
};

#include "../../shared/creatures/skull_stalker/skull_stalker.inc.c"

/// Skull Stalker task callbacks in Task.state order: spawn, update, death.
///
/// The dispatcher copies this value and indexes it without checking; states are 0..2.
static const EnemyTaskFuncTable3 _gSkullStalkerTaskStates = {
    { _skullStalkerSpawnState, _skullStalkerUpdateState, _skullStalkerDeathState }
};

#include "../../shared/creatures/skull_stalker/skull_stalker_task.inc.c"
