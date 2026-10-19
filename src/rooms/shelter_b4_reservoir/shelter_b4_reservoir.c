#include "rooms/shelter_b4_reservoir.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_04400.h"
#include "actors/actor_00400.h"
#include "actors/task_tables.h"

#include "gameplay/companion_load.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
/// Selects the reservoir's private context for scripted wave spawns and phase requests.
///
/// The writable `ScreenWaveCtx` stays in this translation unit at its existing
/// BSS position. Bind it before the shared header and retain the binding through
/// screen_wave_run.inc.c, then undefine it. The wave borrows this same object;
/// keep the room loaded through its final drawing tick and do not overlap spawns.
#define SCREEN_WAVE_SPAWN_CONTEXT (_gScreenWaveSpawnCtx)
#include "../../shared/screen/screen_wave.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/effects/water_effects.h"

static void _waterDrawSpin(const GfxCoord* coord, s16 textureColumn, s16 radiusScale, s16 spinAngle);
static void _waterDrawTile(const GfxCoord* coord, s16 frameIndex, s16 radiusScale);

/// Advances the shared LCG once and yields its upper 16 bits as a u32.
///
/// Captures and writes `gRandomLcgState` with unsigned 32-bit wraparound;
/// the expression result is 0..65535, with no narrowing cast. Every evaluated
/// invocation consumes one draw, including draws followed by failed spawns.
/// Do not combine unsequenced invocations in a single expression. This room
/// uses the same resident sequence as other loaded gameplay effects.
#define SHELTER_B4_RESERVOIR_NEXT_RANDOM_U16() ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16)

enum {
    SHELTER_B4_RESERVOIR_BURST_CHANCE_SCALE     = 100,
    SHELTER_B4_RESERVOIR_BURST_BASE_HALF_EXTENT = 320,
    // Packed sprite argument: movement step 80 and a two-frame animation period.
    SHELTER_B4_RESERVOIR_BURST_MOTION_ARGS = (80 << 16) | (2 << 12),
    // Adds 0..255 to the half-extent and 0..1 to the animation period.
    SHELTER_B4_RESERVOIR_BURST_VARIATION_MASK = (1 << 12) | 0xFF,
};

enum {
    SHELTER_B4_RESERVOIR_BURST_SPRITE_FRAME_COUNT = 6,
    SHELTER_B4_RESERVOIR_MESSAGE_USE_KEY_ITEM     = 5105,
};

enum {
    SHELTER_B4_RESERVOIR_MAIN_WATER_INITIAL_Y = -2000,
    SHELTER_B4_RESERVOIR_MAIN_WATER_Y_TRAVEL  = 1500,
    SHELTER_B4_RESERVOIR_MAIN_WATER_FINAL_Y   = SHELTER_B4_RESERVOIR_MAIN_WATER_INITIAL_Y + SHELTER_B4_RESERVOIR_MAIN_WATER_Y_TRAVEL,
    SHELTER_B4_RESERVOIR_VIEW10_WATER_MIN_Y   = -1800,
    SHELTER_B4_RESERVOIR_VIEW10_WATER_MAX_Y   = 0,
    SHELTER_B4_RESERVOIR_WATER_SPRAY_VIEW     = 10,
    // Event callbacks select reset/stationary placement or restarted motion.
    SHELTER_B4_RESERVOIR_EVENT_MODEL_RESET          = 0,
    SHELTER_B4_RESERVOIR_EVENT_MODEL_RESTART_MOTION = 2,
};

// Suppress a repeated interaction immediately after returning control to play.
enum {
    SHELTER_B4_RESERVOIR_INTERACTION_REARM_UPDATES = 10,
};

// Event-script water commands; both ramps use callback updates, not seconds.
enum {
    SHELTER_B4_RESERVOIR_WATER_START_MAIN_RAMP   = 0,
    SHELTER_B4_RESERVOIR_WATER_FINISH_MAIN_RAMP  = 1,
    SHELTER_B4_RESERVOIR_WATER_START_VIEW10_RAMP = 2,
    SHELTER_B4_RESERVOIR_WATER_RESET_VIEW10      = 3,
};

/// Configuration of the reservoir's per-frame burst-sprite emitter.
///
/// Cleared when the room effect task starts and retained until reconfigured.
/// A zero point count or spawn chance disables emission. The point count must
/// fit the ten-point position table; it is not clamped. The base half-extent
/// plus 0..255 random size variation must fit the spawn argument's low 12 bits
/// so it does not carry into the animation period.
typedef struct {
    u16 pointCount;         // Active emission points (0..10)
    u16 spawnChancePercent; // Per-point, per-frame roll threshold out of 100 (0 disabled, >=100 always)
    u16 baseHalfExtent;     // Base sprite half-extent in world units, before random size variation
} _ShelterB4ReservoirBurstConfig;
STATIC_ASSERT_SIZEOF(_ShelterB4ReservoirBurstConfig, 6);

static ScreenWaveCtx _gScreenWaveSpawnCtx;

static s32 _gScreenWaveAmplitude;

static ScreenWaveCtx* _gScreenWaveContext;

static ScreenWaveOscillator _gScreenWaveColumnWaves[13];
static ScreenWaveOscillator _gScreenWaveRowWaves[32];

/// Half-extent of a water-drift sprite, plus two trailing bytes.
///
/// Each frame the room task ORs `halfExtent` into the low half of a spawn
/// argument and fills the other fields from the three bytes that precede this
/// object. `_waterDriftTask` reads that layout: bits 0..11 are this half-extent,
/// bits 12..15 the frame period, bits 16..23 the speed and bits 24..27 the
/// velocity kind; any set bit in 28..31 selects upright drawing. The initial
/// half-extent is 640, so it occupies only the low 12 bits. The trailing bytes
/// are zero; their role is unproven.
typedef struct {
    s16 halfExtent; // Sprite half-extent in world units, bits 0..11 of the spawn argument
    u8  field_2[2]; // Zero bytes with no accesses; role unproven
} _ShelterB4ReservoirWaterDriftHalfExtent;
STATIC_ASSERT_SIZEOF(_ShelterB4ReservoirWaterDriftHalfExtent, 4);

static void _shelterB4ReservoirInitializeRoomTask(Task* task);
static void _shelterB4ReservoirUpdateRoomTask(Task* task);
static void _shelterB4ReservoirNoopRoomTick(void);
enum {
    SHELTER_B4_RESERVOIR_WATER_DEPTH_BIAS      = 1,
    SHELTER_B4_RESERVOIR_WATER_QUAD_CODE       = 0x2A,
    SHELTER_B4_RESERVOIR_WATER_RED_INTENSITY   = 128,
    SHELTER_B4_RESERVOIR_WATER_DRAW_MODE_WORDS = 1,
    // GPU mode opcode, subtractive blend and retained texture page 10.
    SHELTER_B4_RESERVOIR_WATER_DRAW_MODE = 0xE1000000 | (GPU_BLEND_SUBTRACT << 5) | 10,
};

static void _shelterB4ReservoirDrawMainWaterStrip(Task* task);
static void _shelterB4ReservoirDrawZWaterStrips(Task* task);
static void _shelterB4ReservoirDrawXWaterStrips(Task* task);
static void _shelterB4ReservoirDrawView10WaterStrips(Task* task);
static void _shelterB4ReservoirInitializeWaterTask(Task* task);
static void _shelterB4ReservoirDrawBurstSprite(const GfxCoord* spriteCoord, u16 animationFrame, s16 halfExtent);
static void _shelterB4ReservoirSetBurstConfig(s16 pointCount, u16 spawnChancePercent, s16 baseHalfExtent);

/// Room-task states 0 initialize, 1 update and 2 retire the task.
///
/// Copied whole before unchecked dispatch; the callback code must stay loaded.
static const TaskFuncTable3 _gShelterB4ReservoirRoomStates = {
    { _shelterB4ReservoirInitializeRoomTask, _shelterB4ReservoirUpdateRoomTask, taskKill }
};

static void _shelterB4ReservoirWaterTask(Task* task);

static WorldCollisionGrid     _gShelterB4ReservoirRoomCollisionGrid;
static WorldCollisionOccluder _gShelterB4ReservoirRoomOccluders[2];
static WorldCollisionTrigger  _gShelterB4ReservoirViewBoundaryTriggers[8];
static WorldCollisionTrigger  _gShelterB4ReservoirRoom1ActionTriggers[7];
static WorldCollisionTrigger  _gShelterB4ReservoirRoom2ActionTriggers[9];
static WorldCoordRoomLights   _gShelterB4ReservoirRoomLights;
static s32                    _shelterB4ReservoirRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg);
static s32                    _shelterB4ReservoirResolveTransitionMessage(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32                    _shelterB4ReservoirCommandMessage(Task* task, s32 messageId, s32 command, s32 secondArg);
static s32                    _shelterB4ReservoirIgnoreRoomActionMessage(Task* task, s32 messageId, s32 firstArg, s32 secondArg);
static s32                    _shelterB4ReservoirSoundMessage(Task* task, s32 messageId, s32 soundCommand, s32 secondArg);
static void                   _shelterB4ReservoirWaterEventTask(Task* task);
static void                   _shelterB4ReservoirExitTask(Task* task);
static void                   _shelterB4ReservoirRampMainWaterHeightTask(Task* task);
static void                   _shelterB4ReservoirRampView10WaterHeightTask(Task* task);
static void                   _shelterB4ReservoirView8ModelTask(Task* task);
static void                   _shelterB4ReservoirWaterCommand(s32 waterCommand);
static void                   _shelterB4ReservoirSetEventModelState(s32 modelState);
static void                   _shelterB4ReservoirSetBurstChance(s32 spawnChancePercent);
static void                   _shelterB4ReservoirCancelEffects(void);

/// Bodyless screen-wave recipe in slot 0, followed by TASK_DESC_END.
///
/// Runs at priority 192 and borrows a ScreenWaveCtx through spawnArg2.
/// Only one wave may be active; its context and this carrier's code/storage
/// must remain live through the final draw. The spawner copies the recipe.
static TaskDesc _gScreenWaveTaskDescs[2] = {
    { { { TASK_BODY_NONE, 192 } }, _screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Current sine-displacement strength, recomputed as frame * scale / span.
///
/// Strength 32 corresponds to one pixel at the sine peak. The seed tick resets
/// the initial 256 to zero; the context must have a positive span while drawing.
static s32 _gScreenWaveAmplitude = 256;

static TmdBone _gShelterB4ReservoirModel07220Skeleton[1] = {
#include "assets/shelter_b4_reservoir_model_07220_skeleton.inc"
};

static u32 _gShelterB4ReservoirModel07220PartVerts[1] = {
#include "assets/shelter_b4_reservoir_model_07220_partVerts.inc"
};

static SVECTOR _gShelterB4ReservoirModel07220Verts[15] = {
#include "assets/shelter_b4_reservoir_model_07220_verts.inc"
};

static u32 _gShelterB4ReservoirModel07220Stream[46] = {
#include "assets/shelter_b4_reservoir_model_07220_stream.inc"
};

static TmdSource _gShelterB4ReservoirModel07220 = {
    0,
    320,
    0,
    1,
    _gShelterB4ReservoirModel07220PartVerts,
    _gShelterB4ReservoirModel07220Verts,
    &_gShelterB4ReservoirModel07220Verts[15],
    _gShelterB4ReservoirModel07220Skeleton,
    _gShelterB4ReservoirModel07220Stream,
};

/// Message routes borrowed by the live reservoir room task.
///
/// Transition resolution takes two RoomEventMsg pointers; key-item, command
/// and sound routes take integer first payloads; action ignores both words.
/// Unsupported IDs return zero at the end marker. The table and callbacks
/// must outlive the room task.
static TaskMessageEntry _gShelterB4ReservoirRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB4ReservoirResolveTransitionMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB4ReservoirRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB4ReservoirIgnoreRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, _shelterB4ReservoirCommandMessage },
    { ROOM_MESSAGE_SOUND, _shelterB4ReservoirSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

enum {
    SHELTER_B4_RESERVOIR_TASK_WATER_EVENT       = 0,
    SHELTER_B4_RESERVOIR_TASK_MAIN_WATER_RAMP   = 1,
    SHELTER_B4_RESERVOIR_TASK_VIEW10_WATER_RAMP = 2,
    SHELTER_B4_RESERVOIR_TASK_EXIT              = 3,
    SHELTER_B4_RESERVOIR_EVENT_TASK_COUNT       = 4,
    SHELTER_B4_RESERVOIR_EVENT_TASK_PRIORITY    = 32,
    SHELTER_B4_RESERVOIR_WATER_TASK_PRIORITY    = 96,
    SHELTER_B4_RESERVOIR_DIVER_COMMAND_STRAND   = 6,
};

/// Bodyless event, main-water ramp, view-10 ramp and exit spawn recipes.
///
/// Indexed by SHELTER_B4_RESERVOIR_TASK_*; there is no end record. Ramp
/// spawnArg1 supplies updates and exit spawnArg1 selects the CAP command.
static TaskDesc _gShelterB4ReservoirEventTasks[SHELTER_B4_RESERVOIR_EVENT_TASK_COUNT] = {
    [SHELTER_B4_RESERVOIR_TASK_WATER_EVENT]       = { { { TASK_BODY_NONE, SHELTER_B4_RESERVOIR_EVENT_TASK_PRIORITY } }, _shelterB4ReservoirWaterEventTask, { .value = 0 } },
    [SHELTER_B4_RESERVOIR_TASK_MAIN_WATER_RAMP]   = { { { TASK_BODY_NONE, SHELTER_B4_RESERVOIR_EVENT_TASK_PRIORITY } }, _shelterB4ReservoirRampMainWaterHeightTask, { .value = 0 } },
    [SHELTER_B4_RESERVOIR_TASK_VIEW10_WATER_RAMP] = { { { TASK_BODY_NONE, SHELTER_B4_RESERVOIR_EVENT_TASK_PRIORITY } }, _shelterB4ReservoirRampView10WaterHeightTask, { .value = 0 } },
    [SHELTER_B4_RESERVOIR_TASK_EXIT]              = { { { TASK_BODY_NONE, SHELTER_B4_RESERVOIR_EVENT_TASK_PRIORITY } }, _shelterB4ReservoirExitTask, { .value = 0 } },
};

/// Strands the reservoir's placed Bog Diver after normal or skipped water events.
///
/// Command 6 restores targeting, places root Y at zero and enters ground combat.
/// The scripts borrow this command until dispatch to placed-actor index zero.
static ActorCommand _gShelterB4ReservoirDiverStrandCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_RESERVOIR } }, SHELTER_B4_RESERVOIR_DIVER_COMMAND_STRAND };

/// Single spawn recipe for the event model visible in saved view 8.
///
/// Creates a TMD body while deferring its draw buffer; no end record. The model
/// task borrows the room geometry and stays live for later script state requests.
static TaskDesc _gShelterB4ReservoirEventModelTasks[1] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), SHELTER_B4_RESERVOIR_EVENT_TASK_PRIORITY } }, _shelterB4ReservoirView8ModelTask, { .model = &_gShelterB4ReservoirModel07220 } },
};

/// Retained handle of the most recently started main or view-10 water ramp.
///
/// NULL means no retained ramp. A start overwrites this handle without killing
/// its previous task; either ramp's completion clears it, as does cancellation.
/// The event must sequence ramps to keep the handle live and unambiguous.
static Task* _gShelterB4ReservoirWaterRampTask = NULL;

/// Borrowed event-model task published by room initialization.
///
/// Script state requests require a successful spawn and a live task. The handle
/// is not checked or cleared here; it must not be used after room teardown.
static Task* _gShelterB4ReservoirEventModelTask = NULL;

/// Retained five-word initializer with no established role.
///
/// The values resemble an indexed animation request, but no code or data
/// pointer uses this storage, so that interpretation is unproven.
static u32 D_shelter_b4_reservoir_80184934[5] = { 1, 1, 0, 0, 1 };

enum {
    SHELTER_B4_RESERVOIR_WATER_EVENT_CAP_COMMAND = 4,
    SHELTER_B4_RESERVOIR_WATER_EVENT_RETURN_VIEW = 5,
    SHELTER_B4_RESERVOIR_SOUND_EVENT_PHASE1      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_RESERVOIR, 5),
    SHELTER_B4_RESERVOIR_SOUND_EVENT_PHASE2      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_RESERVOIR, 6),
    SHELTER_B4_RESERVOIR_SOUND_EVENT_PHASE3      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_RESERVOIR, 7),
    SHELTER_B4_RESERVOIR_EVENT_SOUND_ATTENUATION = 20,
};

