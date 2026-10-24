#include "rooms/dryfield_warehouse.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "dryfield_warehouse_private.h"

#include "gameplay/display.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
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
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room_common.h"

#include "assets/engine/animation/player_schema.h"

static void _screenFadeInTask(Task* task);
/// Binds the shared fade-in declaration and definition to this file's private task.
///
/// Requires the bare identifier of the preceding static `void (Task*)` callback.
/// Keep defined from `screen_fade.h` through `screen_fade_in.inc.c`, then undefine.
/// This object-like binding has no arguments, captures or token construction.
#define SCREEN_FADE_IN_TASK _screenFadeInTask
#include "../../shared/screen/screen_fade.h"
#include "../../shared/effects/glow_draw.h"

/// Commands the warehouse cutscene script leaves for the cutscene task.
///
/// Each one is stored over the previous command and restarts `commandStep`.
/// The task carries it out on its later updates. Only the fade-out and the
/// restore clear themselves; the others keep running until the script stores
/// the next command.
enum {
    /// Nothing pending.
    DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_NONE = 0,
    /// Enables the display, spawns the fade-in task, suppresses the player's
    /// effects, installs the room's animation set on the player and plays its
    /// animation 1, and places the player at the cutscene placement. Then
    /// keeps the looping area sound going.
    DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_PLACE_AND_FADE_IN = 1,
    /// Spawns the fade-out task and records it as the room's fade task.
    DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_SPAWN_FADE_OUT = 2,
    /// Restores suppressed player effects, plays the equipped weapon's
    /// animation 1 and places the player at the closing placement on every
    /// update. Spawns the fade-in task first, then raises staggered dust
    /// puffs for `DRYFIELD_WAREHOUSE_CUTSCENE_DUST_FRAMES` updates.
    DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_RESTORE_AND_FADE_IN = 3,
    /// Holds the screen black, selects room 2 in the saved and live location
    /// and, one update later, marks the view and room objects dirty. Plays
    /// area sound 4 on its eleventh update.
    DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_BLACKOUT_AND_SWITCH_ROOM = 4,
    /// Holds the screen black and keeps the looping area sound going,
    /// restarting its interval.
    DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_BLACKOUT_AND_LOOP_SOUND = 5,
};

/// Callback-count modulus for repeating the cutscene's looping area sound.
///
/// The two loop commands test the unsigned-halfword counter before incrementing
/// it; zero requests a play too. Counts divisible by 60 restart the script.
#define DRYFIELD_WAREHOUSE_CUTSCENE_SOUND_LOOP_FRAMES 60

/// Dust updates before the restore-and-fade-in command clears its pending command.
///
/// Setup zeroes the unsigned-halfword count; dust updates increment before the
/// comparison, so update 36 emits its eligible puffs and then ends the command.
#define DRYFIELD_WAREHOUSE_CUTSCENE_DUST_FRAMES 36

/// Work block of the warehouse cutscene task.
///
/// The task allocates and zeroes the whole block when the cutscene starts,
/// then publishes itself so the room's script callbacks can reach it.
typedef struct {
    Task* player;                  // Player task captured when the cutscene starts. The animation install tests NULL; the other sends do not
    u16   command;                 // Pending `DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_*`
    u16   commandStep;             // Step within the command; zeroed with each new command
    u16   commandFrames;           // Updates the restore and room-switch commands have run since their first step. The placement command zeroes it and never reads it
    byte  field_A[2];              // Allocated and cleared with the block; no access. Role and width unproven
    u16   playerEffectsSuppressed; // 0 none pending; 1 player effects were killed and still need to be spawned back
    u16   soundLoopFrames;         // Updates counted towards the looping area sound, played each time the count divides by its interval
} _DryfieldWarehouseCutsceneWork;
STATIC_ASSERT_SIZEOF(_DryfieldWarehouseCutsceneWork, 0x10);

/// Points in the space of the coordinate drawn under: ring centres, one per
/// circle, for the ring drawer, and prism corners for the prism drawer, which
/// the room's only caller points at `[8..15]`.
extern SVECTOR gGlowPrismCorners[];

static WorldCollisionGrid _gDryfieldWarehouseBeforeEventCollisionGrid;
static WorldCollisionGrid _gDryfieldWarehouseAfterEventCollisionGrid;
static WorldCollisionGrid _gDryfieldWarehouseRoom3CollisionGrid;

static void _dryfieldWarehouseCutsceneTask(Task* task);
static void _dryfieldWarehouseFadeOutTask(Task* task);

/// Phases called under the skip script's fade, separated by one script tick.
enum { DRYFIELD_WAREHOUSE_SKIP_RESTORE = 0,
       DRYFIELD_WAREHOUSE_SKIP_REFRESH = 1 };

static void _dryfieldWarehouseSkipCutscene(s32 phase);
static void _dryfieldWarehouseSetCutsceneCommand(s16 command);

const TaskMessageEntry gDryfieldWarehouseRoomMessages[3] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, dryfieldWarehouseResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, dryfieldWarehouseUseKeyItem },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gDryfieldWarehouseRoomTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldWarehouseEventTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldWarehouseAmbienceTask, { .value = 0 } },
};

static AnimationPackedPose _gDryfieldWarehouseAnimation02260Bank1[2] = {
#include "assets/dryfield_warehouse_animation_02260_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWarehouseAnimation02260Bank4[28] = {
#include "assets/dryfield_warehouse_animation_02260_bank4.inc"
};

static AnimationRecord _gDryfieldWarehouseAnimation02260Records[123] = {
#include "assets/dryfield_warehouse_animation_02260_records.inc"
};

static u16 _gDryfieldWarehouseAnimation02260Indices[20] = {
#include "assets/dryfield_warehouse_animation_02260_indices.inc"
};

static AnimationSet _gDryfieldWarehouseAnimation02260 = {
    _gDryfieldWarehouseAnimation02260Records,
    _gDryfieldWarehouseAnimation02260Indices,
    { NULL, _gDryfieldWarehouseAnimation02260Bank1, NULL, NULL, _gDryfieldWarehouseAnimation02260Bank4, NULL, NULL, NULL },
};

/// Player clip table installed for the Monkey Wrench cutscene.
///
/// Entry 0 is absent; entry 1 borrows the room's twenty-part clip. Installation
/// retains the table and data through playback, until equipment-bank restoration.
static AnimationSet* _gDryfieldWarehouseCutscenePlayerAnimationSets[2] = {
    NULL,
    &_gDryfieldWarehouseAnimation02260,
};

/// Player's room-space placement at the start of the Monkey Wrench cutscene.
///
/// XYZ use world game units; rotation is zero in 4096 units per turn.
/// The placement message borrows this record only during synchronous dispatch.
static ActorTransform _gDryfieldWarehouseCutscenePlayerTransform = { { 5540, 0, -2300, 0 }, { 0, 0, 0, 0 } };

/// Player's final room-space placement for normal and skipped warehouse cutscenes.
///
/// XYZ use game-coordinate units; yaw is 512 of 4096 units per turn.
/// Placement messages borrow this record only during synchronous dispatch.
static ActorTransform _gDryfieldWarehouseClosingPlayerTransform = { { 4654, 0, -1968, 0 }, { 0, 512, 0, 0 } };

