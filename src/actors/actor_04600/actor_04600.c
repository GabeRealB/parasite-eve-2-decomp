#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
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
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_dumping_hole.h"

#include "rooms/shelter_b3_garbage_incinerator.h"
#include "../../shared/sucklerceph.h"
#include "../../shared/skull_stalker.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// The first enemy's attack row, packed into its third body's key, and the
/// enemy parameters whose `attacks` name it; `hpMax` seeds the enemy's HP.
extern DamageAttack gSucklercephAttack;
extern EnemyParams  gSucklercephParams;

/// The two script arguments the first enemy's death hands to
/// `Gp_SpawnScript18`.
extern PadScriptCmd              gSucklercephBurstScriptA[];
extern PadScriptVibrationSegment gSucklercephBurstScriptB[];

/// Message table the dropping first enemy's spawn parks in `Task::msgTable`.
// Typed callback views for the task message dispatcher.

extern TaskMessageEntry gSucklercephDropMsgTable[2];

/// Animation-set table bound to the first enemy's context by `animationInitContext`.
extern AnimationSet* gSucklercephAnimSets[4];

/// Offset of the 0x60030 effect the first enemy's death spawns.
extern SVECTOR gSucklercephBurstFxOffset;

/// Offset of the 0x60080 effect the collapsing first enemy spawns.
extern SVECTOR gSucklercephCollapseFxOffset;

/// The second enemy's record; `hpMax` seeds its HP.
extern EnemyParams gSkullStalkerParams;

/// Animation-set table bound to the second enemy's context by `animationInitContext`.
extern AnimationSet* gSkullStalkerAnimSets[3];

/// Offsets of the spark and hit effects the second enemy's hit handler spawns.
extern SVECTOR gSkullStalkerSparkOffset;
extern SVECTOR gSkullStalkerHitFxOffset;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static AnimationSet _gActor04600Actor104600Animation05554;
static AnimationSet _gActor04600Actor104600Animation0569C;
static AnimationSet _gActor04600Actor104600Animation05840;
static TmdSource    _gActor04600SucklercephBody;

DamageAttack gSucklercephAttack = { 30, 7 };

EnemyParams gSucklercephParams = { &gSucklercephAttack, 70, 6, 12, 3, 100, 20, 100, 0 };

PadScriptCmd gSucklercephBurstScriptA[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
};

PadScriptVibrationSegment gSucklercephBurstScriptB[3] = {
    { 0, 0, 9, 0 },
    { 255, 255, 12, 1 },
    { 100, 50, 6, 1 },
};

static TmdBone _gActor04600SucklercephBodySkeleton[3] = {
#include "assets/sucklerceph_body_skeleton.inc"
};

static u32 _gActor04600SucklercephBodyPartVerts[3] = {
#include "assets/sucklerceph_body_partVerts.inc"
};

static SVECTOR _gActor04600SucklercephBodyVerts[68] = {
#include "assets/sucklerceph_body_verts.inc"
};

static SVECTOR _gActor04600SucklercephBodyNormals[68] = {
#include "assets/sucklerceph_body_normals.inc"
};

static u32 _gActor04600SucklercephBodyStream[752] = {
#include "assets/sucklerceph_body_stream.inc"
};

static TmdSource _gActor04600SucklercephBody = {
    0,
    4516,
    584,
    3,
    _gActor04600SucklercephBodyPartVerts,
    _gActor04600SucklercephBodyVerts,
    _gActor04600SucklercephBodyNormals,
    _gActor04600SucklercephBodySkeleton,
    _gActor04600SucklercephBodyStream,
};

static AnimationPackedPose _gActor04600Actor104600Animation05554Bank1[34] = {
#include "assets/actor_104600_animation_05554_bank1.inc"
};

static AnimationPackedRotation _gActor04600Actor104600Animation05554Bank4[26] = {
#include "assets/actor_104600_animation_05554_bank4.inc"
};

static AnimationRecord _gActor04600Actor104600Animation05554Records[74] = {
#include "assets/actor_104600_animation_05554_records.inc"
};

static u16 _gActor04600Actor104600Animation05554Indices[4] = {
#include "assets/actor_104600_animation_05554_indices.inc"
};

