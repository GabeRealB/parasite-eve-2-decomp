#include "rooms/shelter_b2_pod_bottom.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "shelter_b2_pod_bottom_private.h"

#include "actors/actor_361100.h"
#include "actors/actor_403600.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/gameflag_ids.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

static void _shelterB2PodBottomInitializeRoomTask(Task* task);
static void _shelterB2PodBottomIdleRoomTask(Task* unusedTask);

/// Room-task handlers for states 0 initialize, 1 idle and 2 teardown.
///
/// Copied by value before dispatch; only indices 0..2 are valid. The table
/// borrows callback addresses that remain live with the room overlay.
static const TaskFuncTable3 _gShelterB2PodBottomRoomTaskStates = {
    .funcs = { _shelterB2PodBottomInitializeRoomTask, _shelterB2PodBottomIdleRoomTask, taskKill },
};

static s32 _shelterB2PodBottomRejectKeyItemMessage(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _shelterB2PodBottomResolveRoomEventMessage(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB2PodBottomIgnoreCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 commandArg);
static s32 _shelterB2PodBottomIgnoreActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* unusedRequest, s32 unusedArg);

static WorldCollisionGrid    _gShelterB2PodBottomCollisionGrid;
static WorldCollisionTrigger _gShelterB2PodBottomViewBoundaryTriggers[20];

static WorldCoordRoomLights _gShelterB2PodBottomRoomLights;

enum {
    SHELTER_B2_POD_BOTTOM_COLLISION_GRID_CELL_COUNT_X = 5,
    SHELTER_B2_POD_BOTTOM_COLLISION_GRID_CELL_COUNT_Z = 7,
    SHELTER_B2_POD_BOTTOM_COLLISION_GRID_CELL_SIZE    = 4000,
};

/// The room task's four supported messages and terminating record.
///
/// Resolves room-event requests, refuses key items and ignores action/command
/// requests. Unsupported IDs return zero at TASK_MESSAGE_TABLE_END. The task
/// borrows this table and callback code until teardown; payloads are borrowed
/// only during synchronous dispatch and follow each handler's message contract.
static TaskMessageEntry _gShelterB2PodBottomRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB2PodBottomResolveRoomEventMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB2PodBottomRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB2PodBottomIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, _shelterB2PodBottomIgnoreCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

EffectBandShape gShelterB2PodBottomArcFlashBandShapes[3] = {
    { .baseRadius = 512, .lift = 3072, .spread = 512 },
    { .baseRadius = 1024, .lift = 2048, .spread = 768 },
    { .baseRadius = 1536, .lift = 1024, .spread = 1024 },
};

u16 gShelterB2PodBottomEnergyRingChannelShifts[18][3] = {
    { 0, 1, 2 },
    { 0, 2, 1 },
    { 1, 0, 2 },
    { 1, 2, 0 },
    { 2, 0, 1 },
    { 2, 1, 0 },
    { 1, 1, 0 },
    { 1, 0, 1 },
    { 0, 1, 1 },
    { 2, 2, 0 },
    { 2, 0, 2 },
    { 0, 2, 2 },
    { 1, 0, 0 },
    { 0, 0, 1 },
    { 0, 1, 0 },
    { 2, 0, 0 },
    { 0, 0, 2 },
    { 0, 2, 0 },
};

WorldCollisionRoomResources gShelterB2PodBottomRoomCollisionResources[1] = {
    { .grid                 = &_gShelterB2PodBottomCollisionGrid,
      .viewBoundaryTriggers = _gShelterB2PodBottomViewBoundaryTriggers,
      .actionTriggers       = gShelterB2PodBottomActionTriggers,
      .occluders            = NULL },
};

WorldCoordRoomLighting gShelterB2PodBottomRoomLighting[1] = {
    { &_gShelterB2PodBottomRoomLights, gShelterB2PodBottomAmbientByView },
};

u8* gShelterB2PodBottomRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB2PodBottomRoomViewCounts[1] = { 16 };

DirectionWarpEntry gShelterB2PodBottomWarpEntries[1] = {
    { .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 1542, -1998, 7374 },
      .unknown_10     = { 0, 0, 0, 0 },
      .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 1542, -1998, 7374 },
      .unknown_24     = { 0, 0, 0, 0 },
      .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
      .departureSound = DIRECTION_WARP_SOUND_NONE,
      .blockedSound   = DIRECTION_WARP_SOUND_NONE,
      .initialView    = 2,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = GAME_FLAG_MAP_MARK_POD_SERVICE_GANTRY },
};

/// Room-space face normals for the pod-bottom collision grid, with ONE as unit length.
static SVECTOR _gShelterB2PodBottomCollisionGridNormals[40] = {
#include "assets/shelter_b2_pod_bottom_collision_0559C_normals.inc"
};

/// Room-space vertices for the pod-bottom collision grid, in signed game units.
static SVECTOR _gShelterB2PodBottomCollisionGridVerts[132] = {
#include "assets/shelter_b2_pod_bottom_collision_0559C_verts.inc"
};

/// Indexed collision faces using the room's surface classes 0..3.
static WorldCollisionGridFace _gShelterB2PodBottomCollisionGridFaces[88] = {
#include "assets/shelter_b2_pod_bottom_collision_0559C_faces.inc"
};

/// Signed face-index lists, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gShelterB2PodBottomCollisionGridCells[498] = {
#include "assets/shelter_b2_pod_bottom_collision_0559C_cells.inc"
};

/// Resolves a generated cell-directory entry's halfword offset to its face list.
///
/// Captures this grid's packed s16 pool. `halfwordOffset` is evaluated once
/// and must be a constant element offset in 0..497, at a list's first face.
/// The table producer selects this binding through `cell_face_list_macro`;
/// list termination is supplied by WORLD_COLLISION_GRID_CELL_END in the pool.
/// Defined only around this grid's table include; returns a borrowed s16*.
#define SHELTER_B2_POD_BOTTOM_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB2PodBottomCollisionGridCells[(halfwordOffset)])
/// X-major cell directory: entry x * cellCountZ + z selects a terminated face list.
static s16* _gShelterB2PodBottomCollisionGridTable[SHELTER_B2_POD_BOTTOM_COLLISION_GRID_CELL_COUNT_X * SHELTER_B2_POD_BOTTOM_COLLISION_GRID_CELL_COUNT_Z] = {
#include "assets/shelter_b2_pod_bottom_collision_0559C_table.inc"
};
#undef SHELTER_B2_POD_BOTTOM_CELL_FACE_LIST_AT_OFFSET

/// The pod-bottom room's collision mesh and 5-by-7 XZ spatial index.
///
/// Covers X [0, 20000) and Z [-6000, 22000) in room game units. Activation
/// binds the view coordinate; the descriptor and writable pools remain borrowed
/// while this room is active and must not survive unloading the overlay.
static WorldCollisionGrid _gShelterB2PodBottomCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB2PodBottomCollisionGridNormals,
    .vertices    = _gShelterB2PodBottomCollisionGridVerts,
    .faces       = _gShelterB2PodBottomCollisionGridFaces,
    .cellFaceIds = _gShelterB2PodBottomCollisionGridTable,
    .xBias       = 0,
    .zBias       = 6000,
    .cellCountX  = SHELTER_B2_POD_BOTTOM_COLLISION_GRID_CELL_COUNT_X,
    .cellCountZ  = SHELTER_B2_POD_BOTTOM_COLLISION_GRID_CELL_COUNT_Z,
    .cellSize    = SHELTER_B2_POD_BOTTOM_COLLISION_GRID_CELL_SIZE,
    .faceCount   = ARRAY_SIZE(_gShelterB2PodBottomCollisionGridFaces),
};