/// CAP command and fade wait used by the Monkey Wrench scene streams.
enum {
    DRYFIELD_WAREHOUSE_CUTSCENE_CAP_COMMAND  = 5,
    DRYFIELD_WAREHOUSE_SKIP_FADE_WAIT_FRAMES = 8,
};

/// Monkey Wrench scene, advancing player, fades and room changes at CAP cues.
///
/// Starts CAP command 5, posts deferred cutscene commands, then releases
/// scripted player control. Sixteen records include END. The interpreter
/// borrows this stream and its live cutscene task through completion.
static EvsCommand _gDryfieldWarehouseCutsceneScript[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_WAREHOUSE_CUTSCENE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWarehouseSetCutsceneCommand }, { .value = DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_BLACKOUT_AND_LOOP_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWarehouseSetCutsceneCommand }, { .value = DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_PLACE_AND_FADE_IN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWarehouseSetCutsceneCommand }, { .value = DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_SPAWN_FADE_OUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWarehouseSetCutsceneCommand }, { .value = DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_BLACKOUT_AND_SWITCH_ROOM }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldWarehouseSetCutsceneCommand }, { .value = DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_RESTORE_AND_FADE_IN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Input-skip restoration for the Monkey Wrench scene, under a primary fade.
///
/// Restores equipment and the closing player mark, commits room 2, then
/// refreshes changed room objects one script tick later. Eleven records include
/// END. The cutscene work, player and room resources must remain live.
static EvsCommand _gDryfieldWarehouseCutsceneSkipScript[11] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WAREHOUSE_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldWarehouseSkipCutscene }, { .value = DRYFIELD_WAREHOUSE_SKIP_RESTORE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldWarehouseSkipCutscene }, { .value = DRYFIELD_WAREHOUSE_SKIP_REFRESH }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WAREHOUSE_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc gDryfieldWarehouseCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _dryfieldWarehouseCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _dryfieldWarehouseFadeOutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _screenFadeInTask, { .value = 0 } },
};

SVECTOR gGlowPrismCorners[16] = {
    { 1190, -2950, -1540, 0 },
    { 310, -510, -180, 0 },
    { 2130, -2950, -3235, 0 },
    { 1200, -400, -1770, 0 },
    { 4340, -2950, -2940, 0 },
    { 3270, 0, -1290, 0 },
    { 3990, -2950, -1990, 0 },
    { 3030, -290, -510, 0 },
    { 5140, -2100, -3920, 0 },
    { 3860, -2100, -3920, 0 },
    { 3860, -660, -3920, 0 },
    { 5140, -660, -3920, 0 },
    { 4340, 0, -1930, 0 },
    { 3060, 0, -1930, 0 },
    { 3500, 0, -3030, 0 },
    { 4780, 0, -3030, 0 },
};

/// Local-space radii of the four light beams' lit/dark ring pairs.
///
/// Entries 0..7 parallel the ring centres in `gGlowPrismCorners`.
/// Each even index starts a beam and the next index is its dark end;
/// radii use the centres' game-coordinate units, without fixed-point scaling.
static s16 _gDryfieldWarehouseBeamRadii[8] = {
    100,
    150,
    125,
    175,
    135,
    185,
    100,
    150,
};

WorldCollisionRoomResources gDryfieldWarehouseRoomCollisionResources[3] = {
    {
        .grid                 = &_gDryfieldWarehouseBeforeEventCollisionGrid,
        .viewBoundaryTriggers = gDryfieldWarehouseViewBoundaryTriggers,
        .actionTriggers       = gDryfieldWarehouseBeforeEventActionTriggers,
        .occluders            = NULL,
    },
    {
        .grid                 = &_gDryfieldWarehouseAfterEventCollisionGrid,
        .viewBoundaryTriggers = gDryfieldWarehouseViewBoundaryTriggers,
        .actionTriggers       = gDryfieldWarehouseAfterEventActionTriggers,
        .occluders            = NULL,
    },
    {
        .grid                 = &_gDryfieldWarehouseRoom3CollisionGrid,
        .viewBoundaryTriggers = gDryfieldWarehouseViewBoundaryTriggers,
        .actionTriggers       = gDryfieldWarehouseBeforeEventActionTriggers,
        .occluders            = NULL,
    },
};

/// Camera/image/sprite mapping for room 2, after the Monkey Wrench event.
///
/// Logical views 1..9 index bytes 0..8; views 3 and 4 select mapped
/// indices 6 and 7. The last three zero bytes are retained storage, outside
/// the room's logical-view domain; the map has no terminator.
static u8 _gDryfieldWarehouseAfterEventViewMap[12] = {
    1,
    2,
    6,
    7,
    5,
    6,
    7,
    8,
    9,
    0,
    0,
    0,
};

/// Camera/image/sprite mapping for the third configured warehouse room.
///
/// Logical views 1..9 index bytes 0..8; views 3 and 4 select mapped
/// indices 9 and 8. The last three zero bytes are retained storage, outside
/// the room's logical-view domain. The map has no terminator, and the
/// gameplay condition selecting room 3 is unproven.
static u8 _gDryfieldWarehouseRoom3ViewMap[12] = {
    1,
    2,
    9,
    8,
    5,
    6,
    7,
    8,
    9,
    0,
    0,
    0,
};

u8* gDryfieldWarehouseRoomViewMaps[3] = {
    gViewIdentityMap,
    _gDryfieldWarehouseAfterEventViewMap,
    _gDryfieldWarehouseRoom3ViewMap,
};

enum { DRYFIELD_WAREHOUSE_LOGICAL_VIEW_COUNT = 9 };

ViewCount gDryfieldWarehouseRoomViewCounts[3] = {
    DRYFIELD_WAREHOUSE_LOGICAL_VIEW_COUNT,
    DRYFIELD_WAREHOUSE_LOGICAL_VIEW_COUNT,
    DRYFIELD_WAREHOUSE_LOGICAL_VIEW_COUNT,
};

WorldCoordRoomLighting gDryfieldWarehouseRoomLighting[3] = {
    { &gDryfieldWarehouseLights, NULL },
    { &gDryfieldWarehouseLights, NULL },
    { &gDryfieldWarehouseLights, NULL },
};

// Optional door map-marker nibble IDs stored by these endpoints.
enum {
    DRYFIELD_WAREHOUSE_BACK_STREET_MAP_FLAG = 468,
};

