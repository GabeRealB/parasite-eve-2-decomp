#include "rooms/dryfield_motel_room_1.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "dryfield_motel_room_1_private.h"

#include "actors/actor_01200.h"
#include "actors/actor_04000.h"
#include "actors/actor_123200.h"
#include "actors/actor_401800.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_dryfield.h"

#include "assets/engine/animation/player_schema.h"

/// Commands in the motel room 1 actors' stage/area namespace.
///
/// The staged sucklers handle 1..3; the enemy sucklers handle 3 and 4
/// only while alive. Other placed actors decide whether to handle the command.
enum {
    DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_FIRST_STAGING  = 1, // Walk the staged sucklers; placement 1 is fully lit, placement 0 dimmed
    DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_SECOND_STAGING = 2, // Walk both staged sucklers with full lighting
    DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_START_COMBAT   = 3, // Hide the staged sucklers and let the enemy sucklers chase the player
    DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_WALK_IN_PLACE  = 4, // Enemy sucklers loop their walk without moving
};

/// Requests the event script makes of the event task, held in
/// `_DryfieldMotelRoom1EventWork::action`.
///
/// The script sets one between its caption cues and the task carries it out on
/// its next frame. Every request but the turn completes in that frame.
enum {
    DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_NONE            = 0,
    DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_FIRST_STAGING   = 1, // Actor command 1; staged sucklers to their first marks
    DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SECOND_STAGING  = 2, // Actor command 2; player model shown; staged sucklers to their second marks
    DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SOUND_5         = 3, // Character bank 0xC, sound 5
    DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SOUND_2         = 4, // Character bank 0xC, sound 2
    DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_OPENING_SOUND_5 = 5, // The same sound as 3, requested as the script opens
    DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_TURN_PLAYER     = 6, // Turn the player in place; runs over several frames
};

/// Steps of `DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_TURN_PLAYER`, held in
/// `_DryfieldMotelRoom1EventWork::actionStep`.
enum {
    DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_BEGIN    = 0, // Capture the player's position and yaw and pick the direction
    DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_YAW_UP   = 1, // Yaw rising
    DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_YAW_DOWN = 2, // Yaw falling
    DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_SETTLE   = 3, // Turn finished; hold before the closing animation
};

/// Tuning of the player's turn. Angles are in `ActorTransform` units.
enum {
    DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_FACING_YAW    = ACTOR_TRANSFORM_ANGLE_TURN / 4, // Yaw the turn stops on crossing
    DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_YAW_STEP      = 0x96,                           // Yaw turned per frame
    DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_SETTLE_FRAMES = 4,                              // Frames held in the settle step
};

/// Work block of the room's event task, parked at `Task::work`.
///
/// The room plays the event once, on a variant-3 visit. Its task allocates the
/// block when it starts and resolves the tasks the event moves: the player and
/// the room's four placed actors. Placements 0 and 1 are the bone sucklers
/// staged for the event; placements 2 and 3 are the bone suckler enemies set
/// down where the event ends. The pointers are borrowed, and all five tasks
/// have to outlive the event task.
///
/// The event script drives the task through `action`. The turn is the only
/// request with state of its own: it takes the shorter way round to the facing
/// yaw, re-placing the player every frame from `playerPlacement`.
typedef struct {
    Task*          playerTask;            // Player task
    Task*          stagedSucklerTasks[2]; // Staged bone sucklers, placements 0 and 1
    Task*          enemySucklerTasks[2];  // Bone suckler enemies, placements 2 and 3
    ActorTransform playerPlacement;       // Placement sent to the player: the position captured as the turn or the skip script begins, and the angles to take
    u16            action;                // Pending request (`DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_*`); cleared once carried out
    u16            actionStep;            // Step of the pending request (`DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_*`); 0 whenever a request is set
    u16            settleFrames;          // Frames spent in the turn's settle step
    byte           field_32[2];           // Never accessed; role unproven
    s16            turnYaw;               // Player yaw less the facing yaw: 0..4095 as the turn begins, stepped until it leaves 0..4096
} _DryfieldMotelRoom1EventWork;
STATIC_ASSERT_SIZEOF(_DryfieldMotelRoom1EventWork, 0x38);

static void _dryfieldMotelRoom1OpeningEventTask(Task* task);

static void _dryfieldMotelRoom1RestorePlayerAfterSkip(void);
static void _dryfieldMotelRoom1RequestEventAction(s16 action);

static void _dryfieldMotelRoom1FinishOpeningScene(void);

static WorldCollisionGrid     _gDryfieldMotelRoom1CollisionGrid;
static WorldCollisionOccluder _gDryfieldMotelRoom1Occluders[1];
static WorldCollisionTrigger  _gDryfieldMotelRoom1ViewBoundaryTriggers[8];
static WorldCollisionTrigger  _gDryfieldMotelRoom1ActionTriggers[8];
static WorldCoordRoomLights   _gDryfieldMotelRoom1RoomLights;

/// Room-local script, view, warp and placement-table selectors.
enum {
    DRYFIELD_MOTEL_ROOM_1_OPENING_CAP_COMMAND       = 5,
    DRYFIELD_MOTEL_ROOM_1_OPENING_SKIP_VIEW         = 5,
    DRYFIELD_MOTEL_ROOM_1_LOGICAL_VIEW_COUNT        = 9,
    DRYFIELD_MOTEL_ROOM_1_WARP_INITIAL_VIEW         = 2,
    DRYFIELD_MOTEL_ROOM_1_WARP_MAP_FLAG             = 487,
    DRYFIELD_MOTEL_ROOM_1_MAIN_STREET_WARP_SLOTS    = (1 << 4) | 2,
    DRYFIELD_MOTEL_ROOM_1_TRIGGER_OPENING_ACTION    = 1,
    DRYFIELD_MOTEL_ROOM_1_BONE_SUCKLER_ENTRY        = 12,
    DRYFIELD_MOTEL_ROOM_1_ODD_STRANGER_ENTRY        = 18,
    DRYFIELD_MOTEL_ROOM_1_BLOOD_SUCKLER_ENTRY       = 40,
    DRYFIELD_MOTEL_ROOM_1_STAGED_BONE_SUCKLER_ENTRY = 112,
    DRYFIELD_MOTEL_ROOM_1_BONE_SUCKLER_FILE         = 12,
    DRYFIELD_MOTEL_ROOM_1_ODD_STRANGER_FILE         = 18,
    DRYFIELD_MOTEL_ROOM_1_BLOOD_SUCKLER_FILE        = 40,
    DRYFIELD_MOTEL_ROOM_1_STAGED_BONE_SUCKLER_FILE  = 232,
};

/// First staging marks for the two cutscene Bone Sucklers, in placement order.
///
/// World XYZ is (500, 0, 1400) and (1200, 0, 2450); all Euler angles are zero.
/// Each matching staged task consumes its record synchronously during placement.
static ActorTransform _gDryfieldMotelRoom1FirstStagingPlacements[2] = {
    { { 500, 0, 1400, 0 }, { 0, 0, 0, 0 } },
    { { 1200, 0, 2450, 0 }, { 0, 0, 0, 0 } },
};

/// Second staging marks for the same two cutscene Bone Sucklers.
///
/// World XYZ is (500, 0, 2000) and (1200, 0, 2400); all Euler angles are zero.
/// Indices match the staged task pair; messages borrow each record synchronously.
static ActorTransform _gDryfieldMotelRoom1SecondStagingPlacements[2] = {
    { { 500, 0, 2000, 0 }, { 0, 0, 0, 0 } },
    { { 1200, 0, 2400, 0 }, { 0, 0, 0, 0 } },
};

/// Final combat placements for the two enemy Bone Sucklers, in task-pair order.
///
/// Both normal and skipped scripts place them at world (500, 0, 2800) and
/// (1000, 0, 3200) with zero Euler angles. Placement borrows records synchronously.
static ActorTransform _gDryfieldMotelRoom1EnemyCombatPlacements[2] = {
    { { 500, 0, 2800, 0 }, { 0, 0, 0, 0 } },
    { { 1000, 0, 3200, 0 }, { 0, 0, 0, 0 } },
};

/// Caption-synchronized opening encounter script, including its END command.
///
/// CAP command 5 cues the player turn, sounds and two staging moves before
/// combat placement and player-control restoration. Callbacks require the
/// initialized opening task; this stream and callback code stay loaded until END.
static EvsCommand _gDryfieldMotelRoom1OpeningScript[20] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_MOTEL_ROOM_1_OPENING_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldMotelRoom1RequestEventAction }, { .value = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_OPENING_SOUND_5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldMotelRoom1RequestEventAction }, { .value = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_TURN_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldMotelRoom1RequestEventAction }, { .value = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SOUND_5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldMotelRoom1RequestEventAction }, { .value = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SOUND_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldMotelRoom1RequestEventAction }, { .value = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_FIRST_STAGING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldMotelRoom1RequestEventAction }, { .value = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SECOND_STAGING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldMotelRoom1FinishOpeningScene }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip path restoring the opening encounter's player pose and combat placements.
///
/// Fades through logical view 5 and scene cleanup, then restores player control.
/// Both combat-placement callbacks are retained, including the repeated final
/// call. The opening task, this stream and callback code must remain live to END.
static EvsCommand _gDryfieldMotelRoom1OpeningSkipScript[13] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_MOTEL_ROOM_1_OPENING_SKIP_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldMotelRoom1RestorePlayerAfterSkip }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldMotelRoom1FinishOpeningScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldMotelRoom1FinishOpeningScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc gDryfieldMotelRoom1OpeningEventTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
    .callback = _dryfieldMotelRoom1OpeningEventTask,
    .data     = { .value = 0 },
};

