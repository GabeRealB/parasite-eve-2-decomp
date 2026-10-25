#include "actors/actor_04400.h"

#include "types.h"

#include "actors/actor_00100.h"

#include "main/task_types.h"
#include "../../shared/effects/glow_draw.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
Task* gRoomCutsceneSoundTask;

/// Borrowed arrival-cutscene controller used by this overlay's script callbacks.
///
/// Published only after its work and player reference are initialized. Callbacks
/// require the task and work to stay live; teardown leaves this pointer uncleared.
Task* _gDryfieldGasStationArrivalCutsceneTask;

#include "rooms/dryfield_gas_station.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "dryfield_gas_station_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/screen/screen_fade.h"

/// Commands the gas-station cutscene script leaves for the cutscene task.
///
/// Each one is stored over the previous command and restarts `commandStep`.
/// The task carries it out on later updates and then clears it. A command
/// that takes several updates advances `commandStep` and returns until its
/// last step.
enum {
    /// Nothing pending.
    DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_NONE = 0,
    /// Places the player at the first entry of `_gDryfieldGasStationArrivalPlayerPlacements`
    /// and starts the cutscene loop plus area sound 0x12.
    DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLACE_AND_START_AUDIO = 1,
    /// Plays animation 1 of `_gDryfieldGasStationArrivalAnimationSets` from its start
    /// and sets the playback rate to 8.
    DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLAY_ANIMATION = 2,
    /// Plays area sound 0x13, places the player at the second entry, and
    /// blends into animation 2 over 30 frames.
    DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLACE_AND_BLEND = 3,
    /// Plays animation 3 and selects player movement mode 1, then sends one
    /// thirtieth of the first-to-third placement delta each update. After 31
    /// steps, blends into animation 0 over 15 frames.
    DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_WALK_PLACEMENTS = 4,
    /// Restores suppressed player effects, places the player at the third
    /// entry, plays animation 0 from its start, stops the cutscene loop and
    /// enables the display.
    DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_RESTORE_AND_SHOW = 5,
    /// Spawns the fade-in task, waits one frame, and enables the display.
    DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_SPAWN_FADE_IN = 6,
};

/// Work block of the gas-station cutscene task.
///
/// The task allocates and zeroes the whole block when the cutscene starts,
/// then publishes itself so the room's script callbacks can reach it.
typedef struct {
    Task* player;                  // Player task captured when the cutscene starts. Animation installs test NULL; placement, rate and movement sends do not
    u16   command;                 // Pending `DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_*`, cleared once carried out
    u16   commandStep;             // Step within a command that takes several updates; zeroed with each new command
    u16   walkFrames;              // Placement-walk steps already sent. Other commands leave it unread
    byte  field_A[2];              // Allocated and cleared with the block; no access. Role and width unproven
    u16   playerEffectsSuppressed; // 0 none pending; 1 player effects were killed and still need to be spawned back
    byte  field_E[2];              // Trailing bytes of the 0x10 block; no access. Role and width unproven
} _DryfieldGasStationCutsceneWork;
STATIC_ASSERT_SIZEOF(_DryfieldGasStationCutsceneWork, 0x10);

static void _dryfieldGasStationSuppressPlayerEquipment(void);
static void _dryfieldGasStationSetCutsceneCommand(s16 command);

static WorldCollisionGrid    _gDryfieldGasStationCollisionGrid;
static WorldCollisionTrigger _gDryfieldGasStationViewBoundaryTriggers[11];
static WorldCollisionTrigger _gDryfieldGasStationActionTriggers[10];
static WorldCoordRoomLights  _gDryfieldGasStationRoomLights;
static void                  _dryfieldGasStationArrivalCutsceneTask(Task* task);
static void                  _screenFadeInTask(Task* task);
static void                  _dryfieldGasStationRestorePlayerAndShow(void);

/// Fade-in callback slot in the cutscene task descriptor table.
enum { DRYFIELD_GAS_STATION_CUTSCENE_TASK_FADE_IN = 1 };

TaskDesc gDryfieldGasStationArrivalTaskDescs[3] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, dryfieldGasStationArrivalTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, dryfieldGasStationArrivalMovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, screenFadeInTask, { .value = 0 } },
};

static AnimationPackedPose _gDryfieldGasStationAnimation04A70Bank1[2] = {
#include "assets/dryfield_gas_station_animation_04A70_bank1.inc"
};

static AnimationPackedRotation _gDryfieldGasStationAnimation04A70Bank4[8] = {
#include "assets/dryfield_gas_station_animation_04A70_bank4.inc"
};

static AnimationRecord _gDryfieldGasStationAnimation04A70Records[76] = {
#include "assets/dryfield_gas_station_animation_04A70_records.inc"
};

static u16 _gDryfieldGasStationAnimation04A70Indices[20] = {
#include "assets/dryfield_gas_station_animation_04A70_indices.inc"
};