DirectionWarpEntry gDryfieldWarehouseWarpEntries[2] = {
    {
        .player         = { .yaw = { .word = 0 }, .x = 2631, .y = 0, .z = -3535 },
        .unknown_10     = { 0 },
        .companion      = { .yaw = { .word = (ACTOR_TRANSFORM_ANGLE_TURN / 4) }, .x = 1721, .y = 0, .z = -2640 },
        .unknown_24     = { 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WAREHOUSE, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WAREHOUSE, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_WAREHOUSE_BACK_STREET_MAP_FLAG,
    },
    {
        .player         = { .yaw = { .word = (ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4) }, .x = 5437, .y = 2, .z = -1084 },
        .unknown_10     = { 0 },
        .companion      = { .yaw = { .word = (ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4) }, .x = 4856, .y = 2, .z = -2146 },
        .unknown_24     = { 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 4,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Side length of every warehouse collision cell, in game-coordinate units.
enum { DRYFIELD_WAREHOUSE_COLLISION_CELL_SIZE = 4000 };

static SVECTOR _gDryfieldWarehouseBeforeEventCollisionGridNormals[7] = {
#include "assets/dryfield_warehouse_collision_02CE8_normals.inc"
};

static SVECTOR _gDryfieldWarehouseBeforeEventCollisionGridVerts[90] = {
#include "assets/dryfield_warehouse_collision_02CE8_verts.inc"
};

static WorldCollisionGridFace _gDryfieldWarehouseBeforeEventCollisionGridFaces[50] = {
#include "assets/dryfield_warehouse_collision_02CE8_faces.inc"
};

static s16 _gDryfieldWarehouseBeforeEventCollisionGridCells[80] = {
#include "assets/dryfield_warehouse_collision_02CE8_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gDryfieldWarehouseBeforeEventCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define DRYFIELD_WAREHOUSE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldWarehouseBeforeEventCollisionGridCells[(halfwordOffset)])
static s16* _gDryfieldWarehouseBeforeEventCollisionGridTable[2] = {
#include "assets/dryfield_warehouse_collision_02CE8_table.inc"
};
#undef DRYFIELD_WAREHOUSE_CELL_FACE_LIST_AT_OFFSET

/// Warehouse collision mesh for room 1, before the Monkey Wrench event.
///
/// Two X cells and one Z cell cover room coordinates from (0, -4000),
/// with 4000 game units per side. Activation borrows this mesh and binds
/// its view coordinate; keep the overlay loaded while it is active.
static WorldCollisionGrid _gDryfieldWarehouseBeforeEventCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldWarehouseBeforeEventCollisionGridNormals,
    .vertices    = _gDryfieldWarehouseBeforeEventCollisionGridVerts,
    .faces       = _gDryfieldWarehouseBeforeEventCollisionGridFaces,
    .cellFaceIds = _gDryfieldWarehouseBeforeEventCollisionGridTable,
    .xBias       = 0,
    .zBias       = 4000,
    .cellCountX  = 2,
    .cellCountZ  = 1,
    .cellSize    = DRYFIELD_WAREHOUSE_COLLISION_CELL_SIZE,
    .faceCount   = ARRAY_SIZE(_gDryfieldWarehouseBeforeEventCollisionGridFaces),
};

static SVECTOR _gDryfieldWarehouseAfterEventCollisionGridNormals[7] = {
#include "assets/dryfield_warehouse_collision_033EC_normals.inc"
};

static SVECTOR _gDryfieldWarehouseAfterEventCollisionGridVerts[99] = {
#include "assets/dryfield_warehouse_collision_033EC_verts.inc"
};

static WorldCollisionGridFace _gDryfieldWarehouseAfterEventCollisionGridFaces[56] = {
#include "assets/dryfield_warehouse_collision_033EC_faces.inc"
};

static s16 _gDryfieldWarehouseAfterEventCollisionGridCells[112] = {
#include "assets/dryfield_warehouse_collision_033EC_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gDryfieldWarehouseAfterEventCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define DRYFIELD_WAREHOUSE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldWarehouseAfterEventCollisionGridCells[(halfwordOffset)])
static s16* _gDryfieldWarehouseAfterEventCollisionGridTable[4] = {
#include "assets/dryfield_warehouse_collision_033EC_table.inc"
};
#undef DRYFIELD_WAREHOUSE_CELL_FACE_LIST_AT_OFFSET

/// Warehouse collision mesh for room 2, after the Monkey Wrench event.
///
/// Two X cells and two Z cells cover room coordinates from (0, -4100),
/// with 4000 game units per side. Activation borrows this mesh and binds
/// its view coordinate; keep the overlay loaded while it is active.
static WorldCollisionGrid _gDryfieldWarehouseAfterEventCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldWarehouseAfterEventCollisionGridNormals,
    .vertices    = _gDryfieldWarehouseAfterEventCollisionGridVerts,
    .faces       = _gDryfieldWarehouseAfterEventCollisionGridFaces,
    .cellFaceIds = _gDryfieldWarehouseAfterEventCollisionGridTable,
    .xBias       = 0,
    .zBias       = 4100,
    .cellCountX  = 2,
    .cellCountZ  = 2,
    .cellSize    = DRYFIELD_WAREHOUSE_COLLISION_CELL_SIZE,
    .faceCount   = ARRAY_SIZE(_gDryfieldWarehouseAfterEventCollisionGridFaces),
};

static SVECTOR _gDryfieldWarehouseRoom3CollisionGridNormals[7] = {
#include "assets/dryfield_warehouse_collision_03A78_normals.inc"
};

static SVECTOR _gDryfieldWarehouseRoom3CollisionGridVerts[91] = {
#include "assets/dryfield_warehouse_collision_03A78_verts.inc"
};

static WorldCollisionGridFace _gDryfieldWarehouseRoom3CollisionGridFaces[52] = {
#include "assets/dryfield_warehouse_collision_03A78_faces.inc"
};

static s16 _gDryfieldWarehouseRoom3CollisionGridCells[108] = {
#include "assets/dryfield_warehouse_collision_03A78_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gDryfieldWarehouseRoom3CollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define DRYFIELD_WAREHOUSE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldWarehouseRoom3CollisionGridCells[(halfwordOffset)])
static s16* _gDryfieldWarehouseRoom3CollisionGridTable[4] = {
#include "assets/dryfield_warehouse_collision_03A78_table.inc"
};
#undef DRYFIELD_WAREHOUSE_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh for the third configured warehouse room.
///
/// Two X cells and two Z cells cover room coordinates from (0, -4100),
/// with 4000 game units per side. Activation borrows this mesh and binds
/// its view coordinate; keep the overlay loaded while it is active.
/// The gameplay condition selecting room 3 is unproven.
static WorldCollisionGrid _gDryfieldWarehouseRoom3CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldWarehouseRoom3CollisionGridNormals,
    .vertices    = _gDryfieldWarehouseRoom3CollisionGridVerts,
    .faces       = _gDryfieldWarehouseRoom3CollisionGridFaces,
    .cellFaceIds = _gDryfieldWarehouseRoom3CollisionGridTable,
    .xBias       = 0,
    .zBias       = 4100,
    .cellCountX  = 2,
    .cellCountZ  = 2,
    .cellSize    = DRYFIELD_WAREHOUSE_COLLISION_CELL_SIZE,
    .faceCount   = ARRAY_SIZE(_gDryfieldWarehouseRoom3CollisionGridFaces),
};

ViewCamera gDryfieldWarehouseViewCameras[9] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -3000, 7700, 2000 } }, 230 },
    { { { { -981, 0, 3976 }, { -168, 4092, -41 }, { -3972, -173, -981 } }, { -5338, 1066, 1266 } }, 207 },
    { { { { -832, 0, -4010 }, { -256, 4087, 53 }, { 4002, 262, -831 } }, { -288, 1366, 916 } }, 207 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
    { { { { 2311, 0, -3381 }, { -2032, 3273, -1389 }, { 2702, 2461, 1847 } }, { -5038, 1666, 2016 } }, 207 },
    { { { { -832, 0, -4010 }, { -256, 4087, 53 }, { 4002, 262, -831 } }, { -288, 1366, 916 } }, 207 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
    { { { { -832, 0, -4010 }, { -256, 4087, 53 }, { 4002, 262, -831 } }, { -288, 1366, 916 } }, 207 },
};

/// Texture placement and neutral modulation shared by the warehouse's foreground sprites.
enum {
    DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE  = getTPage(1, GPU_BLEND_AVERAGE, 960, 0), // 8-bit textures
    DRYFIELD_WAREHOUSE_SPRITE_CLUT          = getClut(0, 255),
    DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR = 128,
};

SpriteBatch gDryfieldWarehouseView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource gDryfieldWarehouseView2SpriteSources[21] = {
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 48 } }, -8, 16, 1012, { .fields = { 104, 0 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 32 } }, 8, 24, 1025, { .fields = { 48, 216 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 56, 48 } }, -112, 64, 450, { .fields = { 64, 168 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 48, 56 } }, -160, 64, 425, { .fields = { 80, 56 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 16 } }, 80, 56, 650, { .fields = { 80, 32 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 24 } }, 104, 88, 480, { .fields = { 96, 32 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 16 } }, 112, 104, 399, { .fields = { 56, 40 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 32 } }, 88, 64, 650, { .fields = { 72, 64 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 16 } }, 96, 80, 559, { .fields = { 88, 32 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 48, 32 } }, 88, 32, 650, { .fields = { 80, 216 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 48, 16 } }, 96, 64, 553, { .fields = { 24, 72 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 48, 8 } }, 104, 80, 349, { .fields = { 80, 248 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 16 } }, 112, 88, 481, { .fields = { 56, 56 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 32 } }, 128, 88, 346, { .fields = { 72, 0 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 32 } }, 136, 32, 616, { .fields = { 72, 32 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 48 } }, 144, 32, 564, { .fields = { 120, 168 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 56 } }, 152, 32, 521, { .fields = { 120, 0 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 56, 24 } }, -160, 72, 612, { .fields = { 16, 0 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 16 } }, -160, 96, 612, { .fields = { 40, 24 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 56, 56 } }, -160, 0, 600, { .fields = { 72, 112 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 56, 16 } }, -160, 56, 612, { .fields = { 24, 96 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldWarehouseView2SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 2, 0, 0, { 0, 0 } },
    { 4, 13, 0, 0, { 2, 0 } },
    { 17, ARRAY_SIZE(gDryfieldWarehouseView2SpriteSources) - 17, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource gDryfieldWarehouseView3And6SpriteSources[25] = {
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 32 } }, 56, 32, 962, { .fields = { 112, 200 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 32 } }, 72, 32, 875, { .fields = { 112, 136 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 56, 40 } }, 48, 32, 825, { .fields = { 72, 0 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, 32, 750, { .fields = { 104, 224 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 8 } }, -48, 80, 600, { .fields = { 104, 160 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 24 } }, -120, 96, 550, { .fields = { 80, 136 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 24 } }, -88, 96, 550, { .fields = { 80, 200 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 24 } }, -120, 72, 550, { .fields = { 72, 40 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 24 } }, -88, 72, 550, { .fields = { 88, 232 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 24 } }, -56, 72, 600, { .fields = { 120, 232 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 40, 16 } }, -128, 56, 625, { .fields = { 56, 96 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 16 } }, -88, 56, 625, { .fields = { 72, 176 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 16 } }, -56, 56, 700, { .fields = { 88, 64 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 40, 16 } }, -128, 40, 725, { .fields = { 56, 80 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 16 } }, -88, 40, 725, { .fields = { 72, 160 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 16 } }, -56, 40, 725, { .fields = { 96, 104 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 24, 8 } }, -112, 32, 725, { .fields = { 80, 224 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 8 } }, -104, 24, 750, { .fields = { 88, 192 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 8 } }, -88, 24, 750, { .fields = { 80, 112 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 8 } }, -88, 32, 725, { .fields = { 56, 64 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 24, 32 } }, 96, 48, 625, { .fields = { 104, 168 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 32 } }, 120, 48, 625, { .fields = { 96, 72 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 24, 32 } }, 96, 16, 625, { .fields = { 104, 104 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 24, 32 } }, 120, 16, 625, { .fields = { 104, 40 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 16 } }, 128, 0, 625, { .fields = { 88, 120 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldWarehouseView3And6SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 1, 0, 0, { 0, 0 } },
    { 3, 17, 0, 0, { 2, 0 } },
    { 20, ARRAY_SIZE(gDryfieldWarehouseView3And6SpriteSources) - 20, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource gDryfieldWarehouseView4And7SpriteSources[2] = {
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 24, 40 } }, -160, 80, 500, { .fields = { 104, 0 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WAREHOUSE_SPRITE_TEXTURE_PAGE, DRYFIELD_WAREHOUSE_SPRITE_CLUT, { .fields = { 24, 24 } }, -136, 96, 550, { .fields = { 104, 40 } }, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldWarehouseView4And7SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(gDryfieldWarehouseView4And7SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch gDryfieldWarehouseView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

static void _dryfieldWarehouseExecuteCutsceneCommand(Task* task);
static void _dryfieldWarehouseDrawLightBeam(const GfxCoord* coord, s16 firstRing, s16 segmentCount);

/// Restores equipment, weapon animation and final placement for the skipped warehouse scene.
///
/// Requires live cutscene work, its player and the closing room transform.
/// Restoring suppressed equipment clears its latch and holds scripted control;
/// an already clear latch leaves control unchanged. The animation request is
/// borrowed only during synchronous dispatch. Character 1 with weapon 0..32
/// selects loaded animation bank 1..33. Any other character uses the retained
/// base 34, beyond the declared bank table; that path's reachability is unproven.
static inline void _dryfieldWarehouseRestoreSkippedPlayer(_DryfieldWarehouseCutsceneWork* work)
{
    AnimationPlayRequest animationRequest;
    s32                  weaponId;
    s32                  animationBankIndex;

    if (work->playerEffectsSuppressed != 0) {
        playerActorRestoreEquipment();
        work->playerEffectsSuppressed = 0;
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    }
    weaponId                              = gPlayerStatus.weapon;
    animationBankIndex                    = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponId);
    animationRequest.source.index         = animationBankIndex;
    animationRequest.animationId          = PLAYER_ANIMATION_IDLE;
    animationRequest.blend                = ANIMATION_BLEND_RESET;
    animationRequest.blendFrames          = 0;
    animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
    TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_PLAY, &animationRequest, 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldWarehouseClosingPlayerTransform, 0);
}

/// Restores the warehouse cutscene's final player and room state when it is skipped.
///
/// The skip script calls phase 0 under the primary fade, then phase 1 one script
/// tick later. Phase 0 kills the outstanding fade task, enables display, restores
/// suppressed equipment/control, plays the equipped weapon's animation 1 with
/// world collision, and places the player at the closing transform. It selects
/// room 2 in the live save and session only if the saved room differs, remembering
/// whether it changed. Phase 1 marks view and room objects dirty only on that
/// change. Other phases do nothing. Requires the published cutscene task/work,
/// player and room resources to remain live; animation payload storage is borrowed
/// only for synchronous dispatch.
static void _dryfieldWarehouseSkipCutscene(s32 phase)
{
    enum { ROOM_AFTER_CUTSCENE = 2 };

    _DryfieldWarehouseCutsceneWork* work;

    switch (phase) {
        case DRYFIELD_WAREHOUSE_SKIP_RESTORE:
            // Restore the player's equipment and placement before switching room objects.
            if (gDryfieldWarehouseFadeOutTask != NULL) {
                taskKill(gDryfieldWarehouseFadeOutTask);
            }
            SetDispMask(1);
            work = gDryfieldWarehouseCutsceneTask->work;
            _dryfieldWarehouseRestoreSkippedPlayer(work);
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState != ROOM_AFTER_CUTSCENE) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ROOM_AFTER_CUTSCENE;
                gGameSession->location.roomState                            = ROOM_AFTER_CUTSCENE;
                gDryfieldWarehouseSkipRoomChanged                           = true;
                return;
            }
            gDryfieldWarehouseSkipRoomChanged = false;
            return;
        case DRYFIELD_WAREHOUSE_SKIP_REFRESH:
            // The script separates the room commit and its deferred redraw by one tick.
            if (gDryfieldWarehouseSkipRoomChanged != 0) {
                gGameSession->viewDirty     = phase;
                gGameSession->roomObjsDirty = phase;
            }
            return;
    }
}

/// Advances the warehouse cutscene's pending command for one callback tick.
///
/// Requires initialized cutscene work, a live player and loaded room resources.
/// Fade-out and unknown commands clear immediately; restore clears after its
/// 36 dust updates. Other commands repeat until the script replaces them.
/// Restore reads the published event work and repeats equipment animation and
/// placement each tick, then refetches this task's work for dust timing.
/// Animation/placement requests and dust positions are consumed synchronously.
/// Character 1 uses weapon banks 1..33; the alternate-character range is unproven.
/// Task-spawn and effect-allocation failures do not stop command progression.
static void _dryfieldWarehouseExecuteCutsceneCommand(Task* task)
{
    enum {
        DRYFIELD_WAREHOUSE_COMMAND_STEP_BEGIN          = 0,
        DRYFIELD_WAREHOUSE_COMMAND_STEP_RUN            = 1,
        DRYFIELD_WAREHOUSE_COMMAND_STEP_ROOM_REFRESHED = 2,
        DRYFIELD_WAREHOUSE_TASK_FADE_OUT               = 1,
        DRYFIELD_WAREHOUSE_TASK_FADE_IN                = 2,
        DRYFIELD_WAREHOUSE_FADE_INTENSITY_STEP         = 8,
        DRYFIELD_WAREHOUSE_PLAYER_SCENE_CLIP           = 1,
        DRYFIELD_WAREHOUSE_FINAL_ROOM                  = 2,
        DRYFIELD_WAREHOUSE_ROOM_SWITCH_SOUND_FRAME     = 10,
        DRYFIELD_WAREHOUSE_DUST_PERIOD_MASK            = 8 - 1,
        // Low 12 bits: size 768; bits 12..15: two ticks per texture frame;
        // sign bit: permit randomized child puffs.
        DRYFIELD_WAREHOUSE_DUST_SPAWN_ARGUMENT = 0x80002300,
    };
    _DryfieldWarehouseCutsceneWork* work;
    _DryfieldWarehouseCutsceneWork* publishedWork;
    _DryfieldWarehouseCutsceneWork* animationWork;
    // Transient payload storage is reused only after animation dispatch returns.
    union {
        AnimationPlayRequest animation;
        SVECTOR              dustPosition;
    } scratch;

    s32 weaponSlot;
    s32 animationBankIndex;

    /// Restores equipment, plays the weapon-bank scene clip and places the player.
    ///
    /// Work must hold a live player; it may differ from the caller's task work.
    /// Request and both indices are writable locals, evaluated repeatedly. All
    /// arguments must be stable and side-effect-free. Captures this function's
    /// character/bank/clip constants, live player/save and closing transform.
    /// Equipment restoration clears its pending flag before scripted hold resumes.
    /// The two dispatches borrow their payloads synchronously, with collision enabled.
#define DRYFIELD_WAREHOUSE_RESTORE_COMMAND_PLAYER(cutsceneWork, animationRequest, weaponSlot, bankIndex)                                \
    {                                                                                                                                   \
        if ((cutsceneWork)->playerEffectsSuppressed != 0) {                                                                             \
            playerActorRestoreEquipment();                                                                                              \
            (cutsceneWork)->playerEffectsSuppressed = 0;                                                                                \
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);                                                            \
        }                                                                                                                               \
        (weaponSlot)                            = gPlayerStatus.weapon;                                                                 \
        (bankIndex)                             = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponSlot);                                           \
        (animationRequest).source.index         = (bankIndex);                                                                          \
        (animationRequest).animationId          = PLAYER_ANIMATION_IDLE;                                                                \
        (animationRequest).blend                = ANIMATION_BLEND_RESET;                                                                \
        (animationRequest).blendFrames          = 0;                                                                                    \
        (animationRequest).enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;                                                     \
                                                                                                                                        \
        TASK_MESSAGE_DISPATCH_POINTER((cutsceneWork)->player, ANIMATION_MESSAGE_PLAY, &(animationRequest), 0);                          \
        TASK_MESSAGE_DISPATCH_POINTER((cutsceneWork)->player, GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldWarehouseClosingPlayerTransform, 0); \
    }

    work = task->work;
    switch (work->command) {
        case DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_NONE:
            break;
        case DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_PLACE_AND_FADE_IN:
            switch (work->commandStep) {
                case DRYFIELD_WAREHOUSE_COMMAND_STEP_BEGIN:
                    SetDispMask(1);
                    taskSpawnFromTable(gDryfieldWarehouseCutsceneTaskDescs, DRYFIELD_WAREHOUSE_TASK_FADE_IN, DRYFIELD_WAREHOUSE_FADE_INTENSITY_STEP, 0);
                    playerActorRemoveEquipment();
                    work->playerEffectsSuppressed = 1;
                    animationWork                 = task->work;
                    if (animationWork->player != NULL) {
                        scratch.animation.source.sets          = _gDryfieldWarehouseCutscenePlayerAnimationSets;
                        scratch.animation.animationId          = DRYFIELD_WAREHOUSE_PLAYER_SCENE_CLIP;
                        scratch.animation.blend                = ANIMATION_BLEND_RESET;
                        scratch.animation.blendFrames          = 0;
                        scratch.animation.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(animationWork->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &scratch.animation, 0);
                    }
                    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldWarehouseCutscenePlayerTransform, 0);
                    work->commandFrames = 0;
                    work->commandStep++;
                    break;
                case DRYFIELD_WAREHOUSE_COMMAND_STEP_RUN:
                    if ((work->soundLoopFrames % DRYFIELD_WAREHOUSE_CUTSCENE_SOUND_LOOP_FRAMES) == 0) {
                        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WAREHOUSE, 3), 0, 0);
                    }
                    break;
            }
            work->soundLoopFrames++;
            return;
        case DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_SPAWN_FADE_OUT:
            gDryfieldWarehouseFadeOutTask = taskSpawnFromTable(gDryfieldWarehouseCutsceneTaskDescs, DRYFIELD_WAREHOUSE_TASK_FADE_OUT, DRYFIELD_WAREHOUSE_FADE_INTENSITY_STEP, 0);
            break;
        case DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_RESTORE_AND_FADE_IN:
            // The restore goes through the published cutscene task's block.
            publishedWork = gDryfieldWarehouseCutsceneTask->work;
            DRYFIELD_WAREHOUSE_RESTORE_COMMAND_PLAYER(publishedWork, scratch.animation, weaponSlot, animationBankIndex);
            switch (work->commandStep) {
                case DRYFIELD_WAREHOUSE_COMMAND_STEP_BEGIN:
                    taskSpawnFromTable(gDryfieldWarehouseCutsceneTaskDescs, DRYFIELD_WAREHOUSE_TASK_FADE_IN, DRYFIELD_WAREHOUSE_FADE_INTENSITY_STEP, 0);
                    work->commandFrames = 0;
                    work->commandStep++;
                    return;
                case DRYFIELD_WAREHOUSE_COMMAND_STEP_RUN:
                    // Five puffs along the Z axis, one per update in turn, each
                    // repeating every eighth update. The block is fetched again
                    // from the task for the phase tests.
                    {
                        _DryfieldWarehouseCutsceneWork* dustWork;
                        work->commandFrames++;
                        dustWork                = task->work;
                        scratch.dustPosition.vx = 0x1644;
                        scratch.dustPosition.vy = 0;
                        if (!(dustWork->commandFrames & DRYFIELD_WAREHOUSE_DUST_PERIOD_MASK)) {
                            scratch.dustPosition.vz = -500;
                            effectSpawn(EFFECT_DUST_PUFF, NULL, DRYFIELD_WAREHOUSE_DUST_SPAWN_ARGUMENT, &scratch.dustPosition);
                        }
                        if (!((dustWork->commandFrames + 1) & DRYFIELD_WAREHOUSE_DUST_PERIOD_MASK)) {
                            scratch.dustPosition.vz = -700;
                            effectSpawn(EFFECT_DUST_PUFF, NULL, DRYFIELD_WAREHOUSE_DUST_SPAWN_ARGUMENT, &scratch.dustPosition);
                        }
                        if (!((dustWork->commandFrames + 2) & DRYFIELD_WAREHOUSE_DUST_PERIOD_MASK)) {
                            scratch.dustPosition.vz = -900;
                            effectSpawn(EFFECT_DUST_PUFF, NULL, DRYFIELD_WAREHOUSE_DUST_SPAWN_ARGUMENT, &scratch.dustPosition);
                        }
                        if (!((dustWork->commandFrames + 3) & DRYFIELD_WAREHOUSE_DUST_PERIOD_MASK)) {
                            scratch.dustPosition.vz = -1100;
                            effectSpawn(EFFECT_DUST_PUFF, NULL, DRYFIELD_WAREHOUSE_DUST_SPAWN_ARGUMENT, &scratch.dustPosition);
                        }
                        if (!((dustWork->commandFrames + 4) & DRYFIELD_WAREHOUSE_DUST_PERIOD_MASK)) {
                            scratch.dustPosition.vz = -1300;
                            effectSpawn(EFFECT_DUST_PUFF, NULL, DRYFIELD_WAREHOUSE_DUST_SPAWN_ARGUMENT, &scratch.dustPosition);
                        }
                        if (work->commandFrames >= DRYFIELD_WAREHOUSE_CUTSCENE_DUST_FRAMES) {
                            work->command = DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_NONE;
                        }
                        SetDispMask(1);
                        return;
                    }
            }
            break;
        case DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_BLACKOUT_AND_SWITCH_ROOM:
            fadeDrawOverlay(0xFF, 0xFF, 0xFF, GPU_BLEND_SUBTRACT);
            switch (work->commandStep) {
                case DRYFIELD_WAREHOUSE_COMMAND_STEP_BEGIN:
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = DRYFIELD_WAREHOUSE_FINAL_ROOM;
                    gGameSession->location.roomState                            = DRYFIELD_WAREHOUSE_FINAL_ROOM;
                    work->commandFrames                                         = 0;
                    work->commandStep++;
                    break;
                case DRYFIELD_WAREHOUSE_COMMAND_STEP_RUN:
                    gGameSession->viewDirty     = 1;
                    gGameSession->roomObjsDirty = 1;
                    work->commandStep++;
                    break;
                case DRYFIELD_WAREHOUSE_COMMAND_STEP_ROOM_REFRESHED:
                    break;
            }
            if (work->commandFrames == DRYFIELD_WAREHOUSE_ROOM_SWITCH_SOUND_FRAME) {
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WAREHOUSE, 4), 0, 0);
            }
            work->commandFrames++;
            return;
        case DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_BLACKOUT_AND_LOOP_SOUND:
            fadeDrawOverlay(0xFF, 0xFF, 0xFF, GPU_BLEND_SUBTRACT);
            switch (work->commandStep) {
                case DRYFIELD_WAREHOUSE_COMMAND_STEP_BEGIN:
                    gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_RUNNING;
                    work->soundLoopFrames     = 0;
                    work->commandStep++;
                    break;
                case DRYFIELD_WAREHOUSE_COMMAND_STEP_RUN:
                    if ((work->soundLoopFrames % DRYFIELD_WAREHOUSE_CUTSCENE_SOUND_LOOP_FRAMES) == 0) {
                        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WAREHOUSE, 3), 0, 0);
                    }
                    break;
            }
            work->soundLoopFrames++;
            return;
    }
    work->command = DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_NONE;