/// CAP-cued water-event sequence, ending with actor restoration in view 5.
///
/// Drives model motion, bursts, screen distortion and both water ramps while
/// CAP command 4 supplies cues. END releases the event gate. Borrowed by the
/// event interpreter until completion or skip; the room must stay loaded.
static EvsCommand _gShelterB4ReservoirWaterEventScript[48] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = SHELTER_B4_RESERVOIR_WATER_EVENT_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB4ReservoirCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _screenWaveRun }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirSetEventModelState }, { .value = SHELTER_B4_RESERVOIR_EVENT_MODEL_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirSetEventModelState }, { .value = SHELTER_B4_RESERVOIR_EVENT_MODEL_RESTART_MOTION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SHELTER_B4_RESERVOIR_SOUND_EVENT_PHASE1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirSetBurstChance }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirSetBurstChance }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirSetBurstChance }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirSetBurstChance }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirSetBurstChance }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB4ReservoirCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirSetBurstChance }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _screenWaveRun }, { .value = SCREEN_WAVE_RAMP_FINISHED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirWaterCommand }, { .value = SHELTER_B4_RESERVOIR_WATER_START_MAIN_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = SHELTER_B4_RESERVOIR_SOUND_EVENT_PHASE1 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SHELTER_B4_RESERVOIR_SOUND_EVENT_PHASE2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirSetBurstChance }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB4ReservoirCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirWaterCommand }, { .value = SHELTER_B4_RESERVOIR_WATER_FINISH_MAIN_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirWaterCommand }, { .value = SHELTER_B4_RESERVOIR_WATER_START_VIEW10_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FADE_SOUND_ATTENUATION, { .value = SHELTER_B4_RESERVOIR_SOUND_EVENT_PHASE2 }, { .value = SHELTER_B4_RESERVOIR_EVENT_SOUND_ATTENUATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SHELTER_B4_RESERVOIR_SOUND_EVENT_PHASE3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirWaterCommand }, { .value = SHELTER_B4_RESERVOIR_WATER_RESET_VIEW10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = SHELTER_B4_RESERVOIR_WATER_EVENT_RETURN_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gShelterB4ReservoirDiverStrandCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = SHELTER_B4_RESERVOIR_SOUND_EVENT_PHASE2 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip sequence that commits the main water endpoint and restores play in view 5.
///
/// Fades around scene cleanup, finishes distortion, disables bursts, strands
/// the placed diver and restores actor drawing/control. It preserves the
/// current view-10 water height. END releases the event gate.
/// Borrowed by the event interpreter while the reservoir overlay is loaded.
static EvsCommand _gShelterB4ReservoirWaterEventSkipScript[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = SHELTER_B4_RESERVOIR_WATER_EVENT_RETURN_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _screenWaveRun }, { .value = SCREEN_WAVE_RAMP_FINISHED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirWaterCommand }, { .value = SHELTER_B4_RESERVOIR_WATER_FINISH_MAIN_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB4ReservoirSetBurstChance }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gShelterB4ReservoirDiverStrandCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB4ReservoirCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Retained water-drift velocity kind in the low nibble and draw mode in the high.
///
/// Any high-nibble bit selects upright drawing; zero selects rotated drawing.
/// Initial kind 4 leaves direction zero before normalization. The assembled
/// argument has no reader in this image; live spray uses separate arguments.
static u8 _gShelterB4ReservoirWaterDriftKindAndDraw = 4;

/// Retained water-drift launch speed in parent-coordinate units per running update.
///
/// Encodes bits 16..23; zero selects speed 64 when initial velocity is zero.
static u8 _gShelterB4ReservoirWaterDriftInitialSpeed = 8;

/// Retained running updates per water-drift animation cell, encoded in bits 12..15.
///
/// Valid encodings are 0..15, with zero selecting one update per cell. Packing
/// does not mask this byte; larger values would spill into the speed field.
static u8 _gShelterB4ReservoirWaterDriftAnimationPeriod = 3;

/// Retained water-drift size scale and two zero bytes with unproven roles.
///
/// The half-extent must fit 0..4095: raw OR packing does not mask it. Only
/// `halfExtent` is read; the complete four-byte initialized object is retained.
static _ShelterB4ReservoirWaterDriftHalfExtent _gShelterB4ReservoirWaterDriftHalfExtent = { 640, { 0, 0 } };

s16 gShelterB4ReservoirMainWaterY = SHELTER_B4_RESERVOIR_MAIN_WATER_INITIAL_Y;

/// World-unit Y of the separate water plane drawn in view 10.
///
/// The event ramps from 0 to -1800 or resets to 0. Drawing clamps to that
/// range; spray and ripple placement also sample this plane's height.
static s16 _gShelterB4ReservoirView10WaterY = SHELTER_B4_RESERVOIR_VIEW10_WATER_MAX_Y;

/// Single bodyless spawn recipe for the reservoir's water rendering task.
///
/// Initialization supplies zero payloads; there is no terminator. The task
/// publishes water Y and borrows the room's surfaces for its lifetime.
static TaskDesc _gShelterB4ReservoirWaterTasks[1] = {
    { { { TASK_BODY_NONE, SHELTER_B4_RESERVOIR_WATER_TASK_PRIORITY } }, _shelterB4ReservoirWaterTask, { .value = 0 } },
};

/// Main water rectangle and its `WATER_SURFACE_LIST_END` terminator.
///
/// World-unit X/Z extents; its depth is rewritten from the main water height.
/// Drawing applies the separate X offset and subdivides along Z into 32 quads,
/// ignoring the live record's zero segment count.
static RoomWaterSurface _gShelterB4ReservoirMainWaterSurfaces[2] = {
    { -980, -5900, 2023, 9900, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

/// Two fixed water rectangles subdivided along Z at the main water height.
///
/// Extents use world units; each live record draws 64 strips. The third record
/// ends the list with `WATER_SURFACE_LIST_END` and is never drawn.
static RoomWaterSurface _gShelterB4ReservoirZWaterSurfaces[3] = {
    { 1037, -5900, 950, 0x2E18, 64 },
    { 1987, -5900, 950, 0x2E18, 64 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

/// Crosswise water rectangle subdivided along X at the main water height.
///
/// Extents use world units; the live record draws 16 strips. The second record
/// ends the list with `WATER_SURFACE_LIST_END` and is never drawn.
static RoomWaterSurface _gShelterB4ReservoirXWaterSurfaces[2] = {
    { 2940, -3900, 2900, 1800, 16 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

/// Four water rectangles drawn at the separate view-10 water height.
///
/// Extents use world units; Z subdivisions are 64, 64, 1 and 1. The fifth
/// record ends the list with `WATER_SURFACE_LIST_END` and is never drawn.
static RoomWaterSurface _gShelterB4ReservoirView10WaterSurfaces[5] = {
    { 1400, 3100, 1000, 4800, 64 },
    { -1028, 3100, 2428, 4800, 64 },
    { -1028, -5900, 2428, 9000, 1 },
    { 788, -5900, 1500, 3800, 1 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

/// X displacement of the main water rectangle, with retained uninterpreted bytes.
///
/// Only the signed halfword offset is read. The following nonzero bytes have
/// no established role or independent accesses; their meaning remains unproven.
static struct {
    s16  xOffset;      // Added to the rectangle's world X, in world units
    byte unknown_2[2]; // Retained bytes with no observed accesses; role unproven
} _gShelterB4ReservoirMainWaterPlacement = { 0, { 0xC3, 0x96 } };
STATIC_ASSERT_SIZEOF(_gShelterB4ReservoirMainWaterPlacement, 4);

/// Endpoint pairs and event-disc centre for the reservoir's ambient glows.
///
/// Each capsule uses exactly two consecutive XYZ points; mapped view selects
/// among six pairs. The disc tint reflects the reservoir event's completion.
/// Coordinates are signed whole world units and borrowed only during drawing;
/// fourth halfwords are not consumed. The last eight stored bytes have no consumer,
/// so its role and membership in the glow geometry remain unproven.
typedef struct {
    SVECTOR capsuleEndpoints[6][2]; // Six independent endpoint pairs, selected by mapped view
    SVECTOR eventIndicator;         // Disc centre whose tint reflects event completion
    byte    field_68[8];            // Retained bytes; format, role and object boundary unproven
} _ShelterB4ReservoirGlowPoints;
STATIC_ASSERT_SIZEOF(_ShelterB4ReservoirGlowPoints, 0x70);

/// Capsule endpoint pairs and event-indicator centre, in signed world units.
///
/// Pairs 0..5 draw in views 4/5, 7, 3/9, 4, 4 and 2 respectively; the disc
/// draws in views 4..6. Points are borrowed only during each draw call.
static _ShelterB4ReservoirGlowPoints _gShelterB4ReservoirGlowPoints = {
    {
        { { -1580, -4460, 5900, 0 }, { -420, -4460, 5900, 0 } },
        { { -1580, -4460, -5900, 0 }, { -420, -4460, -5900, 0 } },
        { { 2900, -2520, -4440, 0 }, { 2900, -2520, -5600, 0 } },
        { { 2900, -2520, 590, 0 }, { 2900, -2520, -580, 0 } },
        { { 2900, -2520, 4600, 0 }, { 2900, -2520, 3420, 0 } },
        { { 3920, -2390, -3000, 0 }, { 5080, -2390, -3000, 0 } },
    },
    { -2640, -2980, 4900, 0 },
    { 0xB0, 0xF5, 0x5C, 0xF4, 0xC0, 0x12, 0, 0 },
};

/// World-coordinate anchor for burst points scattered in the YZ plane.
///
/// Each point adds 2048 to X and polar radius 0..448 to Y/Z. The fourth
/// halfword is unused; scattering copies XYZ without retaining this pointer.
static SVECTOR _gShelterB4ReservoirBurstAnchor = { -1000, -1000, -5000, 0 };

/// Spray and ripple placement inputs for mapped reservoir view 10.
///
/// The fixed origin emits rotated spray. The second emits upright spray and
/// ripples at the current view-10 water-plane Y, refreshed before spawning.
/// XYZ use signed whole game units in the current GsWSMATRIX's input space;
/// spawning snapshots them and retains a borrowed pointer to the room storage.
/// Neither origin's fourth halfword is consumed.
typedef struct {
    SVECTOR fixedSpray;   // Fixed origin for rotated spray
    SVECTOR waterSurface; // Upright spray/ripple origin; Y follows the view-10 water plane
} _ShelterB4ReservoirView10WaterEffectPoints;
STATIC_ASSERT_SIZEOF(_ShelterB4ReservoirView10WaterEffectPoints, 0x10);

/// View-10 spray and ripple placement in signed whole game units.
///
/// The surface Y is refreshed before spawning. The effect spawner copies XYZ
/// and retains borrowed offsets; this storage stays live with the room.
static _ShelterB4ReservoirView10WaterEffectPoints _gShelterB4ReservoirView10WaterEffectPoints = {
    { 0, -3210, -5000, 0 },
    { 0, 0, -5000, 0 },
};

/// Previous view-space translations of player model coordinates 14 and 17.
///
/// XYZ narrow to signed halfwords. Initialization samples the existing cached
/// matrices; running splash checks compose them and refresh the cache only
/// while below the water plane. The fourth halfwords have no reads or writes.
static SVECTOR _gShelterB4ReservoirPlayerSamplePositions[2] = { 0 };

#include "../../shared/room/room_visual_effects_disc_data.inc.c"

u8 gShelterB4ReservoirStairAscentSurfaces[16] = {
    2,
    2,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    5,
    5,
    5,
    5,
    5,
    5,
    0,
};
u8 gShelterB4ReservoirStairDescentSurfaces[16] = {
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    2,
    2,
    0,
    0,
    0,
};

WorldCoordRoomLighting gShelterB4ReservoirRoomLighting[2] = {
    { &_gShelterB4ReservoirRoomLights, NULL },
    { &_gShelterB4ReservoirRoomLights, NULL },
};

WorldCollisionRoomResources gShelterB4ReservoirRoomCollisionResources[2] = {
    {
        .grid                 = &_gShelterB4ReservoirRoomCollisionGrid,
        .viewBoundaryTriggers = _gShelterB4ReservoirViewBoundaryTriggers,
        .actionTriggers       = _gShelterB4ReservoirRoom1ActionTriggers,
        .occluders            = _gShelterB4ReservoirRoomOccluders,
    },
    {
        .grid                 = &_gShelterB4ReservoirRoomCollisionGrid,
        .viewBoundaryTriggers = _gShelterB4ReservoirViewBoundaryTriggers,
        .actionTriggers       = _gShelterB4ReservoirRoom2ActionTriggers,
        .occluders            = _gShelterB4ReservoirRoomOccluders,
    },
};

u8* gShelterB4ReservoirViewMaps[2] = {
    gViewIdentityMap,
    gViewIdentityMap,
};

ViewCount gShelterB4ReservoirRoomViewCounts[2] = { ARRAY_SIZE(gShelterB4ReservoirViewCameras), ARRAY_SIZE(gShelterB4ReservoirViewCameras) };

DirectionWarpEntry gShelterB4ReservoirWarpEntries[2] = {
    { { { .word = 3072 }, 5500, 0, -3000 }, { 0, 0, 0, 0 }, { { .word = 3584 }, 5000, 0, -3650 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_RESERVOIR, 4), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_RESERVOIR, 3), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -2527, -2000, -2825 }, { 0, 0, 0, 0 }, { { .word = 512 }, -2700, -2000, -2000 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_FADE_DEPARTURE, GAME_FLAG_MAP_MARK_RESERVOIR_1BE },
};

/// Eight waypoints shared by all four reservoir Bog Diver patrol selections.
///
/// X/Z use world units; swimming adds the sampled water Y to waypoint depth.
/// The ring wraps at index 7, with no terminator. Borrowed for each diver's
/// lifetime; the fourth halfwords are unused.
static SVECTOR _gShelterB4ReservoirDiverPatrolRing[8] = {
    { 400, 1000, -2400, 0 },
    { 2000, 1000, 2100, 0 },
    { 2000, 1000, 5000, 0 },
    { 1500, 1000, -4000, 0 },
    { 0, 1000, -600, 0 },
    { 200, 1000, -3900, 0 },
    { 2000, 1000, 400, 0 },
    { 1000, 1000, -4500, 0 },
};

SVECTOR* gShelterB4ReservoirDiverPatrolRings[4] = {
    _gShelterB4ReservoirDiverPatrolRing,
    _gShelterB4ReservoirDiverPatrolRing,
    _gShelterB4ReservoirDiverPatrolRing,
    _gShelterB4ReservoirDiverPatrolRing,
};

SVECTOR gShelterB4ReservoirDiverSurfaceSpots[5] = {
    { 0, 0, 0, 0 },
    { 0, 1000, -3000, 0 },
    { 0, 1000, 0, 0 },
    { 2000, 1000, 3800, 0 },
    { 0, 0, 0, -1 },
};

static SVECTOR _gShelterB4ReservoirRoomCollisionGridNormals[22] = {
#include "assets/shelter_b4_reservoir_collision_084F8_normals.inc"
};

static SVECTOR _gShelterB4ReservoirRoomCollisionGridVerts[98] = {
#include "assets/shelter_b4_reservoir_collision_084F8_verts.inc"
};

static WorldCollisionGridFace _gShelterB4ReservoirRoomCollisionGridFaces[45] = {
#include "assets/shelter_b4_reservoir_collision_084F8_faces.inc"
};

static s16 _gShelterB4ReservoirRoomCollisionGridCells[310] = {
#include "assets/shelter_b4_reservoir_collision_084F8_cells.inc"
};

enum {
    SHELTER_B4_RESERVOIR_GRID_X_CELLS   = 5,
    SHELTER_B4_RESERVOIR_GRID_Z_CELLS   = 5,
    SHELTER_B4_RESERVOIR_GRID_CELL_SIZE = 4000,
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterB4ReservoirRoomCollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..309
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_B4_RESERVOIR_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB4ReservoirRoomCollisionGridCells[(halfwordOffset)])
static s16* _gShelterB4ReservoirRoomCollisionGridTable[SHELTER_B4_RESERVOIR_GRID_X_CELLS * SHELTER_B4_RESERVOIR_GRID_Z_CELLS] = {
#include "assets/shelter_b4_reservoir_collision_084F8_table.inc"
};
#undef SHELTER_B4_RESERVOIR_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh and X-major cell index shared by reservoir room IDs 1 and 2.
///
/// Activation binds the view coordinate. The 45 faces use vertices 0..97 and
/// normals 0..21; the 5 by 5 cell table borrows terminated face-index lists.
/// Positions and 4000-unit cells use room coordinates, normals use Q12.
/// The descriptor and all five pools remain live while the room is loaded.
static WorldCollisionGrid _gShelterB4ReservoirRoomCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB4ReservoirRoomCollisionGridNormals,
    .vertices    = _gShelterB4ReservoirRoomCollisionGridVerts,
    .faces       = _gShelterB4ReservoirRoomCollisionGridFaces,
    .cellFaceIds = _gShelterB4ReservoirRoomCollisionGridTable,
    .xBias       = 10950,
    .zBias       = 6000,
    .cellCountX  = SHELTER_B4_RESERVOIR_GRID_X_CELLS,
    .cellCountZ  = SHELTER_B4_RESERVOIR_GRID_Z_CELLS,
    .cellSize    = SHELTER_B4_RESERVOIR_GRID_CELL_SIZE,
    .faceCount   = ARRAY_SIZE(_gShelterB4ReservoirRoomCollisionGridFaces),
};

ViewCamera gShelterB4ReservoirViewCameras[10] = {
    { { { { 4095, 0, -20 }, { -20, 35, -4095 }, { 0, 4095, 35 } }, { 151, 0x7530, -115 } }, 447 },
    { { { { -1356, 0, -3864 }, { 134, 4093, -47 }, { 3862, -142, -1355 } }, { -836, 880, 2121 } }, 230 },
    { { { { -4037, 0, 688 }, { 118, 4034, 695 }, { -678, 705, -3977 } }, { -1560, 2040, -2150 } }, 257 },
    { { { { 3928, 0, 1160 }, { 175, 4049, -593 }, { -1147, 618, 3883 } }, { -2310, 2260, 4900 } }, 257 },
    { { { { 4066, 0, 488 }, { 137, 3928, -1149 }, { -468, 1158, 3900 } }, { 1360, 4030, 3480 } }, 257 },
    { { { { 1937, 0, 3608 }, { 1304, 3818, -700 }, { -3364, 1480, 1806 } }, { 730, 3443, -4061 } }, 358 },
    { { { { -3980, 0, 967 }, { -85, 4080, -351 }, { -963, -361, -3964 } }, { 1190, 2910, -1600 } }, 257 },
    { { { { -592, 0, 4052 }, { 353, 4080, 51 }, { -4037, 357, -590 } }, { -2060, 1280, 4640 } }, 380 },
    { { { { -2610, 0, -3155 }, { -2761, 1983, 2284 }, { 1528, 3583, -1264 } }, { 560, 5430, 2400 } }, 230 },
    { { { { -3991, 0, -921 }, { -208, 3989, 902 }, { 897, 925, -3887 } }, { 360, 2120, -7690 } }, 257 },
};

/// Empty foreground list for mapped view 1; the zero first count retains image strips.
static SpriteBatch _gShelterB4ReservoirView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty foreground list for mapped view 2; the zero first count retains image strips.
static SpriteBatch _gShelterB4ReservoirView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles in pixels and sorting depths for mapped view 3.
static SpriteSource _gShelterB4ReservoirView3SpriteSources[11] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, -96, 1050, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -136, -104, 1000, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -144, -112, 975, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, -120, 925, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, -56, 925, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -144, -56, 975, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, 8, 925, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, 72, 925, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -144, 8, 975, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -144, 56, 975, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -136, 40, 1000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

/// View-3 foreground range, preceded by the zero-count image-strip selection.
static SpriteBatch _gShelterB4ReservoirView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4ReservoirView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles in pixels and sorting depths for mapped view 4.
static SpriteSource _gShelterB4ReservoirView4SpriteSources[4] = {
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 32, -64, 2000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 8, -8, 1500, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 16, -32, 1525, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 24, -64, 1750, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
};

/// View-4 foreground range, preceded by the zero-count image-strip selection.
static SpriteBatch _gShelterB4ReservoirView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4ReservoirView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles in pixels and sorting depths for mapped view 5.
static SpriteSource _gShelterB4ReservoirView5SpriteSources[58] = {
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 80, 112, 1338, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 80, 104, 1347, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 96, 1384, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 88, 1405, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 80, 1438, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 72, 1740, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 64, 1504, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 56, 1630, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 48, 1578, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 40, 1500, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 32, 1627, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 24, 1693, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 16, 1749, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 8, 1795, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 0, 1811, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, -8, 1996, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -16, 2195, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -24, 2325, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, -40, 1875, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, -40, 1675, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, -32, 1625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 0, 1500, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 128, 0, 1375, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 24, 1300, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 40, 1375, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 72, 1375, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 40, 1300, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 72, 1300, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 144, 56, 1250, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 80, 1225, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 0, 1875, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 8, 1795, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 16, 1685, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 24, 1693, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 32, 1625, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 40, 1600, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 48, 1578, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 56, 1550, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 64, 1504, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 72, 1462, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 80, 1438, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 88, 1405, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 96, 1384, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 104, 1347, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 112, 1338, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -8, 16, 1375, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -8, 24, 1350, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -8, 32, 1325, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 40, 1250, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 48, 1125, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 56, 1075, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 64, 1000, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 72, 925, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 80, 875, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 88, 850, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -8, 96, 825, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -8, 104, 775, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -8, 112, 750, { .fields = { 40, 8 } }, 128, 128, 128, 0 },
};

enum { SHELTER_B4_RESERVOIR_VIEW5_SECOND_BATCH_FIRST_SPRITE = 45 };

/// Two adjacent view-5 foreground ranges after the zero-count image-strip selection.
///
/// The first range retains serialized bytes {1, 0}; their role is unproven.
static SpriteBatch _gShelterB4ReservoirView5SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, SHELTER_B4_RESERVOIR_VIEW5_SECOND_BATCH_FIRST_SPRITE, 0, 0, { 1, 0 } },
    { SHELTER_B4_RESERVOIR_VIEW5_SECOND_BATCH_FIRST_SPRITE, ARRAY_SIZE(_gShelterB4ReservoirView5SpriteSources) - SHELTER_B4_RESERVOIR_VIEW5_SECOND_BATCH_FIRST_SPRITE, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty foreground list for mapped view 6; the zero first count retains image strips.
static SpriteBatch _gShelterB4ReservoirView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles in pixels and sorting depths for mapped view 7.
static SpriteSource _gShelterB4ReservoirView7SpriteSources[9] = {
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -72, 48, 1912, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -72, 56, 1908, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -80, 64, 1570, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -80, 72, 1443, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -80, 80, 1373, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -80, 88, 1241, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -88, 96, 1022, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -88, 104, 963, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -88, 112, 925, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
};

/// View-7 foreground range, preceded by the zero-count image-strip selection.
static SpriteBatch _gShelterB4ReservoirView7SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4ReservoirView7SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles in pixels and sorting depths for mapped view 8.
static SpriteSource _gShelterB4ReservoirView8SpriteSources[36] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 24, 750, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -112, -120, 750, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -112, -48, 750, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, -112, 24, 750, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -104, 72, 750, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 40 } }, -56, 32, 750, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 48 } }, -56, 72, 750, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, 48, 750, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, 8, 72, 750, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, 64, 72, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 120, 72, 750, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 8, 32, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 16, 40, 750, { .fields = { 120, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 40 } }, 64, 32, 750, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 32, 750, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, 56, -40, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -56, 16, 750, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 8, 16, 750, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -56, 24, 750, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -56, 8, 750, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, 0, 750, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 0, 0, 750, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -56, -24, 750, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, 0, -24, 750, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 40, -32, 750, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -56, -32, 750, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -56, -48, 750, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 0, -48, 750, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, 104, -40, 750, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 104, -80, 750, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 40 } }, 104, -120, 750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, 56, -80, 750, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 56, -120, 750, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, 0, -120, 750, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -56, -120, 750, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -56, -8, 750, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
};