ViewCamera gShelterB2PodBottomViewCameras[16] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7510, 0x61A8, -6980 } }, 329 },
    { { { { 3491, 0, -2141 }, { 362, 4036, 590 }, { 2110, -693, 3441 } }, { -490, 2270, -1670 } }, 289 },
    { { { { -3308, 0, -2415 }, { -149, 4088, 204 }, { 2410, 253, -3301 } }, { -610, 5200, -0x3B74 } }, 257 },
    { { { { -2265, 0, 3412 }, { -462, 4058, -307 }, { -3381, -555, -2244 } }, { -0x37FA, 2360, -0x3AAC } }, 289 },
    { { { { -3419, 0, 2255 }, { -657, 3918, -996 }, { -2157, -1193, -3270 } }, { -0x3C64, 320, -0x407E } }, 257 },
    { { { { -3378, 0, 2315 }, { -480, 4006, -700 }, { -2265, -849, -3304 } }, { -0x3DCC, 1990, -0x2F3A } }, 289 },
    { { { { 2628, 0, 3141 }, { 332, 4072, -278 }, { -3123, 433, 2613 } }, { -0x404C, 5510, 960 } }, 257 },
    { { { { 2216, 0, -3444 }, { 452, 4060, 291 }, { 3414, -537, 2197 } }, { -1620, 2360, 860 } }, 289 },
    { { { { 2373, 0, -3338 }, { 1080, 3875, 768 }, { 3158, -1326, 2245 } }, { 2240, 100, 1510 } }, 289 },
    { { { { -1691, 0, 3730 }, { -2234, 3279, -1013 }, { -2987, -2453, -1354 } }, { -0x3480, 1780, -9520 } }, 257 },
    { { { { 1032, 0, -3963 }, { -3485, 1949, -907 }, { 1886, 3602, 491 } }, { -4960, 0x2954, -6390 } }, 289 },
    { { { { -2744, 0, 3040 }, { 2862, 1382, 2583 }, { -1026, 3855, -926 } }, { -7760, 450, -6720 } }, 289 },
    { { { { -2822, 0, 2968 }, { -761, 3959, -723 }, { -2869, -1050, -2727 } }, { -0x3084, 3800, -0x2C42 } }, 289 },
    { { { { -1451, 0, -3830 }, { 2605, 3002, -987 }, { 2807, -2786, -1064 } }, { -980, 9350, -9120 } }, 329 },
    { { { { 4095, 0, 0 }, { 0, 4095, 0 }, { 0, 0, 4095 } }, { 0, 0, 9200 } }, 329 },
    { { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1, -1888, 5969 } }, 329 },
};