#undef DRYFIELD_WAREHOUSE_RESTORE_COMMAND_PLAYER
}

/// Drives the warehouse cutscene's script-selected commands and owned work.
///
/// State 0 waits for the attachment wheel and display transition to clear,
/// allocates zeroed work and selects the player's equipped-bank idle clip.
/// Allocation failure kills the task but retains the subsequent animation
/// request, script start and state writes. State 1 starts the skippable script;
/// state 2 executes its commands until `eventState` clears, then requests release
/// with result zero. Normal teardown frees `Task::work`. Requires a live player,
/// its selected animation bank, and the room overlay throughout the sequence.
static void _dryfieldWarehouseCutsceneTask(Task* task)
{
    enum {
        DRYFIELD_WAREHOUSE_SCENE_INITIALIZE   = 0,
        DRYFIELD_WAREHOUSE_SCENE_START_SCRIPT = 1,
        DRYFIELD_WAREHOUSE_SCENE_EXECUTE      = 2,
        DRYFIELD_WAREHOUSE_PLAYER_TICK_HOLD   = 1,
    };
    _DryfieldWarehouseCutsceneWork* work;
    AnimationPlayRequest            animationRequest;
    s32                             weaponId;
    s32                             bankIndex;

/// Selects and plays the equipped weapon's idle clip for this cutscene.
///
/// Captures weaponId, bankIndex, animationRequest and this function's bank/clip
/// constants. The stack request is borrowed only through synchronous dispatch.
/// Takes no arguments; use inside a braced block with the player and bank live.
#define DRYFIELD_WAREHOUSE_PLAY_CUTSCENE_IDLE_ANIMATION()                                                                    \
    {                                                                                                                        \
        weaponId = gPlayerStatus.weapon;                                                                                     \
        /* Dispatch borrows this request; playback borrows the selected bank. */                                             \
        bankIndex                             = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponId);                                    \
        animationRequest.source.index         = bankIndex;                                                                   \
        animationRequest.animationId          = PLAYER_ANIMATION_IDLE;                                                       \
        animationRequest.blend                = ANIMATION_BLEND_RESET;                                                       \
        animationRequest.blendFrames          = 0;                                                                           \
        animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                           \
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &animationRequest, 0); \
    }

    switch (task->state) {
        case DRYFIELD_WAREHOUSE_SCENE_INITIALIZE:
            if ((gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                work       = memMalloc(sizeof(*work), false);
                task->work = work;
                if (work == NULL) {
                    taskKill(task);
                } else {
                    memFillBytes(work, 0, sizeof(*work));
                    work->player                   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    gDryfieldWarehouseCutsceneTask = task;
                }
                DRYFIELD_WAREHOUSE_PLAY_CUTSCENE_IDLE_ANIMATION();
                gDryfieldWarehouseFadeOutTask = NULL;
                gPlayerActorStateTickHeld     = DRYFIELD_WAREHOUSE_PLAYER_TICK_HOLD;
                task->state                   = task->state + 1;
            } else {
                return;
            }
            // Start the script in the same update as initialization.
        case DRYFIELD_WAREHOUSE_SCENE_START_SCRIPT:
            evsStartScriptWithSkip(_gDryfieldWarehouseCutsceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                   _gDryfieldWarehouseCutsceneSkipScript);
            task->state = task->state + 1;
            return;
        case DRYFIELD_WAREHOUSE_SCENE_EXECUTE:
            if (gGameSession->eventState == 0) {
                taskRequestKill(task, 0);
                return;
            }
            _dryfieldWarehouseExecuteCutsceneCommand(task);
            break;
    }
#undef DRYFIELD_WAREHOUSE_PLAY_CUTSCENE_IDLE_ANIMATION
}