WorldCollisionRoomResources gDryfieldMotelRoom1RoomCollisionResources[2] = {
    { .grid = &_gDryfieldMotelRoom1CollisionGrid, .viewBoundaryTriggers = _gDryfieldMotelRoom1ViewBoundaryTriggers, .actionTriggers = _gDryfieldMotelRoom1ActionTriggers, .occluders = _gDryfieldMotelRoom1Occluders },
    { .grid = &_gDryfieldMotelRoom1CollisionGrid, .viewBoundaryTriggers = _gDryfieldMotelRoom1ViewBoundaryTriggers, .actionTriggers = _gDryfieldMotelRoom1ActionTriggers, .occluders = _gDryfieldMotelRoom1Occluders },
};

/// Shared map from logical views 1..9 to 1-based camera, image and sprite indices.
///
/// Index by logical view minus one. The last three zero bytes are retained
/// storage, outside the nine-entry reverse-search limit; they are not a terminator.
/// Both room entries borrow this writable map while the overlay is loaded.
static u8 _gDryfieldMotelRoom1ViewMap[12] = {
    1,
    9,
    8,
    4,
    5,
    6,
    7,
    3,
    2,
    0,
    0,
    0,
};

u8* gDryfieldMotelRoom1RoomViewMaps[2] = {
    _gDryfieldMotelRoom1ViewMap,
    _gDryfieldMotelRoom1ViewMap,
};

ViewCount gDryfieldMotelRoom1RoomViewCounts[2] = { DRYFIELD_MOTEL_ROOM_1_LOGICAL_VIEW_COUNT, DRYFIELD_MOTEL_ROOM_1_LOGICAL_VIEW_COUNT };

WorldCoordRoomLighting gDryfieldMotelRoom1RoomLighting[2] = {
    { &_gDryfieldMotelRoom1RoomLights, NULL },
    { &_gDryfieldMotelRoom1RoomLights, NULL },
};

DirectionWarpEntry gDryfieldMotelRoom1WarpEntries[1] = {
    { .player = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 4369, 0, 3388 }, .unknown_10 = { 0, 0, 0, 0 }, .companion = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 4369, 0, 3388 }, .unknown_24 = { 0, 0, 0, 0 }, .arrivalSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_1, 2), .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_1, 1), .blockedSound = DIRECTION_WARP_SOUND_NONE, .initialView = DRYFIELD_MOTEL_ROOM_1_WARP_INITIAL_VIEW, .flags = DIRECTION_WARP_FLAG_NONE, .mapFlagId = DRYFIELD_MOTEL_ROOM_1_WARP_MAP_FLAG },
};

/// Room-space face normals, with 4096 representing unit length.
static SVECTOR _gDryfieldMotelRoom1CollisionGridNormals[] = {
#include "assets/dryfield_motel_room_1_collision_014FC_normals.inc"
};

/// Collision-grid vertices in signed room-coordinate units.
static SVECTOR _gDryfieldMotelRoom1CollisionGridVerts[] = {
#include "assets/dryfield_motel_room_1_collision_014FC_verts.inc"
};

/// Collision-grid quads indexing the vertex/normal pools and surface classes 0..3.
static WorldCollisionGridFace _gDryfieldMotelRoom1CollisionGridFaces[] = {
#include "assets/dryfield_motel_room_1_collision_014FC_faces.inc"
};

/// Face-index storage for four lists ending with WORLD_COLLISION_GRID_CELL_END.
///
/// The cell directory selects starts 0, 38, 74 and 100. Stored zero indices
/// at 73, 99 and 119 fall outside those selected lists and remain retained.
static s16 _gDryfieldMotelRoom1CollisionGridCells[] = {
#include "assets/dryfield_motel_room_1_collision_014FC_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldMotelRoom1CollisionGridCells` (120 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_MOTEL_ROOM_1_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldMotelRoom1CollisionGridCells[(halfwordOffset)])

/// X-major 2-by-2 cell directory borrowing lists in the grid's cell-index array.
static s16* _gDryfieldMotelRoom1CollisionGridTable[] = {
#include "assets/dryfield_motel_room_1_collision_014FC_table.inc"
};
#undef DRYFIELD_MOTEL_ROOM_1_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh and XZ cell index shared by both room entries.
///
/// Four 4000-unit cells start at room (200, 200). The mesh has 38 quads,
/// 85 vertices and nine normals; cell lists select only indices in those pools.
/// Activation binds viewCoord to the current view. The record and its writable
/// pools must remain loaded while the grid is active.
static WorldCollisionGrid _gDryfieldMotelRoom1CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldMotelRoom1CollisionGridNormals,
    .vertices    = _gDryfieldMotelRoom1CollisionGridVerts,
    .faces       = _gDryfieldMotelRoom1CollisionGridFaces,
    .cellFaceIds = _gDryfieldMotelRoom1CollisionGridTable,
    .xBias       = -200,
    .zBias       = -200,
    .cellCountX  = 2,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldMotelRoom1CollisionGridFaces),
};

ViewCamera gDryfieldMotelRoom1ViewCameras[9] = {
    { .transform = { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -2500, 0x2968, -3000 } }, .screenDistance = 257 },
    { .transform = { { { 1367, 0, -3861 }, { -708, 4026, -250 }, { 3795, 751, 1344 } }, { -200, 1600, -940 } }, .screenDistance = 212 },
    { .transform = { { { 1557, 0, 3788 }, { 539, 4054, -221 }, { -3749, 583, 1541 } }, { -4800, 1450, -840 } }, .screenDistance = 216 },
    { .transform = { { { 4033, 0, 714 }, { 53, 4084, -300 }, { -712, 305, 4021 } }, { -4400, 1350, -1640 } }, .screenDistance = 269 },
    { .transform = { { { -508, 0, -4064 }, { -1596, 3766, 199 }, { 3737, 1609, -467 } }, { -200, 1850, -5690 } }, .screenDistance = 246 },
    { .transform = { { { -459, 0, 4070 }, { 2448, 3271, 276 }, { -3250, 2464, -367 } }, { -3200, 2600, -5740 } }, .screenDistance = 246 },
    { .transform = { { { -242, 0, 4088 }, { 396, 4076, 23 }, { -4069, 397, -240 } }, { -3361, 275, -2562 } }, .screenDistance = 329 },
    { .transform = { { { 1271, 0, 3893 }, { 1803, 3630, -588 }, { -3450, 1897, 1126 } }, { -4561, 2558, -1173 } }, .screenDistance = 257 },
    { .transform = { { { 1324, 0, -3876 }, { -1721, 3669, -588 }, { 3472, 1819, 1186 } }, { -239, 2334, -1269 } }, .screenDistance = 225 },
};

/// VRAM locations of the room-view sheets' 8-bit texture pages and palette rows.
///
/// Texture pages start at Y zero. Neutral RGB is 1.0 in the GPU's seven-fractional-bit scale.
enum {
    DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832      = getTPage(1, GPU_BLEND_AVERAGE, 832, 0),
    DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896      = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960      = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255  = getClut(0, 255),
    DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_256  = getClut(0, 256),
    DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR = 128,
};