/// View-8 foreground range, preceded by the zero-count image-strip selection.
static SpriteBatch _gShelterB4ReservoirView8SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4ReservoirView8SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles in pixels and sorting depths for mapped view 9.
static SpriteSource _gShelterB4ReservoirView9SpriteSources[11] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, -72, 1274, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 825, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, -80, 942, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -48, 1031, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -120, 909, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -80, -120, 1060, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, -80, 1298, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, -80, 986, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -80, 1030, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, -72, 1009, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, -64, 1013, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
};

/// View-9 foreground range, preceded by the zero-count image-strip selection.
static SpriteBatch _gShelterB4ReservoirView9SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4ReservoirView9SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty foreground list for mapped view 10; the zero first count retains image strips.
static SpriteBatch _gShelterB4ReservoirView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB4ReservoirSpriteViews[10] = {
    { { .empty = _gShelterB4ReservoirView1SpriteBatches }, _gShelterB4ReservoirView1SpriteBatches, NULL },
    { { .empty = _gShelterB4ReservoirView2SpriteBatches }, _gShelterB4ReservoirView2SpriteBatches, NULL },
    { { .elements = _gShelterB4ReservoirView3SpriteSources }, _gShelterB4ReservoirView3SpriteBatches, NULL },
    { { .elements = _gShelterB4ReservoirView4SpriteSources }, _gShelterB4ReservoirView4SpriteBatches, NULL },
    { { .elements = _gShelterB4ReservoirView5SpriteSources }, _gShelterB4ReservoirView5SpriteBatches, NULL },
    { { .empty = _gShelterB4ReservoirView6SpriteBatches }, _gShelterB4ReservoirView6SpriteBatches, NULL },
    { { .elements = _gShelterB4ReservoirView7SpriteSources }, _gShelterB4ReservoirView7SpriteBatches, NULL },
    { { .elements = _gShelterB4ReservoirView8SpriteSources }, _gShelterB4ReservoirView8SpriteBatches, NULL },
    { { .elements = _gShelterB4ReservoirView9SpriteSources }, _gShelterB4ReservoirView9SpriteBatches, NULL },
    { { .empty = _gShelterB4ReservoirView10SpriteBatches }, _gShelterB4ReservoirView10SpriteBatches, NULL },
};

/// Eight world-space point lights shared by both reservoir room selections.
///
/// All views use these lights. RGB intensity has 12 fractional bits and falloff
/// radii use world units. Coordinate composition and attenuation keep them mutable.
static WorldCoordPointLight _gShelterB4ReservoirPointLights[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -4450, 5000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -4450, -5000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2000, -2520, -5000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2000, -2520, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2581, 4061 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2000, -2520, 4000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -4450, -1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 5367, 6365 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -3000, -5000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1228, 1228, 1228 }, { 0, 0 } }, 5000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4500, -2000, -3000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 1500, 2500 },
};

/// Reservoir light collection: eight point lights, with no directional or cone lights.
static WorldCoordRoomLights _gShelterB4ReservoirRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB4ReservoirPointLights),
    .pointLights           = _gShelterB4ReservoirPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Eight directed view boundaries shared by both reservoir room IDs.