#include "../../shared/screen/screen_fade_in.inc.c"
#undef SCREEN_FADE_IN_TASK

/// Advances all fade channels by a borrowed unsigned intensity step.
///
/// Borrows writable fade work and a readable u16 step, loaded separately for
/// each channel. Each addition narrows back to signed 16 bits without clamping;
/// zero holds the channels unchanged.
static inline void _dryfieldWarehouseStepFadeOut(ScreenFadeWork* fade, const u16* intensityStep)
{
    fade->r += *intensityStep;
    fade->g += *intensityStep;
    fade->b += *intensityStep;
}

/// Darkens the warehouse screen, then disables display and releases the fade task.
///
/// State 0 owns a primary-heap `ScreenFadeWork`, initially all zero channels;
/// allocation failure kills the task. State 1 draws red/green/red before each
/// step. The low unsigned halfword of `spawnArg1.value` is intensity per update
/// (the cutscene supplies 8); zero holds indefinitely. Signed-halfword channels
/// may wrap for large rates. At red >= 256 the display is disabled, the room's
/// fade-task handle is cleared and teardown frees the work. Other states idle.
/// Requires the overlay, frame packet arena and foreground ordering table.
static void _dryfieldWarehouseFadeOutTask(Task* task)
{
    enum {
        DRYFIELD_WAREHOUSE_FADE_OUT_INITIALIZE = 0,
        DRYFIELD_WAREHOUSE_FADE_OUT_RAMP       = 1,
        DRYFIELD_WAREHOUSE_FADE_OUT_COMPLETE   = 256,
    };

    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case DRYFIELD_WAREHOUSE_FADE_OUT_INITIALIZE:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                return;
            }
            fade         = allocatedFade;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            task->state += DRYFIELD_WAREHOUSE_FADE_OUT_RAMP - DRYFIELD_WAREHOUSE_FADE_OUT_INITIALIZE;
            // Draw the initial zero overlay and advance in this same update.
            /* fallthrough */
        case DRYFIELD_WAREHOUSE_FADE_OUT_RAMP:
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            _dryfieldWarehouseStepFadeOut(fade, &task->spawnArg1.halves.low);
            if (fade->r < DRYFIELD_WAREHOUSE_FADE_OUT_COMPLETE) {
                return;
            }
            SetDispMask(0);
            gDryfieldWarehouseFadeOutTask = NULL;
            taskKill(task);
            break;
    }
}