/// Empty sprite list for mapped view 1 (logical view 1).
///
/// The zero-count first record keeps decoded background strips; the second
/// ends the list. Its address is retained in the empty source slot but never
/// dereferenced as source sprites. Storage belongs to the loaded room overlay.
static SpriteBatch _gDryfieldMotelRoom1MappedView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 2 (logical view 9).
///
/// The zero-count first record keeps decoded background strips; the second
/// ends the list. Its address is retained in the empty source slot but never
/// dereferenced as source sprites. Storage belongs to the loaded room overlay.
static SpriteBatch _gDryfieldMotelRoom1MappedView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 3 (logical view 8).
///
/// The zero-count first record keeps decoded background strips; the second
/// ends the list. Its address is retained in the empty source slot but never
/// dereferenced as source sprites. Storage belongs to the loaded room overlay.
static SpriteBatch _gDryfieldMotelRoom1MappedView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// 42 depth-sorted room sprites for mapped view 4 (logical view 4).
///
/// Rectangles and positions use pixels relative to the view centre; depth
/// is an unscaled sorting value. The view's batches select source elements.
/// Texture pages contain 8-bit texels; RGB is neutral and code flags are zero.
/// Every sprite uses palette row 255 from this view's loaded image bundle.
/// Sources remain writable and borrowed while the room overlay is loaded.
static SpriteSource _gDryfieldMotelRoom1MappedView4SpriteSources[] = {
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -160, 64, 625, { .fields = { 32, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -160, 48, 625, { .fields = { 88, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -160, 16, 625, { .fields = { 112, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -40, 32, 725, { .fields = { 80, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -160, -120, 712, { .fields = { 8, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -160, -80, 712, { .fields = { 120, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -160, -40, 712, { .fields = { 8, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -160, 0, 712, { .fields = { 24, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -160, 40, 712, { .fields = { 24, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -160, 80, 712, { .fields = { 72, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -120, 80, 712, { .fields = { 72, 32 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -80, 80, 712, { .fields = { 72, 56 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -120, 40, 712, { .fields = { 24, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -80, 40, 712, { .fields = { 112, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -120, 0, 712, { .fields = { 112, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -120, -40, 712, { .fields = { 112, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -120, -80, 712, { .fields = { 96, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -120, -120, 712, { .fields = { 96, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -80, -120, 712, { .fields = { 88, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -80, -80, 712, { .fields = { 88, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -80, -40, 712, { .fields = { 64, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -80, 0, 712, { .fields = { 64, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, -40, 40, 725, { .fields = { 120, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -40, 80, 725, { .fields = { 104, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -56, 64, 725, { .fields = { 104, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -56, 24, 725, { .fields = { 104, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -56, -16, 725, { .fields = { 104, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -56, -56, 725, { .fields = { 104, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -56, -96, 725, { .fields = { 48, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, -64, -96, 712, { .fields = { 72, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -64, -120, 712, { .fields = { 104, 224 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -56, -120, 725, { .fields = { 80, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -32, -120, 1025, { .fields = { 48, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -32, -80, 1025, { .fields = { 48, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -32, -40, 1025, { .fields = { 64, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -32, 0, 1025, { .fields = { 80, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, -32, 40, 1025, { .fields = { 88, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -8, 40, 1056, { .fields = { 112, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -8, 0, 1056, { .fields = { 88, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -8, -40, 1056, { .fields = { 72, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -8, -80, 1056, { .fields = { 72, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -8, -96, 1056, { .fields = { 104, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped view 4 (logical view 4), ending with SPRITE_BATCH_END.
///
/// The ranges cover all 42 source elements. The zero first count keeps
/// decoded background strips; every range is initially visible and included
/// in cached packets. The room overlay owns these mutable records.
static SpriteBatch _gDryfieldMotelRoom1MappedView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 21, 0, 0, { 0, 0 } },
    { 24, 8, 0, 0, { 2, 0 } },
    { 32, ARRAY_SIZE(_gDryfieldMotelRoom1MappedView4SpriteSources) - 32, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// 85 depth-sorted room sprites for mapped view 5 (logical view 5).
///
/// Rectangles and positions use pixels relative to the view centre; depth
/// is an unscaled sorting value. The view's batches select source elements.
/// Texture pages contain 8-bit texels; RGB is neutral and code flags are zero.
/// Every sprite uses palette row 255 from this view's loaded image bundle.
/// Sources remain writable and borrowed while the room overlay is loaded.
static SpriteSource _gDryfieldMotelRoom1MappedView5SpriteSources[] = {
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -160, -120, 725, { .fields = { 0, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -160, -80, 725, { .fields = { 0, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -160, -40, 725, { .fields = { 16, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -160, 0, 725, { .fields = { 40, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 40, 725, { .fields = { 72, 8 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -120, 40, 725, { .fields = { 88, 248 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -120, 0, 700, { .fields = { 40, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -120, -40, 700, { .fields = { 88, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -120, -80, 700, { .fields = { 88, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -120, -120, 700, { .fields = { 32, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -80, -120, 700, { .fields = { 88, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -80, -80, 700, { .fields = { 88, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -80, 0, 731, { .fields = { 88, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -80, -40, 728, { .fields = { 88, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -40, 0, 731, { .fields = { 104, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, -40, -40, 725, { .fields = { 80, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, -40, -80, 700, { .fields = { 80, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, -40, -120, 700, { .fields = { 120, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, 32, 0, 836, { .fields = { 72, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, 32, -40, 836, { .fields = { 56, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, 40, -80, 800, { .fields = { 32, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, 40, -120, 800, { .fields = { 32, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -32, -120, 750, { .fields = { 80, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 8, -120, 750, { .fields = { 0, 248 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, 120, -120, 537, { .fields = { 120, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, 120, -80, 537, { .fields = { 88, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, 120, -40, 537, { .fields = { 72, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, 120, 0, 537, { .fields = { 120, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, 120, 40, 537, { .fields = { 80, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, 120, 80, 537, { .fields = { 80, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, 88, 80, 656, { .fields = { 112, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, 88, 40, 656, { .fields = { 96, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, 88, 0, 653, { .fields = { 0, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, 88, -40, 637, { .fields = { 48, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, 88, -80, 631, { .fields = { 40, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, 88, -120, 631, { .fields = { 56, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, 64, -120, 700, { .fields = { 64, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, 64, -80, 700, { .fields = { 16, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, 64, -40, 736, { .fields = { 112, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, 64, 0, 750, { .fields = { 48, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, 64, 40, 756, { .fields = { 56, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, 72, 80, 762, { .fields = { 104, 168 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 48, 40, 837, { .fields = { 72, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 48, 0, 831, { .fields = { 40, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 48, -40, 831, { .fields = { 32, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 48, -80, 800, { .fields = { 40, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 48, -120, 800, { .fields = { 40, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 40, -120, 831, { .fields = { 40, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 40, -80, 831, { .fields = { 56, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 40, -40, 840, { .fields = { 32, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 40, 0, 846, { .fields = { 32, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, 40, 40, 846, { .fields = { 40, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 32, 0, 866, { .fields = { 8, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 32, -40, 837, { .fields = { 16, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 32, -64, 831, { .fields = { 112, 192 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -80, 56, 675, { .fields = { 80, 144 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -40, 40, 687, { .fields = { 96, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -40, 24, 700, { .fields = { 112, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -96, 16, 750, { .fields = { 104, 216 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -80, 24, 700, { .fields = { 0, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -80, 40, 687, { .fields = { 32, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -88, 40, 687, { .fields = { 72, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -96, 24, 700, { .fields = { 80, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -112, 56, 575, { .fields = { 48, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -120, 40, 600, { .fields = { 80, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 32, 625, { .fields = { 72, 232 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -112, 24, 637, { .fields = { 120, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -96, 72, 437, { .fields = { 112, 224 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -88, 112, 437, { .fields = { 104, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -160, -120, 412, { .fields = { 16, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -160, -80, 412, { .fields = { 8, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -160, -40, 412, { .fields = { 64, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -160, 0, 412, { .fields = { 64, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -160, 40, 412, { .fields = { 8, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -144, 80, 412, { .fields = { 112, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, -136, -120, 412, { .fields = { 88, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -136, -80, 412, { .fields = { 24, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -136, -40, 412, { .fields = { 112, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -136, 0, 412, { .fields = { 104, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -136, 40, 412, { .fields = { 120, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -136, 80, 412, { .fields = { 120, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, -112, 0, 437, { .fields = { 8, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -112, 40, 437, { .fields = { 16, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_832, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -112, 80, 437, { .fields = { 120, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, -96, 80, 437, { .fields = { 0, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped view 5 (logical view 5), ending with SPRITE_BATCH_END.
///
/// The ranges cover all 85 source elements. The zero first count keeps
/// decoded background strips; every range is initially visible and included
/// in cached packets. The room overlay owns these mutable records.
static SpriteBatch _gDryfieldMotelRoom1MappedView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { 24, 31, 0, 0, { 3, 0 } },
    { 55, 8, 0, 0, { 2, 0 } },
    { 63, 4, 0, 0, { 4, 0 } },
    { 67, ARRAY_SIZE(_gDryfieldMotelRoom1MappedView5SpriteSources) - 67, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// 61 depth-sorted room sprites for mapped view 6 (logical view 6).
///
/// Rectangles and positions use pixels relative to the view centre; depth
/// is an unscaled sorting value. The view's batches select source elements.
/// Texture pages contain 8-bit texels; RGB is neutral and code flags are zero.
/// Every sprite uses palette row 255 from this view's loaded image bundle.
/// Sources remain writable and borrowed while the room overlay is loaded.
static SpriteSource _gDryfieldMotelRoom1MappedView6SpriteSources[] = {
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -160, -120, 400, { .fields = { 32, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -160, -80, 407, { .fields = { 32, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -160, -40, 431, { .fields = { 32, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -160, 0, 456, { .fields = { 8, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -160, 40, 456, { .fields = { 24, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -160, 80, 456, { .fields = { 32, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -128, -120, 425, { .fields = { 64, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -128, -80, 437, { .fields = { 64, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -128, -40, 450, { .fields = { 64, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -128, 0, 570, { .fields = { 40, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -128, 40, 575, { .fields = { 56, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -128, 80, 575, { .fields = { 64, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -104, -120, 750, { .fields = { 8, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -104, -80, 750, { .fields = { 112, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -104, -40, 800, { .fields = { 112, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -104, 0, 830, { .fields = { 112, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -104, 40, 830, { .fields = { 112, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -104, 80, 830, { .fields = { 8, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -80, -120, 867, { .fields = { 8, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -80, -80, 868, { .fields = { 8, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -80, -40, 878, { .fields = { 112, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -80, 0, 900, { .fields = { 0, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -80, 40, 906, { .fields = { 64, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -80, 80, 881, { .fields = { 56, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -56, -120, 915, { .fields = { 104, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -56, -80, 921, { .fields = { 104, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -56, -40, 937, { .fields = { 96, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -56, 0, 962, { .fields = { 96, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 80, 40, 475, { .fields = { 112, 248 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 40, 80, 500, { .fields = { 112, 32 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 56, 80, 50, { .fields = { 120, 248 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 64, 80, 500, { .fields = { 120, 32 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 80, -120, 475, { .fields = { 104, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 88, -120, 475, { .fields = { 112, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, 104, -120, 475, { .fields = { 104, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, 104, -80, 475, { .fields = { 112, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 88, -80, 475, { .fields = { 112, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 80, -80, 475, { .fields = { 120, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 72, -80, 475, { .fields = { 120, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 88, -40, 475, { .fields = { 112, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 80, -40, 475, { .fields = { 88, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 72, -40, 475, { .fields = { 88, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 64, -40, 475, { .fields = { 88, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, 88, 0, 500, { .fields = { 64, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 80, 0, 500, { .fields = { 80, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 64, 0, 500, { .fields = { 88, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 72, 0, 500, { .fields = { 88, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 56, 0, 500, { .fields = { 96, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 72, 40, 500, { .fields = { 96, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 64, 40, 500, { .fields = { 96, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 56, 40, 500, { .fields = { 88, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 48, 40, 500, { .fields = { 96, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, 48, 80, 50, { .fields = { 120, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 88, 80, 562, { .fields = { 32, 248 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 56, 104, 500, { .fields = { 72, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, 88, 104, 500, { .fields = { 104, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 56, 96, 500, { .fields = { 8, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 88, 96, 500, { .fields = { 40, 248 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 88, 88, 525, { .fields = { 40, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 56, 88, 525, { .fields = { 104, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 56, 80, 562, { .fields = { 0, 248 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped view 6 (logical view 6), ending with SPRITE_BATCH_END.
///
/// The ranges cover all 61 source elements. The zero first count keeps
/// decoded background strips; every range is initially visible and included
/// in cached packets. The room overlay owns these mutable records.
static SpriteBatch _gDryfieldMotelRoom1MappedView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 28, 0, 0, { 1, 0 } },
    { 28, 25, 0, 0, { 2, 0 } },
    { 53, ARRAY_SIZE(_gDryfieldMotelRoom1MappedView6SpriteSources) - 53, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// 29 depth-sorted room sprites for mapped view 7 (logical view 7).
///
/// Rectangles and positions use pixels relative to the view centre; depth
/// is an unscaled sorting value. The view's batches select source elements.
/// Texture pages contain 8-bit texels; RGB is neutral and code flags are zero.
/// Every sprite uses palette row 255 from this view's loaded image bundle.
/// Sources remain writable and borrowed while the room overlay is loaded.
static SpriteSource _gDryfieldMotelRoom1MappedView7SpriteSources[] = {
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -32, -104, 500, { .fields = { 88, 192 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -160, -120, 500, { .fields = { 40, 24 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -160, -96, 500, { .fields = { 40, 48 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -136, -120, 500, { .fields = { 48, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -136, -96, 500, { .fields = { 48, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -112, -120, 500, { .fields = { 48, 72 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -112, -96, 500, { .fields = { 40, 232 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -88, -120, 500, { .fields = { 32, 96 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -88, -96, 500, { .fields = { 104, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -64, -120, 500, { .fields = { 32, 208 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -64, -96, 500, { .fields = { 96, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -32, -96, 500, { .fields = { 56, 96 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -160, -104, 375, { .fields = { 88, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -160, -64, 375, { .fields = { 24, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -120, -64, 375, { .fields = { 88, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -120, -104, 375, { .fields = { 88, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -80, -104, 375, { .fields = { 88, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -80, -64, 375, { .fields = { 16, 184 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -40, -104, 375, { .fields = { 64, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -40, -64, 375, { .fields = { 48, 136 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, 0, -64, 375, { .fields = { 80, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -24, -40, 375, { .fields = { 64, 104 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -24, -16, 375, { .fields = { 72, 72 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -64, -96, 330, { .fields = { 72, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -56, -56, 330, { .fields = { 104, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, -56, -16, 330, { .fields = { 72, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, -56, 16, 330, { .fields = { 64, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -8, -56, 812, { .fields = { 80, 104 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -8, -24, 812, { .fields = { 56, 184 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped view 7 (logical view 7), ending with SPRITE_BATCH_END.
///
/// The ranges cover all 29 source elements. The zero first count keeps
/// decoded background strips; every range is initially visible and included
/// in cached packets. The room overlay owns these mutable records.
static SpriteBatch _gDryfieldMotelRoom1MappedView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 3, 0 } },
    { 12, 11, 0, 0, { 0, 0 } },
    { 23, 4, 0, 0, { 2, 0 } },
    { 27, ARRAY_SIZE(_gDryfieldMotelRoom1MappedView7SpriteSources) - 27, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// 111 depth-sorted room sprites for mapped view 8 (logical view 3).
///
/// Rectangles and positions use pixels relative to the view centre; depth
/// is an unscaled sorting value. The view's batches select source elements.
/// Texture pages contain 8-bit texels; RGB is neutral and code flags are zero.
/// Every sprite uses palette row 255 from this view's loaded image bundle.
/// Sources remain writable and borrowed while the room overlay is loaded.
static SpriteSource _gDryfieldMotelRoom1MappedView8SpriteSources[] = {
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 40, 56, 500, { .fields = { 64, 24 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -88, 64, 487, { .fields = { 64, 32 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -128, 72, 475, { .fields = { 64, 144 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 80, 112, 425, { .fields = { 64, 88 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -160, 96, 425, { .fields = { 80, 232 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -120, 96, 425, { .fields = { 88, 144 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -80, 96, 425, { .fields = { 88, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -40, 96, 425, { .fields = { 72, 24 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 0, 96, 425, { .fields = { 120, 232 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 40, 96, 425, { .fields = { 104, 96 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -160, 80, 462, { .fields = { 72, 184 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -120, 80, 462, { .fields = { 72, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -80, 80, 462, { .fields = { 72, 216 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -40, 80, 462, { .fields = { 64, 96 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 0, 80, 462, { .fields = { 72, 48 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 40, 80, 462, { .fields = { 96, 168 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -160, 64, 475, { .fields = { 96, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -120, 72, 475, { .fields = { 48, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -80, 72, 475, { .fields = { 48, 72 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -40, 72, 475, { .fields = { 48, 128 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 0, 72, 475, { .fields = { 48, 136 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 40, 72, 475, { .fields = { 32, 24 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -80, 64, 487, { .fields = { 64, 112 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -40, 64, 487, { .fields = { 48, 64 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 0, 64, 487, { .fields = { 48, 16 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 40, 64, 487, { .fields = { 72, 144 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -40, 56, 500, { .fields = { 32, 48 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 0, 56, 500, { .fields = { 32, 56 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -16, 48, 537, { .fields = { 56, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -56, 56, 500, { .fields = { 56, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 0, 48, 537, { .fields = { 32, 8 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -56, -16, 1075, { .fields = { 80, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -8, -16, 1075, { .fields = { 80, 152 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -8, -8, 1042, { .fields = { 72, 176 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 0, -8, 1042, { .fields = { 72, 168 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -144, 48, 1000, { .fields = { 112, 24 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, -120, 48, 1000, { .fields = { 104, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, -96, 48, 1006, { .fields = { 80, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -72, 48, 1012, { .fields = { 56, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -144, 24, 1050, { .fields = { 112, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -120, 24, 1050, { .fields = { 112, 72 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -96, 24, 1050, { .fields = { 112, 48 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -72, 24, 1050, { .fields = { 8, 72 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -48, 24, 1025, { .fields = { 0, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -144, 0, 1050, { .fields = { 0, 136 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -120, 0, 1050, { .fields = { 112, 184 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -96, 0, 1050, { .fields = { 88, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -72, 0, 1045, { .fields = { 112, 208 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -48, 0, 1045, { .fields = { 8, 48 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -48, -8, 1045, { .fields = { 56, 232 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -48, -16, 1075, { .fields = { 56, 208 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -72, -8, 1050, { .fields = { 56, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -96, -8, 1062, { .fields = { 48, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -120, -8, 1062, { .fields = { 56, 248 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -24, 0, 1045, { .fields = { 32, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -24, -8, 1045, { .fields = { 64, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -24, -16, 1075, { .fields = { 64, 152 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -8, 0, 1037, { .fields = { 48, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 0, 0, 1037, { .fields = { 40, 96 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, 8, 8, 1037, { .fields = { 64, 96 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -8, 24, 1025, { .fields = { 104, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 0, 24, 1025, { .fields = { 40, 232 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 8, 24, 1025, { .fields = { 40, 208 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -24, 24, 1025, { .fields = { 80, 232 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -152, 16, 887, { .fields = { 40, 64 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -120, 24, 900, { .fields = { 40, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 32 } }, -160, 48, 875, { .fields = { 32, 32 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -120, 48, 887, { .fields = { 40, 176 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -152, 80, 862, { .fields = { 40, 144 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -120, 80, 875, { .fields = { 8, 208 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, 136, 40, 1125, { .fields = { 8, 24 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, 96, 0, 1200, { .fields = { 56, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, 96, -120, 1152, { .fields = { 88, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 88, -120, 1375, { .fields = { 104, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 80, -40, 1311, { .fields = { 104, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, 80, 0, 1327, { .fields = { 0, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 72, -24, 1450, { .fields = { 32, 88 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 120, -16, 1025, { .fields = { 88, 64 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, 144, -16, 950, { .fields = { 16, 96 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 88, -80, 1292, { .fields = { 96, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 80, -80, 1376, { .fields = { 104, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, 104, -40, 1126, { .fields = { 24, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, 104, -16, 1206, { .fields = { 72, 144 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 96, -40, 1203, { .fields = { 104, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 96, -80, 1125, { .fields = { 48, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, 96, -64, 1099, { .fields = { 8, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 120, -80, 1064, { .fields = { 112, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 136, -80, 960, { .fields = { 112, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 152, -80, 944, { .fields = { 120, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 152, -40, 898, { .fields = { 32, 232 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 144, -40, 931, { .fields = { 32, 64 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, 120, -120, 985, { .fields = { 104, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 144, -120, 888, { .fields = { 112, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 136, -40, 1030, { .fields = { 72, 232 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, 120, -40, 992, { .fields = { 48, 96 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 120, 0, 1050, { .fields = { 112, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 136, 0, 950, { .fields = { 80, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 152, 0, 950, { .fields = { 96, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, 104, 16, 1050, { .fields = { 72, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, 104, -24, 1075, { .fields = { 72, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, 136, -16, 1075, { .fields = { 48, 208 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 136, 16, 1050, { .fields = { 88, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 120, -16, 1150, { .fields = { 72, 32 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, 80, 8, 1125, { .fields = { 16, 112 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, 104, 8, 1125, { .fields = { 24, 136 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, 80, -24, 1150, { .fields = { 72, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, 104, -24, 1150, { .fields = { 72, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 80, -48, 1227, { .fields = { 8, 184 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, 64, -40, 1230, { .fields = { 72, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, 80, -24, 1267, { .fields = { 80, 168 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, 48, -24, 1250, { .fields = { 64, 112 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped view 8 (logical view 3), ending with SPRITE_BATCH_END.
///
/// The ranges cover all 111 source elements. The zero first count keeps
/// decoded background strips; every range is initially visible and included
/// in cached packets. The room overlay owns these mutable records.
static SpriteBatch _gDryfieldMotelRoom1MappedView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 31, 0, 0, { 6, 0 } },
    { 31, 33, 0, 0, { 1, 0 } },
    { 64, 6, 0, 0, { 4, 0 } },
    { 70, 28, 0, 0, { 0, 0 } },
    { 98, 4, 0, 0, { 5, 0 } },
    { 102, 5, 0, 0, { 3, 0 } },
    { 107, 3, 0, 0, { 7, 0 } },
    { 110, ARRAY_SIZE(_gDryfieldMotelRoom1MappedView8SpriteSources) - 110, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// 67 depth-sorted room sprites for mapped view 9 (logical view 2).
///
/// Rectangles and positions use pixels relative to the view centre; depth
/// is an unscaled sorting value. The view's batches select source elements.
/// Texture pages contain 8-bit texels; RGB is neutral and code flags are zero.
/// The first 65 sprites use palette row 255; the last two use row 256.
/// Sources remain writable and borrowed while the room overlay is loaded.
static SpriteSource _gDryfieldMotelRoom1MappedView9SpriteSources[] = {
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -8, 8, 837, { .fields = { 88, 192 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 144, 16, 800, { .fields = { 80, 104 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 128, 96, 800, { .fields = { 120, 128 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 96, 96, 800, { .fields = { 112, 112 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 80, 96, 800, { .fields = { 56, 48 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 128, 72, 800, { .fields = { 120, 48 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 128, 48, 825, { .fields = { 120, 24 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 128, 24, 800, { .fields = { 112, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 96, 72, 800, { .fields = { 120, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 96, 48, 825, { .fields = { 120, 152 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 96, 24, 800, { .fields = { 24, 128 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 64, 72, 800, { .fields = { 24, 48 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 64, 48, 825, { .fields = { 24, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 64, 24, 800, { .fields = { 16, 224 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 64, 16, 800, { .fields = { 32, 72 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 32, 72, 800, { .fields = { 8, 176 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 0, 72, 800, { .fields = { 112, 224 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -32, 72, 800, { .fields = { 112, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 32, 48, 825, { .fields = { 16, 104 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 32, 24, 800, { .fields = { 16, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 32, 8, 800, { .fields = { 88, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 0, 8, 800, { .fields = { 104, 176 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 0, 24, 800, { .fields = { 24, 24 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 0, 48, 825, { .fields = { 24, 152 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -32, 48, 812, { .fields = { 0, 72 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -40, 56, 825, { .fields = { 104, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -32, 32, 812, { .fields = { 88, 24 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -24, 24, 825, { .fields = { 112, 192 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -16, 16, 831, { .fields = { 56, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, 80, 24, 1025, { .fields = { 104, 56 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 64, 0, 1025, { .fields = { 56, 208 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, 64, -24, 1037, { .fields = { 72, 184 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 112, 24, 1025, { .fields = { 48, 104 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, 96, 0, 1025, { .fields = { 64, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, 96, -24, 1050, { .fields = { 72, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 112, -24, 1050, { .fields = { 56, 56 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 112, 0, 1025, { .fields = { 72, 232 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, 96, 24, 1025, { .fields = { 72, 32 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 96, 112, 587, { .fields = { 80, 112 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, 136, 96, 562, { .fields = { 48, 232 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 104, 96, 575, { .fields = { 40, 176 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 104, 72, 575, { .fields = { 32, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 136, 80, 562, { .fields = { 104, 72 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, -136, 40, 1046, { .fields = { 88, 184 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -160, -120, 963, { .fields = { 88, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -160, -80, 963, { .fields = { 104, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -160, -40, 963, { .fields = { 88, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -160, 0, 963, { .fields = { 88, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -160, 40, 963, { .fields = { 56, 128 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -136, -120, 1046, { .fields = { 112, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -136, -80, 1046, { .fields = { 112, 40 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -136, -40, 1046, { .fields = { 112, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -136, 0, 1046, { .fields = { 112, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -120, -104, 1112, { .fields = { 64, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -120, -80, 1112, { .fields = { 112, 80 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -120, -40, 1112, { .fields = { 96, 160 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -120, 0, 1112, { .fields = { 96, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -104, 0, 1175, { .fields = { 56, 152 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -104, -32, 1175, { .fields = { 80, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -160, 0, 787, { .fields = { 88, 200 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -144, 8, 825, { .fields = { 56, 24 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -160, 32, 787, { .fields = { 80, 120 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, -144, 32, 825, { .fields = { 72, 152 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -160, 64, 787, { .fields = { 48, 0 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -144, 64, 825, { .fields = { 112, 240 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_896, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, -88, 104, 237, { .fields = { 112, 96 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_MOTEL_ROOM_1_SPRITE_PAGE_960, DRYFIELD_MOTEL_ROOM_1_SPRITE_CLUT_ROW_256, { .fields = { 32, 8 } }, -56, 112, 237, { .fields = { 16, 248 } }, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, DRYFIELD_MOTEL_ROOM_1_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped view 9 (logical view 2), ending with SPRITE_BATCH_END.
///
/// The ranges cover all 67 source elements. The zero first count keeps
/// decoded background strips; every range is initially visible and included
/// in cached packets. The second zero-count range is retained too.
/// The room overlay owns these mutable records.
static SpriteBatch _gDryfieldMotelRoom1MappedView9SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 4, 0 } },
    { 0, 29, 0, 0, { 5, 0 } },
    { 29, 3, 0, 0, { 2, 0 } },
    { 32, 6, 0, 0, { 6, 0 } },
    { 38, 5, 0, 0, { 1, 0 } },
    { 43, 16, 0, 0, { 7, 0 } },
    { 59, 6, 0, 0, { 3, 0 } },
    { 65, ARRAY_SIZE(_gDryfieldMotelRoom1MappedView9SpriteSources) - 65, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldMotelRoom1SpriteViews[9] = {
    { .sources = { .empty = _gDryfieldMotelRoom1MappedView1SpriteBatches }, .batches = _gDryfieldMotelRoom1MappedView1SpriteBatches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldMotelRoom1MappedView2SpriteBatches }, .batches = _gDryfieldMotelRoom1MappedView2SpriteBatches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldMotelRoom1MappedView3SpriteBatches }, .batches = _gDryfieldMotelRoom1MappedView3SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldMotelRoom1MappedView4SpriteSources }, .batches = _gDryfieldMotelRoom1MappedView4SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldMotelRoom1MappedView5SpriteSources }, .batches = _gDryfieldMotelRoom1MappedView5SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldMotelRoom1MappedView6SpriteSources }, .batches = _gDryfieldMotelRoom1MappedView6SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldMotelRoom1MappedView7SpriteSources }, .batches = _gDryfieldMotelRoom1MappedView7SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldMotelRoom1MappedView8SpriteSources }, .batches = _gDryfieldMotelRoom1MappedView8SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldMotelRoom1MappedView9SpriteSources }, .batches = _gDryfieldMotelRoom1MappedView9SpriteBatches, .drawAreas = NULL },
};

/// Eight directed boundaries between logical views 2, 3, 4, 5 and 6.
///
/// Pairs cover 3/2, 2/4, 4/5 and 6/5 in both directions. Positions/radii use
/// room units and normals use 4096 per unit. Setup binds, links and enables
/// every record through the final LAST flag; storage remains live until unlinked.
static WorldCollisionTrigger _gDryfieldMotelRoom1ViewBoundaryTriggers[8] = {
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2527, -1344, 2927, 0 }, .vertices = { { 22, 1888, 1715, 0 }, { 22, -1888, 1715, 0 }, { -23, 1888, -1716, 0 }, { -23, -1888, -1716, 0 } }, .normal = { -4104, 0, 53, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2547, .control = 0, .parameter0 = 3, .parameter1 = 2, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2368, -1232, 2880, 0 }, .vertices = { { -23, 1808, -1716, 0 }, { -23, -1808, -1716, 0 }, { 22, 1808, 1715, 0 }, { 22, -1808, 1715, 0 } }, .normal = { 4108, 0, -55, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2482, .control = 0, .parameter0 = 2, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 4047, -1264, 4559, 0 }, .vertices = { { -995, 1776, -5, 0 }, { -995, -1776, -5, 0 }, { 995, 1776, 5, 0 }, { 995, -1776, 5, 0 } }, .normal = { 19, 0, -4105, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2031, .control = 0, .parameter0 = 2, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 4095, -1248, 4463, 0 }, .vertices = { { 1011, 1792, -11, 0 }, { 1011, -1792, -11, 0 }, { -1011, 1792, 11, 0 }, { -1011, -1792, 11, 0 } }, .normal = { 43, 0, 4095, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2048, .control = 0, .parameter0 = 4, .parameter1 = 2, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 3200, -1280, 5632, 0 }, .vertices = { { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 }, { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 } }, .normal = { 4101, 0, 19, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2031, .control = 0, .parameter0 = 4, .parameter1 = 5, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 3296, -1344, 5824, 0 }, .vertices = { { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 }, { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 } }, .normal = { -4105, 0, -22, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2031, .control = 0, .parameter0 = 5, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 1920, -1312, 5408, 0 }, .vertices = { { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 }, { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 } }, .normal = { -4105, 0, -22, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2031, .control = 0, .parameter0 = 6, .parameter1 = 5, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 1760, -1344, 5408, 0 }, .vertices = { { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 }, { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 } }, .normal = { 4101, 0, 19, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2031, .control = 0, .parameter0 = 5, .parameter1 = 6, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, .hit = 0 },
};

/// Motel room 1's exit, CAP interactions and automatic opening-event region.
///
/// The exit requests Main Street using departure slot 1 and arrival slot 2.
/// Six facing quads select CAP commands 4, 8, 6, 6, 7 and 1; the remaining
/// quad sends room action 1 without requiring the interaction button.
/// Geometry uses room units and Q12 normals. Binding mutates coordinates,
/// links and hit latches; the LAST-marked eighth record is included. Keep this
/// overlay loaded until every record is unlinked.
static WorldCollisionTrigger _gDryfieldMotelRoom1ActionTriggers[8] = {
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 4544, -50, 3488, 0 }, .vertices = { { -256, 0, -480, 0 }, { 256, 0, -480, 0 }, { -256, 0, 480, 0 }, { 256, 0, 480, 0 } }, .normal = { 0, 4095, 0, 0 }, .facingNormal = { -ONE, 0, 0, 0 }, .radius = 543, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP, .parameter0 = GAME_AREA_DRYFIELD_MAIN_STREET, .parameter1 = DRYFIELD_MOTEL_ROOM_1_MAIN_STREET_WARP_SLOTS, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 384, -64, 3216, 0 }, .vertices = { { -224, 0, -368, 0 }, { 224, 0, -368, 0 }, { -224, 0, 368, 0 }, { 224, 0, 368, 0 } }, .normal = { 0, 4095, 0, 0 }, .facingNormal = { ONE, 0, 0, 0 }, .radius = 430, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 4, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2880, -64, 5504, 0 }, .vertices = { { -160, 0, -1024, 0 }, { 160, 0, -1024, 0 }, { -160, 0, 1024, 0 }, { 160, 0, 1024, 0 } }, .normal = { 0, 4095, 0, 0 }, .facingNormal = { -ONE, 0, 0, 0 }, .radius = 1031, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, .parameter0 = DRYFIELD_MOTEL_ROOM_1_TRIGGER_OPENING_ACTION, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2168, -64, 3760, 0 }, .vertices = { { -568, 0, -336, 0 }, { 232, 0, -336, 0 }, { -568, 0, 336, 0 }, { 904, 0, 336, 0 } }, .normal = { 0, ONE, 0, 0 }, .facingNormal = { 799, 0, -4017, 0 }, .radius = 964, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 8, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 720, -64, 3600, 0 }, .vertices = { { -432, 0, -256, 0 }, { 432, 0, -256, 0 }, { -432, 0, 256, 0 }, { 432, 0, 256, 0 } }, .normal = { 0, 4097, 0, 0 }, .facingNormal = { 0, 0, -ONE, 0 }, .radius = 501, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 6, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 1728, -64, 5632, 0 }, .vertices = { { -368, 0, -256, 0 }, { 368, 0, -256, 0 }, { -368, 0, 256, 0 }, { 368, 0, 256, 0 } }, .normal = { 0, 4099, 0, 0 }, .facingNormal = { 0, 0, -ONE, 0 }, .radius = 448, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 6, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2689, -64, 5600, 0 }, .vertices = { { -432, 0, -256, 0 }, { 432, 0, -256, 0 }, { -432, 0, 256, 0 }, { 432, 0, 256, 0 } }, .normal = { 0, 4097, 0, 0 }, .facingNormal = { 201, 0, -4091, 0 }, .radius = 501, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 7, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 4176, -64, 5600, 0 }, .vertices = { { -736, 0, -256, 0 }, { 736, 0, -256, 0 }, { -736, 0, 256, 0 }, { 736, 0, 256, 0 } }, .normal = { 0, 4099, 0, 0 }, .facingNormal = { 0, 0, -ONE, 0 }, .radius = 778, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 1, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, .hit = 0 },
};

/// One room-space sight-blocking quad centred at (1760, -1408, 4512).
///
/// Its relative corners span X +/-1632 and Y +/-2432; its normal uses 4096
/// per unit. The live record also carries LAST. Linking mutates its list state;
/// bit 0 and the final zero byte retain unproven meanings. Keep storage loaded
/// until unlinked; the stored radius is not used by segment sight tests.
static WorldCollisionOccluder _gDryfieldMotelRoom1Occluders[1] = {
    { .next = NULL, .prevLink = NULL, .origin = { 1760, -1408, 4512, 0 }, .vertices = { { -1632, -2432, 0, 0 }, { 1632, -2432, 0, 0 }, { -1632, 2432, 0, 0 }, { 1632, 2432, 0, 0 } }, .normal = { 0, 0, -4098, 0 }, .radius = 2918, .flags = 1 | WORLD_COLLISION_OCCLUDER_LAST, .unknown_3B = 0 },
};

/// Five white point lights contributing in every view of Dryfield motel room 1.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits (`ONE` is full intensity). The loaded room overlay owns
/// these writable records: coordinate updates parent and compose their
/// transforms, and lighting queries overwrite attenuation. Borrowed pointers
/// must not survive unloading the overlay.
static WorldCoordPointLight _gDryfieldMotelRoom1PointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1234, -1685, 5738 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3300, 3300, 3300 },
        },
        .inner = 1222,
        .outer = 4450,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3463, -1645, 1027 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4250, 4250, 4250 },
        },
        .inner = 2300,
        .outer = 4100,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 926, -1645, 83 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3800, 3800, 3800 },
        },
        .inner = 2153,
        .outer = 4701,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4433, -1645, 1308 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2800, 2800, 2800 },
        },
        .inner = 852,
        .outer = 3970,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4671, -1479, 4462 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4000, 4000, 4000 },
        },
        .inner = 1487,
        .outer = 3789,
    },
};

/// Five all-view point lights shared by both room lighting entries.
///
/// No directional or cone lights are present. Counts measure array elements;
/// the borrowed point-light transforms and attenuation remain writable while
/// the room overlay is loaded.
static WorldCoordRoomLights _gDryfieldMotelRoom1RoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldMotelRoom1PointLights),
    .pointLights           = _gDryfieldMotelRoom1PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Empty resource-recipe list for placement variant 1, containing only END.
///
/// Paired with the map's empty placement list; it queues and spawns nothing.
static AreaResource _gDryfieldMotelRoom1Variant1Resources[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's Odd Stranger and Bone Suckler loading/spawn recipes, then END.
///
/// Entry IDs 18/12 load files 401800/301200 and spawn descriptor index zero.
/// The map supplies five placements; these room-owned recipes remain loaded
/// through file loading, actor spawning and model-flag updates.
static AreaResource _gDryfieldMotelRoom1Variant2Resources[3] = {
    { .entryId = DRYFIELD_MOTEL_ROOM_1_ODD_STRANGER_ENTRY, .fileNumber = DRYFIELD_MOTEL_ROOM_1_ODD_STRANGER_FILE, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor401800OddStrangerTaskDesc },
    { .entryId = DRYFIELD_MOTEL_ROOM_1_BONE_SUCKLER_ENTRY, .fileNumber = DRYFIELD_MOTEL_ROOM_1_BONE_SUCKLER_FILE, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor301200BoneSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 3's staged and combat Bone Suckler loading/spawn recipes, then END.
///
/// Entry IDs 112/12 load files 123200/201200 and spawn descriptor index zero.
/// The map supplies the two staged placements followed by two enemy placements.
/// Keep this room's recipes loaded through loading, spawning and model updates.
static AreaResource _gDryfieldMotelRoom1Variant3Resources[3] = {
    { .entryId = DRYFIELD_MOTEL_ROOM_1_STAGED_BONE_SUCKLER_ENTRY, .fileNumber = DRYFIELD_MOTEL_ROOM_1_STAGED_BONE_SUCKLER_FILE, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor123200BoneSucklerTaskDesc },
    { .entryId = DRYFIELD_MOTEL_ROOM_1_BONE_SUCKLER_ENTRY, .fileNumber = DRYFIELD_MOTEL_ROOM_1_BONE_SUCKLER_FILE, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor201200BoneSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 7's Odd Stranger and Blood Suckler loading/spawn recipes, then END.
///
/// Entry IDs 18/40 load files 401800/304000 and spawn descriptor index zero.
/// The map supplies five placements; keep the room recipes loaded through
/// file loading, actor spawning and model-flag updates.
static AreaResource _gDryfieldMotelRoom1Variant7Resources[3] = {
    { .entryId = DRYFIELD_MOTEL_ROOM_1_ODD_STRANGER_ENTRY, .fileNumber = DRYFIELD_MOTEL_ROOM_1_ODD_STRANGER_FILE, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor401800OddStrangerTaskDesc },
    { .entryId = DRYFIELD_MOTEL_ROOM_1_BLOOD_SUCKLER_ENTRY, .fileNumber = DRYFIELD_MOTEL_ROOM_1_BLOOD_SUCKLER_FILE, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor304000BloodSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 10's staged and combat Bone Suckler loading/spawn recipes, then END.
///
/// Entry IDs 112/12 load files 123200/201200 at descriptor index zero, as in
/// variant 3. This variant instead borrows the room's own four placements.
/// Keep these separate writable recipes loaded through loading and spawning.
static AreaResource _gDryfieldMotelRoom1Variant10Resources[3] = {
    { .entryId = DRYFIELD_MOTEL_ROOM_1_STAGED_BONE_SUCKLER_ENTRY, .fileNumber = DRYFIELD_MOTEL_ROOM_1_STAGED_BONE_SUCKLER_FILE, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor123200BoneSucklerTaskDesc },
    { .entryId = DRYFIELD_MOTEL_ROOM_1_BONE_SUCKLER_ENTRY, .fileNumber = DRYFIELD_MOTEL_ROOM_1_BONE_SUCKLER_FILE, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor201200BoneSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 10's two staged and two combat Bone Sucklers, followed by END.
///
/// All four start at world (-3473, 0, 993), yaw one full turn, with zero
/// actor variant/mode and parameter row. They use base files, no texture-page
/// shift and a two-row CLUT shift. Placements remain distinct despite equal
/// transforms; actors borrow them while this room overlay stays loaded.
static AreaPlacement _gDryfieldMotelRoom1Variant10Placements[5] = {
    { .entryId = DRYFIELD_MOTEL_ROOM_1_STAGED_BONE_SUCKLER_ENTRY, .variant = 0, .mode = 0, .x = -3473, .y = 0, .z = 993, .yaw = ACTOR_TRANSFORM_ANGLE_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = DRYFIELD_MOTEL_ROOM_1_STAGED_BONE_SUCKLER_ENTRY, .variant = 0, .mode = 0, .x = -3473, .y = 0, .z = 993, .yaw = ACTOR_TRANSFORM_ANGLE_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = DRYFIELD_MOTEL_ROOM_1_BONE_SUCKLER_ENTRY, .variant = 0, .mode = 0, .x = -3473, .y = 0, .z = 993, .yaw = ACTOR_TRANSFORM_ANGLE_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = DRYFIELD_MOTEL_ROOM_1_BONE_SUCKLER_ENTRY, .variant = 0, .mode = 0, .x = -3473, .y = 0, .z = 993, .yaw = ACTOR_TRANSFORM_ANGLE_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldMotelRoom1PlacementVariants[13] = {
    { .placements = NULL, .resources = NULL },
    { .placements = gMapDryfieldMotelRoom1Variant1Placements, .resources = _gDryfieldMotelRoom1Variant1Resources },
    { .placements = gMapDryfieldMotelRoom1Variant2Placements, .resources = _gDryfieldMotelRoom1Variant2Resources },
    { .placements = gMapDryfieldMotelRoom1Variant3Placements, .resources = _gDryfieldMotelRoom1Variant3Resources },
    { .placements = NULL, .resources = NULL },
    { .placements = NULL, .resources = NULL },
    { .placements = NULL, .resources = NULL },
    { .placements = gMapDryfieldMotelRoom1Variant7Placements, .resources = _gDryfieldMotelRoom1Variant7Resources },
    { .placements = NULL, .resources = NULL },
    { .placements = NULL, .resources = NULL },
    { .placements = _gDryfieldMotelRoom1Variant10Placements, .resources = _gDryfieldMotelRoom1Variant10Resources },
    { .placements = NULL, .resources = NULL },
    { .placements = NULL, .resources = NULL },
};

/// Cue-2 footstep bases in the currently loaded type-1 bank; cue 1 adds one.
enum {
    DRYFIELD_MOTEL_ROOM_1_CLASS_1_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_MOTEL_ROOM_1_CLASS_1_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_MOTEL_ROOM_1_CLASS_2_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x01,
    DRYFIELD_MOTEL_ROOM_1_CLASS_2_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x03,
};

/// Class 1's walking, running and scripted-jump footstep cue bases.
///
/// Walking and scripted jumps use entries 0x35/0x36; running uses 0x37/0x38
/// in the loaded type-1 bank. Surface properties borrow this overlay's record.
static WorldCollisionFootstepSounds _gDryfieldMotelRoom1Class1FootstepSounds = {
    .walk         = DRYFIELD_MOTEL_ROOM_1_CLASS_1_WALK_CUE_BASE,
    .run          = DRYFIELD_MOTEL_ROOM_1_CLASS_1_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_MOTEL_ROOM_1_CLASS_1_WALK_CUE_BASE,
};

/// Class 2's walking, running and scripted-jump footstep cue bases.
///
/// Walking and scripted jumps use entries 1/2; running uses 3/4 in the loaded
/// type-1 bank. Surface properties borrow this overlay's record.
static WorldCollisionFootstepSounds _gDryfieldMotelRoom1Class2FootstepSounds = {
    .walk         = DRYFIELD_MOTEL_ROOM_1_CLASS_2_WALK_CUE_BASE,
    .run          = DRYFIELD_MOTEL_ROOM_1_CLASS_2_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_MOTEL_ROOM_1_CLASS_2_WALK_CUE_BASE,
};

/// Shared silent, probe-blocking properties for surface classes 0 and 4..7.
///
/// Weapon impacts and pushback remain enabled. Class 0 occurs in the mesh;
/// the four retained higher classes are still read during room setup.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom1SolidSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1's blocking, impact-enabled floor properties with its own footstep cues.
///
/// Pushback remains enabled; the cue record is borrowed for the overlay's lifetime.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom1Class1SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMotelRoom1Class1FootstepSounds,
};

/// Class 2's blocking, impact-enabled floor properties with a second footstep set.
///
/// Pushback remains enabled; the cue record is borrowed for the overlay's lifetime.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom1Class2SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMotelRoom1Class2FootstepSounds,
};

/// Class 3's silent, probe-passing surface with weapon impacts disabled.
///
/// Pushback remains enabled independently of probe passage and impact policy.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom1Class3SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gDryfieldMotelRoom1SurfacePropertiesByClass[8] = {
    &_gDryfieldMotelRoom1SolidSurfaceProperties,
    &_gDryfieldMotelRoom1Class1SurfaceProperties,
    &_gDryfieldMotelRoom1Class2SurfaceProperties,
    &_gDryfieldMotelRoom1Class3SurfaceProperties,
    &_gDryfieldMotelRoom1SolidSurfaceProperties,
    &_gDryfieldMotelRoom1SolidSurfaceProperties,
    &_gDryfieldMotelRoom1SolidSurfaceProperties,
    &_gDryfieldMotelRoom1SolidSurfaceProperties,
};

/// Borrowed opening-event task handle used by the normal and skip callbacks.
///
/// Published only after work allocation succeeds; callbacks require initialized
/// work and run before END clears eventState. The task then waits for view 2
/// before teardown. Teardown does not clear this pointer: it becomes stale and
/// must not be dereferenced until another successful initialization publishes it.
static Task* _gDryfieldMotelRoom1OpeningEventTask = NULL;

static void _dryfieldMotelRoom1InitializeOpeningEvent(Task* task);

/// Broadcasts an actor command in the active stage/area namespace.
///
/// Requires the active session and a live scene-manager task. `command` is a
/// 16-bit receiver-specific selector; this room uses
/// `DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_*`. The scene forwards the request to
/// every placed actor with a zero second payload, and discards their results.
/// Dispatch is synchronous: receivers borrow the stack record for the call.
static inline void _dryfieldMotelRoom1BroadcastActorCommand(u16 command)
{
    ActorCommand request;

    request.context.loc.stage = gGameSession->location.stage;
    request.context.loc.area  = gGameSession->location.area;
    request.command           = command;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &request, ACTOR_COMMAND_MESSAGE_APPLY);
}

/// Takes scripted player control and plays a clip from the equipped-weapon bank.
///
/// The u16 selectors and duration are promoted into signed request words.
/// `blend` is an `ANIMATION_BLEND_*` choice; blending requires `blendFrames`
/// in 0..2047 normal-rate frames, while reset ignores it. Requires the live
/// player/model, character 1, weapon slot 0..32 and a loaded bank/clip supplying
/// every active animation track. The other-character branch retains offset
/// 34, whose reachable bank storage is unproven. World collision is disabled.
/// Dispatch consumes the stack request synchronously; animation resources
/// remain borrowed through playback.
static inline void _dryfieldMotelRoom1PlayPlayerAnimation(u16 animationId, u16 blend, u16 blendFrames)
{

    AnimationPlayRequest request;
    s32                  weaponSlot;

    weaponSlot                   = gPlayerStatus.weapon;
    request.source.index         = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponSlot);
    request.animationId          = animationId;
    request.blend                = blend;
    request.blendFrames          = blendFrames;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
}

/// Executes the motel encounter's pending actor, sound or player-turn action.
///
/// Borrows the event task's initialized work, live player and two staged sucklers.
/// Staging and sound actions clear in one update. The turn freezes the player's
/// position, steps yaw by the room's turn increment toward its facing yaw and
/// settles for four updates before the final clip. Angles use 4096 units per
/// turn. Its player-animation requests borrow loaded equipped-weapon banks;
/// the room, actors and animation resources must outlive the action.
/// Requires character 1 and an equipped-weapon slot in 0..32.
static void _dryfieldMotelRoom1ExecuteEventAction(Task* task)
{
    enum { DRYFIELD_MOTEL_ROOM_1_EVENT_CLIP_STAND       = 1,
           DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_BLEND       = 5,
           DRYFIELD_MOTEL_ROOM_1_EVENT_STAND_BLEND      = 3,
           DRYFIELD_MOTEL_ROOM_1_EVENT_AFTER_TURN_BLEND = 10,
           DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_RATE        = ANIMATION_RATE_ONE * 3 };

    _DryfieldMotelRoom1EventWork* work = task->work;
    PlayerStatus*                 playerStatus;

    switch (work->action) {
        case DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_FIRST_STAGING:
            _dryfieldMotelRoom1BroadcastActorCommand(DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_FIRST_STAGING);
            TASK_MESSAGE_DISPATCH_POINTER(work->stagedSucklerTasks[0], ACTOR_MESSAGE_PLACE, &_gDryfieldMotelRoom1FirstStagingPlacements[0], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->stagedSucklerTasks[1], ACTOR_MESSAGE_PLACE, &_gDryfieldMotelRoom1FirstStagingPlacements[1], 0);
            break;
        case DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SECOND_STAGING:
            _dryfieldMotelRoom1BroadcastActorCommand(DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_SECOND_STAGING);
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->stagedSucklerTasks[0], ACTOR_MESSAGE_PLACE, &_gDryfieldMotelRoom1SecondStagingPlacements[0], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->stagedSucklerTasks[1], ACTOR_MESSAGE_PLACE, &_gDryfieldMotelRoom1SecondStagingPlacements[1], 0);
            break;
        case DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SOUND_5:
        case DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_OPENING_SOUND_5:
            sndEvtRequestScriptStart(SOUND_ID(SOUND_BANK_TYPE_CHARACTER, 0xC, 5), 0, 0);
            break;
        case DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SOUND_2:
            sndEvtRequestScriptStart(SOUND_ID(SOUND_BANK_TYPE_CHARACTER, 0xC, 2), 0, 0);
            break;
        case DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_TURN_PLAYER:
            switch (work->actionStep) {
                case DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_BEGIN:
                    playerStatus                 = &gPlayerStatus;
                    work->playerPlacement.pos.vx = playerStatus->coordMtx->t[0];
                    work->playerPlacement.pos.vy = playerStatus->coordMtx->t[1];
                    work->playerPlacement.pos.vz = playerStatus->coordMtx->t[2];
                    work->playerPlacement.rot.vx = 0;
                    work->playerPlacement.rot.vy = 0;
                    work->playerPlacement.rot.vz = 0;
                    // Measure the yaw from the facing yaw, so that either direction ends at a range limit.
                    work->turnYaw =
                        (((GameActor*)work->playerTask->work)->rotation.vy + (ACTOR_TRANSFORM_ANGLE_TURN - DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_FACING_YAW)) % ACTOR_TRANSFORM_ANGLE_TURN;
                    if (work->turnYaw > ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                        _dryfieldMotelRoom1PlayPlayerAnimation(PLAYER_ANIMATION_TURN_LEFT, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_BLEND);
                        taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_RATE, 0);
                        work->actionStep += 1;
                    } else {
                        _dryfieldMotelRoom1PlayPlayerAnimation(PLAYER_ANIMATION_TURN_RIGHT, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_BLEND);
                        taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_RATE, 0);
                        work->actionStep += 2;
                    }
                    return;
                case DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_YAW_UP:
                    work->turnYaw               += DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_YAW_STEP;
                    work->playerPlacement.rot.vy = work->turnYaw + DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_FACING_YAW;
                    if (work->turnYaw > ACTOR_TRANSFORM_ANGLE_TURN) {
                        _dryfieldMotelRoom1PlayPlayerAnimation(PLAYER_ANIMATION_IDLE, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_MOTEL_ROOM_1_EVENT_STAND_BLEND);
                        work->settleFrames = 0;
                        work->actionStep   = DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_SETTLE;
                        return;
                    }
                    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
                    return;
                case DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_YAW_DOWN:
                    work->turnYaw               -= DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_YAW_STEP;
                    work->playerPlacement.rot.vy = work->turnYaw + DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_FACING_YAW;
                    if (work->turnYaw < 0) {
                        _dryfieldMotelRoom1PlayPlayerAnimation(PLAYER_ANIMATION_IDLE, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_MOTEL_ROOM_1_EVENT_STAND_BLEND);
                        work->settleFrames = 0;
                        work->actionStep   = DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_SETTLE;
                        return;
                    }
                    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
                    return;
                case DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_SETTLE:
                    work->settleFrames += 1;
                    if (work->settleFrames >= DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_SETTLE_FRAMES) {
                        _dryfieldMotelRoom1PlayPlayerAnimation(PLAYER_ANIMATION_AIM_IDLE, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_MOTEL_ROOM_1_EVENT_AFTER_TURN_BLEND);
                        work->action = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_NONE;
                    }
                    return;
                default:
                    return;
            }
            return;
        case DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_NONE:
        default:
            break;
    }
    work->action = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_NONE;
}

/// Allocates the opening encounter's work and resolves its player and four sucklers.
///
/// The task owns the zeroed work through normal teardown and is published for
/// script callbacks. Requires a live player and scene placements 0..3 in the
/// current stage/area: two staged sucklers followed by two combat sucklers.
/// Their tasks are borrowed and must outlive the event. Allocation failure
/// kills the task; its caller continues starting the script on that path.
static void _dryfieldMotelRoom1InitializeOpeningEvent(Task* task)
{
    enum { DRYFIELD_MOTEL_ROOM_1_SECOND_STAGED_PLACEMENT = 1,
           DRYFIELD_MOTEL_ROOM_1_FIRST_ENEMY_PLACEMENT   = 2,
           DRYFIELD_MOTEL_ROOM_1_SECOND_ENEMY_PLACEMENT  = 3 };
    _DryfieldMotelRoom1EventWork* work;
    s32                           placeKey;

    work       = memMalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    memFillBytes(work, 0, sizeof(*work));
    work->playerTask                     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    _gDryfieldMotelRoom1OpeningEventTask = task;
    placeKey                             = gGameSession->location.area | (gGameSession->location.stage << ENEMY_PLACE_STAGE_SHIFT);
    work->stagedSucklerTasks[0]          = sceneFindEnemyByPlaceKey(placeKey)->task;
    placeKey                             = ((gGameSession->location.stage << ENEMY_PLACE_STAGE_SHIFT) | (DRYFIELD_MOTEL_ROOM_1_SECOND_STAGED_PLACEMENT << ENEMY_PLACE_INDEX_SHIFT)) | gGameSession->location.area;
    work->stagedSucklerTasks[1]          = sceneFindEnemyByPlaceKey(placeKey)->task;
    placeKey                             = ((gGameSession->location.stage << ENEMY_PLACE_STAGE_SHIFT) | (DRYFIELD_MOTEL_ROOM_1_FIRST_ENEMY_PLACEMENT << ENEMY_PLACE_INDEX_SHIFT)) | gGameSession->location.area;
    work->enemySucklerTasks[0]           = sceneFindEnemyByPlaceKey(placeKey)->task;
    placeKey                             = ((gGameSession->location.stage << ENEMY_PLACE_STAGE_SHIFT) | (DRYFIELD_MOTEL_ROOM_1_SECOND_ENEMY_PLACEMENT << ENEMY_PLACE_INDEX_SHIFT)) | gGameSession->location.area;
    work->enemySucklerTasks[1]           = sceneFindEnemyByPlaceKey(placeKey)->task;
}

/// Runs the opening suckler encounter, then releases it when view 2 is selected.
///
/// State 0 waits for the attachment wheel and display transition to close,
/// initializes work, plays player clip 1 with a five-frame blend and starts
/// the event/skip scripts. State 1 waits for script completion before looping
/// the combat sucklers' walk; state 2 starts their chase and kills this task
/// on view 2. Each nonterminal update executes any pending script action.
/// Requires the room, player, placed actors and animation bank to stay live.
/// Character 1 uses weapon banks 1..33; the alternate-character range is unproven.
static void _dryfieldMotelRoom1OpeningEventTask(Task* task)
{
    enum { DRYFIELD_MOTEL_ROOM_1_OPENING_START            = 0,
           DRYFIELD_MOTEL_ROOM_1_OPENING_WAIT_SCRIPT      = 1,
           DRYFIELD_MOTEL_ROOM_1_OPENING_WAIT_COMBAT_VIEW = 2,
           DRYFIELD_MOTEL_ROOM_1_OPENING_BLEND_FRAMES     = 5 };

    switch (task->state) {
        case DRYFIELD_MOTEL_ROOM_1_OPENING_START:
            if ((gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                _dryfieldMotelRoom1InitializeOpeningEvent(task);
                _dryfieldMotelRoom1PlayPlayerAnimation(PLAYER_ANIMATION_IDLE, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_MOTEL_ROOM_1_OPENING_BLEND_FRAMES);
                evsStartScriptWithSkip(_gDryfieldMotelRoom1OpeningScript, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                       _gDryfieldMotelRoom1OpeningSkipScript);
                task->state = task->state + 1;
                break;
            }
            return;
        case DRYFIELD_MOTEL_ROOM_1_OPENING_WAIT_SCRIPT:
            if (gGameSession->eventState == 0) {
                _dryfieldMotelRoom1BroadcastActorCommand(DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_WALK_IN_PLACE);
                task->state = task->state + 1;
                break;
            }
            break;
        case DRYFIELD_MOTEL_ROOM_1_OPENING_WAIT_COMBAT_VIEW:
            // The final state value also selects the view that starts the chase.
            if (gGameSession->location.view == task->state) {
                _dryfieldMotelRoom1BroadcastActorCommand(DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_START_COMBAT);
                taskKill(task);
                return;
            }
            break;
    }
    _dryfieldMotelRoom1ExecuteEventAction(task);
}

/// Ends the opening scene by engaging battle and placing both bone sucklers.
///
/// Used by normal and skipped event scripts. Requires the retained event task,
/// its initialized work, both live enemy tasks and the scene-manager task.
/// The start-combat command and placement records are borrowed synchronously;
/// this callback neither releases the event task nor waits for actor motion.
static void _dryfieldMotelRoom1FinishOpeningScene(void)
{
    _DryfieldMotelRoom1EventWork* work = _gDryfieldMotelRoom1OpeningEventTask->work;

    sceneEngageBattle(1);
    _dryfieldMotelRoom1BroadcastActorCommand(DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_START_COMBAT);
    TASK_MESSAGE_DISPATCH_POINTER(work->enemySucklerTasks[0], ACTOR_MESSAGE_PLACE, &_gDryfieldMotelRoom1EnemyCombatPlacements[0], 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->enemySucklerTasks[1], ACTOR_MESSAGE_PLACE, &_gDryfieldMotelRoom1EnemyCombatPlacements[1], 0);
}

/// Posts an event action and restarts its per-action step at the beginning.
///
/// The event task and its work must still be live. Script callers supply a
/// `DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_*` value; the signed argument is stored
/// as an unsigned halfword without validation. Replaces any pending action,
/// leaving the turn's placement, yaw and settle counter intact.
static void _dryfieldMotelRoom1RequestEventAction(s16 action)
{
    _DryfieldMotelRoom1EventWork* work = _gDryfieldMotelRoom1OpeningEventTask->work;

    work->action     = action;
    work->actionStep = DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_BEGIN;
}

/// Restores the opening event's final player pose and facing after a skip.
///
/// Requires the live event work and its cached player task, the registered
/// player/model, character 1 and equipped weapon slot 0..32 with clip 9 loaded.
/// Takes scripted control and requests disabled grid participation. The current
/// player translation is sampled after animation dispatch and placed at a yaw
/// of 5/16 turn, with pitch and roll cleared. Requests are borrowed synchronously;
/// animation resources remain live through playback. Script cleanup resumes play.
static void _dryfieldMotelRoom1RestorePlayerAfterSkip(void)
{
    enum {
        DRYFIELD_MOTEL_ROOM_1_SKIP_PLAYER_YAW = ACTOR_TRANSFORM_ANGLE_TURN * 5 / 16,
    };
    _DryfieldMotelRoom1EventWork* work;
    PlayerStatus*                 playerStatus;

    work = _gDryfieldMotelRoom1OpeningEventTask->work;
    _dryfieldMotelRoom1PlayPlayerAnimation(PLAYER_ANIMATION_AIM_IDLE, ANIMATION_BLEND_RESET, 0);
    playerStatus                 = &gPlayerStatus;
    work->playerPlacement.pos.vx = playerStatus->coordMtx->t[0];
    work->playerPlacement.pos.vy = playerStatus->coordMtx->t[1];
    work->playerPlacement.pos.vz = playerStatus->coordMtx->t[2];
    work->playerPlacement.rot.vx = 0;
    work->playerPlacement.rot.vy = DRYFIELD_MOTEL_ROOM_1_SKIP_PLAYER_YAW;
    work->playerPlacement.rot.vz = 0;
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
}
void dryfieldMotelRoom1NoOpEffectTask(Task* unusedTask)
{
}
