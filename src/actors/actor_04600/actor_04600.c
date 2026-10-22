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
#include "../../shared/creatures/sucklerceph/sucklerceph.h"

static void _sucklercephDropState(Enemy* enemy, Task* task);
#include "../../shared/creatures/skull_stalker/skull_stalker.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static AnimationSet _gAkropolisForkedRoadSucklercephIdleClip;
static AnimationSet _gAkropolisForkedRoadSucklercephCrawlClip;
static AnimationSet _gAkropolisForkedRoadSucklercephAnim03Clip;
static TmdSource    _gAkropolisForkedRoadSucklercephBody;

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

#include "assets/rooms/akropolis/forked_road/sucklerceph_body.inc"

#include "assets/rooms/akropolis/forked_road/sucklerceph_and_skull_stalker_1_clips_1.inc"

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
    .data     = { .model = &_gAkropolisForkedRoadSucklercephBody },
};

/// Retained hidden/drop Sucklerceph recipe, with no automatic primitive buffer.
///
/// Priority 96; reveal allocates the buffer explicitly. No current placement selects it.
static TaskDesc _gActor04600SucklercephDropTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER, .priority = 96 } },
    .callback = _sucklercephDropTask,
    .data     = { .model = &_gAkropolisForkedRoadSucklercephBody },
};

/// Sucklerceph animation sets: 0 empty, 1 idle, 2 crawl, 3 retained but unrequested.
///
/// Contexts borrow this table and its sets for the model's lifetime.
#include "assets/rooms/akropolis/forked_road/sucklerceph_bank.inc"

/// Burst-particle offset in signed game-coordinate units relative to the model root.
static SVECTOR _gSucklercephBurstParticleOffset = { 0, -10, 0, 0 };

/// Additive-puff offset in signed game-coordinate units relative to the model root.
static SVECTOR _gSucklercephPuffOffset = { 0, -300, 0, 0 };

/// Zero-power ordinary-reaction row retained by the Skull Stalker parameters.
///
/// Its handlers do not pack an outgoing attack; this stored row is not a null attack key.
static DamageAttack _gSkullStalkerAttacks[1] = { { .power = 0, .reaction = GAME_ACTOR_REACTION_ORDINARY } };

/// Skull Stalker combat, reaction and release-reward parameters.
///
/// Spawn starts each enemy with one HP and borrows this record until release.
/// The retained zero-power attack row is not selected by its handlers. The
/// damage-over-time fields store a 100-percent base chance and 99 base pulses.
static EnemyParams _gSkullStalkerParams = {
    .attacks              = _gSkullStalkerAttacks,
    .hpMax                = 1,
    .exp                  = 2,
    .bp                   = 32,
    .mp                   = 1,
    .critChance           = 100,
    .buildupSteps         = 20,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 99,
};

#include "assets/rooms/akropolis/forked_road/skull_stalker_body.inc"

#include "assets/rooms/akropolis/forked_road/sucklerceph_and_skull_stalker_1_clips_2.inc"

TaskDesc gActor04600SkullStalkerTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = 96 } },
    .callback = _skullStalkerTask,
    .data     = { .model = &_gAkropolisForkedRoadSkullStalkerBody },
};

/// Skull Stalker animation sets: 0 empty, 1 idle, 2 alert.
///
/// Contexts borrow this table and its sets for the model's lifetime.
#include "assets/rooms/akropolis/forked_road/skull_stalker_bank.inc"

/// Both death particles' offset in signed game-coordinate units relative to the model root.
static SVECTOR _gSkullStalkerDeathParticleOffset = { 0, -100, 0, 0 };

/// Red death glow's offset in signed game-coordinate units relative to the model root.
static SVECTOR _gSkullStalkerDeathGlowOffset = { 0, 0, 100, 0 };

#include "../../shared/creatures/sucklerceph/sucklerceph_inlines.inc.c"

#include "../../shared/creatures/sucklerceph/sucklerceph_spawn_state.inc.c"

#include "../../shared/creatures/sucklerceph/sucklerceph.inc.c"

#include "../../shared/creatures/sucklerceph/sucklerceph_drop_state.inc.c"

#include "../../shared/creatures/skull_stalker/skull_stalker.inc.c"

/// Skull Stalker task callbacks in Task.state order: spawn, update, death.
///
/// The dispatcher copies this value and indexes it without checking; states are 0..2.
static const EnemyTaskFuncTable3 _gSkullStalkerTaskStates = {
    { _skullStalkerSpawnState, _skullStalkerUpdateState, _skullStalkerDeathState },
};

#include "../../shared/creatures/skull_stalker/skull_stalker_task.inc.c"

PACKAGE_ALIASES