/// Posts a command to the live warehouse cutscene and restarts its command step.
///
/// The event script supplies a `DRYFIELD_WAREHOUSE_CUTSCENE_COMMAND_*` signed
/// halfword. Its bits are stored as u16, replacing any pending command; execution
/// begins on a later task update. The published cutscene task and its work must
/// still be live. This callback neither allocates nor retains caller storage.
static void _dryfieldWarehouseSetCutsceneCommand(s16 command)
{
    enum { DRYFIELD_WAREHOUSE_CUTSCENE_FIRST_COMMAND_STEP = 0 };

    _DryfieldWarehouseCutsceneWork* work = gDryfieldWarehouseCutsceneTask->work;

    work->command     = command;
    work->commandStep = DRYFIELD_WAREHOUSE_CUTSCENE_FIRST_COMMAND_STEP;
}

#include "../../shared/effects/glow_draw_grey_prism.inc.c"

/// Draws a rotating, pulsing grey light beam between two circular local-space rings.
///
/// `firstRing` selects a lit centre/radius and the following dark centre/radius;
/// callers select 0, 2, 4 or 6. Distances use local coordinate units. The borrowed
/// `coord->workm` must already map these positions into camera space.
/// `segmentCount` is 1..4096 (all callers use 8); angles use 4096 units per turn
/// with a truncated integer step, so counts not dividing 4096 leave a gap.
/// Local and transformed components narrow to signed 16 bits before projection.
///
/// Requires initialized scratch storage, view matrices, ordering table and packet
/// arena. Queues one Gouraud quad and additive blend command per segment, fading
/// grey `20 + (rsin(animFrame * 1024) >> 11)` to black. Sorts at the last corner's
/// camera Z / 4 without clipping or testing projection flags.
static void _dryfieldWarehouseDrawLightBeam(const GfxCoord* coord, s16 firstRing, s16 segmentCount)
{
    enum {
        DRYFIELD_WAREHOUSE_BEAM_PULSE_PHASE_SHIFT = 10,
        DRYFIELD_WAREHOUSE_BEAM_PULSE_TRIG_SHIFT  = 11,
        DRYFIELD_WAREHOUSE_BEAM_BRIGHTNESS_BASE   = 20,
    };

    EffectQuadCornersScratch* block;
    POLY_G4*                  prim;
    s16                       brightness;
    s16                       angleStep;
    s16                       startAngle;
    s32                       angle;
    s32                       nextAngle;

    brightness = (rsin(gDisplayState.animFrame << DRYFIELD_WAREHOUSE_BEAM_PULSE_PHASE_SHIFT) >> DRYFIELD_WAREHOUSE_BEAM_PULSE_TRIG_SHIFT) + DRYFIELD_WAREHOUSE_BEAM_BRIGHTNESS_BASE;
    SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    block      = SCRATCH_STACK_CURSOR(EffectQuadCornersScratch);
    startAngle = gDisplayState.animFrame & (GLOW_FULL_TURN - 1);
    angleStep  = GLOW_FULL_TURN / segmentCount;
    gte_SetTransMatrix(&GsWSMATRIX);
    /// Rotates one scratch corner in place with the coordinate's composed matrix.
    ///
    /// Captures `coord` (const GfxCoord*) and `block` (EffectQuadCornersScratch*).
    /// `vertexIndex` is evaluated twice and must be a pure integer in 0..3.
    /// Loads the Q12 rotation and stores signed-halfword XYZ; translation
    /// remains for the caller. Expands to statements inside braces, and is
    /// undefined before this drawer returns.
#define DRYFIELD_WAREHOUSE_ROTATE_BEAM_CORNER(vertexIndex) \
    gte_SetRotMatrix(&coord->workm);                       \
    gte_ldv0(&block->vertices[(vertexIndex)]);             \
    gte_rtv0();                                            \
    gte_stsv(&block->vertices[(vertexIndex)]);

    // Generate the local XZ ring pair, then place each corner in camera space.
    for (angle = startAngle; angle < startAngle + angleStep * segmentCount; angle = nextAngle) {
        block->vertices[0].vx = gGlowPrismCorners[firstRing].vx + ((rsin(angle) * _gDryfieldWarehouseBeamRadii[firstRing]) >> GLOW_TRIG_SHIFT);
        block->vertices[0].vy = gGlowPrismCorners[firstRing].vy;
        block->vertices[0].vz = gGlowPrismCorners[firstRing].vz + ((rcos(angle) * _gDryfieldWarehouseBeamRadii[firstRing]) >> GLOW_TRIG_SHIFT);
        DRYFIELD_WAREHOUSE_ROTATE_BEAM_CORNER(0);
        block->vertices[0].vx += coord->workm.t[0];
        block->vertices[0].vy += coord->workm.t[1];
        nextAngle              = angle + angleStep;
        block->vertices[0].vz += coord->workm.t[2];

        block->vertices[1].vx = gGlowPrismCorners[firstRing].vx + ((rsin(nextAngle) * _gDryfieldWarehouseBeamRadii[firstRing]) >> GLOW_TRIG_SHIFT);
        block->vertices[1].vy = gGlowPrismCorners[firstRing].vy;
        block->vertices[1].vz = gGlowPrismCorners[firstRing].vz + ((rcos(nextAngle) * _gDryfieldWarehouseBeamRadii[firstRing]) >> GLOW_TRIG_SHIFT);
        DRYFIELD_WAREHOUSE_ROTATE_BEAM_CORNER(1);
        block->vertices[1].vx += coord->workm.t[0];
        block->vertices[1].vy += coord->workm.t[1];
        block->vertices[1].vz += coord->workm.t[2];

        block->vertices[2].vx = gGlowPrismCorners[firstRing + 1].vx + ((rsin(angle) * _gDryfieldWarehouseBeamRadii[firstRing + 1]) >> GLOW_TRIG_SHIFT);
        block->vertices[2].vy = gGlowPrismCorners[firstRing + 1].vy;
        block->vertices[2].vz = gGlowPrismCorners[firstRing + 1].vz + ((rcos(angle) * _gDryfieldWarehouseBeamRadii[firstRing + 1]) >> GLOW_TRIG_SHIFT);
        DRYFIELD_WAREHOUSE_ROTATE_BEAM_CORNER(2);
        block->vertices[2].vx += coord->workm.t[0];
        block->vertices[2].vy += coord->workm.t[1];
        block->vertices[2].vz += coord->workm.t[2];

        block->vertices[3].vx = gGlowPrismCorners[firstRing + 1].vx + ((rsin(nextAngle) * _gDryfieldWarehouseBeamRadii[firstRing + 1]) >> GLOW_TRIG_SHIFT);
        block->vertices[3].vy = gGlowPrismCorners[firstRing + 1].vy;
        block->vertices[3].vz = gGlowPrismCorners[firstRing + 1].vz + ((rcos(nextAngle) * _gDryfieldWarehouseBeamRadii[firstRing + 1]) >> GLOW_TRIG_SHIFT);
        DRYFIELD_WAREHOUSE_ROTATE_BEAM_CORNER(3);
        block->vertices[3].vx += coord->workm.t[0];
        block->vertices[3].vy += coord->workm.t[1];
        block->vertices[3].vz += coord->workm.t[2];

        // Project the quad, fading its lit edge to black at the far ring.
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vertices[0]);
        gte_rtps();
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&block->vertices[1], &block->vertices[2], &block->vertices[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&block->depth);
        setRGB0(prim, brightness, brightness, brightness);
        setRGB1(prim, brightness, brightness, brightness);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
    }