///
/// Pairs connect views 7/5, 5/4, 4/3 and 3/2. Setup binds the current view,
/// links and enables every record including the eighth, marked LAST.
/// Positions use room units and normals Q12; storage stays live while linked.
static WorldCollisionTrigger _gShelterB4ReservoirViewBoundaryTriggers[8] = {
    { NULL, NULL, NULL, { -3072, -1856, -1137, 0 }, { { -2192, -4304, 0, 0 }, { 2192, -4304, 0, 0 }, { -2192, 4304, 0, 0 }, { 2192, 4304, 0, 0 } }, { 0, 0, -4120, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3008, -1729, -1264, 0 }, { { 2128, -4304, 0, 0 }, { -2128, -4304, 0, 0 }, { 2128, 4304, 0, 0 }, { -2128, 4304, 0, 0 } }, { 0, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -32, -1856, 3200, 0 }, { { 1008, -4304, 0, 0 }, { -1008, -4304, 0, 0 }, { 1008, 4304, 0, 0 }, { -1008, 4304, 0, 0 } }, { 0, 0, 4108, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -16, -1889, 3328, 0 }, { { -1088, -4304, 0, 0 }, { 1088, -4304, 0, 0 }, { -1088, 4304, 0, 0 }, { 1088, 4304, 0, 0 } }, { 0, 0, -4116, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1184, -1952, -1632, 0 }, { { 2188, -4304, 204, 0 }, { -2191, -4304, -207, 0 }, { 2188, 4304, 204, 0 }, { -2191, 4304, -207, 0 } }, { -384, 0, 4088, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1168, -1888, -1504, 0 }, { { -2276, -4304, -229, 0 }, { 2270, -4304, 222, 0 }, { -2276, 4304, -229, 0 }, { 2270, 4304, 222, 0 } }, { 406, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 4857, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3008, -2016, -2992, 0 }, { { 0, -4304, -1392, 0 }, { 0, -4304, 1392, 0 }, { 0, 4304, -1392, 0 }, { 0, 4304, 1392, 0 } }, { 4103, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3136, -1984, -2976, 0 }, { { 0, -4304, 1264, 0 }, { 0, -4304, -1264, 0 }, { 0, 4304, 1264, 0 }, { 0, 4304, -1264, 0 } }, { -4108, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4463, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

enum {
    // Indexed stair-surface row 4, with ten steps; control bit 8 selects descent.
    SHELTER_B4_RESERVOIR_STAIR_SURFACES = 0x80 | (4 << 4) | 10,
    SHELTER_B4_RESERVOIR_STAIR_DESCEND  = 0x100,
    // High nibble: this area's departure warp. Low nibble: destination warp.
    SHELTER_B4_RESERVOIR_WATER_SUPPLY_WARP_PAIR = (1 << 4) | 2,
    SHELTER_B4_RESERVOIR_UPPER_SEWER_WARP_PAIR  = (2 << 4) | 2,
};

/// Seven action quads selected by reservoir room ID 1.
///
/// Includes stair ascent, four CAP/room interactions and the water-supply and
/// upper-sewer exits. Setup binds the view and links/enables all records through
/// the seventh's LAST marker; positions use room units and normals Q12.
/// These mutable records must remain live until unlinked.
static WorldCollisionTrigger _gShelterB4ReservoirRoom1ActionTriggers[7] = {
    { NULL, NULL, NULL, { 0, -48, 864, 0 }, { { -1024, 0, -160, 0 }, { 1024, 0, -160, 0 }, { -1024, 0, 160, 0 }, { 1024, 0, 160, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1031, WORLD_COLLISION_TRIGGER_ACTION_FACING, SHELTER_B4_RESERVOIR_STAIR_SURFACES, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 224, -1888, 3888, 0 }, { { -1024, 0, -320, 0 }, { 1024, 0, -320, 0 }, { -1024, 0, 320, 0 }, { 1024, 0, 320, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1070, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5661, -48, -3072, 0 }, { { 448, 0, -1024, 0 }, { 448, 0, 1024, 0 }, { -448, 0, -1024, 0 }, { -448, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B4_WATER_SUPPLY, SHELTER_B4_RESERVOIR_WATER_SUPPLY_WARP_PAIR, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2736, -2096, -3552, 0 }, { { 240, 0, -1024, 0 }, { 240, 0, 1024, 0 }, { -240, 0, -1024, 0 }, { -240, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B4_UPPER_SEWER, SHELTER_B4_RESERVOIR_UPPER_SEWER_WARP_PAIR, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1968, -1888, 5152, 0 }, { { 576, 0, -1024, 0 }, { 576, 0, 1024, 0 }, { -576, 0, -1024, 0 }, { -576, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2817, -2080, -5648, 0 }, { { -1024, 0, -432, 0 }, { 1024, 0, -432, 0 }, { -1024, 0, 432, 0 }, { 1024, 0, 432, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2720, -2080, -816, 0 }, { { -448, 0, -1216, 0 }, { 448, 0, -1216, 0 }, { -448, 0, 1216, 0 }, { 448, 0, 1216, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 1292, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Nine action quads selected by reservoir room ID 2.
///
/// Includes stair ascent/descent, six CAP interactions and the
/// water-supply exit. The upper-sewer-side quad runs CAP command 2 instead of
/// warping. Setup binds the view and links/enables all records through the ninth's
/// LAST marker; positions use room units and normals Q12. Storage lives until unlink.
static WorldCollisionTrigger _gShelterB4ReservoirRoom2ActionTriggers[9] = {
    { NULL, NULL, NULL, { -512, -48, 864, 0 }, { { -1024, 0, -160, 0 }, { 1024, 0, -160, 0 }, { -1024, 0, 160, 0 }, { 1024, 0, 160, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1031, WORLD_COLLISION_TRIGGER_ACTION_FACING, SHELTER_B4_RESERVOIR_STAIR_SURFACES, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 384, -1888, 3776, 0 }, { { -1024, 0, -208, 0 }, { 1024, 0, -208, 0 }, { -1024, 0, 208, 0 }, { 1024, 0, 208, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_FACING | SHELTER_B4_RESERVOIR_STAIR_DESCEND, SHELTER_B4_RESERVOIR_STAIR_SURFACES, 128, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5872, -48, -3072, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B4_WATER_SUPPLY, SHELTER_B4_RESERVOIR_WATER_SUPPLY_WARP_PAIR, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2736, -2096, -3552, 0 }, { { 240, 0, -1024, 0 }, { 240, 0, 1024, 0 }, { -240, 0, -1024, 0 }, { -240, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2144, -2080, 5232, 0 }, { { 848, 0, -816, 0 }, { 848, 0, 816, 0 }, { -848, 0, -816, 0 }, { -848, 0, 816, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1056, -64, -5728, 0 }, { { 848, 0, -816, 0 }, { 848, 0, 816, 0 }, { -848, 0, -816, 0 }, { -848, 0, 816, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2080, -64, 5616, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2816, -2064, -5648, 0 }, { { -1024, 0, -352, 0 }, { 1024, 0, -352, 0 }, { -1024, 0, 352, 0 }, { 1024, 0, 352, 0 } }, { 0, 4094, 0, 0 }, { 0, 0, 4096, 0 }, 1078, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2768, -2080, -944, 0 }, { { -496, 0, -1200, 0 }, { 496, 0, -1200, 0 }, { -496, 0, 1200, 0 }, { 496, 0, 1200, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 1292, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Variant-1 resource list: one slot-1 Bog Diver recipe and an end record.
///
/// Entry ID 4 matches its placement; the loaded actor package and room table
/// must remain live through loading, spawning and actor teardown.
static AreaResource _gShelterB4ReservoirAreaVariant1Resources[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant-2 resources: Mad Chaser, Creeping Stranger and Skull Stalker recipes.
///
/// Entry IDs 44, 72 and 73 select these recipes; the placements use only 44/72,
/// and the retained ID-73 file request has no cataloged file. The fourth record ends the list.
/// The room and referenced actor packages must remain live through their consumers.
static AreaResource _gShelterB4ReservoirAreaVariant2Resources[4] = {
    { 44, 44, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_04400_MAD_CHASER_TASK_ORDINARY, { 0, 0 }, gActor04400MadChaserTaskDescs },
    { 72, 72, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor207200CreepingStrangerTaskDesc },
    { 73, 73, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor207200SkullStalkerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant-3 resource list: Amoeba, Slouch and Sucklerceph recipes, then an end record.
///
/// Entry IDs 24, 70 and 71 match the three placements. File 207100 requested by
/// ID 71 is absent; that actor uses the Sucklerceph recipe loaded with ID 70.
/// The room and actor packages must remain live through loading, spawning and teardown.
static AreaResource _gShelterB4ReservoirAreaVariant3Resources[4] = {
    { 24, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102400Tasks },
    { 70, 70, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor207000SlouchTaskDescs },
    { 71, 71, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor207000SucklercephTaskDescs },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant-11 resource list: one Bog Diver recipe shared by two placements.
///
/// Entry ID 4 selects the slot-1 body descriptor; the second record ends the list.
/// The room and actor package must remain live through loading, spawning and teardown.
static AreaResource _gShelterB4ReservoirAreaVariant11Resources[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

enum {
    SHELTER_B4_RESERVOIR_DIVER_STRANDED              = 0,
    SHELTER_B4_RESERVOIR_DIVER_WATER_OR_GROUND       = 1,
    SHELTER_B4_RESERVOIR_SUCKLERCEPH_TEXTURE_VARIANT = 1,
};

/// Variant-1 placement list: one Bog Diver at (900, 1000, -2400), then an end record.
///
/// Mode 1 selects its water/reservoir-ground behavior; row 4 selects actor tuning.
/// XYZ use world units and yaw uses 4096 per turn. The actor may retain its
/// placement pointer, so room storage must stay loaded through teardown.
static AreaPlacement _gShelterB4ReservoirAreaVariant1Placements[2] = {
    { 4, 0, SHELTER_B4_RESERVOIR_DIVER_WATER_OR_GROUND, 900, 1000, -2400, 3072, 0, 0, 2, 4 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant-2 placement list: two Mad Chasers and two Creeping Strangers.
///
/// Four world-unit transforms with 4096-unit yaw precede `AREA_PLACEMENT_END`.
/// IDs 44/72 select the resource recipes; there is no placement for ID 73.
/// Actors may retain pointers into this loaded-room storage until teardown.
static AreaPlacement _gShelterB4ReservoirAreaVariant2Placements[5] = {
    { 44, 0, 0, 2950, 0, -2800, 1200, 0, 0, 2, 0 },
    { 44, 0, 0, 1650, 0, -400, 2100, 0, 0, 2, 0 },
    { 72, 0, 0, -1800, -2000, -2150, 200, 0, 2, 4, 0 },
    { 72, 0, 0, -2350, -2000, -4350, 400, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant-3 placement list: one Amoeba, one Slouch and one Sucklerceph.
///
/// IDs 24/70/71 select their resource recipes. Three world-unit transforms with
/// 4096-unit yaw precede `AREA_PLACEMENT_END`; the Sucklerceph's mode 1 shifts
/// its texture page and CLUT row by one beyond the placement offsets.
/// Actors may retain pointers into this loaded-room storage until teardown.
static AreaPlacement _gShelterB4ReservoirAreaVariant3Placements[4] = {
    { 24, 0, 0, -2100, -2000, 780, 2048, 0, 0, 2, 0 },
    { 70, 0, 0, 780, 0, -4300, 3584, 0, 2, 4, 0 },
    { 71, 0, SHELTER_B4_RESERVOIR_SUCKLERCEPH_TEXTURE_VARIANT, 1750, 0, 3450, 2048, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant-11 placement list: two stranded Bog Divers, then an end record.
///
/// Both use resource ID 4 and spawn mode 0. XYZ use world units and yaw uses
/// 4096 per turn. Actors may retain their records until teardown; keep the room loaded.
static AreaPlacement _gShelterB4ReservoirAreaVariant11Placements[3] = {
    { 4, 0, SHELTER_B4_RESERVOIR_DIVER_STRANDED, 850, 0, -1000, 3200, 0, 0, 2, 0 },
    { 4, 0, SHELTER_B4_RESERVOIR_DIVER_STRANDED, 350, 0, -4600, 300, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB4ReservoirAreaVariants[12] = {
    [1]  = { _gShelterB4ReservoirAreaVariant1Placements, _gShelterB4ReservoirAreaVariant1Resources },
    [2]  = { _gShelterB4ReservoirAreaVariant2Placements, _gShelterB4ReservoirAreaVariant2Resources },
    [3]  = { _gShelterB4ReservoirAreaVariant3Placements, _gShelterB4ReservoirAreaVariant3Resources },
    [11] = { _gShelterB4ReservoirAreaVariant11Placements, _gShelterB4ReservoirAreaVariant11Resources },
};

/// Two line-of-sight blocking quads shared by reservoir room IDs 1 and 2.
///
/// Setup links/enables both records; the second carries LAST. Room-unit geometry
/// and Q12 normals stay writable and live until the runtime list is unlinked.
/// The retained low flag bit has no established interpretation.
static WorldCollisionOccluder _gShelterB4ReservoirRoomOccluders[2] = {
    { NULL, NULL, { 4544, -2432, -1024, 0 }, { { -1408, 4896, -768, 0 }, { 1408, 4896, 768, 0 }, { -1408, -4896, -768, 0 }, { 1408, -4896, 768, 0 } }, { -1963, 0, 3597, 0 }, 5145, 1, 0 },
    { NULL, NULL, { 4447, 0, -4993, 0 }, { { -1455, 4896, 834, 0 }, { 1456, 4896, -833, 0 }, { -1455, -4896, 834, 0 }, { 1456, -4896, -833, 0 } }, { 2036, 0, 3555, 0 }, 5170, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Three retained words with no established role.
///
/// The values resemble type-1 sound requests, but no code or data pointer
/// uses the list. Its purpose and any slot meanings remain unproven.
static s32 D_shelter_b4_reservoir_80187428[3] = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

enum {
    SHELTER_B4_RESERVOIR_SURFACE5_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x41,
    SHELTER_B4_RESERVOIR_SURFACE5_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x43,
    SHELTER_B4_RESERVOIR_SURFACE3_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    SHELTER_B4_RESERVOIR_SURFACE3_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
    SHELTER_B4_RESERVOIR_SURFACE2_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x25,
    SHELTER_B4_RESERVOIR_SURFACE2_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x27,
    SHELTER_B4_RESERVOIR_SURFACE2_JUMP_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x29,
};

/// Type-1 footstep cue bases for reservoir surface class 5.
///
/// Cue 1 adds one; companion playback adds 100 to each nonzero request.
/// Walking and scripted jumps share a pair; running selects the other pair.
static WorldCollisionFootstepSounds _gShelterB4ReservoirSurface5FootstepSounds = {
    .walk         = SHELTER_B4_RESERVOIR_SURFACE5_WALK_CUE_BASE,
    .run          = SHELTER_B4_RESERVOIR_SURFACE5_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B4_RESERVOIR_SURFACE5_WALK_CUE_BASE,
};

/// Type-1 footstep cue bases for reservoir surface class 3.
///
/// Cue 1 adds one; companion playback adds 100 to each nonzero request.
/// Walking and scripted jumps share a pair; running selects the other pair.
static WorldCollisionFootstepSounds _gShelterB4ReservoirSurface3FootstepSounds = {
    .walk         = SHELTER_B4_RESERVOIR_SURFACE3_WALK_CUE_BASE,
    .run          = SHELTER_B4_RESERVOIR_SURFACE3_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B4_RESERVOIR_SURFACE3_WALK_CUE_BASE,
};

/// Type-1 footstep cue bases for reservoir surface class 2.
///
/// Cue 1 adds one; companion playback adds 100 to each nonzero request.
/// Walking, running and scripted jumps each select a distinct pair.
static WorldCollisionFootstepSounds _gShelterB4ReservoirSurface2FootstepSounds = {
    .walk         = SHELTER_B4_RESERVOIR_SURFACE2_WALK_CUE_BASE,
    .run          = SHELTER_B4_RESERVOIR_SURFACE2_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B4_RESERVOIR_SURFACE2_JUMP_CUE_BASE,
};

/// Collision policy without footstep cues, shared by surface classes 0, 4, 6 and 7.
static WorldCollisionSurfaceProperties _gShelterB4ReservoirDefaultSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class-1 collision policy: pass probes, ignore weapon impacts, apply pushback, no footsteps.
static WorldCollisionSurfaceProperties _gShelterB4ReservoirSurface1 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class-2 collision policy: block probes, allow weapon impacts and apply pushback, with footsteps.
static WorldCollisionSurfaceProperties _gShelterB4ReservoirSurface2 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB4ReservoirSurface2FootstepSounds,
};

/// Class-3 collision policy: block probes, allow weapon impacts and apply pushback, with footsteps.
static WorldCollisionSurfaceProperties _gShelterB4ReservoirSurface3 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB4ReservoirSurface3FootstepSounds,
};

/// Class-5 collision policy: block probes, allow weapon impacts and apply pushback, with footsteps.
static WorldCollisionSurfaceProperties _gShelterB4ReservoirSurface5 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB4ReservoirSurface5FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB4ReservoirSurfacePropertiesByClass[8] = {
    &_gShelterB4ReservoirDefaultSurface,
    &_gShelterB4ReservoirSurface1,
    &_gShelterB4ReservoirSurface2,
    &_gShelterB4ReservoirSurface3,
    &_gShelterB4ReservoirDefaultSurface,
    &_gShelterB4ReservoirSurface5,
    &_gShelterB4ReservoirDefaultSurface,
    &_gShelterB4ReservoirDefaultSurface,
};

enum {
    SHELTER_B4_RESERVOIR_AREA_MAP_MARK_CLEAR = 0,
    SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET   = 1,
};

/// Saved-area layout and map-mark updates committed after the water event.
///
/// Updates the Shelter elevator/sewers and nighttime Dryfield, discarding
/// saved enemy poses for accepted entries. Duplicate sewer/underpass entries
/// select layouts by save-mode group. The final stage byte ends traversal.
static AreaApplyRec _gShelterB4ReservoirWaterEventAreaUpdates[24] = {
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_ELEVATOR_HALL, 3, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_CLEAR },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_LOWER_SEWER, 2, AREA_APPLY_MODE_REPLAY_SCAVENGER | SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_LOWER_SEWER, 8, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 2, AREA_APPLY_MODE_REPLAY_SCAVENGER | SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 8, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION, 11, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, 1, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP, 11, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE, 11, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3, 1, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TOILET, 11, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN, 11, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER, 11, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY, 11, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_CLEAR },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GARAGE, 4, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY, 10, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_CLEAR },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5, 1, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY, 11, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT, 11, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, 10, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_CLEAR },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_CELLAR, 11, SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 11, AREA_APPLY_MODE_REPLAY_SCAVENGER | SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 1, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | SHELTER_B4_RESERVOIR_AREA_MAP_MARK_SET },
    { AREA_APPLY_END, 0, 0, 0 },
};

/// Mutable subtractive fade request for a confirmed exit to the upper sewer.
///
/// The exit task sets a 30-update ramp before spawning the fade task, which
/// borrows and mutates this record. Exits must not overlap or outlive the room.
static ScreenFade _gShelterB4ReservoirExitFade = { 0 };

/// Borrowed context of this renderer's active wave task.
///
/// Set on the seed tick and left unchanged at teardown. Only one wave may use
/// this storage; the context must stay live through its final drawing tick.
static ScreenWaveCtx* _gScreenWaveContext = NULL;

/// Resolved location selectors retained until the asynchronous exit commits.
///
/// Only room, area and warp are staged and copied to the live save; the other
/// five bytes are retained with no accesses. Exit requests must not overlap.
/// Keep the original halfword alignment and complete eight-byte location cell.
static GameLocationKey _gShelterB4ReservoirPendingExitLocation __attribute__((aligned(2))) = { 0 };

/// Retained packed water-drift argument rebuilt by each room update.
///
/// Bits 0..11 size, 12..15 cell period, 16..23 launch speed, 24..27 velocity
/// kind and 28..31 upright drawing. Initial configuration yields 0x04083280.
/// No instruction or data pointer in this image reads the word.
static s32 _gShelterB4ReservoirWaterDriftSpawnArgs = 0;

/// Column sine oscillators for horizontal screen displacement.
///
/// The per-frame renderer uses entries 0..10; the prebuilt-grid renderer uses
/// 0..8. Any remaining declared capacity is unaccessed; its role is unproven.
static ScreenWaveOscillator _gScreenWaveColumnWaves[13] = { 0 };

/// Thirty active row sine oscillators for vertical screen displacement.
///
/// Seeded at wave startup and advanced on drawing updates. Any declared
/// entries beyond 29 are unaccessed; their role is unproven.
static ScreenWaveOscillator _gScreenWaveRowWaves[32] = { 0 };

/// Persistent ramp and tint context borrowed by the reservoir's scripted wave.
///
/// A non-positive event request seeds span 1, strength 96 and RGB 64/128/128;
/// the task resets frame/state on initialization. Positive requests write the
/// low signed-halfword ramp phase (1 falling, 2 finished). Only one task may
/// borrow it at a time; keep the room loaded through that task's final draw.
static ScreenWaveCtx _gScreenWaveSpawnCtx = { 0 };

/// Next free byte in the frame's borrowed reservoir water packet half.
///
/// Each draw resets it to an unused actor arena's 0xC000-byte otBuffer half.
/// Valid projections append a 24-byte POLY_F4 and a 12-byte DR_MODE; their casts
/// select successive packet formats in the same word-aligned byte arena.
/// The largest pass set uses 6336 bytes. This cursor owns no allocation, and
/// packet storage must remain intact through GPU DMA before actor loading or reuse.
static u8* _gShelterB4ReservoirWaterPrimCursor = NULL;

/// Ten mutable world-unit emission points for the reservoir's event burst sprites.
///
/// The effect task scatters all points, then uses the configuration's active
/// prefix (0..10), occasionally rerandomizing selected points before spawning.
/// XYZ narrow to signed halfwords; pad is unused. Spawn placement is copied,
/// but effects retain the borrowed offset pointer, so storage lives through teardown.
static SVECTOR _gShelterB4ReservoirBurstPoints[10] = { 0 };

/// Retained active-point count, spawn chance and base size of the burst emitter.
///
/// Initialization clears all three halfwords. Event commands enable ten points
/// with a 320-world-unit base half-extent; zero chance disables emission.
/// Chance is a threshold out of 100 and is not clamped. This room-owned state
/// stays live while the ambient effect task and event callbacks can access it.
static _ShelterB4ReservoirBurstConfig _gShelterB4ReservoirBurstConfig = { 0, 0, 0 };

static void _shelterB4ReservoirUpdateWaterDriftSpawnArgs(void);
static void _shelterB4ReservoirDrawWaterTask(Task* task);

#include "../../shared/screen/screen_wave.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Restores player and companion control and model drawing after a declined event.
///
/// Requires a live player and, when present, a live companion with its loaded
/// animation bank. Resumes scripted control and selects automatic model drawing;
/// the caller releases the HUD and event holds separately. Retains no task pointer.
static inline void _shelterB4ReservoirRestoreEventActors(void)
{
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
}

/// Runs the reservoir water event's CAP choice, scripted sequence and progress commit.
///
/// Bodyless task; start at state zero with room, CAP and actor resources live.
/// CAP key 12 cancels and restores play; other keys start the skippable event.
/// Completion waits for the script to clear the session event gate, then selects
/// room 2 and commits reservoir, map and control-room progress. No work is owned.
static void _shelterB4ReservoirWaterEventTask(Task* task)
{
    enum {
        SHELTER_B4_RESERVOIR_EVENT_INITIALIZE         = 0,
        SHELTER_B4_RESERVOIR_EVENT_START_CHOICE       = 1,
        SHELTER_B4_RESERVOIR_EVENT_WAIT_CHOICE        = 2,
        SHELTER_B4_RESERVOIR_EVENT_RESOLVE_CHOICE     = 3,
        SHELTER_B4_RESERVOIR_EVENT_WAIT_SCRIPT        = 4,
        SHELTER_B4_RESERVOIR_EVENT_CAP_COMMAND        = 3,
        SHELTER_B4_RESERVOIR_EVENT_CANCEL_KEY         = 12,
        SHELTER_B4_RESERVOIR_EVENT_RETURN_VIEW        = 5,
        SHELTER_B4_RESERVOIR_EVENT_COMPLETED_ROOM     = 2,
        SHELTER_B4_RESERVOIR_EVENT_IDLE               = 0,
        SHELTER_B4_RESERVOIR_EVENT_ACTIVE             = 1,
        SHELTER_B4_RESERVOIR_CAP_RETAIN_ACTOR_CONTROL = 1,
        SHELTER_B4_RESERVOIR_CAP_CHOICE_ROW_STRIDE    = 5,
        SHELTER_B4_RESERVOIR_EVENT_PLACED_ACTOR       = 0,
        SHELTER_B4_RESERVOIR_EVENT_ACTOR_HIDE         = 0,
        SHELTER_B4_RESERVOIR_EVENT_DONE               = 1,
        SHELTER_B4_RESERVOIR_EVENT_MAP_MARK           = 2,
        SHELTER_B4_RESERVOIR_CONTROL_ROOM_AFTER_EVENT = 1,
    };

    switch (task->state) {
        case SHELTER_B4_RESERVOIR_EVENT_INITIALIZE:
            gGameSession->eventState = SHELTER_B4_RESERVOIR_EVENT_ACTIVE;
            gGameSession->hideHud    = 1;
            task->state++;
            break;
        case SHELTER_B4_RESERVOIR_EVENT_START_CHOICE:
            capRunCommand(SHELTER_B4_RESERVOIR_EVENT_CAP_COMMAND, CAP_PLAYBACK_IN_PLACE);
            gCapPreserveActorControl = SHELTER_B4_RESERVOIR_CAP_RETAIN_ACTOR_CONTROL;
            gCapChoiceRowStride      = SHELTER_B4_RESERVOIR_CAP_CHOICE_ROW_STRIDE;
            task->state++;
            break;
        case SHELTER_B4_RESERVOIR_EVENT_WAIT_CHOICE:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHELTER_B4_RESERVOIR_EVENT_RESOLVE_CHOICE:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            if (capGetVariantKey() == SHELTER_B4_RESERVOIR_EVENT_CANCEL_KEY) {
                taskKill(task);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = SHELTER_B4_RESERVOIR_EVENT_RETURN_VIEW;
                _shelterB4ReservoirRestoreEventActors();
                gGameSession->eventState          = SHELTER_B4_RESERVOIR_EVENT_IDLE;
                gGameSession->hideHud             = 0;
                gDirectionInteractionRearmUpdates = SHELTER_B4_RESERVOIR_INTERACTION_REARM_UPDATES;
                break;
            }
            sceneSetPlacedActorDrawMode(SHELTER_B4_RESERVOIR_EVENT_PLACED_ACTOR, SHELTER_B4_RESERVOIR_EVENT_ACTOR_HIDE);
            evsStartScriptWithSkip(_gShelterB4ReservoirWaterEventScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gShelterB4ReservoirWaterEventSkipScript);
            task->state++;
            break;
        case SHELTER_B4_RESERVOIR_EVENT_WAIT_SCRIPT:
            // The event script owns the gate until either normal completion or skip.
            if (gGameSession->eventState == SHELTER_B4_RESERVOIR_EVENT_IDLE) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = SHELTER_B4_RESERVOIR_EVENT_COMPLETED_ROOM;
                gGameSession->location.roomState                            = SHELTER_B4_RESERVOIR_EVENT_COMPLETED_ROOM;
                gGameSession->roomObjsDirty                                 = 1;
                gameFlagSetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE, SHELTER_B4_RESERVOIR_EVENT_DONE);
                gameFlagSetNibble(GAME_FLAG_MAP_MARK_RESERVOIR_1BF, SHELTER_B4_RESERVOIR_EVENT_MAP_MARK);
                gameFlagSetNibble(GAME_FLAG_INCINERATOR_CONTROL_ROOM_STATE, SHELTER_B4_RESERVOIR_CONTROL_ROOM_AFTER_EVENT);
                gameFlagSetNibble(GAME_FLAG_MAP_MARK_RESERVOIR_1BE, SHELTER_B4_RESERVOIR_EVENT_MAP_MARK);
                areaApplySavedUpdates(_gShelterB4ReservoirWaterEventAreaUpdates);
                gDirectionInteractionRearmUpdates = SHELTER_B4_RESERVOIR_INTERACTION_REARM_UPDATES;
                taskKill(task);
            }
            break;
    }
}

/// Rebuilds the retained water-drift spawn word from the room configuration.
///
/// Bits 0..11 supply half-extent, 12..15 frame period, 16..23 speed,
/// 24..27 velocity kind and 28..31 upright-draw selection. Inputs are ORed
/// without masking; the half-extent must fit 0..4095 and period 0..15.
/// The initialized kind 4 has no direction-generation arm in the particle task.
/// This retained word currently has no reader; live view-10 spray packs its own arguments.
static void _shelterB4ReservoirUpdateWaterDriftSpawnArgs(void)
{
    enum {
        SHELTER_B4_RESERVOIR_DRIFT_KIND_AND_DRAW_SHIFT = 24,
        SHELTER_B4_RESERVOIR_DRIFT_PERIOD_SHIFT        = 12,
        SHELTER_B4_RESERVOIR_DRIFT_SPEED_SHIFT         = 16,
    };
    _gShelterB4ReservoirWaterDriftSpawnArgs = (_gShelterB4ReservoirWaterDriftKindAndDraw << SHELTER_B4_RESERVOIR_DRIFT_KIND_AND_DRAW_SHIFT) | (_gShelterB4ReservoirWaterDriftAnimationPeriod << SHELTER_B4_RESERVOIR_DRIFT_PERIOD_SHIFT) | (_gShelterB4ReservoirWaterDriftInitialSpeed << SHELTER_B4_RESERVOIR_DRIFT_SPEED_SHIFT) | _gShelterB4ReservoirWaterDriftHalfExtent.halfExtent;
}

/// Confirms and performs the deferred exit from the reservoir to the upper sewer.
///
/// Bodyless task; start at state zero. `spawnArg1.value` selects a live CAP command.
/// Key 10 accepts: start a 30-frame fade, wait 30 callback updates independently,
/// then wait for the transit sound and load the retained area/room/warp within
/// the current stage. Other keys restore
/// player control and retire the task. The pending destination and fade record
/// are shared room storage, so exits must not overlap or outlive this overlay.
static void _shelterB4ReservoirExitTask(Task* task)
{
    enum {
        SHELTER_B4_RESERVOIR_EXIT_START_CHOICE   = 0,
        SHELTER_B4_RESERVOIR_EXIT_WAIT_CHOICE    = 1,
        SHELTER_B4_RESERVOIR_EXIT_RESOLVE_CHOICE = 2,
        SHELTER_B4_RESERVOIR_EXIT_WAIT_FADE      = 3,
        SHELTER_B4_RESERVOIR_EXIT_WAIT_SOUND     = 4,
        SHELTER_B4_RESERVOIR_EXIT_RELOAD         = 5,
        SHELTER_B4_RESERVOIR_EXIT_ACCEPT_KEY     = 10,
        SHELTER_B4_RESERVOIR_EXIT_FADE_UPDATES   = 30,
        SHELTER_B4_RESERVOIR_EXIT_FADE_TASK_BANK = 1,
        SHELTER_B4_RESERVOIR_EXIT_FADE_TASK_TYPE = 0x31,
        SHELTER_B4_RESERVOIR_EXIT_SPRITE_VARIANT = 1,
    };

    switch (task->state) {
        case SHELTER_B4_RESERVOIR_EXIT_START_CHOICE:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case SHELTER_B4_RESERVOIR_EXIT_WAIT_CHOICE:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHELTER_B4_RESERVOIR_EXIT_RESOLVE_CHOICE:
            if (capGetVariantKey() != SHELTER_B4_RESERVOIR_EXIT_ACCEPT_KEY) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl    = SCENE_COMBAT_ACTORS_RUNNING;
                gDirectionInteractionRearmUpdates = SHELTER_B4_RESERVOIR_INTERACTION_REARM_UPDATES;
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            sceneQueueBattleEscapeResult();
            _gShelterB4ReservoirExitFade.blend      = SCREEN_FADE_SUBTRACT;
            _gShelterB4ReservoirExitFade.phase      = SCREEN_FADE_RUNNING;
            _gShelterB4ReservoirExitFade.rampFrames = SHELTER_B4_RESERVOIR_EXIT_FADE_UPDATES;
            taskSpawn(SHELTER_B4_RESERVOIR_EXIT_FADE_TASK_BANK, SHELTER_B4_RESERVOIR_EXIT_FADE_TASK_TYPE, 0, &_gShelterB4ReservoirExitFade);
            task->killCountdown = SHELTER_B4_RESERVOIR_EXIT_FADE_UPDATES;
            task->state++;
            break;
        case SHELTER_B4_RESERVOIR_EXIT_WAIT_FADE:
            if (--task->killCountdown == 0) {
                sndEvtRequestScriptStart(SOUND_SHELTER_B4_RESERVOIR_EXIT_TRANSIT, 0, 0);
                task->state++;
            }
            break;
        case SHELTER_B4_RESERVOIR_EXIT_WAIT_SOUND:
            if (sndScriptHasActiveId(SOUND_SHELTER_B4_RESERVOIR_EXIT_TRANSIT) == 0) {
                task->state++;
            }
            break;
        case SHELTER_B4_RESERVOIR_EXIT_RELOAD:
            // This retained storage uses location bytes: room, area and warp.
            gDisplayState.spriteVariant                                 = SHELTER_B4_RESERVOIR_EXIT_SPRITE_VARIANT;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = _gShelterB4ReservoirPendingExitLocation.area;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = _gShelterB4ReservoirPendingExitLocation.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = _gShelterB4ReservoirPendingExitLocation.roomState;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE, 0);
            taskKill(task);
            break;
    }
}

/// Rejects key-item use in the reservoir by returning zero without consuming the item.
///
/// Handles `SHELTER_B4_RESERVOIR_MESSAGE_USE_KEY_ITEM`; all four arguments are ignored.
static s32 _shelterB4ReservoirRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return 0;
}

/// Resolves room transitions and intercepts departure to the upper sewer.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; request and writable reply borrow complete
/// records for this call and may alias. Copies all eight bytes before the map
/// resolver. Queries perform no effects. Upper-sewer requests return 0: execution
/// plays CAP command 2 after progress exactly 1, otherwise snapshots the resolved
/// destination and starts the exit confirmation. Other areas return 1 for normal
/// departure. The receiving task and messageId are unused; neither pointer is retained.
static s32 _shelterB4ReservoirResolveTransitionMessage(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        SHELTER_B4_RESERVOIR_TRANSITION_HANDLED      = 0,
        SHELTER_B4_RESERVOIR_TRANSITION_DEFAULT      = 1,
        SHELTER_B4_RESERVOIR_TRANSITION_EVENT_DONE   = 1,
        SHELTER_B4_RESERVOIR_UPPER_SEWER_CAP_COMMAND = 2,
        SHELTER_B4_RESERVOIR_DEPARTURE_FLAG_STATE    = 2,
        SHELTER_B4_RESERVOIR_EXIT_CAP_COMMAND        = 11,
    };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B4_UPPER_SEWER) {
        if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) == SHELTER_B4_RESERVOIR_TRANSITION_EVENT_DONE) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibbleIfPresent(request->flagId, SHELTER_B4_RESERVOIR_DEPARTURE_FLAG_STATE);
                capRunCommandWithTransition(SHELTER_B4_RESERVOIR_UPPER_SEWER_CAP_COMMAND);
            }
        } else {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                // Snapshot the resolved selectors for the asynchronous exit task.
                _gShelterB4ReservoirPendingExitLocation.area      = (u8)reply->areaId;
                _gShelterB4ReservoirPendingExitLocation.warp      = reply->warp;
                _gShelterB4ReservoirPendingExitLocation.roomState = reply->roomState;
                taskSpawnFromTable(_gShelterB4ReservoirEventTasks, SHELTER_B4_RESERVOIR_TASK_EXIT, SHELTER_B4_RESERVOIR_EXIT_CAP_COMMAND, 0);
            }
        }
        return SHELTER_B4_RESERVOIR_TRANSITION_HANDLED;
    }
    return SHELTER_B4_RESERVOIR_TRANSITION_DEFAULT;
}

/// Starts the reservoir water event when room command 3 arrives.
///
/// Handles `ROOM_MESSAGE_COMMAND` with an integer first payload. Hides and holds
/// both actors, selects saved view 6 and starts the bodyless water-event task.
/// Other commands are ignored. Always returns 0; task, messageId and secondArg
/// are unused. Requires live actor slots and loaded reservoir/CAP resources.
static s32 _shelterB4ReservoirCommandMessage(Task* task, s32 messageId, s32 command, s32 secondArg)
{
    enum {
        SHELTER_B4_RESERVOIR_COMMAND_START_WATER_EVENT = 3,
        SHELTER_B4_RESERVOIR_EVENT_START_VIEW          = 6,
    };

    if (command == SHELTER_B4_RESERVOIR_COMMAND_START_WATER_EVENT) {
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
        companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = SHELTER_B4_RESERVOIR_EVENT_START_VIEW;
        gSceneCombatState.actorControl                         = SCENE_COMBAT_ACTORS_HIDDEN;
        taskSpawnFromTable(_gShelterB4ReservoirEventTasks, SHELTER_B4_RESERVOIR_TASK_WATER_EVENT, 0, 0);
    }
    return 0;
}

/// Ignores `DIRECTION_MESSAGE_ROOM_ACTION` and returns zero.
///
/// Neither payload word nor the receiving task is inspected or retained.
static s32 _shelterB4ReservoirIgnoreRoomActionMessage(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    return 0;
}

/// Starts the reservoir's sound script 2 when room sound command 2 arrives.
///
/// All other commands are ignored. Always returns zero; the receiver,
/// message ID and second payload word are unused.
static s32 _shelterB4ReservoirSoundMessage(Task* task, s32 messageId, s32 soundCommand, s32 secondArg)
{
    enum {
        SHELTER_B4_RESERVOIR_SOUND_COMMAND_PLAY_SCRIPT_2 = 2,
        SHELTER_B4_RESERVOIR_SOUND_SCRIPT_2              = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_RESERVOIR, 2),
    };

    if (soundCommand == SHELTER_B4_RESERVOIR_SOUND_COMMAND_PLAY_SCRIPT_2) {
        sndEvtRequestScriptStart(SHELTER_B4_RESERVOIR_SOUND_SCRIPT_2, 0, 0);
    }
    return 0;
}

/// Moves the main water plane from world Y -2000 to -500 over the requested updates.
///
/// Bodyless callback; spawnArg1.value is a positive duration in callback
/// updates and killCountdown starts at zero as an elapsed-frame counter.
/// Durations through 32766 avoid signed-halfword wrap at the final increment.
/// The endpoint is written before the task is killed and the shared ramp handle
/// is cleared. Integer division truncates toward zero; no clamping occurs here.
static void _shelterB4ReservoirRampMainWaterHeightTask(Task* task)
{
    s16 nextElapsedFrame;
    s32 heightDelta;

    // Sample the old elapsed frame, then advance the signed halfword counter.
    heightDelta                   = (task->killCountdown * SHELTER_B4_RESERVOIR_MAIN_WATER_Y_TRAVEL) / task->spawnArg1.value;
    nextElapsedFrame              = (u16)task->killCountdown + 1;
    task->killCountdown           = nextElapsedFrame;
    gShelterB4ReservoirMainWaterY = heightDelta + SHELTER_B4_RESERVOIR_MAIN_WATER_INITIAL_Y;
    if (task->spawnArg1.value < nextElapsedFrame) {
        taskKill(task);
        _gShelterB4ReservoirWaterRampTask = 0;
    }
}

/// Moves the view-10 water plane from world Y 0 to -1800 over the requested updates.
///
/// Bodyless callback; spawnArg1.value is a positive duration in callback
/// updates and killCountdown starts at zero as an elapsed-frame counter.
/// Durations through 32766 avoid signed-halfword wrap at the final increment.
/// The endpoint is written before the task is killed and the shared ramp handle
/// is cleared. Integer division truncates toward zero; no clamping occurs here.
static void _shelterB4ReservoirRampView10WaterHeightTask(Task* task)
{
    s16 nextElapsedFrame;
    s32 heightDelta;

    // Sample the old elapsed frame, then advance the signed halfword counter.
    heightDelta                      = -(task->killCountdown * (-SHELTER_B4_RESERVOIR_VIEW10_WATER_MIN_Y)) / task->spawnArg1.value;
    nextElapsedFrame                 = (u16)task->killCountdown + 1;
    task->killCountdown              = nextElapsedFrame;
    _gShelterB4ReservoirView10WaterY = heightDelta;
    if (task->spawnArg1.value < nextElapsedFrame) {
        taskKill(task);
        _gShelterB4ReservoirWaterRampTask = 0;
    }
}

/// Restores the reservoir event model's starting local translation.
///
/// Borrows a writable model root coordinate. The translation is (-1000,
/// -1000, -5000) in parent-coordinate units; rotation and parent stay intact.
/// Marks the cached composition stale without refreshing `workm`, so the
/// coordinate must be composed before drawing. Retains no pointer.
static inline void _shelterB4ReservoirResetEventModelTranslation(GfxCoord* modelCoord)
{
    modelCoord->coord.t[0]   = -1000;
    modelCoord->coord.t[1]   = -1000;
    modelCoord->coord.t[2]   = -5000;
    modelCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Positions and moves the event model that is drawn only in saved view 8.
///
/// Requires a live TMD body with a root coordinate. State 0 resets placement
/// and advances to stationary state 1; state 2 resets and advances to moving
/// state 3 in the same update. State 3 adds four parent-coordinate units to Y
/// each update. The room's event script can restart either phase. Other states
/// leave placement intact. The object remains live when another view hides it.
static void _shelterB4ReservoirView8ModelTask(Task* task)
{
    enum {
        SHELTER_B4_RESERVOIR_MODEL_INITIALIZE     = 0,
        SHELTER_B4_RESERVOIR_MODEL_RESTART_MOTION = 2,
        SHELTER_B4_RESERVOIR_MODEL_MOVING         = 3,
        SHELTER_B4_RESERVOIR_MODEL_VISIBLE_VIEW   = 8,
    };
    TmdObject* model      = task->extra.tmd;
    GfxCoord*  modelCoord = model->coords;

    if (task->state == SHELTER_B4_RESERVOIR_MODEL_INITIALIZE) {
        _shelterB4ReservoirResetEventModelTranslation(modelCoord);
        task->state++;
    }
    if (task->state == SHELTER_B4_RESERVOIR_MODEL_RESTART_MOTION) {
        _shelterB4ReservoirResetEventModelTranslation(modelCoord);
        task->state++;
    }
    // Restarted motion takes its first step immediately after the reset.
    if (task->state == SHELTER_B4_RESERVOIR_MODEL_MOVING) {
        modelCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        modelCoord->coord.t[1]  += 4;
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view != SHELTER_B4_RESERVOIR_MODEL_VISIBLE_VIEW) {
        model->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    } else {
        model->flags    = 0;
        model->otOffset = 0;
    }
}

#include "../../shared/screen/screen_wave_run.inc.c"
#undef SCREEN_WAVE_SPAWN_CONTEXT

/// Stops the retained water-ramp task, if live, and clears its shared handle.
///
/// The room's shared handle must be NULL or a live main/view-10 ramp task.
/// Teardown precedes clearing it. Water heights remain at their current values;
/// the finishing command sets the appropriate endpoint after this returns.
static inline void _shelterB4ReservoirStopWaterRamp(void)
{
    if (_gShelterB4ReservoirWaterRampTask != NULL) {
        taskKill(_gShelterB4ReservoirWaterRampTask);
        _gShelterB4ReservoirWaterRampTask = NULL;
    }
}

/// Starts or finishes the reservoir event's main and view-10 water ramps.
///
/// Commands 0/2 start 150-update main/view-10 ramps and retain the task handle.
/// Commands 1/3 stop that task and set main Y to -500/view-10 Y to zero.
/// Other values do nothing. Start commands overwrite the handle without stopping
/// its old task: the event must finish one ramp before starting another.
/// Requires the room overlay and its height/handle storage to remain loaded.
static void _shelterB4ReservoirWaterCommand(s32 waterCommand)
{
    enum {
        SHELTER_B4_RESERVOIR_WATER_RAMP_UPDATES = 150,
    };

    switch (waterCommand) {
        case SHELTER_B4_RESERVOIR_WATER_START_MAIN_RAMP:
            _gShelterB4ReservoirWaterRampTask = taskSpawnFromTable(_gShelterB4ReservoirEventTasks, SHELTER_B4_RESERVOIR_TASK_MAIN_WATER_RAMP, SHELTER_B4_RESERVOIR_WATER_RAMP_UPDATES, 0);
            break;
        case SHELTER_B4_RESERVOIR_WATER_FINISH_MAIN_RAMP:
            _shelterB4ReservoirStopWaterRamp();
            gShelterB4ReservoirMainWaterY = SHELTER_B4_RESERVOIR_MAIN_WATER_FINAL_Y;
            break;
        case SHELTER_B4_RESERVOIR_WATER_START_VIEW10_RAMP:
            _gShelterB4ReservoirWaterRampTask = taskSpawnFromTable(_gShelterB4ReservoirEventTasks, SHELTER_B4_RESERVOIR_TASK_VIEW10_WATER_RAMP, SHELTER_B4_RESERVOIR_WATER_RAMP_UPDATES, 0);
            break;
        case SHELTER_B4_RESERVOIR_WATER_RESET_VIEW10:
            _shelterB4ReservoirStopWaterRamp();
            _gShelterB4ReservoirView10WaterY = SHELTER_B4_RESERVOIR_VIEW10_WATER_MAX_Y;
            break;
    }
}

/// Selects the live reservoir event model's placement or motion state.
///
/// The room initializer must already have published a live model task.
/// modelState is stored unchanged: 0 resets into stationary state 1,
/// 2 resets into moving state 3.
/// Other values are stored without validation, including state 3 for continued
/// motion from the current placement. The callback does not own the model task.
static void _shelterB4ReservoirSetEventModelState(s32 modelState)
{
    _gShelterB4ReservoirEventModelTask->state = modelState;
}

/// Sets the per-point, per-frame burst chance for all ten reservoir emission points.
///
/// The event callback retains the low 16 bits of spawnChancePercent.
/// Zero disables emission; values at least 100 always pass the roll.
/// Uses a 320-world-unit base half-extent before random size variation.
static void _shelterB4ReservoirSetBurstChance(s32 spawnChancePercent)
{
    _shelterB4ReservoirSetBurstConfig(ARRAY_SIZE(_gShelterB4ReservoirBurstPoints), spawnChancePercent,
                                      SHELTER_B4_RESERVOIR_BURST_BASE_HALF_EXTENT);
}

/// Requests cancellation of the current room effects at an event-script boundary.
///
/// Requires the live room-effect controller; cancellation is processed by later
/// controller and effect updates rather than releasing tasks in this callback.
static void _shelterB4ReservoirCancelEffects(void)
{
    roomEffectRequestCancelAll();
}

/// Registers reservoir messages and starts water rendering and the event model.
///
/// Room-task state zero requires loaded room resources.
/// Sets main water Y to -500 for any nonzero completed-event nibble, otherwise
/// -2000. Publishes the model task for later event callbacks, then advances the
/// room task to its next state. Child allocations are not checked for failure.
static void _shelterB4ReservoirInitializeRoomTask(Task* task)
{
    task->msgTable = _gShelterB4ReservoirRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(_gShelterB4ReservoirWaterTasks, 0, 0, 0);
    if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
        gShelterB4ReservoirMainWaterY = SHELTER_B4_RESERVOIR_MAIN_WATER_FINAL_Y;
    } else {
        gShelterB4ReservoirMainWaterY = SHELTER_B4_RESERVOIR_MAIN_WATER_INITIAL_Y;
    }
    _gShelterB4ReservoirEventModelTask = taskSpawnFromTable(_gShelterB4ReservoirEventModelTasks, 0, 0, 0);
    task->state++;
}

/// Updates the reservoir's retained spray configuration each room tick.
///
/// Runs in room state 1 and then calls the empty room hook. task is unused
/// but retained as the task-state callback argument.
static void _shelterB4ReservoirUpdateRoomTask(Task* task)
{
    _shelterB4ReservoirUpdateWaterDriftSpawnArgs();
    _shelterB4ReservoirNoopRoomTick();
}

void shelterB4ReservoirRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gShelterB4ReservoirRoomStates;
    states.funcs[task->state](task);
}

/// Empty hook called by the room's per-frame state after updating spray arguments.
static void _shelterB4ReservoirNoopRoomTick(void)
{
}

/// Selects the frame's packet arena and draws the active reservoir water planes.
///
/// Borrows actor arena 2 without a companion, or arena 1 with one, using
/// otBuffer 0 or 1 to select a 0xC000-byte half. That storage must remain
/// word aligned and intact through GPU DMA, without concurrent actor loading.
/// Normal views draw 176 quads at most (6336 bytes); raw view 10 draws 130
/// (4680 bytes). The main strip's depth follows its water height; view 10
/// clamps its separate height to -1800..0. task is passed to the drawers unchanged.
static void _shelterB4ReservoirDrawWaterTask(Task* task)
{
    enum {
        SHELTER_B4_RESERVOIR_WATER_PACKET_HALF_BYTES  = 0xC000,
        SHELTER_B4_RESERVOIR_MAIN_WATER_INITIAL_DEPTH = 9000,
    };
    RoomWaterSurface* surface = _gShelterB4ReservoirMainWaterSurfaces;

    // Borrow one half of the actor arena unused by the current companion setup.
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        _gShelterB4ReservoirWaterPrimCursor = gFsActorBuffer2Base + gDisplayState.otBuffer * SHELTER_B4_RESERVOIR_WATER_PACKET_HALF_BYTES;
    } else {
        _gShelterB4ReservoirWaterPrimCursor = gFsActorBuffer1Base + gDisplayState.otBuffer * SHELTER_B4_RESERVOIR_WATER_PACKET_HALF_BYTES;
    }
    if (gGameSession->location.view != SHELTER_B4_RESERVOIR_WATER_SPRAY_VIEW) {
        surface->depth = SHELTER_B4_RESERVOIR_MAIN_WATER_INITIAL_DEPTH - (((gShelterB4ReservoirMainWaterY - SHELTER_B4_RESERVOIR_MAIN_WATER_INITIAL_Y) * 0x31) >> 5);
        _shelterB4ReservoirDrawMainWaterStrip(task);
        _shelterB4ReservoirDrawZWaterStrips(task);
        _shelterB4ReservoirDrawXWaterStrips(task);
        return;
    }
    if (_gShelterB4ReservoirView10WaterY < SHELTER_B4_RESERVOIR_VIEW10_WATER_MIN_Y) {
        _gShelterB4ReservoirView10WaterY = SHELTER_B4_RESERVOIR_VIEW10_WATER_MIN_Y;
    } else if (_gShelterB4ReservoirView10WaterY > SHELTER_B4_RESERVOIR_VIEW10_WATER_MAX_Y) {
        _gShelterB4ReservoirView10WaterY = SHELTER_B4_RESERVOIR_VIEW10_WATER_MAX_Y;
    }
    _shelterB4ReservoirDrawView10WaterStrips(task);
}

/// Queues a red-subtractive flat quad and its drawing-mode command for a water strip.
///
/// Captures writable POLY_F4* waterQuad, DR_MODE* drawMode, packed signed-pixel
/// words screenXY0..3 and s32 orderingDepth from each of the three red-water
/// drawers. Requires a word-aligned frame cursor with 36 available bytes and a
/// valid biased depth bucket. Reserves 24 + 12 bytes, transmitting only the first
/// mode word; the second slot is retained storage. Both packets use depth + 1.
/// Head insertion links the mode last so the GPU selects subtractive blending
/// before drawing RGB (128, 0, 0). Packets stay live until GPU consumption.
/// Expands to a statement list: invoke inside braces after a valid projection.
#define SHELTER_B4_RESERVOIR_EMIT_RED_WATER_QUAD()                                                                                                                                               \
    waterQuad                           = (POLY_F4*)_gShelterB4ReservoirWaterPrimCursor;                                                                                                         \
    _gShelterB4ReservoirWaterPrimCursor = (u8*)(waterQuad + 1);                                                                                                                                  \
    setlen(waterQuad, sizeof(*waterQuad) / sizeof(u32) - 1);                                                                                                                                     \
    setcode(waterQuad, SHELTER_B4_RESERVOIR_WATER_QUAD_CODE);                                                                                                                                    \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 0) = screenXY0;                                                                                                                                             \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 1) = screenXY1;                                                                                                                                             \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 2) = screenXY2;                                                                                                                                             \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 3) = screenXY3;                                                                                                                                             \
    waterQuad->r0                       = SHELTER_B4_RESERVOIR_WATER_RED_INTENSITY;                                                                                                              \
    waterQuad->g0                       = 0;                                                                                                                                                     \
    waterQuad->b0                       = 0;                                                                                                                                                     \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(orderingDepth + SHELTER_B4_RESERVOIR_WATER_DEPTH_BIAS) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), \
            waterQuad);                                                                                                                                                                          \
    drawMode                            = (DR_MODE*)_gShelterB4ReservoirWaterPrimCursor;                                                                                                         \
    _gShelterB4ReservoirWaterPrimCursor = (u8*)(drawMode + 1);                                                                                                                                   \
    setlen(drawMode, SHELTER_B4_RESERVOIR_WATER_DRAW_MODE_WORDS);                                                                                                                                \
    drawMode->code[0] = SHELTER_B4_RESERVOIR_WATER_DRAW_MODE;                                                                                                                                    \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(orderingDepth + SHELTER_B4_RESERVOIR_WATER_DEPTH_BIAS) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), \
            drawMode);

/// Draws the changing main water rectangle as 32 flat quads along Z.
///
/// Uses the room's main world Y, X offset and dynamically shortened depth.
/// The descriptor count only marks the list end; drawable entries may store zero.
/// Borrows the terminated descriptor list, one WaterQuadScratch block and
/// the frame's byte packet cursor. Each valid projection consumes a POLY_F4
/// and DR_MODE (36 bytes); only the first draw-mode word is transmitted.
/// Subtractive blending sorts at projected depth + 1; the three main-water
/// passes subtract red intensity 128. Negative GTE flags reject a quad.
/// Scratch is released on return. task is unused in the retained call interface.
static void _shelterB4ReservoirDrawMainWaterStrip(Task* task)
{

    enum { SHELTER_B4_RESERVOIR_MAIN_WATER_SEGMENTS = 32 };
    SVECTOR                 corner0, corner1, corner2, corner3;
    long                    screenXY0, screenXY1, screenXY2, screenXY3;
    long                    depthCue, projectionFlags;
    WaterQuadScratch*       scratchEnd;
    WaterQuadScratch*       scratch;
    const RoomWaterSurface* surface;
    POLY_F4*                waterQuad;
    DR_MODE*                drawMode;
    s32                     orderingDepth;
    s32                     segmentIndex;

    surface                    = _gShelterB4ReservoirMainWaterSurfaces;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    (scratchEnd - 1)->y = gShelterB4ReservoirMainWaterY;
    for (; surface->segmentCount != WATER_SURFACE_LIST_END; surface++) {
        scratch->dx = surface->width;
        scratch->dz = surface->depth / SHELTER_B4_RESERVOIR_MAIN_WATER_SEGMENTS;
        scratch->x  = surface->x + _gShelterB4ReservoirMainWaterPlacement.xOffset;
        scratch->z  = surface->z;
        for (segmentIndex = 0; segmentIndex < SHELTER_B4_RESERVOIR_MAIN_WATER_SEGMENTS; segmentIndex++) {
            corner0.vx       = scratch->x;
            corner0.vy       = scratch->y;
            corner0.vz       = scratch->z + scratch->dz * segmentIndex;
            corner1.vx       = scratch->x;
            corner1.vy       = scratch->y;
            corner1.vz       = scratch->z + scratch->dz * (segmentIndex + 1);
            scratch->yOffset = 0;
            corner2.vx       = scratch->x + scratch->dx;
            corner2.vy       = scratch->y + scratch->yOffset;
            corner2.vz       = scratch->z + scratch->dz * segmentIndex;
            scratch->yOffset = 0;
            corner3.vx       = scratch->x + scratch->dx;
            corner3.vy       = scratch->y + scratch->yOffset;
            corner3.vz       = scratch->z + scratch->dz * (segmentIndex + 1);
            orderingDepth    = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &depthCue, &projectionFlags);
            if (projectionFlags >= 0) {
                SHELTER_B4_RESERVOIR_EMIT_RED_WATER_QUAD();
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
}

/// Draws the two fixed reservoir water rectangles as flat strips along Z.
///
/// Uses the main world Y and each descriptor's positive segment count.
/// Depth is divided by that count with truncation toward zero.
/// Borrows the terminated descriptor list, one WaterQuadScratch block and
/// the frame's byte packet cursor. Each valid projection consumes a POLY_F4
/// and DR_MODE (36 bytes); only the first draw-mode word is transmitted.
/// Subtractive blending sorts at projected depth + 1; the three main-water
/// passes subtract red intensity 128. Negative GTE flags reject a quad.
/// Scratch is released on return. task is unused in the retained call interface.
static void _shelterB4ReservoirDrawZWaterStrips(Task* task)
{

    SVECTOR                 corner0, corner1, corner2, corner3;
    long                    screenXY0, screenXY1, screenXY2, screenXY3;
    long                    depthCue, projectionFlags;
    WaterQuadScratch*       scratchEnd;
    WaterQuadScratch*       scratch;
    const RoomWaterSurface* surface;
    POLY_F4*                waterQuad;
    DR_MODE*                drawMode;
    s32                     orderingDepth;
    s32                     segmentIndex;

    surface                    = _gShelterB4ReservoirZWaterSurfaces;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    (scratchEnd - 1)->y = gShelterB4ReservoirMainWaterY;
    for (; surface->segmentCount != WATER_SURFACE_LIST_END; surface++) {
        scratch->dx = surface->width;
        scratch->dz = surface->depth / surface->segmentCount;
        scratch->x  = surface->x;
        scratch->z  = surface->z;
        for (segmentIndex = 0; segmentIndex < surface->segmentCount; segmentIndex++) {
            corner0.vx       = scratch->x;
            corner0.vy       = scratch->y;
            corner0.vz       = scratch->z + scratch->dz * segmentIndex;
            corner1.vx       = scratch->x;
            corner1.vy       = scratch->y;
            corner1.vz       = scratch->z + scratch->dz * (segmentIndex + 1);
            scratch->yOffset = 0;
            corner2.vx       = scratch->x + scratch->dx;
            corner2.vy       = scratch->y + scratch->yOffset;
            corner2.vz       = scratch->z + scratch->dz * segmentIndex;
            scratch->yOffset = 0;
            corner3.vx       = scratch->x + scratch->dx;
            corner3.vy       = scratch->y + scratch->yOffset;
            corner3.vz       = scratch->z + scratch->dz * (segmentIndex + 1);
            orderingDepth    = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &depthCue, &projectionFlags);
            if (projectionFlags >= 0) {
                SHELTER_B4_RESERVOIR_EMIT_RED_WATER_QUAD();
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
}

/// Draws the crosswise reservoir water rectangle as a flat strip along X.
///
/// Uses the main world Y and each descriptor's positive segment count.
/// Width is divided by that count with truncation toward zero.
/// Borrows the terminated descriptor list, one WaterQuadScratch block and
/// the frame's byte packet cursor. Each valid projection consumes a POLY_F4
/// and DR_MODE (36 bytes); only the first draw-mode word is transmitted.
/// Subtractive blending sorts at projected depth + 1; the three main-water
/// passes subtract red intensity 128. Negative GTE flags reject a quad.
/// Scratch is released on return. task is unused in the retained call interface.
static void _shelterB4ReservoirDrawXWaterStrips(Task* task)
{

    SVECTOR                 corner0, corner1, corner2, corner3;
    long                    screenXY0, screenXY1, screenXY2, screenXY3;
    long                    depthCue, projectionFlags;
    WaterQuadScratch*       scratchEnd;
    WaterQuadScratch*       scratch;
    const RoomWaterSurface* surface;
    POLY_F4*                waterQuad;
    DR_MODE*                drawMode;
    s32                     orderingDepth;
    s32                     segmentIndex;

    surface                    = _gShelterB4ReservoirXWaterSurfaces;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    (scratchEnd - 1)->y = gShelterB4ReservoirMainWaterY;
    for (; surface->segmentCount != WATER_SURFACE_LIST_END; surface++) {
        scratch->dx = surface->width / surface->segmentCount;
        scratch->dz = surface->depth;
        scratch->x  = surface->x;
        scratch->z  = surface->z;
        for (segmentIndex = 0; segmentIndex < surface->segmentCount; segmentIndex++) {
            corner0.vx       = scratch->x + scratch->dx * segmentIndex;
            corner0.vy       = scratch->y;
            corner0.vz       = scratch->z;
            corner1.vx       = scratch->x + scratch->dx * (segmentIndex + 1);
            corner1.vy       = scratch->y;
            corner1.vz       = scratch->z;
            scratch->yOffset = 0;
            corner2.vx       = scratch->x + scratch->dx * segmentIndex;
            corner2.vy       = scratch->y + scratch->yOffset;
            corner2.vz       = scratch->z + scratch->dz;
            scratch->yOffset = 0;
            corner3.vx       = scratch->x + scratch->dx * (segmentIndex + 1);
            corner3.vy       = scratch->y + scratch->yOffset;
            corner3.vz       = scratch->z + scratch->dz;
            orderingDepth    = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &depthCue, &projectionFlags);
            if (projectionFlags >= 0) {
                SHELTER_B4_RESERVOIR_EMIT_RED_WATER_QUAD();
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
}

#undef SHELTER_B4_RESERVOIR_EMIT_RED_WATER_QUAD

/// Draws the four view-10 water rectangles with height-dependent blue shading.
///
/// Uses the separate world Y in -1800..0 and positive descriptor counts
/// for strips along Z. Blue is -Y * 16 / 225 (0..128), green its quarter.
/// Borrows the terminated list, one WaterQuadScratch block and the frame's
/// byte packet cursor. Each valid projection consumes 36 bytes for a POLY_F4
/// and DR_MODE, transmitting only one mode word. Subtractive blending sorts
/// at projected depth + 1; negative GTE flags reject a quad. Releases scratch
/// on return. task is unused in the retained call interface.
static void _shelterB4ReservoirDrawView10WaterStrips(Task* task)
{
    /// Queues one projected water quad and its subtractive blend command.
    ///
    /// Captures screenXY0..3, orderingDepth, waterQuad, drawMode and blueIntensity;
    /// also uses the room packet cursor and current display ordering table.
    /// Advances the aligned frame byte cursor by 24 + 12 bytes; the mode sends
    /// only its first word. Call as a statement inside braces after projection;
    /// expands to a statement list, repeats no side effects, and links both packets.
    /// RGB is (0, blueIntensity / 4, blueIntensity). Head insertion makes the
    /// mode execute before the quad; the packed XY casts are intentional word
    /// views of adjacent signed pixel halfwords. Packets outlive this call.
#define SHELTER_B4_RESERVOIR_EMIT_VIEW10_WATER_STRIPS_QUAD()                                                                                                                                     \
    waterQuad                           = (POLY_F4*)_gShelterB4ReservoirWaterPrimCursor;                                                                                                         \
    _gShelterB4ReservoirWaterPrimCursor = (u8*)(waterQuad + 1);                                                                                                                                  \
    setlen(waterQuad, sizeof(*waterQuad) / sizeof(u32) - 1);                                                                                                                                     \
    setcode(waterQuad, SHELTER_B4_RESERVOIR_WATER_QUAD_CODE);                                                                                                                                    \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 0) = screenXY0;                                                                                                                                             \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 1) = screenXY1;                                                                                                                                             \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 2) = screenXY2;                                                                                                                                             \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 3) = screenXY3;                                                                                                                                             \
    waterQuad->r0                       = 0;                                                                                                                                                     \
    waterQuad->g0                       = blueIntensity >> 2;                                                                                                                                    \
    waterQuad->b0                       = blueIntensity;                                                                                                                                         \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(orderingDepth + SHELTER_B4_RESERVOIR_WATER_DEPTH_BIAS) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), \
            waterQuad);                                                                                                                                                                          \
    drawMode                            = (DR_MODE*)_gShelterB4ReservoirWaterPrimCursor;                                                                                                         \
    _gShelterB4ReservoirWaterPrimCursor = (u8*)(drawMode + 1);                                                                                                                                   \
    setlen(drawMode, SHELTER_B4_RESERVOIR_WATER_DRAW_MODE_WORDS);                                                                                                                                \
    drawMode->code[0] = SHELTER_B4_RESERVOIR_WATER_DRAW_MODE;                                                                                                                                    \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(orderingDepth + SHELTER_B4_RESERVOIR_WATER_DEPTH_BIAS) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), \
            drawMode);

    SVECTOR                 corner0, corner1, corner2, corner3;
    long                    screenXY0, screenXY1, screenXY2, screenXY3;
    long                    depthCue, projectionFlags;
    WaterQuadScratch*       scratchEnd;
    WaterQuadScratch*       scratch;
    const RoomWaterSurface* surface;
    POLY_F4*                waterQuad;
    DR_MODE*                drawMode;
    s32                     orderingDepth;
    s32                     segmentIndex;
    u8                      blueIntensity;

    surface                    = _gShelterB4ReservoirView10WaterSurfaces;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    (scratchEnd - 1)->y = _gShelterB4ReservoirView10WaterY;
    blueIntensity       = -(_gShelterB4ReservoirView10WaterY * 16) / 225;
    for (; surface->segmentCount != WATER_SURFACE_LIST_END; surface++) {
        scratch->dx = surface->width;
        scratch->dz = surface->depth / surface->segmentCount;
        scratch->x  = surface->x;
        scratch->z  = surface->z;
        for (segmentIndex = 0; segmentIndex < surface->segmentCount; segmentIndex++) {
            corner0.vx       = scratch->x;
            corner0.vy       = scratch->y;
            corner0.vz       = scratch->z + scratch->dz * segmentIndex;
            corner1.vx       = scratch->x;
            corner1.vy       = scratch->y;
            corner1.vz       = scratch->z + scratch->dz * (segmentIndex + 1);
            scratch->yOffset = 0;
            corner2.vx       = scratch->x + scratch->dx;
            corner2.vy       = scratch->y + scratch->yOffset;
            corner2.vz       = scratch->z + scratch->dz * segmentIndex;
            scratch->yOffset = 0;
            corner3.vx       = scratch->x + scratch->dx;
            corner3.vy       = scratch->y + scratch->yOffset;
            corner3.vz       = scratch->z + scratch->dz * (segmentIndex + 1);
            orderingDepth    = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &depthCue, &projectionFlags);
            if (projectionFlags >= 0) {
                SHELTER_B4_RESERVOIR_EMIT_VIEW10_WATER_STRIPS_QUAD();
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);

#undef SHELTER_B4_RESERVOIR_EMIT_VIEW10_WATER_STRIPS_QUAD
}

/// Runs the reservoir water renderer and publishes the main world-unit water height.
///
/// Requires state 0 (borrowed arena setup) or 1 (per-frame drawing).
/// The two-handler table is copied to the stack before dispatch, with no bounds
/// check. Publishes the main plane even when view 10 draws its separate plane.
static void _shelterB4ReservoirWaterTask(Task* task)
{
    TaskFunc states[2] = { _shelterB4ReservoirInitializeWaterTask, _shelterB4ReservoirDrawWaterTask };

    states[task->state](task);
    gGameSession->waterY = gShelterB4ReservoirMainWaterY;
}

/// Prepares the actor buffer reused by the reservoir's water renderer.
///
/// Called in state 0 of the water task. Clears the session marker for actor
/// buffer 2 without a companion, or buffer 1 with a companion, then advances
/// to state 1. The markers' nonzero meaning is unproven; both buffers are
/// borrowed rendering storage, and this initializer allocates nothing.
static void _shelterB4ReservoirInitializeWaterTask(Task* task)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    task->state++;
}

/// Places one burst point around the room anchor, consuming two random draws.
///
/// pointIndex must be 0..9. In world units, X is anchor X +2048 and Y/Z
/// scatter in a circle of radius 0..448 in steps of 64. The angle is
/// pointIndex*512 plus jitter 0..511, in 4096 units per turn: points 8/9
/// revisit the first two sectors without normalizing the stored angle.
/// Borrows writable work for signed-halfword radius/angle scratch and writes
/// only the selected point's XYZ, narrowing to signed halfwords. The anchor,
/// ten-point table and work must stay live; neither allocation nor draw occurs.
static inline void _shelterB4ReservoirScatterBurstPoint(EffectWork* work, s32 pointIndex)
{
    enum {
        SHELTER_B4_RESERVOIR_BURST_RADIUS_MASK  = 0x1C0,
        SHELTER_B4_RESERVOIR_BURST_ANGLE_MASK   = 511,
        SHELTER_B4_RESERVOIR_BURST_SECTOR_SHIFT = 9,
        SHELTER_B4_RESERVOIR_BURST_Q12_SHIFT    = 12,
        SHELTER_B4_RESERVOIR_BURST_X_OFFSET     = 2048,
    };

    work->scale                                    = SHELTER_B4_RESERVOIR_NEXT_RANDOM_U16() & SHELTER_B4_RESERVOIR_BURST_RADIUS_MASK;
    work->angle                                    = (SHELTER_B4_RESERVOIR_NEXT_RANDOM_U16() & SHELTER_B4_RESERVOIR_BURST_ANGLE_MASK) + (pointIndex << SHELTER_B4_RESERVOIR_BURST_SECTOR_SHIFT);
    _gShelterB4ReservoirBurstPoints[pointIndex].vx = _gShelterB4ReservoirBurstAnchor.vx + SHELTER_B4_RESERVOIR_BURST_X_OFFSET;
    _gShelterB4ReservoirBurstPoints[pointIndex].vy =
        _gShelterB4ReservoirBurstAnchor.vy + ((work->scale * rsin(work->angle)) >> SHELTER_B4_RESERVOIR_BURST_Q12_SHIFT);
    _gShelterB4ReservoirBurstPoints[pointIndex].vz =
        _gShelterB4ReservoirBurstAnchor.vz + ((work->scale * rcos(work->angle)) >> SHELTER_B4_RESERVOIR_BURST_Q12_SHIFT);
}

void shelterB4ReservoirAmbientEffectsTask(Task* task)
{
    enum {
        SHELTER_B4_RESERVOIR_AMBIENT_INITIALIZE         = 0,
        SHELTER_B4_RESERVOIR_AMBIENT_RUNNING            = 1,
        SHELTER_B4_RESERVOIR_PLAYER_SAMPLE_FIRST_COORD  = 14,
        SHELTER_B4_RESERVOIR_PLAYER_SAMPLE_COORD_STRIDE = 3,
        SHELTER_B4_RESERVOIR_SPLASH_CHANCE_MASK         = 511,
        SHELTER_B4_RESERVOIR_RIPPLE_CHANCE_BIAS         = 32,
        SHELTER_B4_RESERVOIR_BURST_REPOSITION_MASK      = 31,
        SHELTER_B4_RESERVOIR_PLAYER_RIPPLE_HALF_SIZE    = 64,
        // Rotated kind-1 spray, speed 32, period 2, half-extent 384.
        SHELTER_B4_RESERVOIR_PLAYER_SPRAY_ARGS = (1 << 24) | (32 << 16) | (2 << 12) | 384,
        // Rotated kind 4 is retained despite having no direction-generation arm.
        SHELTER_B4_RESERVOIR_VIEW10_ROTATED_SPRAY_ARGS = (4 << 24) | (160 << 16) | (3 << 12) | 1536,
        // Upright kind-1 spray, speed 96, period 2, half-extent 768.
        SHELTER_B4_RESERVOIR_VIEW10_UPRIGHT_SPRAY_ARGS   = (1 << 28) | (1 << 24) | (96 << 16) | (2 << 12) | 768,
        SHELTER_B4_RESERVOIR_SPRAY_PERIOD_VARIATION      = 1 << 12,
        SHELTER_B4_RESERVOIR_SPRAY_SIZE_PERIOD_VARIATION = (1 << 12) | 255,
        SHELTER_B4_RESERVOIR_VIEW10_RIPPLE_SIZE_MASK     = 127,
        SHELTER_B4_RESERVOIR_VIEW10_RIPPLE_MIN_HALF_SIZE = 128,
        SHELTER_B4_RESERVOIR_CAPSULE_RADIUS_SCALE        = 512,
        SHELTER_B4_RESERVOIR_DISC_RADIUS_SCALE           = 256,
        // Glow draw RGB nibbles, with the disc's flicker shift in bits 12..15.
        SHELTER_B4_RESERVOIR_GLOW_DIM_WHITE           = 0x222,
        SHELTER_B4_RESERVOIR_GLOW_MID_WHITE           = 0x333,
        SHELTER_B4_RESERVOIR_GLOW_WHITE               = 0x444,
        SHELTER_B4_RESERVOIR_DISC_EVENT_DONE_COLOR    = 0x504C,
        SHELTER_B4_RESERVOIR_DISC_EVENT_PENDING_COLOR = 0x5C40,
        SHELTER_B4_RESERVOIR_SPRAY_CHANCE_MASK        = 1,
        SHELTER_B4_RESERVOIR_RIPPLE_CHANCE_MASK       = 3,
    };
    EffectWork* work;
    Task*       player;
    GfxCoord*   playerRoot;
    GfxCoord*   sampledPlayerCoord;
    GfxCoord    splashCoord;
    s32         loopIndex;
    s32         baseHalfExtent;
    s32         randomizedSpawnArgs;

    work       = task->spawnArg2.pointer;
    player     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerRoot = player->extra.tmd->coords;
    if (task->state == SHELTER_B4_RESERVOIR_AMBIENT_INITIALIZE) {
        gRoomEffectWaterRippleId  = EFFECT_SHELTER_B4_RESERVOIR_WATER_RIPPLE;
        gRoomEffectWaterSprayId   = EFFECT_SHELTER_B4_RESERVOIR_WATER_SPRAY;
        gRoomEffectGlowDiscId     = EFFECT_SHELTER_B4_RESERVOIR_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B4_RESERVOIR_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B4_RESERVOIR_ORANGE_BURST_2;
        task->state               = SHELTER_B4_RESERVOIR_AMBIENT_RUNNING;
        // Scatter the burst points around the anchor. The work's `scale` and
        // `angle` hold the polar radius and angle of the point being placed.
        for (loopIndex = 0; loopIndex < (s32)ARRAY_SIZE(_gShelterB4ReservoirBurstPoints); loopIndex++) {
            _shelterB4ReservoirScatterBurstPoint(work, loopIndex);
        }
        _gShelterB4ReservoirBurstConfig.pointCount         = 0;
        _gShelterB4ReservoirBurstConfig.spawnChancePercent = 0;
        _gShelterB4ReservoirBurstConfig.baseHalfExtent     = 0;
        for (loopIndex = 0; loopIndex < (s32)ARRAY_SIZE(_gShelterB4ReservoirPlayerSamplePositions); loopIndex++) {
            sampledPlayerCoord                                      = &player->extra.tmd->coords[loopIndex * SHELTER_B4_RESERVOIR_PLAYER_SAMPLE_COORD_STRIDE + SHELTER_B4_RESERVOIR_PLAYER_SAMPLE_FIRST_COORD];
            _gShelterB4ReservoirPlayerSamplePositions[loopIndex].vx = sampledPlayerCoord->workm.t[0];
            _gShelterB4ReservoirPlayerSamplePositions[loopIndex].vy = sampledPlayerCoord->workm.t[1];
            _gShelterB4ReservoirPlayerSamplePositions[loopIndex].vz = sampledPlayerCoord->workm.t[2];
        }
    }
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
            // `age` counts the frames run with the event done; the splash pass waits for the second.
            if (gGameSession->waterY < playerRoot->coord.t[1] && work->age != 0) {
                for (loopIndex = 0; loopIndex < (s32)ARRAY_SIZE(_gShelterB4ReservoirPlayerSamplePositions); loopIndex++) {
                    sampledPlayerCoord = &player->extra.tmd->coords[loopIndex * SHELTER_B4_RESERVOIR_PLAYER_SAMPLE_COORD_STRIDE + SHELTER_B4_RESERVOIR_PLAYER_SAMPLE_FIRST_COORD];
                    actorRenderComposeCoord(sampledPlayerCoord);
                    // `angle` is reused as the splash chance out of 512: the distance
                    // this coordinate moved since the previous frame, plus a ripple bias.
                    work->angle = ABS(_gShelterB4ReservoirPlayerSamplePositions[loopIndex].vx - sampledPlayerCoord->workm.t[0]) +
                                  ABS(_gShelterB4ReservoirPlayerSamplePositions[loopIndex].vy - sampledPlayerCoord->workm.t[1]) +
                                  ABS(_gShelterB4ReservoirPlayerSamplePositions[loopIndex].vz - sampledPlayerCoord->workm.t[2]) + SHELTER_B4_RESERVOIR_RIPPLE_CHANCE_BIAS;
                    // Place the composed player sample on the water plane in view-parent space.
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &sampledPlayerCoord->workm, &splashCoord.coord);
                    splashCoord.parent       = &gGfxViewCoord;
                    splashCoord.coord.t[1]   = gGameSession->waterY;
                    splashCoord.composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(&splashCoord);
                    if ((s32)(SHELTER_B4_RESERVOIR_NEXT_RANDOM_U16() & SHELTER_B4_RESERVOIR_SPLASH_CHANCE_MASK) < work->angle) {
                        effectSpawn(gRoomEffectWaterRippleId, &splashCoord, SHELTER_B4_RESERVOIR_PLAYER_RIPPLE_HALF_SIZE, NULL);
                    }
                    work->angle -= SHELTER_B4_RESERVOIR_RIPPLE_CHANCE_BIAS;
                    if ((s32)(SHELTER_B4_RESERVOIR_NEXT_RANDOM_U16() & SHELTER_B4_RESERVOIR_SPLASH_CHANCE_MASK) < work->angle) {
                        effectSpawn(gRoomEffectWaterSprayId, &splashCoord, SHELTER_B4_RESERVOIR_PLAYER_SPRAY_ARGS, NULL);
                    }
                    _gShelterB4ReservoirPlayerSamplePositions[loopIndex].vx = sampledPlayerCoord->workm.t[0];
                    _gShelterB4ReservoirPlayerSamplePositions[loopIndex].vy = sampledPlayerCoord->workm.t[1];
                    _gShelterB4ReservoirPlayerSamplePositions[loopIndex].vz = sampledPlayerCoord->workm.t[2];
                }
            }
            work->age++;
        }
        // Emit independently at each active point, occasionally moving its position.
        if (_gShelterB4ReservoirBurstConfig.pointCount != 0 && _gShelterB4ReservoirBurstConfig.spawnChancePercent != 0) {
            for (loopIndex = 0; loopIndex < _gShelterB4ReservoirBurstConfig.pointCount; loopIndex++) {
                if ((SHELTER_B4_RESERVOIR_NEXT_RANDOM_U16() & SHELTER_B4_RESERVOIR_BURST_REPOSITION_MASK) == 0) {
                    _shelterB4ReservoirScatterBurstPoint(work, loopIndex);
                }
                if ((u16)(SHELTER_B4_RESERVOIR_NEXT_RANDOM_U16() % SHELTER_B4_RESERVOIR_BURST_CHANCE_SCALE) < _gShelterB4ReservoirBurstConfig.spawnChancePercent) {
                    baseHalfExtent      = _gShelterB4ReservoirBurstConfig.baseHalfExtent;
                    randomizedSpawnArgs = (SHELTER_B4_RESERVOIR_NEXT_RANDOM_U16() & SHELTER_B4_RESERVOIR_BURST_VARIATION_MASK) + SHELTER_B4_RESERVOIR_BURST_MOTION_ARGS;
                    effectSpawn(EFFECT_SHELTER_B4_RESERVOIR_BURST_SPRITE, NULL, baseHalfExtent + randomizedSpawnArgs,
                                &_gShelterB4ReservoirBurstPoints[loopIndex]);
                }
            }
        }
    }
    // View spray and glow drawing continue independently of effect-control pause.
    if ((u8)viewGetMappedIndex() == SHELTER_B4_RESERVOIR_WATER_SPRAY_VIEW) {
        _gShelterB4ReservoirView10WaterEffectPoints.waterSurface.vy = _gShelterB4ReservoirView10WaterY;
        if ((SHELTER_B4_RESERVOIR_NEXT_RANDOM_U16() & SHELTER_B4_RESERVOIR_SPRAY_CHANCE_MASK) == 0) {
            effectSpawn(gRoomEffectWaterSprayId, NULL, (SHELTER_B4_RESERVOIR_NEXT_RANDOM_U16() & SHELTER_B4_RESERVOIR_SPRAY_PERIOD_VARIATION) + SHELTER_B4_RESERVOIR_VIEW10_ROTATED_SPRAY_ARGS, &_gShelterB4ReservoirView10WaterEffectPoints.fixedSpray);
        }
        if ((SHELTER_B4_RESERVOIR_NEXT_RANDOM_U16() & SHELTER_B4_RESERVOIR_SPRAY_CHANCE_MASK) == 0) {
            effectSpawn(gRoomEffectWaterSprayId, NULL, (SHELTER_B4_RESERVOIR_NEXT_RANDOM_U16() & SHELTER_B4_RESERVOIR_SPRAY_SIZE_PERIOD_VARIATION) | SHELTER_B4_RESERVOIR_VIEW10_UPRIGHT_SPRAY_ARGS, &_gShelterB4ReservoirView10WaterEffectPoints.waterSurface);
        }
        if ((SHELTER_B4_RESERVOIR_NEXT_RANDOM_U16() & SHELTER_B4_RESERVOIR_RIPPLE_CHANCE_MASK) == 0) {
            effectSpawn(gRoomEffectWaterRippleId, NULL, (SHELTER_B4_RESERVOIR_NEXT_RANDOM_U16() & SHELTER_B4_RESERVOIR_VIEW10_RIPPLE_SIZE_MASK) | SHELTER_B4_RESERVOIR_VIEW10_RIPPLE_MIN_HALF_SIZE, &_gShelterB4ReservoirView10WaterEffectPoints.waterSurface);
        }
    }
    // Per-view point pairs supply capsule glow; the event flag changes the disc tint.
    switch ((u8)viewGetMappedIndex()) {
        case 2:
            _glowDrawCapsule(_gShelterB4ReservoirGlowPoints.capsuleEndpoints[5], SHELTER_B4_RESERVOIR_CAPSULE_RADIUS_SCALE, SHELTER_B4_RESERVOIR_GLOW_WHITE);
            break;
        case 4:
            _glowDrawCapsule(_gShelterB4ReservoirGlowPoints.capsuleEndpoints[0], SHELTER_B4_RESERVOIR_CAPSULE_RADIUS_SCALE, SHELTER_B4_RESERVOIR_GLOW_DIM_WHITE);
            _glowDrawCapsule(_gShelterB4ReservoirGlowPoints.capsuleEndpoints[3], SHELTER_B4_RESERVOIR_CAPSULE_RADIUS_SCALE, SHELTER_B4_RESERVOIR_GLOW_WHITE);
            _glowDrawCapsule(_gShelterB4ReservoirGlowPoints.capsuleEndpoints[4], SHELTER_B4_RESERVOIR_CAPSULE_RADIUS_SCALE, SHELTER_B4_RESERVOIR_GLOW_MID_WHITE);
            if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
                glowDrawTintedDiscNoBias(&_gShelterB4ReservoirGlowPoints.eventIndicator, SHELTER_B4_RESERVOIR_DISC_RADIUS_SCALE, SHELTER_B4_RESERVOIR_DISC_EVENT_DONE_COLOR);
            } else {
                glowDrawTintedDiscNoBias(&_gShelterB4ReservoirGlowPoints.eventIndicator, SHELTER_B4_RESERVOIR_DISC_RADIUS_SCALE, SHELTER_B4_RESERVOIR_DISC_EVENT_PENDING_COLOR);
            }
            break;
        case 5:
            _glowDrawCapsule(_gShelterB4ReservoirGlowPoints.capsuleEndpoints[0], SHELTER_B4_RESERVOIR_CAPSULE_RADIUS_SCALE, SHELTER_B4_RESERVOIR_GLOW_WHITE);
            if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
                glowDrawTintedDiscNoBias(&_gShelterB4ReservoirGlowPoints.eventIndicator, SHELTER_B4_RESERVOIR_DISC_RADIUS_SCALE, SHELTER_B4_RESERVOIR_DISC_EVENT_DONE_COLOR);
            } else {
                glowDrawTintedDiscNoBias(&_gShelterB4ReservoirGlowPoints.eventIndicator, SHELTER_B4_RESERVOIR_DISC_RADIUS_SCALE, SHELTER_B4_RESERVOIR_DISC_EVENT_PENDING_COLOR);
            }
            break;
        case 6:
            if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
                glowDrawTintedDiscNoBias(&_gShelterB4ReservoirGlowPoints.eventIndicator, SHELTER_B4_RESERVOIR_DISC_RADIUS_SCALE, SHELTER_B4_RESERVOIR_DISC_EVENT_DONE_COLOR);
            } else {
                glowDrawTintedDiscNoBias(&_gShelterB4ReservoirGlowPoints.eventIndicator, SHELTER_B4_RESERVOIR_DISC_RADIUS_SCALE, SHELTER_B4_RESERVOIR_DISC_EVENT_PENDING_COLOR);
            }
            break;
        case 7:
            _glowDrawCapsule(_gShelterB4ReservoirGlowPoints.capsuleEndpoints[1], SHELTER_B4_RESERVOIR_CAPSULE_RADIUS_SCALE, SHELTER_B4_RESERVOIR_GLOW_WHITE);
            break;
        case 3:
        case 9:
            _glowDrawCapsule(_gShelterB4ReservoirGlowPoints.capsuleEndpoints[2], SHELTER_B4_RESERVOIR_CAPSULE_RADIUS_SCALE, SHELTER_B4_RESERVOIR_GLOW_WHITE);
            break;
    }
}