static AnimationSet _gActor04600Actor104600Animation05554 = {
    _gActor04600Actor104600Animation05554Records,
    _gActor04600Actor104600Animation05554Indices,
    { NULL, _gActor04600Actor104600Animation05554Bank1, NULL, NULL, _gActor04600Actor104600Animation05554Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04600Actor104600Animation0569CBank1[11] = {
#include "assets/actor_104600_animation_0569C_bank1.inc"
};

static AnimationPackedRotation _gActor04600Actor104600Animation0569CBank4[9] = {
#include "assets/actor_104600_animation_0569C_bank4.inc"
};

static AnimationRecord _gActor04600Actor104600Animation0569CRecords[28] = {
#include "assets/actor_104600_animation_0569C_records.inc"
};

static u16 _gActor04600Actor104600Animation0569CIndices[4] = {
#include "assets/actor_104600_animation_0569C_indices.inc"
};

static AnimationSet _gActor04600Actor104600Animation0569C = {
    _gActor04600Actor104600Animation0569CRecords,
    _gActor04600Actor104600Animation0569CIndices,
    { NULL, _gActor04600Actor104600Animation0569CBank1, NULL, NULL, _gActor04600Actor104600Animation0569CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04600Actor104600Animation05840Bank1[15] = {
#include "assets/actor_104600_animation_05840_bank1.inc"
};

static AnimationPackedRotation _gActor04600Actor104600Animation05840Bank4[12] = {
#include "assets/actor_104600_animation_05840_bank4.inc"
};

static AnimationRecord _gActor04600Actor104600Animation05840Records[36] = {
#include "assets/actor_104600_animation_05840_records.inc"
};

static u16 _gActor04600Actor104600Animation05840Indices[4] = {
#include "assets/actor_104600_animation_05840_indices.inc"
};

static AnimationSet _gActor04600Actor104600Animation05840 = {
    _gActor04600Actor104600Animation05840Records,
    _gActor04600Actor104600Animation05840Indices,
    { NULL, _gActor04600Actor104600Animation05840Bank1, NULL, NULL, _gActor04600Actor104600Animation05840Bank4, NULL, NULL, NULL },
};

TaskMessageEntry gSucklercephDropMsgTable[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, sucklercephMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor04600_D05878 = { { { TASK_BODY_TMD, 96 } }, sucklercephTask, { .model = &_gActor04600SucklercephBody } };

TaskDesc Actor04600_D05884 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, sucklercephDropTask, { .model = &_gActor04600SucklercephBody } };

AnimationSet* gSucklercephAnimSets[4] = {
    NULL,
    &_gActor04600Actor104600Animation05554,
    &_gActor04600Actor104600Animation0569C,
    &_gActor04600Actor104600Animation05840,
};

SVECTOR gSucklercephBurstFxOffset = { 0, -10, 0, 0 };

SVECTOR gSucklercephCollapseFxOffset = { 0, -300, 0, 0 };

DamageAttack Actor04600_D058B0[1] = { 0 };

EnemyParams gSkullStalkerParams = { Actor04600_D058B0, 1, 2, 32, 1, 100, 20, 100, 99 };

static TmdBone _gActor04600SkullStalkerBodySkeleton[3] = {
#include "assets/skull_stalker_body_skeleton.inc"
};

static u32 _gActor04600SkullStalkerBodyPartVerts[3] = {
#include "assets/skull_stalker_body_partVerts.inc"
};

static SVECTOR _gActor04600SkullStalkerBodyVerts[37] = {
#include "assets/skull_stalker_body_verts.inc"
};

static SVECTOR _gActor04600SkullStalkerBodyNormals[47] = {
#include "assets/skull_stalker_body_normals.inc"
};

static u32 _gActor04600SkullStalkerBodyStream[377] = {
#include "assets/skull_stalker_body_stream.inc"
};

static TmdSource _gActor04600SkullStalkerBody = {
    0,
    2184,
    332,
    3,
    _gActor04600SkullStalkerBodyPartVerts,
    _gActor04600SkullStalkerBodyVerts,
    _gActor04600SkullStalkerBodyNormals,
    _gActor04600SkullStalkerBodySkeleton,
    _gActor04600SkullStalkerBodyStream,
};

static AnimationPackedPose _gActor04600Actor104600Animation06220Bank1[2] = {
#include "assets/actor_104600_animation_06220_bank1.inc"
};

static AnimationPackedRotation _gActor04600Actor104600Animation06220Bank4[1] = {
#include "assets/actor_104600_animation_06220_bank4.inc"
};

static AnimationRecord _gActor04600Actor104600Animation06220Records[6] = {
#include "assets/actor_104600_animation_06220_records.inc"
};

static u16 _gActor04600Actor104600Animation06220Indices[4] = {
#include "assets/actor_104600_animation_06220_indices.inc"
};

static AnimationSet _gActor04600Actor104600Animation06220 = {
    _gActor04600Actor104600Animation06220Records,
    _gActor04600Actor104600Animation06220Indices,
    { NULL, _gActor04600Actor104600Animation06220Bank1, NULL, NULL, _gActor04600Actor104600Animation06220Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04600Actor104600Animation06474Bank1[24] = {
#include "assets/actor_104600_animation_06474_bank1.inc"
};

static AnimationPackedRotation _gActor04600Actor104600Animation06474Bank4[10] = {
#include "assets/actor_104600_animation_06474_bank4.inc"
};

static AnimationRecord _gActor04600Actor104600Animation06474Records[55] = {
#include "assets/actor_104600_animation_06474_records.inc"
};

static u16 _gActor04600Actor104600Animation06474Indices[4] = {
#include "assets/actor_104600_animation_06474_indices.inc"
};

static AnimationSet _gActor04600Actor104600Animation06474 = {
    _gActor04600Actor104600Animation06474Records,
    _gActor04600Actor104600Animation06474Indices,
    { NULL, _gActor04600Actor104600Animation06474Bank1, NULL, NULL, _gActor04600Actor104600Animation06474Bank4, NULL, NULL, NULL },
};

TaskDesc Actor04600_D0649C = { { { TASK_BODY_TMD, 96 } }, skullStalkerTask, { .model = &_gActor04600SkullStalkerBody } };

AnimationSet* gSkullStalkerAnimSets[3] = {
    NULL,
    &_gActor04600Actor104600Animation06220,
    &_gActor04600Actor104600Animation06474,
};

SVECTOR gSkullStalkerSparkOffset = { 0, -100, 0, 0 };

SVECTOR gSkullStalkerHitFxOffset = { 0, 0, 100, 0 };

#include "../../shared/sucklerceph_inlines.inc.c"

#include "../../shared/sucklerceph_spawn_state.inc.c"

/// Task states of the first enemy as `sucklercephTask` dispatches them:
/// spawn, per-frame update and death.
static const EnemyTaskFuncTable3 gSucklercephTaskStates = {
    { sucklercephSpawnState, sucklercephUpdateState, sucklercephDeathState },
};

/// Task states of the dropping first enemy as `sucklercephDropTask` dispatches
/// them: the same update and death after a spawn that parks the enemy hidden,
/// and a fourth state for its drop into place.
static const EnemyTaskFuncTable4 gSucklercephDropTaskStates = {
    { sucklercephDropSpawnState, sucklercephUpdateState, sucklercephDeathState, sucklercephDropState },
};

#include "../../shared/sucklerceph_reaction_dispatch.inc.c"

#include "../../shared/sucklerceph_dormant_tick.inc.c"

#include "../../shared/sucklerceph_awake_tick.inc.c"

#include "../../shared/sucklerceph_contacts.inc.c"

#include "../../shared/sucklerceph_take_damage.inc.c"

#include "../../shared/sucklerceph_turn_to_player.inc.c"

#include "../../shared/sucklerceph_death_state.inc.c"

#include "../../shared/sucklerceph_kill.inc.c"

#include "../../shared/sucklerceph_drop_spawn_state.inc.c"

#include "../../shared/sucklerceph_drop_state.inc.c"

#include "../../shared/sucklerceph_drop_collide.inc.c"

#include "../../shared/sucklerceph_message.inc.c"

#include "../../shared/sucklerceph_task.inc.c"

#include "../../shared/sucklerceph_update_state.inc.c"

#include "../../shared/sucklerceph_reaction_flags.inc.c"

#include "../../shared/sucklerceph_step.inc.c"

#include "../../shared/sucklerceph_animate.inc.c"

#include "../../shared/sucklerceph_colour.inc.c"

#include "../../shared/sucklerceph_draw_shadow.inc.c"

#include "../../shared/sucklerceph_scale_part.inc.c"

#include "../../shared/sucklerceph_flatten.inc.c"

#include "../../shared/sucklerceph_exit.inc.c"

#include "../../shared/sucklerceph_drop_task.inc.c"

#include "../../shared/sucklerceph_fall_step.inc.c"

#include "../../shared/skull_stalker_spawn_state.inc.c"

#include "../../shared/skull_stalker_idle_tick.inc.c"

#include "../../shared/skull_stalker_hits.inc.c"

#include "../../shared/skull_stalker_inlines.inc.c"

#include "../../shared/skull_stalker_death_state.inc.c"

/// Task states of the second enemy as `skullStalkerTask` dispatches them:
/// spawn, per-frame update and the dying tick.
static const EnemyTaskFuncTable3 gSkullStalkerTaskStates = {
    { skullStalkerSpawnState, skullStalkerUpdateState, skullStalkerDeathState },
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