#undef DRYFIELD_WAREHOUSE_ROTATE_BEAM_CORNER
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
}

void dryfieldWarehouseDrawGlowsTask(Task* task)
{
    enum {
        DRYFIELD_WAREHOUSE_BEAM_SEGMENTS               = 8,
        DRYFIELD_WAREHOUSE_FIRST_BEAM_RING             = 0,
        DRYFIELD_WAREHOUSE_SECOND_BEAM_RING            = 2,
        DRYFIELD_WAREHOUSE_THIRD_BEAM_RING             = 4,
        DRYFIELD_WAREHOUSE_FOURTH_BEAM_RING            = 6,
        DRYFIELD_WAREHOUSE_PRISM_FIRST_CORNER          = 8,
        DRYFIELD_WAREHOUSE_FIRST_BEAM_VIEWS            = 1 << 2,
        DRYFIELD_WAREHOUSE_PRISM_AND_SECOND_BEAM_VIEWS = (1 << 2) | (1 << 3) | (1 << 6) | (1 << 9),
        DRYFIELD_WAREHOUSE_LAST_BEAMS_VIEWS            = (1 << 2) | (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9),
    };

    s32             viewMask;
    s32             prismAndSecondBeamViews;
    const GfxCoord* coord;

    viewMask                = 1 << gGameSession->location.view;
    prismAndSecondBeamViews = viewMask & DRYFIELD_WAREHOUSE_PRISM_AND_SECOND_BEAM_VIEWS;
    coord                   = task->extra.coordBody->coord;
    // Select visible lights with the room-local view ID, before camera remapping.
    if (prismAndSecondBeamViews != 0) {
        _glowDrawGreyPrism(coord, DRYFIELD_WAREHOUSE_PRISM_FIRST_CORNER);
    }
    if (viewMask & DRYFIELD_WAREHOUSE_FIRST_BEAM_VIEWS) {
        _dryfieldWarehouseDrawLightBeam(coord, DRYFIELD_WAREHOUSE_FIRST_BEAM_RING, DRYFIELD_WAREHOUSE_BEAM_SEGMENTS);
    }
    if (prismAndSecondBeamViews != 0) {
        _dryfieldWarehouseDrawLightBeam(coord, DRYFIELD_WAREHOUSE_SECOND_BEAM_RING, DRYFIELD_WAREHOUSE_BEAM_SEGMENTS);
    }
    if (viewMask & DRYFIELD_WAREHOUSE_LAST_BEAMS_VIEWS) {
        _dryfieldWarehouseDrawLightBeam(coord, DRYFIELD_WAREHOUSE_THIRD_BEAM_RING, DRYFIELD_WAREHOUSE_BEAM_SEGMENTS);
        _dryfieldWarehouseDrawLightBeam(coord, DRYFIELD_WAREHOUSE_FOURTH_BEAM_RING, DRYFIELD_WAREHOUSE_BEAM_SEGMENTS);
    }
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
}