#include "../../shared/effects/water_ripple_task.inc.c"

void shelterB4ReservoirWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/effects/water_splash.inc.c"

#include "../../shared/effects/water_drift_task.inc.c"

void shelterB4ReservoirWaterDriftTask(Task* task)
{
    _waterDriftTask(task);
}

#include "../../shared/effects/water_spin.inc.c"

/// Sets a reservoir burst sprite's launch velocity along negative parent X.
///
/// Borrows writable effect work with `step` already set to the launch speed
/// in parent-coordinate units per update (the caller supplies 1..255).
/// Consumes one shared random draw to seed a nonzero direction of magnitude
/// 64..127, then normalizes in Q12 and scales by `step`. SDK approximation
/// and fixed-point rounding are retained; the result need not equal `-step`.
/// Only the three `move` components change. The vector's unused halfword and
/// the other work fields stay intact; GTE state is overwritten. No pointer
/// is retained.
static inline void _shelterB4ReservoirInitializeBurstVelocity(EffectWork* spriteWork)
{
    enum {
        SHELTER_B4_RESERVOIR_BURST_DIRECTION_MAGNITUDE_MIN  = 64,
        SHELTER_B4_RESERVOIR_BURST_DIRECTION_VARIATION_MASK = 63,
    };
    u32 randomSample;

    spriteWork->move.vy = 0;
    spriteWork->move.vz = 0;
    randomSample        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState     = randomSample;
    spriteWork->move.vx = -((randomSample >> 16) & SHELTER_B4_RESERVOIR_BURST_DIRECTION_VARIATION_MASK) -
                          SHELTER_B4_RESERVOIR_BURST_DIRECTION_MAGNITUDE_MIN;
    VectorNormalSS(&spriteWork->move, &spriteWork->move);
    gte_lddp(spriteWork->step);
    gte_ldsv(&spriteWork->move);
    gte_gpf12();
    gte_stsv(&spriteWork->move);
}