static AnimationSet _gDryfieldGasStationAnimation04A70 = {
    _gDryfieldGasStationAnimation04A70Records,
    _gDryfieldGasStationAnimation04A70Indices,
    { NULL, _gDryfieldGasStationAnimation04A70Bank1, NULL, NULL, _gDryfieldGasStationAnimation04A70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldGasStationAnimation05210Bank1[13] = {
#include "assets/dryfield_gas_station_animation_05210_bank1.inc"
};

static AnimationPackedRotation _gDryfieldGasStationAnimation05210Bank4[179] = {
#include "assets/dryfield_gas_station_animation_05210_bank4.inc"
};

static AnimationRecord _gDryfieldGasStationAnimation05210Records[250] = {
#include "assets/dryfield_gas_station_animation_05210_records.inc"
};

static u16 _gDryfieldGasStationAnimation05210Indices[20] = {
#include "assets/dryfield_gas_station_animation_05210_indices.inc"
};

static AnimationSet _gDryfieldGasStationAnimation05210 = {
    _gDryfieldGasStationAnimation05210Records,
    _gDryfieldGasStationAnimation05210Indices,
    { NULL, _gDryfieldGasStationAnimation05210Bank1, NULL, NULL, _gDryfieldGasStationAnimation05210Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldGasStationAnimation055A4Bank1[7] = {
#include "assets/dryfield_gas_station_animation_055A4_bank1.inc"
};

static AnimationPackedRotation _gDryfieldGasStationAnimation055A4Bank4[65] = {
#include "assets/dryfield_gas_station_animation_055A4_bank4.inc"
};

static AnimationRecord _gDryfieldGasStationAnimation055A4Records[123] = {
#include "assets/dryfield_gas_station_animation_055A4_records.inc"
};

static u16 _gDryfieldGasStationAnimation055A4Indices[20] = {
#include "assets/dryfield_gas_station_animation_055A4_indices.inc"
};

static AnimationSet _gDryfieldGasStationAnimation055A4 = {
    _gDryfieldGasStationAnimation055A4Records,
    _gDryfieldGasStationAnimation055A4Indices,
    { NULL, _gDryfieldGasStationAnimation055A4Bank1, NULL, NULL, _gDryfieldGasStationAnimation055A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldGasStationAnimation05848Bank1[2] = {
#include "assets/dryfield_gas_station_animation_05848_bank1.inc"
};

static AnimationPackedRotation _gDryfieldGasStationAnimation05848Bank4[48] = {
#include "assets/dryfield_gas_station_animation_05848_bank4.inc"
};

static AnimationRecord _gDryfieldGasStationAnimation05848Records[95] = {
#include "assets/dryfield_gas_station_animation_05848_records.inc"
};

static u16 _gDryfieldGasStationAnimation05848Indices[20] = {
#include "assets/dryfield_gas_station_animation_05848_indices.inc"
};

static AnimationSet _gDryfieldGasStationAnimation05848 = {
    _gDryfieldGasStationAnimation05848Records,
    _gDryfieldGasStationAnimation05848Indices,
    { NULL, _gDryfieldGasStationAnimation05848Bank1, NULL, NULL, _gDryfieldGasStationAnimation05848Bank4, NULL, NULL, NULL },
};

/// Player animation-set directory borrowed by the warp-1 arrival cutscene.
///
/// Indices 0..3 select the initial pose, first cue, second cue and walk;
/// entry 4 is NULL and is not a playable clip. Playback retains this table
/// and its sets, so this overlay must stay loaded until the player changes banks.
static AnimationSet* _gDryfieldGasStationArrivalAnimationSets[5] = {
    &_gDryfieldGasStationAnimation04A70,
    &_gDryfieldGasStationAnimation055A4,
    &_gDryfieldGasStationAnimation05848,
    &_gDryfieldGasStationAnimation05210,
    NULL,
};

/// Player start, blend and final transforms of the warp-1 arrival cutscene.
///
/// Positions use room-coordinate units; rotations use 4096 angle units per turn.
/// Placement messages borrow a record synchronously. The walk uses the X/Z
/// delta from start to final; blend and final share a position but differ in yaw.
static ActorTransform _gDryfieldGasStationArrivalPlayerPlacements[3] = {
    { .pos = { 14408, 0, -2630, 0 }, .rot = { 0, 3584, 0, 0 } },
    { .pos = { 14158, 0, -2380, 0 }, .rot = { 0, 2560, 0, 0 } },
    { .pos = { 14158, 0, -2380, 0 }, .rot = { 0, 3072, 0, 0 } },
};

/// Frame waits, view selection and default-duration sentinel for arrival scripts.
enum {
    DRYFIELD_GAS_STATION_ARRIVAL_CAP_COMMAND        = 12,
    DRYFIELD_GAS_STATION_ARRIVAL_FINISH_WAIT_FRAMES = 3,
    DRYFIELD_GAS_STATION_SKIP_FADE_WAIT_FRAMES      = 8,
    DRYFIELD_GAS_STATION_SKIP_VIEW_WAIT_FRAMES      = 2,
    DRYFIELD_GAS_STATION_ARRIVAL_END_VIEW           = 2,
    DRYFIELD_GAS_STATION_SCRIPT_DEFAULT_FADE_FRAMES = 0, // Interpreter selects a seven-frame ramp
};

/// CAP-cue-driven event script for the in-room warp-1 arrival sequence.
///
/// Suppresses equipment, queues scene commands between CAP cues, restores the
/// player and ends scripted control after three countdown updates. Includes END;
/// the interpreter borrows these instructions and the published controller.
static EvsCommand _gDryfieldGasStationArrivalScript[18] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_GAS_STATION_ARRIVAL_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldGasStationSuppressPlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldGasStationSetCutsceneCommand }, { .value = DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLACE_AND_START_AUDIO }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldGasStationSetCutsceneCommand }, { .value = DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_SPAWN_FADE_IN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldGasStationSetCutsceneCommand }, { .value = DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_WALK_PLACEMENTS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldGasStationSetCutsceneCommand }, { .value = DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLAY_ANIMATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldGasStationSetCutsceneCommand }, { .value = DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLACE_AND_BLEND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldGasStationSetCutsceneCommand }, { .value = DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_RESTORE_AND_SHOW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_GAS_STATION_ARRIVAL_FINISH_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip branch that fades the arrival scene out, restores the player and returns to view 2.
///
/// Uses the interpreter's default seven-frame black ramp, eight countdown
/// updates after starting and returning the fade and two before scene cleanup. Includes
/// END and borrows the live published controller through its restore callback.
static EvsCommand _gDryfieldGasStationArrivalSkipScript[10] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = DRYFIELD_GAS_STATION_SCRIPT_DEFAULT_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_GAS_STATION_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_GAS_STATION_ARRIVAL_END_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldGasStationRestorePlayerAndShow }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_GAS_STATION_SKIP_VIEW_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_GAS_STATION_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc gDryfieldGasStationArrivalCutsceneTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _dryfieldGasStationArrivalCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _screenFadeInTask, { .value = 0 } },
};

/// Fixed cyan-glow centre in the effect coordinate's local game units.
///
/// Both glow drawers borrow this signed-halfword point for the call; its overlay
/// storage lasts for the room stay and is not changed by projection.
static SVECTOR _gDryfieldGasStationGlowLocalPoint = { 4378, -1383, -215, 0 };

WorldCollisionRoomResources gDryfieldGasStationRoomCollisionResources[1] = {
    { .grid                 = &_gDryfieldGasStationCollisionGrid,
      .viewBoundaryTriggers = _gDryfieldGasStationViewBoundaryTriggers,
      .actionTriggers       = _gDryfieldGasStationActionTriggers,
      .occluders            = NULL },
};

u8* gDryfieldGasStationRoomViewMaps[1] = {
    gViewIdentityMap,
};

WorldCoordRoomLighting gDryfieldGasStationRoomLighting[1] = {
    { &_gDryfieldGasStationRoomLights, NULL },
};

ViewCount gDryfieldGasStationRoomViewCounts[1] = { ARRAY_SIZE(gDryfieldGasStationViewCameras) };

/// Departure endpoint slots and their optional map-marker nibble indices.
enum {
    DRYFIELD_GAS_STATION_WARP_MAIN_STREET            = 2,
    DRYFIELD_GAS_STATION_WARP_GENERAL_STORE          = 3,
    DRYFIELD_GAS_STATION_MAIN_STREET_WARP_MAP_FLAG   = 488,
    DRYFIELD_GAS_STATION_GENERAL_STORE_WARP_MAP_FLAG = 489,
};

DirectionWarpEntry gDryfieldGasStationWarpEntries[3] = {
    { .player         = { { .word = 2816 }, 14408, 0, -2630 },
      .companion      = { { .word = 2816 }, 14408, 0, -1440 },
      .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
      .departureSound = DIRECTION_WARP_SOUND_NONE,
      .blockedSound   = DIRECTION_WARP_SOUND_NONE,
      .initialView    = 3,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE },
    { .player         = { { .word = 1024 }, 294, -5, -3482 },
      .companion      = { { .word = 1024 }, 294, -5, -4466 },
      .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 2),
      .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 1),
      .blockedSound   = DIRECTION_WARP_SOUND_NONE,
      .initialView    = 4,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DRYFIELD_GAS_STATION_MAIN_STREET_WARP_MAP_FLAG },
    { .player         = { { .word = 2048 }, 3039, 0, -433 },
      .companion      = { { .word = 2048 }, 4316, 0, -535 },
      .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 4),
      .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 3),
      .blockedSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x0F),
      .initialView    = 6,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DRYFIELD_GAS_STATION_GENERAL_STORE_WARP_MAP_FLAG },
};

/// Dimensions of the collision grid's X-major cell directory.
enum {
    DRYFIELD_GAS_STATION_COLLISION_CELLS_X = 8,
    DRYFIELD_GAS_STATION_COLLISION_CELLS_Z = 6,
};

/// Room-space collision normals, with 4096 representing unit length.
static SVECTOR _gDryfieldGasStationCollisionGridNormals[32] = {
#include "assets/dryfield_gas_station_collision_068E4_normals.inc"
};

/// Collision vertices in signed room-coordinate units.
static SVECTOR _gDryfieldGasStationCollisionGridVerts[126] = {
#include "assets/dryfield_gas_station_collision_068E4_verts.inc"
};

/// Indexed collision triangles and quads carrying room-local surface classes 0..4.
static WorldCollisionGridFace _gDryfieldGasStationCollisionGridFaces[63] = {
#include "assets/dryfield_gas_station_collision_068E4_faces.inc"
};