/// Empty sprite list for mapped view 1, retaining decoded background strips.
///
/// The zero-count first record is followed by SPRITE_BATCH_END. Both source
/// and batch slots borrow this list; no source elements are ever selected.
static SpriteBatch _gShelterB2PodBottomView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprite rectangles for mapped view 2.
///
/// Uses 8-bit texture pages (896, 0) and (960, 0) and CLUT row 255.
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. The view's batches select all 54 source elements.
/// The renderer borrows this storage while the room overlay remains loaded.
static SpriteSource _gShelterB2PodBottomView2SpriteSources[54] = {
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -160, -16, 2336, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -160, -8, 2346, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -160, 80, 941, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 112 } }, -152, 0, 934, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 120 } }, -144, 0, 794, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -136, 96, 786, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, -136, 8, 623, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -128, 48, 531, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -120, 96, 480, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 104, 24 } }, 24, -8, 2114, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 128, -8, 2015, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, -40, 0, 2266, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, -104, 0, 2386, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -152, -16, 2326, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -160, -8, 2332, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -160, 0, 2320, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, -160, 8, 2234, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, -160, 16, 2181, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, -160, 24, 2028, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 104, 8 } }, -160, 32, 1941, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 112, 8 } }, -160, 40, 1867, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 104, 8 } }, -160, 48, 1747, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -160, 72, 1381, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -160, 64, 1680, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, -88, 64, 1689, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -88, 56, 1699, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -160, 56, 1699, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, -112, 56, 1232, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 120, 8 } }, -128, 64, 1236, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 136, 8 } }, -136, 72, 1198, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 152, 8 } }, -144, 80, 1147, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 16 } }, -144, 104, 994, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, -64, 104, 1069, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, 0, 104, 1062, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -144, 96, 1085, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, -64, 96, 1079, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -144, 88, 1093, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -64, 88, 1096, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 40 } }, -56, 56, 1185, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 144 } }, -160, -48, 1366, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -152, -8, 1397, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -96, 0, 2317, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, -72, 0, 2757, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, -40, 24, 2985, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -16, 40, 2999, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 104, 88, 3598, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 120, 80, 3384, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 136, 64, 3184, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 72 } }, 8, 48, 3066, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 72 } }, -16, 48, 3066, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 8, 48, 3028, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, 32, 48, 3412, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 56, 64, 3408, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 88, 88, 3592, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 2, indexing `_gShelterB2PodBottomView2SpriteSources`.
///
/// A zero-count first record retains decoded background strips. Subsequent
/// ranges cover all 54 sources; SPRITE_BATCH_END ends the list. Visibility and
/// cached-packet exclusion start clear. Keep this mutable list live while used.
static SpriteBatch _gShelterB2PodBottomView2SpriteBatches[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 0, 0 } },
    { 2, 11, 0, 0, { 5, 0 } },
    { 13, 14, 0, 0, { 4, 0 } },
    { 27, 11, 0, 0, { 6, 0 } },
    { 38, 1, 0, 0, { 1, 0 } },
    { 39, 2, 0, 0, { 7, 0 } },
    { 41, 7, 0, 0, { 3, 0 } },
    { 48, 2, 0, 0, { 8, 0 } },
    { 50, ARRAY_SIZE(_gShelterB2PodBottomView2SpriteSources) - 50, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprite rectangles for mapped view 3.
///
/// Uses 8-bit texture pages (896, 0) and (960, 0) and CLUT row 255.
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. The view's batches select all 48 source elements.
/// The renderer borrows this storage while the room overlay remains loaded.
static SpriteSource _gShelterB2PodBottomView3SpriteSources[48] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -40, 0, 4470, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 8, 3248, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -40, 16, 2550, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -40, 24, 2100, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -40, 32, 1800, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -40, 40, 1600, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -40, 48, 1375, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -32, 56, 1125, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -32, 64, 1075, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -32, 72, 1025, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -32, 80, 925, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -32, 88, 900, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -32, 96, 800, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -56, 48, 1100, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 16, 48, 1125, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -80, 56, 1050, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -8, 56, 1050, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, 56, 56, 1050, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 80, 8 } }, -112, 64, 1025, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 80, 8 } }, -32, 64, 1042, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, 48, 64, 1025, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -136, 72, 950, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, -64, 72, 950, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, 32, 72, 950, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 112, 72, 950, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -144, 80, 900, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 88, 40 } }, 24, 80, 900, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 40 } }, 112, 80, 900, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 96, 40 } }, -72, 80, 900, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 88, 32 } }, -160, 88, 638, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, -160, -8, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 16 } }, -144, -8, 375, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -48, -8, 375, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -144, 56, 375, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -144, 80, 375, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 16 } }, -128, 64, 375, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 96 } }, -32, 24, 375, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -24, 40, 375, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, 40, 32, 375, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 80, -8, 375, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, 80, 16, 375, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 16 } }, 104, -8, 375, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 152, 8, 375, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 16 } }, 96, 56, 375, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 144, 48, 375, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 144, 72, 375, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 72, 88, 375, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 136, 96, 618, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 3, indexing `_gShelterB2PodBottomView3SpriteSources`.
///
/// A zero-count first record retains decoded background strips. Subsequent
/// ranges cover all 48 sources; SPRITE_BATCH_END ends the list. Visibility and
/// cached-packet exclusion start clear. Keep this mutable list live while used.
static SpriteBatch _gShelterB2PodBottomView3SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 17, 0, 0, { 2, 0 } },
    { 30, ARRAY_SIZE(_gShelterB2PodBottomView3SpriteSources) - 30, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprite rectangles for mapped view 4.
///
/// Uses 8-bit texture pages (896, 0) and (960, 0) and CLUT row 255.
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. The view's batches select all 61 source elements.
/// The renderer borrows this storage while the room overlay remains loaded.
static SpriteSource _gShelterB2PodBottomView4SpriteSources[61] = {
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 24 } }, -80, -8, 2460, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 24 } }, 0, -8, 2567, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 24 } }, 80, -8, 2331, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 24 } }, -160, -8, 2464, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -160, 40, 3311, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -136, 56, 3526, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -120, 64, 3644, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -104, 72, 3870, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 80 } }, -32, 40, 3373, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 80 } }, 8, 40, 3319, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, 136, -8, 1720, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, 144, -40, 2278, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 152, -48, 2192, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -80, 72, 3948, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -64, 56, 3761, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -48, 48, 3697, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -32, 40, 3430, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -8, 40, 3320, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 8, 32, 3286, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 24, 24, 3120, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 40, 8, 3033, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 56, 0, 2953, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 72, 0, 2950, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, -160, 40, 3191, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, 16, 48, 1522, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 16 } }, -32, 104, 1281, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, 48, 104, 1289, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, 112, 104, 1241, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -24, 96, 1335, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 48, 96, 1334, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 112, 96, 1321, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, -16, 88, 1362, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, 80, 88, 1354, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, -8, 80, 1383, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, 88, 80, 1375, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, 0, 72, 1413, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 96, 72, 1423, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, 8, 64, 1467, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 96, 64, 1451, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, 16, 56, 1507, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 104, 56, 1507, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 104, -16, 2505, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, 88, -8, 2723, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, 80, 0, 2431, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, 72, 8, 2331, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, 72, 16, 2201, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, 64, 24, 2119, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, 56, 32, 2045, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, 56, 40, 1922, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 16, 56, 1854, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, 80, 56, 1804, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, 16, 48, 1854, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 88, 48, 1854, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 136, 16, 1170, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 96 } }, 128, -8, 1062, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, 120, -8, 1006, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 104 } }, 112, 16, 892, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, 104, 32, 665, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 96, 56, 390, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 80, 80, 251, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 24, 0, 4220, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 4, indexing `_gShelterB2PodBottomView4SpriteSources`.
///
/// A zero-count first record retains decoded background strips. Subsequent
/// ranges cover all 61 sources; SPRITE_BATCH_END ends the list. Visibility and
/// cached-packet exclusion start clear. Keep this mutable list live while used.
static SpriteBatch _gShelterB2PodBottomView4SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 3, 0 } },
    { 4, 9, 0, 0, { 4, 0 } },
    { 13, 10, 0, 0, { 1, 0 } },
    { 23, 18, 0, 0, { 5, 0 } },
    { 41, 12, 0, 0, { 0, 0 } },
    { 53, 7, 0, 0, { 6, 0 } },
    { 60, ARRAY_SIZE(_gShelterB2PodBottomView4SpriteSources) - 60, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprite rectangles for mapped view 5.
///
/// Uses 8-bit texture pages (896, 0) and (960, 0) and CLUT rows 255 and 256.
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. The view's batches select all 95 source elements.
/// The renderer borrows this storage while the room overlay remains loaded.
static SpriteSource _gShelterB2PodBottomView5SpriteSources[95] = {
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -64, 88, 1184, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -136, 88, 1178, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 24 } }, -160, 96, 1177, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 24 } }, 0, 96, 1175, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 24 } }, 48, 96, 1175, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 24 } }, -88, 96, 1175, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, -40, 96, 1175, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -160, 64, 341, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 104 } }, -152, 16, 339, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, -136, 16, 340, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -136, 56, 346, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, -16, 8, 325, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, -16, 56, 325, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 72 } }, 48, 16, 325, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 40, 88, 325, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 72, 8, 350, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 96, -16, 375, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 120, -24, 400, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 136, -40, 425, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 72, 56, 350, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 88, 40, 375, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 104, 24, 400, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 120, 8, 425, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 136, -8, 450, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -32, 8, 325, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -32, 32, 325, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -32, 56, 325, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -104, 32, 325, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -112, 56, 325, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -160, -16, 1859, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -152, 24, 2034, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -152, 16, 2050, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 136, 0, 1850, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 104, 8, 1850, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, -8, 88, 1250, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, 56, 88, 1250, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, 104, 88, 1200, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, 8, 80, 1361, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 80, 80, 1361, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 16, 72, 1400, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, 80, 72, 1400, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, 80, 64, 1425, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 24, 64, 1425, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, 32, 56, 1475, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 104, 56, 1475, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, -160, 88, 1480, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -112, 88, 1480, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, -80, 88, 1480, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 48, 48, 1537, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 104, 48, 1537, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -160, 80, 1499, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -104, 80, 1499, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -160, 72, 1565, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -104, 72, 1565, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 56, 40, 1600, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 104, 40, 1600, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 64, 32, 1625, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 112, 32, 1625, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 80, 24, 1750, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, 128, 24, 1750, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 120, 16, 1800, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, 88, 16, 1800, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -160, 64, 1650, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -96, 64, 1650, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -112, 56, 1725, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -160, 56, 1725, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -160, 48, 1794, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -112, 48, 1794, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -160, 40, 1844, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -112, 40, 1844, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -112, 32, 2000, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -152, 32, 2007, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 16 } }, 80, -8, 2803, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 56, -8, 2939, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, -8, 0, 3080, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 16 } }, -80, 0, 3173, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 16 } }, -152, 8, 2850, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -152, -8, 3256, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -160, 0, 3169, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -160, 8, 3003, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -160, 16, 2500, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -160, 24, 2000, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -160, 32, 1875, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -152, 40, 1750, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, -40, 48, 3975, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, -64, 48, 3975, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -8, 56, 4349, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 8, 64, 4292, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 24, 80, 4500, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -112, 8, 3599, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -88, 16, 3707, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -72, 24, 3795, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -48, 40, 3934, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 32, 16 } }, -104, 40, 816, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 48, 16 } }, -72, 40, 325, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 5, indexing `_gShelterB2PodBottomView5SpriteSources`.
///
/// A zero-count first record retains decoded background strips. Subsequent
/// ranges cover all 95 sources; SPRITE_BATCH_END ends the list. Visibility and
/// cached-packet exclusion start clear. Keep this mutable list live while used.
static SpriteBatch _gShelterB2PodBottomView5SpriteBatches[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 4, 0 } },
    { 7, 22, 0, 0, { 5, 0 } },
    { 29, 1, 0, 0, { 2, 0 } },
    { 30, 42, 0, 0, { 6, 0 } },
    { 72, 5, 0, 0, { 1, 0 } },
    { 77, 12, 0, 0, { 7, 0 } },
    { 89, 4, 0, 0, { 3, 0 } },
    { 93, ARRAY_SIZE(_gShelterB2PodBottomView5SpriteSources) - 93, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprite rectangles for mapped view 6.
///
/// Uses 8-bit texture pages (896, 0) and (960, 0) and CLUT row 255.
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. The view's batches select all 58 source elements.
/// The renderer borrows this storage while the room overlay remains loaded.
static SpriteSource _gShelterB2PodBottomView6SpriteSources[58] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -104, 0, 2500, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, -72, 8, 2768, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -16, 48, 3000, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -32, 32, 3000, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -48, 24, 2963, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, 0, 56, 3082, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 64, 80, 3515, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, 40, 64, 3437, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 24, 56, 3127, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 8, 56, 3019, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 0, 56, 3002, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -152, -16, 2375, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -160, -8, 2375, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -160, 0, 2325, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, -160, 8, 2200, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, -160, 16, 2100, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, -160, 24, 2000, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 104, 8 } }, -160, 32, 1875, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 112, 8 } }, -160, 40, 1800, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 112, 8 } }, -160, 48, 1725, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 120, 8 } }, -160, 56, 1725, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -40, 48, 1250, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -104, 48, 1275, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 144, 8 } }, -128, 72, 1225, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 152, 8 } }, -128, 80, 1200, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 168, 8 } }, -128, 88, 1175, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 176, 8 } }, -128, 96, 1150, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 184, 8 } }, -128, 104, 1125, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 136, 8 } }, -128, 64, 1250, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 112, 8 } }, -120, 56, 1275, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -128, 112, 727, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -56, 112, 1000, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, 16, 112, 1000, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 136 } }, -160, -48, 1353, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -152, -24, 1301, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -56, 112, 265, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -144, 32, 925, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, -136, -16, 868, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, -128, -16, 863, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -120, -16, 613, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -112, 0, 555, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -104, 16, 439, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -96, 32, 409, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -88, 48, 364, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -80, 64, 318, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -72, 80, 294, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -64, 96, 283, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -120, 48, 529, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -112, 72, 468, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -104, 96, 375, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, 136, 64, 3250, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 112, 80, 3381, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 96, 96, 3631, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -160, 8, 2350, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 24 } }, -128, 0, 2350, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 16 } }, -48, 0, 2263, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, 24, 0, 2170, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 24 } }, 88, -8, 2076, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 6, indexing `_gShelterB2PodBottomView6SpriteSources`.
///
/// A zero-count first record retains decoded background strips. Subsequent
/// ranges cover all 58 sources; SPRITE_BATCH_END ends the list. Visibility and
/// cached-packet exclusion start clear. Keep this mutable list live while used.
static SpriteBatch _gShelterB2PodBottomView6SpriteBatches[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { 5, 1, 0, 0, { 5, 0 } },
    { 6, 5, 0, 0, { 4, 0 } },
    { 11, 10, 0, 0, { 6, 0 } },
    { 21, 12, 0, 0, { 1, 0 } },
    { 33, 2, 0, 0, { 7, 0 } },
    { 35, 15, 0, 0, { 3, 0 } },
    { 50, 3, 0, 0, { 8, 0 } },
    { 53, ARRAY_SIZE(_gShelterB2PodBottomView6SpriteSources) - 53, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprite rectangles for mapped view 7.
///
/// Uses 8-bit texture pages (896, 0) and (960, 0) and CLUT row 255.
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. The view's batches select all 57 source elements.
/// The renderer borrows this storage while the room overlay remains loaded.
static SpriteSource _gShelterB2PodBottomView7SpriteSources[57] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 8, -8, 4626, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, 0, 0, 3451, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, 0, 8, 2805, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, 0, 16, 2333, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -8, 24, 2002, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -8, 32, 1774, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -16, 40, 1610, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -16, 48, 1454, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -24, 56, 1368, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -24, 64, 1271, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -32, 72, 1125, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -32, 80, 1050, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -40, 88, 925, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 16 } }, -40, 96, 850, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -48, 112, 825, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 80, 24 } }, 80, 96, 825, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 80, 24 } }, 0, 96, 825, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 80, 24 } }, -80, 96, 825, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 80, 24 } }, -160, 96, 825, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 16 } }, 80, 80, 925, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 16 } }, 0, 80, 925, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 16 } }, -80, 80, 925, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 16 } }, -152, 80, 925, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -144, 72, 975, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -80, 72, 975, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, 0, 72, 975, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 72, 72, 975, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -128, 64, 1075, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -64, 64, 1075, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 8, 64, 1075, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 72, 64, 1075, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -104, 56, 1200, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -32, 56, 1218, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, 40, 56, 1210, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -72, 48, 1286, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -32, 48, 1283, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, 24, 48, 1193, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -160, 80, 2050, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -160, 24, 325, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -152, 0, 1052, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -152, 48, 3000, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -144, 40, 300, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -144, 0, 300, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 88, 72 } }, -136, 0, 275, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -128, 72, 275, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -120, 80, 275, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -64, 72, 275, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -56, 88, 275, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 16 } }, -48, 40, 275, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 72 } }, 32, 8, 275, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 32, 80, 275, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, 56, 8, 275, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, 56, 64, 275, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 120, 0, 275, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 104, 56, 275, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 136, 0, 5505, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 136, 40, 275, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 7, indexing `_gShelterB2PodBottomView7SpriteSources`.
///
/// A zero-count first record retains decoded background strips. Subsequent
/// ranges cover all 57 sources; SPRITE_BATCH_END ends the list. Visibility and
/// cached-packet exclusion start clear. Keep this mutable list live while used.
static SpriteBatch _gShelterB2PodBottomView7SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 3, 0 } },
    { 15, 22, 0, 0, { 0, 0 } },
    { 37, 1, 0, 0, { 2, 0 } },
    { 38, ARRAY_SIZE(_gShelterB2PodBottomView7SpriteSources) - 38, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprite rectangles for mapped view 8.
///
/// Uses 8-bit texture pages (896, 0) and (960, 0) and CLUT row 255.
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. The view's batches select all 50 source elements.
/// The renderer borrows this storage while the room overlay remains loaded.
static SpriteSource _gShelterB2PodBottomView8SpriteSources[50] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 16 } }, -160, -8, 2278, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 16 } }, -96, -8, 2378, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 16 } }, -24, -8, 2508, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 48, -8, 2603, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 72, -8, 2640, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 16 } }, 88, 0, 2510, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -160, 40, 3675, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -136, 56, 3700, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -120, 64, 3730, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -80, 72, 3917, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -64, 56, 3753, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -48, 48, 3806, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 80 } }, -32, 40, 3450, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 56 } }, -8, 40, 3475, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 0, 40, 3275, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 16, 24, 3163, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 32, 16, 3148, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 48, 0, 2999, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 64, -8, 2945, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 80, -8, 2750, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, 88, -8, 2712, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 96, -16, 2712, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, 80, 0, 2550, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, 72, 8, 2450, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, 72, 16, 2300, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, 64, 24, 2237, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 104, 8 } }, 56, 32, 2125, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, 56, 40, 2050, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 128, 8 } }, 24, 48, 2025, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 136, 8 } }, 16, 56, 2000, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 104, 8 } }, 16, 56, 1450, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, 32, 48, 1450, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 120, 8 } }, 16, 64, 1400, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 128, 8 } }, 8, 72, 1362, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 144, 8 } }, -8, 80, 1325, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 152, 8 } }, -16, 88, 1300, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 144, 8 } }, -16, 96, 1262, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 152, 8 } }, -24, 104, 1212, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 168, 8 } }, -32, 112, 1175, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, 72, 1670, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 152, -48, 2050, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, 144, -32, 1750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, 136, 0, 1670, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 96 } }, 136, -8, 1050, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 96 } }, 128, -8, 875, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 112 } }, 120, 8, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, 112, 32, 522, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 104, 64, 468, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 96, 88, 438, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 144, 16, 1075, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 8, indexing `_gShelterB2PodBottomView8SpriteSources`.
///
/// A zero-count first record retains decoded background strips. Subsequent
/// ranges cover all 50 sources; SPRITE_BATCH_END ends the list. Visibility and
/// cached-packet exclusion start clear. Keep this mutable list live while used.
static SpriteBatch _gShelterB2PodBottomView8SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 3, 0 } },
    { 14, 6, 0, 0, { 0, 0 } },
    { 20, 10, 0, 0, { 5, 0 } },
    { 30, 9, 0, 0, { 1, 0 } },
    { 39, 4, 0, 0, { 4, 0 } },
    { 43, ARRAY_SIZE(_gShelterB2PodBottomView8SpriteSources) - 43, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprite rectangles for mapped view 9.
///
/// Uses 8-bit texture pages (896, 0) and (960, 0) and CLUT row 255.
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. The view's batches select all 55 source elements.
/// The renderer borrows this storage while the room overlay remains loaded.
static SpriteSource _gShelterB2PodBottomView9SpriteSources[55] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -160, 24, 2075, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 16, 2075, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -160, 32, 2050, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -160, 40, 2000, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 104, 8 } }, -160, 56, 1900, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, -160, 48, 1925, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 112, 8 } }, -160, 64, 1800, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 120, 8 } }, -160, 72, 1725, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 128, 8 } }, -160, 80, 1662, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 120, 8 } }, -144, 88, 1575, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 144, 8 } }, -152, 96, 1475, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 144, 16 } }, -152, 104, 1425, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, 72, 32, 2220, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, 120, 24, 2222, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, 64, 40, 2125, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 104, 8 } }, 56, 48, 2100, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 112, 8 } }, 48, 56, 2050, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 120, 8 } }, 40, 64, 1950, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 128, 8 } }, 32, 72, 1900, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 128, 8 } }, 32, 80, 1825, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 128, 8 } }, 32, 88, 1700, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 136, 8 } }, 24, 96, 1600, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 144, 16 } }, 16, 104, 1600, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 24 } }, -96, 96, 750, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 72, 24 } }, -32, 96, 750, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 24 } }, 40, 96, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, -96, 24, 525, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -72, 32, 500, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 16 } }, -56, 24, 500, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -56, 64, 500, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, -8, 24, 500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 16 } }, 8, 48, 475, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 56, 32, 500, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, 72, 32, 500, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 72, 64, 500, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 88 } }, 104, 32, 550, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 120, 16, 550, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 128, -8, 600, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 136, -8, 650, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 144, 8, 750, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -112, 16, 600, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -104, 16, 575, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -120, 8, 650, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -128, 0, 700, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -136, -8, 750, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -144, -8, 850, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -152, -16, 950, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -160, -24, 1050, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 144, -8, 1250, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 152, -16, 1500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 16 } }, -160, 8, 3375, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 16 } }, -96, 8, 3375, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -32, 8, 3375, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 88, 16 } }, 72, 16, 3508, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 72, 16 } }, 0, 16, 3425, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 9, indexing `_gShelterB2PodBottomView9SpriteSources`.
///
/// A zero-count first record retains decoded background strips. Subsequent
/// ranges cover all 55 sources; SPRITE_BATCH_END ends the list. Visibility and
/// cached-packet exclusion start clear. Keep this mutable list live while used.
static SpriteBatch _gShelterB2PodBottomView9SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 0, 0 } },
    { 23, 3, 0, 0, { 3, 0 } },
    { 26, 22, 0, 0, { 2, 0 } },
    { 48, 2, 0, 0, { 4, 0 } },
    { 50, ARRAY_SIZE(_gShelterB2PodBottomView9SpriteSources) - 50, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprite rectangles for mapped view 10.
///
/// Uses 8-bit texture page (960, 0) and CLUT row 255.
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. The view's batches select all 11 source elements.
/// The renderer borrows this storage while the room overlay remains loaded.
static SpriteSource _gShelterB2PodBottomView10SpriteSources[11] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, 104, 1071, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -160, 32, 1000, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -136, 40, 1000, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -104, 40, 1104, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 40 } }, -40, 48, 1118, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 16, 64, 1166, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 72, 72, 1236, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 88, 72, 1273, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, 120, 80, 1315, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 32 } }, -88, 48, 1058, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 48, 64, 1203, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 10, indexing `_gShelterB2PodBottomView10SpriteSources`.
///
/// A zero-count first record retains decoded background strips. Subsequent
/// ranges cover all 11 sources; SPRITE_BATCH_END ends the list. Visibility and
/// cached-packet exclusion start clear. Keep this mutable list live while used.
static SpriteBatch _gShelterB2PodBottomView10SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB2PodBottomView10SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprite rectangles for mapped view 11.
///
/// Uses 8-bit texture pages (896, 0) and (960, 0) and CLUT row 255.
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. The view's batches select all 25 source elements.
/// The renderer borrows this storage while the room overlay remains loaded.
static SpriteSource _gShelterB2PodBottomView11SpriteSources[25] = {
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 56 } }, -160, 64, 1551, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, -128, 48, 1580, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, -96, 32, 1624, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 64 } }, -64, 16, 1668, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 72 } }, -32, -8, 1727, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, 8, -24, 1790, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, 40, -40, 1845, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 56 } }, 72, -56, 1916, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 56 } }, 104, -72, 1980, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, 136, -88, 2045, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 32, 8, 3500, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 40 } }, 88, 80, 2500, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 16 } }, 96, 64, 2500, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 32 } }, 112, 32, 2750, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 24 } }, 88, 8, 3000, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 40 } }, -160, 80, 2500, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 96, 48 } }, -160, 32, 2750, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, -24, -64, 3500, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 88, 72 } }, 0, -64, 3250, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 72, 72 } }, 88, -64, 3000, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 48 } }, -88, -64, 3500, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 48 } }, -160, -64, 3500, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 48 } }, -160, -16, 3000, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 24 } }, -112, -16, 3000, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, -112, 8, 3000, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 11, indexing `_gShelterB2PodBottomView11SpriteSources`.
///
/// A zero-count first record retains decoded background strips. Subsequent
/// ranges cover all 25 sources; SPRITE_BATCH_END ends the list. Visibility and
/// cached-packet exclusion start clear. Keep this mutable list live while used.
static SpriteBatch _gShelterB2PodBottomView11SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, ARRAY_SIZE(_gShelterB2PodBottomView11SpriteSources) - 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 12, retaining decoded background strips.
///
/// A zero-count record precedes SPRITE_BATCH_END. Both descriptor slots retain
/// this list's address; no source elements are selected. Keep the mutable list
/// loaded while this view is in use.
static SpriteBatch _gShelterB2PodBottomView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Five depth-sorted textured rectangles for mapped view 13.
///
/// Uses the 8-bit texture page at (960, 0) and CLUT row 255. Positions and
/// dimensions are pixels relative to the view centre; depths are unscaled
/// sorting values. Its batches select all five elements. The renderer borrows
/// the mutable sources while this room overlay remains loaded.
static SpriteSource _gShelterB2PodBottomView13SpriteSources[5] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 16 } }, -160, 56, 1394, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 80, 16 } }, -96, 56, 1402, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -16, 48, 1402, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 72, 16 } }, 16, 56, 1415, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 72, 16 } }, 88, 56, 1424, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 13, indexing its five source elements.
///
/// The first zero-count record retains decoded background strips; the next
/// selects every source, with visibility and cached-packet exclusion clear.
/// SPRITE_BATCH_END terminates the borrowed mutable list.
static SpriteBatch _gShelterB2PodBottomView13SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB2PodBottomView13SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 14, retaining decoded background strips.
///
/// A zero-count record precedes SPRITE_BATCH_END. Both descriptor slots retain
/// this list's address; no source elements are selected. Keep the mutable list
/// loaded while this view is in use.
static SpriteBatch _gShelterB2PodBottomView14SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 15, retaining decoded background strips.
///
/// A zero-count record precedes SPRITE_BATCH_END. Both descriptor slots retain
/// this list's address; no source elements are selected. Keep the mutable list
/// loaded while this view is in use.
static SpriteBatch _gShelterB2PodBottomView15SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 16, retaining decoded background strips.
///
/// A zero-count record precedes SPRITE_BATCH_END. Both descriptor slots retain
/// this list's address; no source elements are selected. Keep the mutable list
/// loaded while this view is in use.
static SpriteBatch _gShelterB2PodBottomView16SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB2PodBottomSpriteViews[16] = {
    { { .empty = _gShelterB2PodBottomView1SpriteBatches }, _gShelterB2PodBottomView1SpriteBatches, NULL },
    { { .elements = _gShelterB2PodBottomView2SpriteSources }, _gShelterB2PodBottomView2SpriteBatches, NULL },
    { { .elements = _gShelterB2PodBottomView3SpriteSources }, _gShelterB2PodBottomView3SpriteBatches, NULL },
    { { .elements = _gShelterB2PodBottomView4SpriteSources }, _gShelterB2PodBottomView4SpriteBatches, NULL },
    { { .elements = _gShelterB2PodBottomView5SpriteSources }, _gShelterB2PodBottomView5SpriteBatches, NULL },
    { { .elements = _gShelterB2PodBottomView6SpriteSources }, _gShelterB2PodBottomView6SpriteBatches, NULL },
    { { .elements = _gShelterB2PodBottomView7SpriteSources }, _gShelterB2PodBottomView7SpriteBatches, NULL },
    { { .elements = _gShelterB2PodBottomView8SpriteSources }, _gShelterB2PodBottomView8SpriteBatches, NULL },
    { { .elements = _gShelterB2PodBottomView9SpriteSources }, _gShelterB2PodBottomView9SpriteBatches, NULL },
    { { .elements = _gShelterB2PodBottomView10SpriteSources }, _gShelterB2PodBottomView10SpriteBatches, NULL },
    { { .elements = _gShelterB2PodBottomView11SpriteSources }, _gShelterB2PodBottomView11SpriteBatches, NULL },
    { { .empty = _gShelterB2PodBottomView12SpriteBatches }, _gShelterB2PodBottomView12SpriteBatches, NULL },
    { { .elements = _gShelterB2PodBottomView13SpriteSources }, _gShelterB2PodBottomView13SpriteBatches, NULL },
    { { .empty = _gShelterB2PodBottomView14SpriteBatches }, _gShelterB2PodBottomView14SpriteBatches, NULL },
    { { .empty = _gShelterB2PodBottomView15SpriteBatches }, _gShelterB2PodBottomView15SpriteBatches, NULL },
    { { .empty = _gShelterB2PodBottomView16SpriteBatches }, _gShelterB2PodBottomView16SpriteBatches, NULL },
};