void shelterB4ReservoirBurstSpriteTask(Task* task)
{
    enum {
        SHELTER_B4_RESERVOIR_BURST_SPRITE_INITIALIZE          = 0,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_ANIMATE             = 1,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_SIZE_MASK           = 0xFFF,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_PERIOD_PRESENT_MASK = 0xF000,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_PERIOD_SHIFT        = 12,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_PERIOD_MASK         = 7,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_DEFAULT_PERIOD      = 1,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_SPEED_PRESENT_MASK  = 0xFF0000,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_SPEED_SHIFT         = 16,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_SPEED_MASK          = 0xFF,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_DEFAULT_SPEED       = 64,
    };
    EffectWork* spriteWork  = task->spawnArg2.pointer;
    GfxCoord*   spriteCoord = task->extra.coordBody->coord;
    s16         speed;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        // Frozen and cancelling updates draw the retained cell before any teardown.
        _shelterB4ReservoirDrawBurstSprite(spriteCoord, spriteWork->index, spriteWork->scale);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(spriteWork, task);
        }
        return;
    }

    spriteWork->age++;
    switch (task->state) {
        case SHELTER_B4_RESERVOIR_BURST_SPRITE_INITIALIZE:
            spriteWork->scale = task->spawnArg1.value & SHELTER_B4_RESERVOIR_BURST_SPRITE_SIZE_MASK;

            if (task->spawnArg1.value & SHELTER_B4_RESERVOIR_BURST_SPRITE_PERIOD_PRESENT_MASK) {
                spriteWork->period = (task->spawnArg1.value >> SHELTER_B4_RESERVOIR_BURST_SPRITE_PERIOD_SHIFT) & SHELTER_B4_RESERVOIR_BURST_SPRITE_PERIOD_MASK;
            } else {
                spriteWork->period = SHELTER_B4_RESERVOIR_BURST_SPRITE_DEFAULT_PERIOD;
            }

            spriteWork->age = 0;
            task->state     = SHELTER_B4_RESERVOIR_BURST_SPRITE_ANIMATE;

            if (task->spawnArg1.value & SHELTER_B4_RESERVOIR_BURST_SPRITE_SPEED_PRESENT_MASK) {
                speed = (task->spawnArg1.value >> SHELTER_B4_RESERVOIR_BURST_SPRITE_SPEED_SHIFT) & SHELTER_B4_RESERVOIR_BURST_SPRITE_SPEED_MASK;
            } else {
                speed = SHELTER_B4_RESERVOIR_BURST_SPRITE_DEFAULT_SPEED;
            }

            spriteWork->step = speed;
            _shelterB4ReservoirInitializeBurstVelocity(spriteWork);
            break;
        case SHELTER_B4_RESERVOIR_BURST_SPRITE_ANIMATE:
            // The coordinate-list pass composes movement between task updates.
            _shelterB4ReservoirDrawBurstSprite(spriteCoord, spriteWork->index, spriteWork->scale);
            if (spriteWork->step != 0) {
                spriteCoord->coord.t[0]  += spriteWork->move.vx;
                spriteCoord->coord.t[1]  += spriteWork->move.vy;
                spriteCoord->coord.t[2]  += spriteWork->move.vz;
                spriteCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            if ((spriteWork->age % spriteWork->period) == 0) {
                spriteWork->index++;
                if (spriteWork->index >= SHELTER_B4_RESERVOIR_BURST_SPRITE_FRAME_COUNT) {
                    effectKillTask(spriteWork, task);
                }
            }
            break;
    }
}