/// Signed face-index lists ending with `WORLD_COLLISION_GRID_CELL_END`.
///
/// Retains the complete authored halfword storage, including values beyond
/// each terminator; the cell directory selects the starts of the live lists.
static s16 _gDryfieldGasStationCollisionGridCells[502] = {
#include "assets/dryfield_gas_station_collision_068E4_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldGasStationCollisionGridCells` (502 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_GAS_STATION_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldGasStationCollisionGridCells[(halfwordOffset)])

/// X-major directory of cell lists, indexed by X * cellCountZ + Z.
static s16* _gDryfieldGasStationCollisionGridTable[DRYFIELD_GAS_STATION_COLLISION_CELLS_X * DRYFIELD_GAS_STATION_COLLISION_CELLS_Z] = {
#include "assets/dryfield_gas_station_collision_068E4_table.inc"
};
#undef DRYFIELD_GAS_STATION_CELL_FACE_LIST_AT_OFFSET

/// The gas station's collision mesh and 8-by-6 spatial cell directory.
///
/// Covers room X/Z from (-5300, -15000) in 4000-unit cells. Activation binds
/// the mutable view coordinate; the mesh pools and cell lists remain borrowed
/// from this overlay until the grid is replaced or cleared.
static WorldCollisionGrid _gDryfieldGasStationCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldGasStationCollisionGridNormals,
    .vertices    = _gDryfieldGasStationCollisionGridVerts,
    .faces       = _gDryfieldGasStationCollisionGridFaces,
    .cellFaceIds = _gDryfieldGasStationCollisionGridTable,
    .xBias       = 5300,
    .zBias       = 15000,
    .cellCountX  = DRYFIELD_GAS_STATION_COLLISION_CELLS_X,
    .cellCountZ  = DRYFIELD_GAS_STATION_COLLISION_CELLS_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldGasStationCollisionGridFaces),
};

ViewCamera gDryfieldGasStationViewCameras[14] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2710, 0x7530, 1000 } }, 380 },
    { { { { 1609, 0, -3766 }, { -1268, 3856, -541 }, { 3546, 1379, 1515 } }, { -7536, 2780, 4972 } }, 230 },
    { { { { 1495, 0, -3813 }, { -23, 4095, -9 }, { 3813, 25, 1495 } }, { -1316, 1337, 5204 } }, 230 },
    { { { { 1473, 0, 3821 }, { -736, 4019, 283 }, { -3750, -788, 1445 } }, { -9489, 595, 5193 } }, 230 },
    { { { { 546, 0, -4059 }, { -3221, 2492, -433 }, { 2469, 3250, 332 } }, { -0x2EA0, 3781, 2877 } }, 329 },
    { { { { 3396, 0, 2288 }, { -180, 4083, 267 }, { -2281, -322, 3386 } }, { -5346, 792, 3101 } }, 230 },
    { { { { 1107, 0, -3943 }, { -2662, 3021, -747 }, { 2908, 2765, 816 } }, { -0x3206, 1776, 5127 } }, 230 },
    { { { { 3985, 0, 945 }, { 289, 3899, -1220 }, { -900, 1254, 3793 } }, { -4447, 1449, 747 } }, 230 },
    { { { { 1679, 0, -3735 }, { 590, 4044, 265 }, { 3689, -646, 1658 } }, { -0x2D82, 940, 3440 } }, 541 },
    { { { { 3799, 0, 1530 }, { 233, 4047, -580 }, { -1512, 626, 3754 } }, { -0x3815, 1503, 2328 } }, 230 },
    { { { { 2694, 0, 3085 }, { -516, 4038, 451 }, { -3041, -685, 2656 } }, { -0x29C2, 716, 3608 } }, 230 },
    { { { { 1200, 0, 3916 }, { -1855, 3607, 568 }, { -3448, -1940, 1057 } }, { -9280, 950, 3960 } }, 230 },
    { { { { 97, 0, -4094 }, { -3605, 1942, -85 }, { 1941, 3606, 46 } }, { -0x33FE, 930, 3500 } }, 230 },
    { { { { 3432, 0, -2234 }, { -527, 3980, -810 }, { 2171, 967, 3335 } }, { -0x2DBF, 1537, 1164 } }, 230 },
};

/// Shared GPU settings for the gas station's view sprites.
enum {
    DRYFIELD_GAS_STATION_SPRITE_TEXTURE_PAGE  = getTPage(1, 0, 960, 0), // 8-bit indexed texture, blend mode 0
    DRYFIELD_GAS_STATION_SPRITE_CLUT          = getClut(0, 255),
    DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR = 128,
};