/// Authored point lights for model shading throughout the pod-bottom room.
///
/// All view filters admit every logical view. Positions and falloff radii use
/// room game units; RGB uses Q12 intensity. Setup binds parents to the view,
/// and composition and lighting queries overwrite caches and attenuation.
/// Keep these writable records alive while the room's lighting is active.
static WorldCoordPointLight _gShelterB2PodBottomPointLights[57] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7995, 0x3A4D, 6998 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1005, 879, 752 }, { 0, 0 } }, 10, 0x186A0 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -291, -3647, 6898 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 461, 527, 478 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5327, -981, 8386 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3543, 2364, 1913 }, { 0, 0 } }, 6000, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2941, -981, 9449 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3543, 2364, 1913 }, { 0, 0 } }, 6000, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2C54, -981, 0x2C54 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3543, 2364, 1913 }, { 0, 0 } }, 6000, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x297D, -981, 2932 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3543, 2364, 1913 }, { 0, 0 } }, 6000, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8004, -981, 0x28FC } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3543, 2364, 1913 }, { 0, 0 } }, 6000, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5803, -2722, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2B5A, -682, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x28AF, -1083, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9789, -1452, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2D76, -371, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -1535, 8627 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -452, 0x28FA } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -804, 9888 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -1179, 9235 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -3109, 4111 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -2735, 9235 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -2735, 4765 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -2383, 5366 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -3466, 3503 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -364, 3360 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -1445, 5223 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -1102, 4622 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -3115, 9888 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -722, 3968 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2CCD, -3457, 1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9620, -2381, 999 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2806, -2744, 999 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2AB1, -3144, 999 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5693, -1140, 999 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5010, -740, 999 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4384, -377, 999 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6233, -1453, 1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -3458, 0x28FA } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x366F, -1935, 7157 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2085, 2085, 1663 }, { 0, 0 } }, 500, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1677, -3804, 0x2FA1 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x327A, -3804, 825 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -2377, 8627 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x375A, -3804, 2093 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2834, 165, 1041 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2033, 165, 1791 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x33A7, 165, 0x3296 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x367F, 165, 0x2FA6 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8016, -981, 3492 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3543, 2364, 1912 }, { 0, 0 } }, 6000, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3D86, -2530, 7117 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2940, 2940, 2344 }, { 0, 0 } }, 1000, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3D86, -3523, 7117 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2940, 2940, 2344 }, { 0, 0 } }, 1000, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2792, -3804, 0x33F8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3D86, -1589, 7117 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2940, 2940, 2344 }, { 0, 0 } }, 1000, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3D86, -4548, 7117 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2940, 2940, 2344 }, { 0, 0 } }, 1000, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3D86, -5485, 7117 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2940, 2940, 2344 }, { 0, 0 } }, 1000, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8129, -1935, 0x328E } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2085, 2085, 1663 }, { 0, 0 } }, 500, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7905, -1935, 1059 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2085, 2085, 1663 }, { 0, 0 } }, 500, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2017, -1935, 6456 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2085, 2085, 1663 }, { 0, 0 } }, 500, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6347, -2377, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4498, -3458, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5124, -3111, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
};