/// Sets a reservoir burst quad's square corners around its projected centre.
///
/// Borrows a writable packet and read-only projection with `screenX`,
/// `screenY` and `screenExtent` initialized. The extent is a pixel half-size;
/// its low 16 bits and the raw centre encodings determine each edge modulo
/// 65536, without clipping. With a nonnegative extent before wrapping,
/// corners 0/1 are the top row and 0/2 the left column.
/// Writes only the packet's eight XY halfwords and retains neither pointer.
static inline void _shelterB4ReservoirSetBurstSpriteBounds(POLY_FT4* quad, const EffectCentreScratch* projection)
{
    quad->x0 = quad->x2 = projection->screenX - (u16)projection->screenExtent;
    quad->x1 = quad->x3 = projection->screenX + (u16)projection->screenExtent;
    quad->y0 = quad->y1 = projection->screenY - (u16)projection->screenExtent;
    quad->y2 = quad->y3 = projection->screenY + (u16)projection->screenExtent;
}

/// Draws one raw-texture, semi-transparent cell of the reservoir's six-cell burst strip.
///
/// Borrows a composed coordinate, narrowing its world translation to signed
/// 16-bit units. `animationFrame` wraps modulo six into 32-texel cells;
/// `halfExtent` is a signed world-unit perspective scale. The screen radius is
/// `halfExtent * 31 / (SZ3 / 4)`, with no depth bias or clamp: accepted projections
/// must have nonzero depth. Negative GTE flags suppress drawing. Reserves and
/// releases one complete scratch block; the emitted quad lives for this frame.
static void _shelterB4ReservoirDrawBurstSprite(const GfxCoord* spriteCoord, u16 animationFrame, s16 halfExtent)
{
    enum {
        // POLY_FT4 with semitransparency enabled and texture modulation disabled.
        SHELTER_B4_RESERVOIR_BURST_SPRITE_GPU_CODE     = 0x2F,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_TEXTURE_PAGE = 0x2B,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_CLUT         = 0x4393,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_U_ORIGIN     = 0x40,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_V_ORIGIN     = 0x40,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_CELL_TEXELS  = 32,
        SHELTER_B4_RESERVOIR_BURST_SPRITE_CELL_SHIFT   = 5,
    };
    EffectCentreScratch* scratchEnd;
    EffectCentreScratch* projection;
    POLY_FT4*            quad;
    SVECTOR*             worldPoint;
    DisplayState*        displayState;
    s32                  textureColumn;
    u16                  worldZ;

    // Stage the world point in the block below the downward-growing cursor.
    scratchEnd                                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    scratchEnd[-1].worldPoint.vx              = (u16)spriteCoord->workm.t[0];
    projection                                = scratchEnd - 1;
    projection->worldPoint.vy                 = (u16)spriteCoord->workm.t[1];
    worldZ                                    = (u16)spriteCoord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectCentreScratch) = projection;
    projection->worldPoint.vz                 = worldZ;
    worldPoint                                = &projection->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&scratchEnd[-1].screenX);
    gte_stflg(&scratchEnd[-1].projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&scratchEnd[-1].depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        setcode(quad, SHELTER_B4_RESERVOIR_BURST_SPRITE_GPU_CODE);
        quad->tpage              = SHELTER_B4_RESERVOIR_BURST_SPRITE_TEXTURE_PAGE;
        quad->clut               = SHELTER_B4_RESERVOIR_BURST_SPRITE_CLUT;
        quad->v0                 = SHELTER_B4_RESERVOIR_BURST_SPRITE_V_ORIGIN;
        quad->v1                 = SHELTER_B4_RESERVOIR_BURST_SPRITE_V_ORIGIN;
        quad->v2                 = SHELTER_B4_RESERVOIR_BURST_SPRITE_V_ORIGIN + SHELTER_B4_RESERVOIR_BURST_SPRITE_CELL_TEXELS - 1;
        quad->v3                 = SHELTER_B4_RESERVOIR_BURST_SPRITE_V_ORIGIN + SHELTER_B4_RESERVOIR_BURST_SPRITE_CELL_TEXELS - 1;
        textureColumn            = (u16)(animationFrame % SHELTER_B4_RESERVOIR_BURST_SPRITE_FRAME_COUNT) << SHELTER_B4_RESERVOIR_BURST_SPRITE_CELL_SHIFT;
        quad->u0                 = textureColumn + SHELTER_B4_RESERVOIR_BURST_SPRITE_U_ORIGIN;
        quad->u2                 = textureColumn + SHELTER_B4_RESERVOIR_BURST_SPRITE_U_ORIGIN;
        quad->u1                 = textureColumn + SHELTER_B4_RESERVOIR_BURST_SPRITE_U_ORIGIN + SHELTER_B4_RESERVOIR_BURST_SPRITE_CELL_TEXELS - 1;
        quad->u3                 = textureColumn + SHELTER_B4_RESERVOIR_BURST_SPRITE_U_ORIGIN + SHELTER_B4_RESERVOIR_BURST_SPRITE_CELL_TEXELS - 1;
        projection->screenExtent = (halfExtent * (SHELTER_B4_RESERVOIR_BURST_SPRITE_CELL_TEXELS - 1)) / projection->depth;
        _shelterB4ReservoirSetBurstSpriteBounds(quad, projection);
        displayState = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << displayState->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

#include "../../shared/effects/glow_draw_capsule.inc.c"

#include "../../shared/effects/glow_draw_tinted_disc_no_bias.inc.c"

/// Replaces the reservoir burst emitter's active point count, chance and size.
///
/// pointCount must be 0..10; neither it nor spawnChancePercent is clamped.
/// Zero count or chance disables emission, and chance >=100 always passes.
/// baseHalfExtent is in world units and must be 0..3840 so random 0..255
/// variation fits the packed argument's low 12 bits. Signed parameters retain
/// their low 16 bits when stored; negative values violate the emitter bounds.
static void _shelterB4ReservoirSetBurstConfig(s16 pointCount, u16 spawnChancePercent, s16 baseHalfExtent)
{
    _gShelterB4ReservoirBurstConfig.pointCount         = pointCount;
    _gShelterB4ReservoirBurstConfig.spawnChancePercent = spawnChancePercent;
    _gShelterB4ReservoirBurstConfig.baseHalfExtent     = baseHalfExtent;
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flying_tasks.inc.c"

void shelterB4ReservoirRoomVisualEffectsGlowDiscTask(Task* task)
{
    _roomVisualEffectsGlowDiscTask(task);
}

void shelterB4ReservoirRoomVisualEffectsFlyingSparkTask(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room/room_visual_effects_burst.inc.c"

void shelterB4ReservoirRoomVisualEffectsFlyingOrangeBurstTask(Task* task)
{
    _roomVisualEffectsFlyingOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_burst_draw.inc.c"