/// View 1's decoded backdrop selection and end record, with no source sprites.
static SpriteBatch _gDryfieldGasStationView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Six texture sprites sorted with the decoded backdrop of view 2.
///
/// Positions are pixels relative to the view centre; dimensions are pixels
/// and depth is the unscaled OT sorting value. RGB is neutral for direct drawing;
/// cached packets retain their own colour and sample these depths each frame.
static SpriteSource _gDryfieldGasStationView2SpriteSources[6] = {
    { DRYFIELD_GAS_STATION_SPRITE_TEXTURE_PAGE, DRYFIELD_GAS_STATION_SPRITE_CLUT, { .fields = { 8, 8 } }, -8, -32, 2362, { .fields = { 120, 152 } }, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_GAS_STATION_SPRITE_TEXTURE_PAGE, DRYFIELD_GAS_STATION_SPRITE_CLUT, { .fields = { 8, 32 } }, 0, -40, 2329, { .fields = { 120, 40 } }, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_GAS_STATION_SPRITE_TEXTURE_PAGE, DRYFIELD_GAS_STATION_SPRITE_CLUT, { .fields = { 8, 32 } }, 8, -40, 2325, { .fields = { 120, 72 } }, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_GAS_STATION_SPRITE_TEXTURE_PAGE, DRYFIELD_GAS_STATION_SPRITE_CLUT, { .fields = { 8, 40 } }, 16, -40, 2325, { .fields = { 120, 0 } }, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_GAS_STATION_SPRITE_TEXTURE_PAGE, DRYFIELD_GAS_STATION_SPRITE_CLUT, { .fields = { 16, 24 } }, 24, -40, 2325, { .fields = { 112, 128 } }, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_GAS_STATION_SPRITE_TEXTURE_PAGE, DRYFIELD_GAS_STATION_SPRITE_CLUT, { .fields = { 48, 24 } }, 40, -40, 2325, { .fields = { 80, 104 } }, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 2's decoded backdrop followed by all six texture sprites and an end record.
static SpriteBatch _gDryfieldGasStationView2SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldGasStationView2SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Six texture sprites sorted with the decoded backdrop of view 3.
///
/// Positions are pixels relative to the view centre; dimensions are pixels
/// and depth is the unscaled OT sorting value. RGB is neutral for direct drawing;
/// cached packets retain their own colour and sample these depths each frame.
static SpriteSource _gDryfieldGasStationView3SpriteSources[6] = {
    { DRYFIELD_GAS_STATION_SPRITE_TEXTURE_PAGE, DRYFIELD_GAS_STATION_SPRITE_CLUT, { .fields = { 16, 24 } }, 24, 0, 3701, { .fields = { 112, 152 } }, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_GAS_STATION_SPRITE_TEXTURE_PAGE, DRYFIELD_GAS_STATION_SPRITE_CLUT, { .fields = { 40, 32 } }, 40, -8, 3300, { .fields = { 88, 120 } }, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_GAS_STATION_SPRITE_TEXTURE_PAGE, DRYFIELD_GAS_STATION_SPRITE_CLUT, { .fields = { 24, 24 } }, 80, 0, 3424, { .fields = { 104, 176 } }, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_GAS_STATION_SPRITE_TEXTURE_PAGE, DRYFIELD_GAS_STATION_SPRITE_CLUT, { .fields = { 16, 40 } }, -56, 0, 2250, { .fields = { 112, 40 } }, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_GAS_STATION_SPRITE_TEXTURE_PAGE, DRYFIELD_GAS_STATION_SPRITE_CLUT, { .fields = { 8, 40 } }, -40, 0, 2250, { .fields = { 120, 0 } }, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_GAS_STATION_SPRITE_TEXTURE_PAGE, DRYFIELD_GAS_STATION_SPRITE_CLUT, { .fields = { 8, 40 } }, -32, 0, 2342, { .fields = { 120, 80 } }, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, DRYFIELD_GAS_STATION_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 3's decoded backdrop followed by two three-sprite ranges and an end record.
///
/// The first range retains its authored `SpriteBatch.field_6` bytes { 1, 0 }; their role
/// is unproven. Both ranges participate in cached packet processing.
static SpriteBatch _gDryfieldGasStationView3SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's decoded backdrop selection and end record, with no source sprites.
static SpriteBatch _gDryfieldGasStationView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's decoded backdrop selection and end record, with no source sprites.
static SpriteBatch _gDryfieldGasStationView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's decoded backdrop selection and end record, with no source sprites.
static SpriteBatch _gDryfieldGasStationView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's decoded backdrop selection and end record, with no source sprites.
static SpriteBatch _gDryfieldGasStationView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8's decoded backdrop selection and end record, with no source sprites.
static SpriteBatch _gDryfieldGasStationView8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 9's decoded backdrop selection and end record, with no source sprites.
static SpriteBatch _gDryfieldGasStationView9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 10's decoded backdrop selection and end record, with no source sprites.
static SpriteBatch _gDryfieldGasStationView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 11's decoded backdrop selection and end record, with no source sprites.
static SpriteBatch _gDryfieldGasStationView11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 12's decoded backdrop selection and end record, with no source sprites.
static SpriteBatch _gDryfieldGasStationView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 13's decoded backdrop selection and end record, with no source sprites.
static SpriteBatch _gDryfieldGasStationView13SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 14's decoded backdrop selection and end record, with no source sprites.
static SpriteBatch _gDryfieldGasStationView14SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldGasStationSpriteViews[14] = {
    { { .empty = _gDryfieldGasStationView1SpriteBatches }, _gDryfieldGasStationView1SpriteBatches, NULL },
    { { .elements = _gDryfieldGasStationView2SpriteSources }, _gDryfieldGasStationView2SpriteBatches, NULL },
    { { .elements = _gDryfieldGasStationView3SpriteSources }, _gDryfieldGasStationView3SpriteBatches, NULL },
    { { .empty = _gDryfieldGasStationView4SpriteBatches }, _gDryfieldGasStationView4SpriteBatches, NULL },
    { { .empty = _gDryfieldGasStationView5SpriteBatches }, _gDryfieldGasStationView5SpriteBatches, NULL },
    { { .empty = _gDryfieldGasStationView6SpriteBatches }, _gDryfieldGasStationView6SpriteBatches, NULL },
    { { .empty = _gDryfieldGasStationView7SpriteBatches }, _gDryfieldGasStationView7SpriteBatches, NULL },
    { { .empty = _gDryfieldGasStationView8SpriteBatches }, _gDryfieldGasStationView8SpriteBatches, NULL },
    { { .empty = _gDryfieldGasStationView9SpriteBatches }, _gDryfieldGasStationView9SpriteBatches, NULL },
    { { .empty = _gDryfieldGasStationView10SpriteBatches }, _gDryfieldGasStationView10SpriteBatches, NULL },
    { { .empty = _gDryfieldGasStationView11SpriteBatches }, _gDryfieldGasStationView11SpriteBatches, NULL },
    { { .empty = _gDryfieldGasStationView12SpriteBatches }, _gDryfieldGasStationView12SpriteBatches, NULL },
    { { .empty = _gDryfieldGasStationView13SpriteBatches }, _gDryfieldGasStationView13SpriteBatches, NULL },
    { { .empty = _gDryfieldGasStationView14SpriteBatches }, _gDryfieldGasStationView14SpriteBatches, NULL },
};

/// Eleven directed view-boundary quads linking room views 2, 3, 4, 5 and 6.
///
/// The final live record carries LAST. Setup mutates list links, view coordinate
/// and enable/hit state; keep this overlay loaded until the list is unlinked.
/// Origins, relative corners and radii use game units; normals use 4096 per unit.
static WorldCollisionTrigger _gDryfieldGasStationViewBoundaryTriggers[11] = {
    { NULL, NULL, NULL, { 5791, -2544, -3505, 0 }, { { 147, -3568, 2869, 0 }, { -146, -3568, -2868, 0 }, { 147, 3568, 2869, 0 }, { -146, 3568, -2868, 0 } }, { -4101, 0, 209, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5503, -2544, -3521, 0 }, { { -146, -3568, -2868, 0 }, { 147, -3568, 2869, 0 }, { -146, 3568, -2868, 0 }, { 147, 3568, 2869, 0 } }, { 4100, 0, -210, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4319, -2544, -1009, 0 }, { { -210, -3568, -804, 0 }, { 211, -3568, 805, 0 }, { -210, 3568, -804, 0 }, { 211, 3568, 805, 0 } }, { 3963, 0, -1038, 0 }, { 0, 0, 4096, 0 }, 3656, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2752, -2528, -1264, 0 }, { { -1410, -3552, 524, 0 }, { 1411, -3552, -523, 0 }, { -1410, 3552, 524, 0 }, { 1411, 3552, -523, 0 } }, { -1432, 0, -3857, 0 }, { 0, 0, 4096, 0 }, 3857, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2623, -2608, -1345, 0 }, { { 1716, -3632, -661, 0 }, { -1716, -3632, 662, 0 }, { 1716, 3632, -661, 0 }, { -1716, 3632, 662, 0 } }, { 1474, 0, 3824, 0 }, { 0, 0, 4096, 0 }, 4063, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4431, -2624, -1297, 0 }, { { 283, -3648, 1064, 0 }, { -283, -3648, -1064, 0 }, { 283, 3648, 1064, 0 }, { -283, 3648, -1064, 0 } }, { -3966, 0, 1054, 0 }, { 0, 0, 4096, 0 }, 3805, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x28FF, -2608, -1952, 0 }, { { -933, -3632, -2562, 0 }, { 911, -3632, 2537, 0 }, { -933, 3632, -2562, 0 }, { 911, 3632, 2537, 0 } }, { 3855, 0, -1395, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A06, -2592, -1961, 0 }, { { 875, -3616, 2397, 0 }, { -875, -3616, -2396, 0 }, { 875, 3616, 2397, 0 }, { -875, 3616, -2396, 0 } }, { -3858, 0, 1407, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x361F, -2624, -2482, 0 }, { { 857, -3616, -1637, 0 }, { -856, -3616, 1638, 0 }, { 857, 3616, -1637, 0 }, { -856, 3616, 1638, 0 } }, { 3633, 0, 1900, 0 }, { 0, 0, 4096, 0 }, 4055, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x29DD, -2624, -5092, 0 }, { { -867, -3616, 888, 0 }, { 867, -3616, -888, 0 }, { -867, 3616, 888, 0 }, { 867, 3616, -888, 0 } }, { -2937, 0, -2867, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x28AD, -2688, -5043, 0 }, { { 899, -3616, -888, 0 }, { -899, -3616, 888, 0 }, { 899, 3616, -888, 0 }, { -899, 3616, 888, 0 } }, { 2895, 0, 2931, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Two exits and eight CAP/room interactions tested against room-local quads.
///
/// The exits use departure warp 3 for General Store and warp 2 for Main Street,
/// both requesting destination arrival warp 1. Other entries select CAP commands
/// 5, 6, 9, 10, 15 and 26, or send room command 1 through the room-message sentinel.
/// Requires those loaded CAP entries; the list includes its LAST-marked record.
/// Setup mutates links, the view coordinate and enable/hit state. Geometry uses
/// game units and normals use 4096 per unit; storage lives until room unlinking.
static WorldCollisionTrigger _gDryfieldGasStationActionTriggers[10] = {
    { NULL, NULL, NULL, { 2657, -108, -331, 0 }, { { -1024, 0, -384, 0 }, { 1024, 0, -384, 0 }, { -1024, 0, 384, 0 }, { 1024, 0, 384, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_GENERAL_STORE, (DRYFIELD_GAS_STATION_WARP_GENERAL_STORE << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 192, -80, -3328, 0 }, { { 384, 0, -1440, 0 }, { 384, 0, 1024, 0 }, { -384, 0, -1440, 0 }, { -384, 0, 1024, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1487, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MAIN_STREET, (DRYFIELD_GAS_STATION_WARP_MAIN_STREET << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3430, -96, -224, 0 }, { { -1776, 0, -384, 0 }, { 1776, 0, -384, 0 }, { -1776, 0, 384, 0 }, { 1776, 0, 384, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1814, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x36EF, -96, -4817, 0 }, { { -1877, 0, -131, 0 }, { -397, 0, -2069, 0 }, { -1650, 0, 1270, 0 }, { 694, 0, -764, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, -4096, 0 }, 2095, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6960, -64, -1104, 0 }, { { -1407, 0, -624, 0 }, { 1408, 0, -624, 0 }, { -1407, 0, 624, 0 }, { 1408, 0, 624, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1536, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4448, -96, 256, 0 }, { { -880, 0, -1408, 0 }, { 720, 0, -1408, 0 }, { -880, 0, -320, 0 }, { 720, 0, -320, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1659, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x36C0, -64, -3488, 0 }, { { -501, 0, -973, 0 }, { 1016, 0, 403, 0 }, { -1497, 0, -20, 0 }, { 20, 0, 1356, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4120, -64, 0, 0 }, { { -736, 0, -624, 0 }, { 736, 0, -624, 0 }, { -736, 0, 624, 0 }, { 736, 0, 624, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 964, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9264, -64, -1184, 0 }, { { -911, 0, -624, 0 }, { 912, 0, -624, 0 }, { -911, 0, 624, 0 }, { 912, 0, 624, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_CAP, 26, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, -528, 0 }, { { -544, 0, -576, 0 }, { 544, 0, -576, 0 }, { -544, 0, 576, 0 }, { 544, 0, 576, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 791, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Placement-resource keys, file numbers and Mad Chaser spawn modes of these layouts.
enum {
    DRYFIELD_GAS_STATION_MAD_CHASER_ENTRY_ID       = 44,
    DRYFIELD_GAS_STATION_MAD_CHASER_FILE_NUMBER    = 44,
    DRYFIELD_GAS_STATION_MAD_CHASER_SPAWN_LURK     = 0,
    DRYFIELD_GAS_STATION_MAD_CHASER_SPAWN_DANGLE   = 1,
    DRYFIELD_GAS_STATION_DESERT_CHASER_ENTRY_ID    = 1,
    DRYFIELD_GAS_STATION_DESERT_CHASER_FILE_NUMBER = 1,
};

/// Empty resource list paired with the map-owned empty placement list in variant 1.
static AreaResource _gDryfieldGasStationVariant1Resources[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 9's resource key 44: file 104400 and the ordinary Mad Chaser recipe.
///
/// Includes END and borrows the loaded actor package's descriptor table.
static AreaResource _gDryfieldGasStationVariant9MadChaserResources[2] = {
    { .entryId        = DRYFIELD_GAS_STATION_MAD_CHASER_ENTRY_ID,
      .fileNumber     = DRYFIELD_GAS_STATION_MAD_CHASER_FILE_NUMBER,
      .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR,
      .taskIndex      = ACTOR_04400_MAD_CHASER_TASK_ORDINARY,
      .taskTable      = gActor04400MadChaserTaskDescs },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 9's three lurking Mad Chasers and one dangling Mad Chaser, followed by END.
///
/// Uses resource key 44, base images with no page offset and a two-row CLUT offset.
/// Positions are world game units and yaw is 4096 units per turn. Spawned enemies
/// retain these placement pointers, so this overlay must outlive them.
static AreaPlacement _gDryfieldGasStationVariant9MadChaserPlacements[5] = {
    { .entryId = DRYFIELD_GAS_STATION_MAD_CHASER_ENTRY_ID, .mode = DRYFIELD_GAS_STATION_MAD_CHASER_SPAWN_LURK, .x = 4226, .y = 0, .z = -4480, .yaw = 0, .clutRowOffset = 2 },
    { .entryId = DRYFIELD_GAS_STATION_MAD_CHASER_ENTRY_ID, .mode = DRYFIELD_GAS_STATION_MAD_CHASER_SPAWN_DANGLE, .x = 472, .y = -4624, .z = -2512, .yaw = 1024, .clutRowOffset = 2 },
    { .entryId = DRYFIELD_GAS_STATION_MAD_CHASER_ENTRY_ID, .mode = DRYFIELD_GAS_STATION_MAD_CHASER_SPAWN_LURK, .x = 7277, .y = 0, .z = -3553, .yaw = 512, .clutRowOffset = 2 },
    { .entryId = DRYFIELD_GAS_STATION_MAD_CHASER_ENTRY_ID, .mode = DRYFIELD_GAS_STATION_MAD_CHASER_SPAWN_LURK, .x = 6255, .y = 0, .z = -5322, .yaw = -1024, .clutRowOffset = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 10's resource key 1: file 400100 and the Desert Chaser's single recipe.
///
/// Task index 0 addresses the scalar descriptor; includes END. The actor package
/// owns the borrowed descriptor and must be loaded before spawning.
static AreaResource _gDryfieldGasStationVariant10DesertChaserResources[2] = {
    { .entryId        = DRYFIELD_GAS_STATION_DESERT_CHASER_ENTRY_ID,
      .fileNumber     = DRYFIELD_GAS_STATION_DESERT_CHASER_FILE_NUMBER,
      .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40,
      .taskIndex      = 0,
      .taskTable      = &gActor00100DesertChaserTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 10's approaching Desert Chaser with default tuning, followed by END.
///
/// Uses resource key 1, base images with no page offset and a two-row CLUT offset.
/// Positions are world game units and yaw is 4096 units per turn. The enemy retains
/// the placement pointer, so this overlay must outlive it.
static AreaPlacement _gDryfieldGasStationVariant10DesertChaserPlacements[2] = {
    { .entryId = DRYFIELD_GAS_STATION_DESERT_CHASER_ENTRY_ID, .variant = 0, .mode = 0, .x = 6255, .y = 0, .z = -5322, .yaw = -1024, .clutRowOffset = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldGasStationPlacementVariants[12] = {
    [1]  = { .placements = gMapDryfieldGasStationVariant1Placements, .resources = _gDryfieldGasStationVariant1Resources },
    [9]  = { .placements = _gDryfieldGasStationVariant9MadChaserPlacements, .resources = _gDryfieldGasStationVariant9MadChaserResources },
    [10] = { .placements = _gDryfieldGasStationVariant10DesertChaserPlacements, .resources = _gDryfieldGasStationVariant10DesertChaserResources },
};

/// The gas station's two directional model lights, contributing in every room view.
///
/// Local translations supply direction vectors, normalized when shading;
/// RGB intensities have 12 fractional bits (`ONE` is 1.0). The loaded room
/// overlay owns these records. Coordinate updates attach the view parent and
/// compose the transforms; shading overwrites attenuation, so they stay writable.
static WorldCoordLight _gDryfieldGasStationDirectionalLights[] = {
    {
        .transform = {
            .lighting = {
                .composeStamp = GRAPHICS_COORD_DIRTY,
                .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 2000, -4000, -2000 } },
                .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                .unknown_46   = { 0, 0, 0, 0 },
                .attenuation  = 0,
                .parent       = NULL,
            },
        },
        .color      = { .r = 0x2856, .g = 8167, .b = 7585 },
        .unknown_56 = { 0, 0 },
    },
    {
        .transform = {
            .lighting = {
                .composeStamp = GRAPHICS_COORD_DIRTY,
                .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -2000, 2000, -1000 } },
                .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                .unknown_46   = { 0, 0, 0, 0 },
                .attenuation  = 0,
                .parent       = NULL,
            },
        },
        .color      = { .r = 2457, .g = 1966, .b = 2048 },
        .unknown_56 = { 0, 0 },
    },
};

/// The gas station's two mutable directional lights, with no point or cone lights.
///
/// The room lighting descriptor borrows this single collection. Coordinates
/// and attenuation are updated during play; all storage lives with the overlay.
static WorldCoordRoomLights _gDryfieldGasStationRoomLights = {
    .directionalLightCount = ARRAY_SIZE(_gDryfieldGasStationDirectionalLights),
    .directionalLights     = _gDryfieldGasStationDirectionalLights,
    .pointLightCount       = 0,
    .pointLights           = NULL,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Cue-2 footstep requests in the currently loaded type-1 sound bank.
enum {
    DRYFIELD_GAS_STATION_SURFACE_CLASS4_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x49,
    DRYFIELD_GAS_STATION_SURFACE_CLASS4_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x4B,
    DRYFIELD_GAS_STATION_SURFACE_CLASS2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x1D,
    DRYFIELD_GAS_STATION_SURFACE_CLASS2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x1F,
    DRYFIELD_GAS_STATION_SURFACE_CLASS3_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    DRYFIELD_GAS_STATION_SURFACE_CLASS3_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
};

/// Walking, running and scripted-jump footstep cue bases for surface class 4.
///
/// Walking and scripted jumps share entry 73; running uses 75. Cue 1 adds
/// one and companion playback adds 100. Requires the current sound bank;
/// the surface policy borrows this record for the room overlay's lifetime.
static WorldCollisionFootstepSounds _gDryfieldGasStationSurfaceClass4FootstepSounds = {
    .walk         = DRYFIELD_GAS_STATION_SURFACE_CLASS4_WALK_SOUND,
    .run          = DRYFIELD_GAS_STATION_SURFACE_CLASS4_RUN_SOUND,
    .scriptedJump = DRYFIELD_GAS_STATION_SURFACE_CLASS4_WALK_SOUND,
};

/// Walking, running and scripted-jump footstep cue bases for surface class 2.
///
/// Walking and scripted jumps share entry 29; running uses 31. Cue 1 adds
/// one and companion playback adds 100. Requires the current sound bank;
/// the surface policy borrows this record for the room overlay's lifetime.
static WorldCollisionFootstepSounds _gDryfieldGasStationSurfaceClass2FootstepSounds = {
    .walk         = DRYFIELD_GAS_STATION_SURFACE_CLASS2_WALK_SOUND,
    .run          = DRYFIELD_GAS_STATION_SURFACE_CLASS2_RUN_SOUND,
    .scriptedJump = DRYFIELD_GAS_STATION_SURFACE_CLASS2_WALK_SOUND,
};

/// Walking, running and scripted-jump footstep cue bases for surface class 3.
///
/// Walking and scripted jumps share entry 21; running uses 23. Cue 1 adds
/// one and companion playback adds 100. Requires the current sound bank;
/// the surface policy borrows this record for the room overlay's lifetime.
static WorldCollisionFootstepSounds _gDryfieldGasStationSurfaceClass3FootstepSounds = {
    .walk         = DRYFIELD_GAS_STATION_SURFACE_CLASS3_WALK_SOUND,
    .run          = DRYFIELD_GAS_STATION_SURFACE_CLASS3_RUN_SOUND,
    .scriptedJump = DRYFIELD_GAS_STATION_SURFACE_CLASS3_WALK_SOUND,
};

/// Classes 0 and 5..7: blocked probes, enabled weapon impacts and no footstep cues.
///
/// Applies pushback. The authored first byte remains zero; its role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldGasStationBlockingSilentSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 4: blocked probes, enabled weapon impacts and class-4 footstep cues.
///
/// Applies pushback. The authored first byte remains zero; its role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldGasStationSurfaceClass4Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldGasStationSurfaceClass4FootstepSounds,
};

/// Class 2: blocked probes, enabled weapon impacts and class-2 footstep cues.
///
/// Applies pushback. The authored first byte remains zero; its role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldGasStationSurfaceClass2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldGasStationSurfaceClass2FootstepSounds,
};

/// Class 3: blocked probes, enabled weapon impacts and class-3 footstep cues.
///
/// Applies pushback. The authored first byte remains zero; its role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldGasStationSurfaceClass3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldGasStationSurfaceClass3FootstepSounds,
};

/// Class 1: probe passage, ignored weapon impacts and no footstep cues.
///
/// Applies pushback independently of probe passage. The authored first byte
/// remains zero; its role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldGasStationPassThroughSilentSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gDryfieldGasStationSurfacePropertiesByClass[8] = {
    &_gDryfieldGasStationBlockingSilentSurface,
    &_gDryfieldGasStationPassThroughSilentSurface,
    &_gDryfieldGasStationSurfaceClass2Properties,
    &_gDryfieldGasStationSurfaceClass3Properties,
    &_gDryfieldGasStationSurfaceClass4Properties,
    &_gDryfieldGasStationBlockingSilentSurface,
    &_gDryfieldGasStationBlockingSilentSurface,
    &_gDryfieldGasStationBlockingSilentSurface,
};

Task* gDryfieldGasStationArrivalTask = NULL;

RoomCutsceneRec gDryfieldGasStationRoomCutscene;

/// Executes a pending gas-station scene command, retaining multi-update commands until completion.
///
/// Requires initialized work, its live player and the published scene task for
/// restoration. Placement and animation records are borrowed synchronously;
/// installed animation banks must remain loaded. The walk sends 31 room-space
/// X/Z displacements, each one thirtieth of the first-to-third placement delta,
/// with collision requests clear. Ordinary commands clear after one update;
/// walking and fade setup advance `commandStep` and return until completion.
static void _dryfieldGasStationExecuteCutsceneCommand(Task* task)
{
    enum { DRYFIELD_GAS_STATION_COMMAND_STEP_BEGIN     = 0,
           DRYFIELD_GAS_STATION_COMMAND_STEP_WALK      = 1,
           DRYFIELD_GAS_STATION_FADE_STEP_WAIT         = 1,
           DRYFIELD_GAS_STATION_FADE_STEP_SHOW         = 2,
           DRYFIELD_GAS_STATION_PLACEMENT_START        = 0,
           DRYFIELD_GAS_STATION_PLACEMENT_BLEND        = 1,
           DRYFIELD_GAS_STATION_PLACEMENT_END          = 2,
           DRYFIELD_GAS_STATION_CLIP_INITIAL           = 0,
           DRYFIELD_GAS_STATION_CLIP_FIRST_CUE         = 1,
           DRYFIELD_GAS_STATION_CLIP_SECOND_CUE        = 2,
           DRYFIELD_GAS_STATION_CLIP_WALK              = 3,
           DRYFIELD_GAS_STATION_CUTSCENE_RATE          = ANIMATION_RATE_ONE / 2,
           DRYFIELD_GAS_STATION_WALK_DELTA_DIVISOR     = 30,
           DRYFIELD_GAS_STATION_WALK_UPDATE_COUNT      = 31,
           DRYFIELD_GAS_STATION_PLACEMENT_BLEND_FRAMES = 30,
           DRYFIELD_GAS_STATION_WALK_END_BLEND_FRAMES  = 15,
           DRYFIELD_GAS_STATION_FADE_IN_FRAMES         = 30,
           DRYFIELD_GAS_STATION_LOOP_FADE_TICKS        = 60 };

    _DryfieldGasStationCutsceneWork* work;
    _DryfieldGasStationCutsceneWork* animationWork;
    _DryfieldGasStationCutsceneWork* publishedWork;
    Task*                            publishedTask;
    union {
        AnimationPlayRequest animation;
        GameActorMoveBy      move;
    } payload;
    AnimationPlayRequest  blendAnimation;
    AnimationPlayRequest* blendRequest;
    u16                   commandStep;

    /// Installs a loaded scene clip after reloading the source task's player.
    ///
    /// Evaluates sourceTask once; clip, blendMode and duration once only when
    /// the player exists. Captures this function's animationWork local and
    /// writable payload.animation; invoke only in a braced block.
    /// Dispatch borrows the request synchronously and playback retains the bank.
    /// Blend duration is in normal-rate frames; world collision is disabled.
#define DRYFIELD_GAS_STATION_INSTALL_COMMAND_ANIMATION(sourceTask, clip, blendMode, duration)                            \
    animationWork = (sourceTask)->work;                                                                                  \
    if (animationWork->player != NULL) {                                                                                 \
        payload.animation.source.sets          = _gDryfieldGasStationArrivalAnimationSets;                               \
        payload.animation.animationId          = (clip);                                                                 \
        payload.animation.blend                = (blendMode);                                                            \
        payload.animation.blendFrames          = (duration);                                                             \
        payload.animation.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                      \
        TASK_MESSAGE_DISPATCH_POINTER(animationWork->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &payload.animation, 0); \
    }

    work = task->work;
    switch (work->command) {
        case DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_NONE:
            break;
        case DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLACE_AND_START_AUDIO:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldGasStationArrivalPlayerPlacements[DRYFIELD_GAS_STATION_PLACEMENT_START], 0);
            sndEvtRequestScriptStart(SOUND_GAS_STATION_CUTSCENE_LOOP, 0, 0);
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x12), 0, 0);
            break;
        case DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLAY_ANIMATION:
            DRYFIELD_GAS_STATION_INSTALL_COMMAND_ANIMATION(task, DRYFIELD_GAS_STATION_CLIP_FIRST_CUE, ANIMATION_BLEND_RESET, 0);
            taskMessageDispatch(work->player, ANIMATION_MESSAGE_SET_RATE, DRYFIELD_GAS_STATION_CUTSCENE_RATE, 0);
            break;
        case DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_PLACE_AND_BLEND:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x13), 0, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldGasStationArrivalPlayerPlacements[DRYFIELD_GAS_STATION_PLACEMENT_BLEND], 0);
            DRYFIELD_GAS_STATION_INSTALL_COMMAND_ANIMATION(task, DRYFIELD_GAS_STATION_CLIP_SECOND_CUE, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_GAS_STATION_PLACEMENT_BLEND_FRAMES);
            break;
        case DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_WALK_PLACEMENTS:
            commandStep = work->commandStep;
            switch (commandStep) {
                case DRYFIELD_GAS_STATION_COMMAND_STEP_BEGIN:
                    DRYFIELD_GAS_STATION_INSTALL_COMMAND_ANIMATION(task, DRYFIELD_GAS_STATION_CLIP_WALK, ANIMATION_BLEND_RESET, 0);
                    taskMessageDispatch(work->player, ANIMATION_MESSAGE_SET_RATE, DRYFIELD_GAS_STATION_CUTSCENE_RATE, 0);
                    // Use walking speed with the scripted walk clip.
                    taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_RUN_MOVEMENT, false, 0);
                    work->walkFrames = 0;
                    work->commandStep++;
                    return;
                case DRYFIELD_GAS_STATION_COMMAND_STEP_WALK:
                    // The original leaves move.keepControl unwritten in this reused payload.
                    payload.move.displacement.vx   = (_gDryfieldGasStationArrivalPlayerPlacements[DRYFIELD_GAS_STATION_PLACEMENT_END].pos.vx - _gDryfieldGasStationArrivalPlayerPlacements[DRYFIELD_GAS_STATION_PLACEMENT_START].pos.vx) / DRYFIELD_GAS_STATION_WALK_DELTA_DIVISOR;
                    payload.move.displacement.vy   = 0;
                    payload.move.displacement.vz   = (_gDryfieldGasStationArrivalPlayerPlacements[DRYFIELD_GAS_STATION_PLACEMENT_END].pos.vz - _gDryfieldGasStationArrivalPlayerPlacements[DRYFIELD_GAS_STATION_PLACEMENT_START].pos.vz) / DRYFIELD_GAS_STATION_WALK_DELTA_DIVISOR;
                    payload.move.collisionRequests = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_MOVE_BY, &payload.move, 0);
                    work->walkFrames++;
                    if (work->walkFrames < DRYFIELD_GAS_STATION_WALK_UPDATE_COUNT) {
                        return;
                    }
                    // Take the blend request's address before reloading the player.
                    blendRequest  = &blendAnimation;
                    animationWork = task->work;
                    if (animationWork->player != NULL) {
                        blendAnimation.source.sets          = _gDryfieldGasStationArrivalAnimationSets;
                        blendAnimation.animationId          = DRYFIELD_GAS_STATION_CLIP_INITIAL;
                        blendRequest->blend                 = commandStep;
                        blendRequest->blendFrames           = DRYFIELD_GAS_STATION_WALK_END_BLEND_FRAMES;
                        blendAnimation.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(animationWork->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, blendRequest, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_RESTORE_AND_SHOW:
            publishedTask = _gDryfieldGasStationArrivalCutsceneTask;
            publishedWork = publishedTask->work;
            if (publishedWork->playerEffectsSuppressed != 0) {
                playerActorRestoreEquipment();
                publishedWork->playerEffectsSuppressed = 0;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            }
            TASK_MESSAGE_DISPATCH_POINTER(publishedWork->player, GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldGasStationArrivalPlayerPlacements[DRYFIELD_GAS_STATION_PLACEMENT_END], 0);
            DRYFIELD_GAS_STATION_INSTALL_COMMAND_ANIMATION(publishedTask, DRYFIELD_GAS_STATION_CLIP_INITIAL, ANIMATION_BLEND_RESET, 0);
            sndEvtRequestScriptStop(SOUND_GAS_STATION_CUTSCENE_LOOP, DRYFIELD_GAS_STATION_LOOP_FADE_TICKS);
            SetDispMask(1);
            break;
        case DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_SPAWN_FADE_IN:
            switch (work->commandStep) {
                case DRYFIELD_GAS_STATION_COMMAND_STEP_BEGIN:
                    taskSpawnFromTable(gDryfieldGasStationArrivalCutsceneTaskDescs, DRYFIELD_GAS_STATION_CUTSCENE_TASK_FADE_IN, DRYFIELD_GAS_STATION_FADE_IN_FRAMES, 0);
                    // The spawn and the one-frame wait share this update.
                case DRYFIELD_GAS_STATION_FADE_STEP_WAIT:
                    work->commandStep++;
                    return;
                case DRYFIELD_GAS_STATION_FADE_STEP_SHOW:
                    SetDispMask(1);
                    break;
            }
            break;
    }
#undef DRYFIELD_GAS_STATION_INSTALL_COMMAND_ANIMATION
    work->command = DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_NONE;
}

/// Reinstalls the arrival cutscene's initial player animation when a player is present.
///
/// Requires live allocated cutscene work even when its player pointer is NULL.
/// Reloads that work for each call. The stack request is consumed synchronously;
/// playback borrows the loaded animation sets, with collision disabled and no
/// blending. No request pointer survives dispatch.
static inline void _dryfieldGasStationInstallInitialPlayerAnimation(Task* task)
{
    enum { INITIAL_PLAYER_CLIP = 0 };
    _DryfieldGasStationCutsceneWork* currentWork = task->work;
    AnimationPlayRequest             animationRequest;

    if (currentWork->player != NULL) {
        animationRequest.source.sets          = _gDryfieldGasStationArrivalAnimationSets;
        animationRequest.animationId          = INITIAL_PLAYER_CLIP;
        animationRequest.blend                = ANIMATION_BLEND_RESET;
        animationRequest.blendFrames          = 0;
        animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        TASK_MESSAGE_DISPATCH_POINTER(currentWork->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &animationRequest, 0);
    }
}

/// Runs and publishes the gas-station arrival's in-room cutscene controller.
///
/// Waits for attachment-wheel and display transitions before allocating and
/// zeroing its owned work, then captures the player and starts the main/skip
/// scripts. Script callbacks require this published task to remain live.
/// Pending commands run while the event script is active; completion requests
/// removal. The retained allocation-failure path kills but continues into a
/// work reload, so safe startup requires allocation to succeed.
static void _dryfieldGasStationArrivalCutsceneTask(Task* task)
{
    enum {
        DRYFIELD_GAS_STATION_ARRIVAL_CUTSCENE_INIT = 0,
        DRYFIELD_GAS_STATION_ARRIVAL_CUTSCENE_RUN  = 1,
    };

    _DryfieldGasStationCutsceneWork* work;

    switch (task->state) {
        case DRYFIELD_GAS_STATION_ARRIVAL_CUTSCENE_INIT:
            if ((gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                work       = memMalloc(sizeof(*work), false);
                task->work = work;
                if (work == NULL) {
                    taskKill(task);
                } else {
                    memFillBytes(work, 0, sizeof(*work));
                    work->player                            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    _gDryfieldGasStationArrivalCutsceneTask = task;
                }
                _dryfieldGasStationInstallInitialPlayerAnimation(task);
                gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, EVENT_FLAG_OBJECTIVE_DRYFIELD_ARRIVED);
                evsStartScriptWithSkip(_gDryfieldGasStationArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                       _gDryfieldGasStationArrivalSkipScript);
                task->state = task->state + 1;
                return;
            }
            return;

        case DRYFIELD_GAS_STATION_ARRIVAL_CUTSCENE_RUN:
            if (gGameSession->eventState == 0) {
                taskRequestKill(task, 0);
                return;
            }
            _dryfieldGasStationExecuteCutsceneCommand(task);
            break;
    }
}

/// Removes the player's equipment models and persistent weapon effect once per scene.
///
/// Requires the published gas-station cutscene task and its live work. Sets
/// the restoration latch before removal, even if the removal finds no actor work.
/// The scene's restore command or skip callback rebuilds the equipment later.
static void _dryfieldGasStationSuppressPlayerEquipment(void)
{
    enum {
        DRYFIELD_GAS_STATION_EQUIPMENT_RESTORE_NOT_PENDING = 0,
        DRYFIELD_GAS_STATION_EQUIPMENT_RESTORE_PENDING     = 1,
    };

    _DryfieldGasStationCutsceneWork* work = _gDryfieldGasStationArrivalCutsceneTask->work;
    if (work->playerEffectsSuppressed == DRYFIELD_GAS_STATION_EQUIPMENT_RESTORE_NOT_PENDING) {
        work->playerEffectsSuppressed = DRYFIELD_GAS_STATION_EQUIPMENT_RESTORE_PENDING;
        playerActorRemoveEquipment();
    }
}

#undef SCREEN_FADE_IN_TASK
/// Binds the fade-in fragment to the cutscene table's private task callback.
#define SCREEN_FADE_IN_TASK _screenFadeInTask
#include "../../shared/screen/screen_fade_in.inc.c"
#undef SCREEN_FADE_IN_TASK

/// Restores the player at the arrival cutscene's final placement and shows the display.
///
/// Requires the published cutscene and live player. Restores suppressed
/// equipment once, retaining scripted control, then places the player and
/// reinstalls the initial animation when the reloaded player is non-NULL.
/// Stops the cutscene loop over 60 sound ticks and enables presentation.
/// The placement and animation request are consumed synchronously.
static void _dryfieldGasStationRestorePlayerAndShow(void)
{
    enum { END_PLACEMENT   = 2,
           LOOP_FADE_TICKS = 60 };
    Task*                            task;
    _DryfieldGasStationCutsceneWork* work;

    task = _gDryfieldGasStationArrivalCutsceneTask;
    work = task->work;
    if (work->playerEffectsSuppressed != 0) {
        playerActorRestoreEquipment();
        work->playerEffectsSuppressed = 0;
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    }
    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldGasStationArrivalPlayerPlacements[END_PLACEMENT], 0);
    _dryfieldGasStationInstallInitialPlayerAnimation(task);
    sndEvtRequestScriptStop(SOUND_GAS_STATION_CUTSCENE_LOOP, LOOP_FADE_TICKS);
    SetDispMask(1);
}

/// Replaces the pending gas-station cutscene command and restarts its first step.
///
/// Requires the published cutscene task and live work. The script supplies the
/// low signed halfword; storage preserves its bits as u16. Use the local
/// `DRYFIELD_GAS_STATION_CUTSCENE_COMMAND_*` values (0 none, 1..6 scene actions).
/// Execution is deferred to a later task update; a new command replaces any
/// unfinished one. Other counters and the captured player stay unchanged.
static void _dryfieldGasStationSetCutsceneCommand(s16 command)
{
    _DryfieldGasStationCutsceneWork* work = _gDryfieldGasStationArrivalCutsceneTask->work;

    work->command     = command;
    work->commandStep = 0;
}

#include "../../shared/effects/glow_draw_star_local.inc.c"

/// Sets the outer ray-star disc's centre to half-intensity cyan.
///
/// Writes only vertex 2's RGB bytes of a writable `POLY_G4*`. Green and blue
/// use the supplied `halfIntensity`, already halved and normally 0..120;
/// stores retain its low byte. Arguments must have no side effects:
/// `primitive` is evaluated three times and `halfIntensity` twice. Use as a
/// statement. Requires `setRGB2` from `psyq/libgpu.h`. Define before including
/// `glow_draw_ray_star.inc.c`, which undefines this binding afterwards.
#define GLOW_DRAW_RAY_STAR_OUTER(primitive, halfIntensity) (setRGB2((primitive), 0, (halfIntensity), (halfIntensity)))
/// Sets the inner ray-star disc's centre to pulsing cyan.
///
/// Writes only vertex 2's RGB bytes of a writable `POLY_G4*`. Green and blue
/// use `intensity`, normally 0..240; stores retain its low byte. Arguments
/// must have no side effects: `primitive` is evaluated three times,
/// `intensity` twice, and `halfIntensity` is unused and unevaluated.
/// Use as a statement. Requires `setRGB2` from `psyq/libgpu.h`. Define before
/// including `glow_draw_ray_star.inc.c`, which undefines this binding afterwards.
#define GLOW_DRAW_RAY_STAR_INNER(primitive, intensity, halfIntensity) (setRGB2((primitive), 0, (intensity), (intensity)))
/// Sets the ray-star rays' centres to half-intensity cyan.
///
/// Writes only vertex 2's RGB bytes of a writable `POLY_G4*`. Green and blue
/// use the supplied `halfIntensity`, already halved and normally 0..120; red
/// is zero. Stores retain the intensity's low byte. Arguments must have no
/// side effects: `primitive` is evaluated three times and `halfIntensity`
/// twice. Use as a statement. Requires `setRGB2` from `psyq/libgpu.h`.
/// Define before including `glow_draw_ray_star.inc.c`, which undefines this
/// binding afterwards.
#define GLOW_DRAW_RAY_STAR_RAY(primitive, halfIntensity) (setRGB2((primitive), 0, (halfIntensity), (halfIntensity)))
#include "../../shared/effects/glow_draw_ray_star.inc.c"

void dryfieldGasStationCyanGlowTask(Task* task)
{
    enum {
        DRYFIELD_GAS_STATION_GLOW_DIAMOND_VIEWS        = (1 << 4) | (1 << 6) | (1 << 11) | (1 << 12),
        DRYFIELD_GAS_STATION_GLOW_PULSE_RATE           = 96, // 4096 angle units per turn, per animation frame
        DRYFIELD_GAS_STATION_GLOW_DIAMOND_RADIUS_SCALE = 128,
        DRYFIELD_GAS_STATION_GLOW_DISC_RADIUS_SCALE    = 64,
    };

    s32       viewBit;
    GfxCoord* glowCoord;

    viewBit   = 1 << gGameSession->location.view;
    glowCoord = task->extra.coordBody->coord;
    // The diamond and disc scales give the same nominal screen radius.
    if (viewBit & DRYFIELD_GAS_STATION_GLOW_DIAMOND_VIEWS) {
        _glowDrawStarLocal(glowCoord, &_gDryfieldGasStationGlowLocalPoint,
                           DRYFIELD_GAS_STATION_GLOW_PULSE_RATE, DRYFIELD_GAS_STATION_GLOW_DIAMOND_RADIUS_SCALE);
    } else if (viewBit != 0) {
        _glowDrawRayStar(glowCoord, &_gDryfieldGasStationGlowLocalPoint,
                         DRYFIELD_GAS_STATION_GLOW_PULSE_RATE, DRYFIELD_GAS_STATION_GLOW_DISC_RADIUS_SCALE);
    }
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
}