/// Cone light aimed along room +Y, contributing in every logical view.
///
/// Position and radii use room game units; RGB and the +Y axis use Q12.
/// The full opening is 568 units on a 4096-unit turn. Setup derives rotation
/// from the axis; composition and queries mutate the cache and attenuation.
/// Keep this writable array alive while the room's lighting is active.
static WorldCoordSpotLight _gShelterB2PodBottomConeLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8000, -0x48A8, 7000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3618, 2633, 2264 }, { 0, 0 } }, { 0, ONE, 0, 0 }, 10, 0x4E20, 568 },
};

/// The room's authored light collection: no directional lights, 57 points and one cone.
///
/// Counts are elements in the borrowed mutable arrays, which stay live with
/// this overlay. Per-view ambient minima are supplied separately.
static WorldCoordRoomLights _gShelterB2PodBottomRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB2PodBottomPointLights),
    .pointLights           = _gShelterB2PodBottomPointLights,
    .coneLightCount        = ARRAY_SIZE(_gShelterB2PodBottomConeLights),
    .coneLights            = _gShelterB2PodBottomConeLights,
};

/// Twenty directional boundaries switching between logical views 2..10.
///
/// Each parameter pair is source/destination view, with the final record
/// carrying WORLD_COLLISION_TRIGGER_LAST. Origins, vertices and radii use room
/// game units; normals use twelve fractional bits. Setup binds and links these
/// mutable records to the active view; keep them loaded until unlinked.
static WorldCollisionTrigger _gShelterB2PodBottomViewBoundaryTriggers[20] = {
    { NULL, NULL, NULL, { 2000, -4784, 5327, 0 }, { { -2181, -6160, -16, 0 }, { 2181, -6160, 16, 0 }, { -2181, 6160, -16, 0 }, { 2181, 6160, 16, 0 } }, { 29, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 6516, 0, 9, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1903, -5152, 4959, 0 }, { { 2234, -6160, -8, 0 }, { -2233, -6160, 9, 0 }, { 2234, 6160, -8, 0 }, { -2233, 6160, 9, 0 } }, { 15, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 6536, 0, 2, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4627, -4928, 0x34F6, 0 }, { { 5, -6160, -1799, 0 }, { -5, -6160, 1799, 0 }, { 5, 6160, -1799, 0 }, { -5, 6160, 1799, 0 } }, { 4108, 0, 11, 0 }, { 0, 0, 4096, 0 }, 6415, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4858, -4896, 0x353B, 0 }, { { 1, -6160, 1796, 0 }, { 0, -6160, -1796, 0 }, { 1, 6160, 1796, 0 }, { 0, 6160, -1796, 0 } }, { -4102, 0, 1, 0 }, { 0, 0, 4096, 0 }, 6415, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x379F, -4576, 8991, 0 }, { { -2181, -6160, -16, 0 }, { 2181, -6160, 16, 0 }, { -2181, 6160, -16, 0 }, { 2181, 6160, 16, 0 } }, { 29, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 6516, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x377F, -4576, 8703, 0 }, { { 2234, -6160, -9, 0 }, { -2234, -6160, 9, 0 }, { 2234, 6160, -9, 0 }, { -2234, 6160, 9, 0 } }, { 16, 0, 4097, 0 }, { 0, 0, 4096, 0 }, 6536, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5108, -4800, 1076, 0 }, { { -40, -6160, -1795, 0 }, { 40, -6160, 1796, 0 }, { -40, 6160, -1795, 0 }, { 40, 6160, 1796, 0 } }, { 4100, 0, -92, 0 }, { 0, 0, 4096, 0 }, 6415, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5472, -4864, 1075, 0 }, { { 39, -6160, 1797, 0 }, { -38, -6160, -1796, 0 }, { 39, 6160, 1797, 0 }, { -38, 6160, -1796, 0 } }, { -4104, 0, 87, 0 }, { 0, 0, 4096, 0 }, 6415, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5375, -4384, 9502, 0 }, { { 1357, -6160, 1179, 0 }, { -1357, -6160, -1178, 0 }, { 1357, 6160, 1179, 0 }, { -1357, 6160, -1178, 0 } }, { -2692, 0, 3098, 0 }, { 0, 0, 4096, 0 }, 6415, 0, 3, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5183, -4192, 9694, 0 }, { { -1297, -6160, -1244, 0 }, { 1298, -6160, 1244, 0 }, { -1297, 6160, -1244, 0 }, { 1298, 6160, 1244, 0 } }, { 2840, 0, -2964, 0 }, { 0, 0, 4096, 0 }, 6415, 0, 10, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x289F, -4416, 4703, 0 }, { { -1297, -6160, -1244, 0 }, { 1298, -6160, 1244, 0 }, { -1297, 6160, -1244, 0 }, { 1298, 6160, 1244, 0 } }, { 2840, 0, -2964, 0 }, { 0, 0, 4096, 0 }, 6415, 0, 7, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x28FF, -4288, 4543, 0 }, { { 1357, -6160, 1179, 0 }, { -1357, -6160, -1178, 0 }, { 1357, 6160, 1179, 0 }, { -1357, 6160, -1178, 0 } }, { -2692, 0, 3098, 0 }, { 0, 0, 4096, 0 }, 6415, 0, 10, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 895, 0, 0x2A3F, 0 }, { { -2181, -6160, -16, 0 }, { 2182, -6160, 17, 0 }, { -2181, 6160, -16, 0 }, { 2182, 6160, 17, 0 } }, { 30, 0, -4099, 0 }, { 0, 0, 4096, 0 }, 6516, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 895, 0, 0x293F, 0 }, { { 2234, -6160, -8, 0 }, { -2233, -6160, 9, 0 }, { 2234, 6160, -8, 0 }, { -2233, 6160, 9, 0 } }, { 15, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 6536, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3A9F, 0, 3455, 0 }, { { -2182, -6160, 11, 0 }, { 2181, -6160, -12, 0 }, { -2182, 6160, 11, 0 }, { 2181, 6160, -12, 0 } }, { -22, 0, -4099, 0 }, { 0, 0, 4096, 0 }, 6516, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A00, 0, 1088, 0 }, { { -40, -6160, -1795, 0 }, { 40, -6160, 1796, 0 }, { -40, 6160, -1795, 0 }, { 40, 6160, 1796, 0 } }, { 4100, 0, -92, 0 }, { 0, 0, 4096, 0 }, 6415, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2B00, 0, 1088, 0 }, { { 39, -6160, 1797, 0 }, { -38, -6160, -1796, 0 }, { 39, 6160, 1797, 0 }, { -38, 6160, -1796, 0 } }, { -4104, 0, 87, 0 }, { 0, 0, 4096, 0 }, 6415, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2740, 0, 0x3320, 0 }, { { -72, -6160, -1797, 0 }, { 73, -6160, 1798, 0 }, { -72, 6160, -1797, 0 }, { 73, 6160, 1798, 0 } }, { 4105, 0, -166, 0 }, { 0, 0, 4096, 0 }, 6415, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2920, 0, 0x3320, 0 }, { { 145, -6160, 1791, 0 }, { -144, -6160, -1791, 0 }, { 145, 6160, 1791, 0 }, { -144, 6160, -1791, 0 } }, { -4091, 0, 329, 0 }, { 0, 0, 4096, 0 }, 6415, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3ADF, 0, 3199, 0 }, { { 2234, -6160, -8, 0 }, { -2233, -6160, 9, 0 }, { 2234, 6160, -8, 0 }, { -2233, 6160, 9, 0 } }, { 15, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 6536, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

enum {
    SHELTER_B2_POD_BOTTOM_VARIANT_ENTRY_SCENE = 1,
    SHELTER_B2_POD_BOTTOM_VARIANT_FIGHT       = 2,
    SHELTER_B2_POD_BOTTOM_RESOURCE_BOSS       = 36,
    SHELTER_B2_POD_BOTTOM_RESOURCE_TENTACLE   = 45,
    SHELTER_B2_POD_BOTTOM_RESOURCE_EVE        = 132,
};

/// Resource recipes for placement variant 1's entry scene.
///
/// Entry IDs match its boss, tentacle and Eve placements. Each selects
/// descriptor zero; file-group selectors and decimal file numbers identify
/// their loaded actor packages. AREA_PLACEMENT_END terminates the list, which
/// remains borrowed with its descriptor pointers during loading and spawning.
static AreaResource _gShelterB2PodBottomEntrySceneResources[4] = {
    { .entryId = SHELTER_B2_POD_BOTTOM_RESOURCE_BOSS, .fileNumber = 36, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor403600TaskTable },
    { .entryId = SHELTER_B2_POD_BOTTOM_RESOURCE_TENTACLE, .fileNumber = 611, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor361100TentacleTaskDesc },
    { .entryId = SHELTER_B2_POD_BOTTOM_RESOURCE_EVE, .fileNumber = 611, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_60, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor361100EveTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Boss, tentacle and Eve placements for variant 1's entry scene.
///
/// The boss starts at (8000, -3000, 6400), yaw -1024; the scene actors start
/// at the origin with texture-page/CLUT-row offsets (1, 4) and (-1, 2).
/// Positions use room game units and yaw uses 4096 units per turn. Spawn
/// variant, mode, additional-file component and actor parameter row are zero.
/// AREA_PLACEMENT_END ends the list; spawned actors borrow records until teardown.
static AreaPlacement _gShelterB2PodBottomEntryScenePlacements[4] = {
    { SHELTER_B2_POD_BOTTOM_RESOURCE_BOSS, 0, 0, 8000, -3000, 6400, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0, 0, 0 },
    { SHELTER_B2_POD_BOTTOM_RESOURCE_TENTACLE, 0, 0, 0, 0, 0, 0, 0, 1, 4, 0 },
    { SHELTER_B2_POD_BOTTOM_RESOURCE_EVE, 0, 0, 0, 0, 0, 0, 0, -1, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Resource recipes for placement variant 2's immediate boss fight.
///
/// Loads the same boss, tentacle and Eve recipes as the entry scene into
/// a separately stored list. Entry IDs match its placements, descriptor indices
/// are zero and AREA_PLACEMENT_END terminates the table. Borrowed list and
/// descriptor storage must remain loaded during loading and spawning.
static AreaResource _gShelterB2PodBottomFightResources[4] = {
    { .entryId = SHELTER_B2_POD_BOTTOM_RESOURCE_BOSS, .fileNumber = 36, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor403600TaskTable },
    { .entryId = SHELTER_B2_POD_BOTTOM_RESOURCE_TENTACLE, .fileNumber = 611, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor361100TentacleTaskDesc },
    { .entryId = SHELTER_B2_POD_BOTTOM_RESOURCE_EVE, .fileNumber = 611, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_60, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor361100EveTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Boss, tentacle and Eve placements for variant 2's immediate boss fight.
///
/// The boss starts at (8000, -3000, 6400), yaw -1024; the other actors start
/// at the origin with texture-page/CLUT-row offsets (1, 4) and (-1, 2).
/// Positions use room game units and yaw uses 4096 units per turn. Spawn
/// variant, mode, additional-file component and actor parameter row are zero.
/// AREA_PLACEMENT_END ends the list; spawned actors borrow records until teardown.
static AreaPlacement _gShelterB2PodBottomFightPlacements[4] = {
    { SHELTER_B2_POD_BOTTOM_RESOURCE_BOSS, 0, 0, 8000, -3000, 6400, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0, 0, 0 },
    { SHELTER_B2_POD_BOTTOM_RESOURCE_TENTACLE, 0, 0, 0, 0, 0, 0, 0, 1, 4, 0 },
    { SHELTER_B2_POD_BOTTOM_RESOURCE_EVE, 0, 0, 0, 0, 0, 0, 0, -1, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB2PodBottomAreaVariants[11] = {
    [SHELTER_B2_POD_BOTTOM_VARIANT_ENTRY_SCENE] = { .placements = _gShelterB2PodBottomEntryScenePlacements, .resources = _gShelterB2PodBottomEntrySceneResources },
    [SHELTER_B2_POD_BOTTOM_VARIANT_FIGHT]       = { .placements = _gShelterB2PodBottomFightPlacements, .resources = _gShelterB2PodBottomFightResources },
};

/// Refuses every key-item use without consuming the item or changing room state.
///
/// `ROOM_MESSAGE_USE_KEY_ITEM` supplies the collected-item ID and zero second
/// payload. Both are ignored; the result selects the menu's refusal presentation.
static s32 _shelterB2PodBottomRejectKeyItemMessage(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts a room transition after resolving its destination from Shelter progress.
///
/// `ROOM_EVENT_MESSAGE_RESOLVE` borrows a complete request and writable reply;
/// they may alias. Copies all eight bytes before resolving the reply's room.
/// Query requests preserve that copy. The map overlay must be loaded; neither
/// pointer is retained. Returns 1 even when the destination is unchanged.
static s32 _shelterB2PodBottomResolveRoomEventMessage(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_B2_POD_BOTTOM_ROOM_EVENT_ACCEPTED = 1 };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return SHELTER_B2_POD_BOTTOM_ROOM_EVENT_ACCEPTED;
}

/// Ignores `ROOM_MESSAGE_COMMAND` and both integer payloads, returning zero.
static s32 _shelterB2PodBottomIgnoreCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores `DIRECTION_MESSAGE_ROOM_ACTION` and returns zero without room changes.
///
/// The direction system supplies a borrowed four-byte request and zero second
/// payload. Neither is read or retained.
static s32 _shelterB2PodBottomIgnoreActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* unusedRequest, s32 unusedArg)
{
    return 0;
}

/// Publishes the pod-bottom room task and starts its entry scene or boss fight.
///
/// Runs in state 0 and advances to idle state 1. Placement variant 1 starts the
/// skippable actor_361100 scene after clearing its retained head-aim task handle;
/// its actor overlay and both scripts must stay live through playback. Other
/// variants require the boss at placement 0 and synchronously send its start-fight
/// command. The room message table remains borrowed while this task is registered.
static void _shelterB2PodBottomInitializeRoomTask(Task* task)
{
    enum { ROOM_VARIANT_ENTRY_SCENE = 1,
           BOSS_PLACEMENT_INDEX     = 0 };

    ActorCommand command;

    task->msgTable = _gShelterB2PodBottomRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.variant == ROOM_VARIANT_ENTRY_SCENE) {
        actor361100ClearHeadAimTaskHandle(0);
        evsStartScriptWithSkip(gActor361100PodSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor361100PodSceneSkipScript);
    } else {
        // This boss command ignores its context; keep the complete record initialized.
        command.context.loc.stage = GAME_STAGE_NONE;
        command.context.loc.area  = 0;
        command.command           = ACTOR_403600_COMMAND_START_FIGHT;
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(BOSS_PLACEMENT_INDEX), ACTOR_COMMAND_MESSAGE_APPLY, &command, 0);
    }
    task->state++;
}

/// Keeps the initialized room task live to receive messages without per-frame work.
///
/// The binary reserves 16 stack bytes without accessing them.
static void _shelterB2PodBottomIdleRoomTask(Task* unusedTask)
{
    char unusedStackSpace[0x10];
}

void shelterB2PodBottomRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gShelterB2PodBottomRoomTaskStates;
    handlers.funcs[task->state](task);
}
