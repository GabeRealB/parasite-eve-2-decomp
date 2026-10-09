#include "actors/task_tables.h"

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

static void _sucklercephDropState(Enemy* enemy, Task* task);
#include "../../shared/skull_stalker.h"

/// The second enemy's record; `hpMax` seeds its HP.
extern EnemyParams gSkullStalkerParams;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static AnimationSet _gActor04600Actor104600Animation05554;
static AnimationSet _gActor04600Actor104600Animation0569C;
static AnimationSet _gActor04600Actor104600Animation05840;
static TmdSource    _gActor04600SucklercephBody;

/// Burst damage to the player, enabled only while the burst attack sphere is armed.
static DamageAttack _gSucklercephBurstAttack = { .power = 30, .reaction = SUCKLERCEPH_BURST_PLAYER_REACTION };

/// Enemy-kind parameters borrowed by every standing or dropping Sucklerceph.
///
/// The zero damage-over-time pulse base is retained; this kind's pulses do not expire by count.
static EnemyParams _gSucklercephParams = {
    .attacks              = &_gSucklercephBurstAttack,
    .hpMax                = 70,
    .exp                  = 6,
    .bp                   = 12,
    .mp                   = 3,
    .critChance           = 100,
    .buildupSteps         = 20,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 0,
};

/// Segment indices used by the two lanes of the burst vibration script.
enum {
    SUCKLERCEPH_BURST_VIBRATION_HOLD = 0,
    SUCKLERCEPH_BURST_VIBRATION_FULL = 1,
    SUCKLERCEPH_BURST_VIBRATION_FADE = 2
};

/// Two-lane vibration commands for the Sucklerceph burst.
///
/// The binary motor holds for nine script frames, then stops. The variable motor
/// plays twelve full-intensity frames, then a six-frame fade from 100 to 50.
/// Both lanes stop within these three commands; the script task borrows the array.
static PadScriptCmd _gSucklercephBurstPadScript[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, SUCKLERCEPH_BURST_VIBRATION_HOLD), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, SUCKLERCEPH_BURST_VIBRATION_FULL) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, SUCKLERCEPH_BURST_VIBRATION_FADE) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
};

/// Timed segments borrowed by the burst script: binary hold, full intensity and fade.
///
/// Durations count script frames; variable-motor intensities are bytes in 0..255.
/// The trailing field_3 bytes are retained, with no interpreter read and an unproven role.
static PadScriptVibrationSegment _gSucklercephBurstVibrationSegments[3] = {
    [SUCKLERCEPH_BURST_VIBRATION_HOLD] = { .startIntensity = 0, .endIntensity = 0, .durationFrames = 9, .field_3 = 0 },
    [SUCKLERCEPH_BURST_VIBRATION_FULL] = { .startIntensity = 255, .endIntensity = 255, .durationFrames = 12, .field_3 = 1 },
    [SUCKLERCEPH_BURST_VIBRATION_FADE] = { .startIntensity = 100, .endIntensity = 50, .durationFrames = 6, .field_3 = 1 },
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

/// Commands accepted by a hidden/drop Sucklerceph, followed by the lookup terminator.
///
/// APPLY borrows an ActorCommand and returns zero. Unsupported IDs return zero;
/// never dispatch the reserved end ID. Tasks borrow this table and its callback.
static TaskMessageEntry _gSucklercephDropMsgTable[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _sucklercephMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor04600SucklercephTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = 96 } },
    .callback = _sucklercephTask,
    .data     = { .model = &_gActor04600SucklercephBody },
};

/// Retained hidden/drop Sucklerceph recipe, with no automatic primitive buffer.
///
/// Priority 96; reveal allocates the buffer explicitly. No current placement selects it.
static TaskDesc _gActor04600SucklercephDropTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER, .priority = 96 } },
    .callback = _sucklercephDropTask,
    .data     = { .model = &_gActor04600SucklercephBody },
};

/// Sucklerceph animation sets: 0 empty, 1 idle, 2 crawl, 3 retained but unrequested.
///
/// Contexts borrow this table and its sets for the model's lifetime.
static AnimationSet* _gSucklercephAnimSets[4] = {
    NULL,
    &_gActor04600Actor104600Animation05554,
    &_gActor04600Actor104600Animation0569C,
    &_gActor04600Actor104600Animation05840,
};

/// Burst-particle offset in signed game-coordinate units relative to the model root.
static SVECTOR _gSucklercephBurstParticleOffset = { 0, -10, 0, 0 };

/// Additive-puff offset in signed game-coordinate units relative to the model root.
static SVECTOR _gSucklercephPuffOffset = { 0, -300, 0, 0 };

/// Zero-power ordinary-reaction row retained by the Skull Stalker parameters.
///
/// Its handlers do not pack an outgoing attack; this stored row is not a null attack key.
static DamageAttack _gSkullStalkerAttacks[1] = { { .power = 0, .reaction = GAME_ACTOR_REACTION_ORDINARY } };

EnemyParams gSkullStalkerParams = { _gSkullStalkerAttacks, 1, 2, 32, 1, 100, 20, 100, 99 };

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

TaskDesc gActor04600SkullStalkerTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = 96 } },
    .callback = _skullStalkerTask,
    .data     = { .model = &_gActor04600SkullStalkerBody },
};

/// Skull Stalker animation sets: 0 empty, 1 idle, 2 alert.
///
/// Contexts borrow this table and its sets for the model's lifetime.
static AnimationSet* _gSkullStalkerAnimSets[3] = {
    NULL,
    &_gActor04600Actor104600Animation06220,
    &_gActor04600Actor104600Animation06474,
};

/// Both death particles' offset in signed game-coordinate units relative to the model root.
static SVECTOR _gSkullStalkerDeathParticleOffset = { 0, -100, 0, 0 };

/// Red death glow's offset in signed game-coordinate units relative to the model root.
static SVECTOR _gSkullStalkerDeathGlowOffset = { 0, 0, 100, 0 };

#include "../../shared/sucklerceph_inlines.inc.c"

#include "../../shared/sucklerceph_spawn_state.inc.c"

/// Standing Sucklerceph task callbacks in Task.state order: spawn, update, death.
///
/// The dispatcher copies this value and indexes it without checking; states are 0..2.
static const EnemyTaskFuncTable3 _gSucklercephTaskStates = {
    { _sucklercephSpawnState, _sucklercephUpdateState, _sucklercephDeathState },
};

/// Hidden/drop Sucklerceph task callbacks: hidden setup, update, death, drop.
///
/// The dispatcher copies this value and indexes it without checking; states are 0..3.
static const EnemyTaskFuncTable4 _gSucklercephDropTaskStates = {
    { _sucklercephDropSpawnState, _sucklercephUpdateState, _sucklercephDeathState, _sucklercephDropState },
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

/// Skull Stalker task callbacks in Task.state order: spawn, update, death.
///
/// The dispatcher copies this value and indexes it without checking; states are 0..2.
static const EnemyTaskFuncTable3 _gSkullStalkerTaskStates = {
    { _skullStalkerSpawnState, _skullStalkerUpdateState, _skullStalkerDeathState },
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

PACKAGE_ALIASES
