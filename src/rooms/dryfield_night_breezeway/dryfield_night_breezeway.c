#include "rooms/dryfield_night_breezeway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00100.h"
#include "actors/actor_01500.h"
#include "actors/actor_02500.h"
#include "actors/actor_03700.h"
#include "actors/actor_300700.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"

static void _dryfieldNightBreezewayInitRoomTask(Task* task);
static void _dryfieldNightBreezewayIdleRoomTask(Task* task);

/// Room receiver lifecycle: registration, idle and teardown.
///
/// `Task::state` selects 0 register messages, 1 idle or 2 kill. The dispatcher
/// copies all three callbacks before selecting one; keep this overlay loaded
/// through task teardown.
static const TaskFuncTable3 _gDryfieldNightBreezewayRoomTaskStates = {
    .funcs = { _dryfieldNightBreezewayInitRoomTask, _dryfieldNightBreezewayIdleRoomTask, taskKill },
};

static s32 _dryfieldNightBreezewayRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg);
static s32 _dryfieldNightBreezewayAcceptTransition(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldNightBreezewayHandleRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArgument);
static s32 _dryfieldNightBreezewayIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

static WorldCollisionGrid    _gDryfieldNightBreezewayCollisionGrid;
static WorldCollisionTrigger _gDryfieldNightBreezewayViewBoundaries[4];
static WorldCollisionTrigger _gDryfieldNightBreezewayActionTriggers[3];
static WorldCoordRoomLights  _gDryfieldNightBreezewayRoomLights;

/// Synchronous messages accepted by the night breezeway's room receiver.
///
/// Transition resolution borrows a readable `RoomEventMsg` and writable reply,
/// which may alias, and returns 1. Key-item use carries an integer item ID and
/// returns 0 to refuse it. Room actions borrow a `DirectionActionRequest` and
/// are ignored; room commands carry an integer CAP command ID. Both return 0.
/// The second word of key-item, action and command messages is unused.
/// `TASK_MESSAGE_TABLE_END` makes other IDs return 0; never dispatch that ID.
/// The task borrows this table and its callbacks until receiver teardown.
static TaskMessageEntry _gDryfieldNightBreezewayRoomMessages[5] = {
    { .messageId = ROOM_EVENT_MESSAGE_RESOLVE, .handler = _dryfieldNightBreezewayAcceptTransition },
    { .messageId = ROOM_MESSAGE_USE_KEY_ITEM, .handler = _dryfieldNightBreezewayRejectKeyItemUse },
    { .messageId = DIRECTION_MESSAGE_ROOM_ACTION, .handler = _dryfieldNightBreezewayIgnoreRoomAction },
    { .messageId = ROOM_MESSAGE_COMMAND, .handler = _dryfieldNightBreezewayHandleRoomCommand },
    { .messageId = TASK_MESSAGE_TABLE_END, .handler = NULL },
};

enum {
    DRYFIELD_NIGHT_BREEZEWAY_GLOW_STAR_POINT         = 0,
    DRYFIELD_NIGHT_BREEZEWAY_GLOW_FLARE_POINT        = 1,
    DRYFIELD_NIGHT_BREEZEWAY_GLOW_STAR_SHAFT_POINTS  = 2,
    DRYFIELD_NIGHT_BREEZEWAY_GLOW_VIEW3_SHAFT_POINTS = 4,
    DRYFIELD_NIGHT_BREEZEWAY_GLOW_VIEW2_SHAFT_POINTS = 6,
};

/// World-space glow anchors in integer game coordinates.
///
/// Points 0 and 1 are the star and flare centres. Consecutive pairs 2/3,
/// 4/5 and 6/7 are ordered shaft endpoints for views 3/4, 3 and 2 respectively.
/// The draw routines borrow these points only while submitting this frame.
static SVECTOR _gDryfieldNightBreezewayGlowPoints[8] = {
    { 17990, -1365, 2825, 0 },
    { 4290, -3080, 1810, 0 },
    { 17500, -2850, 2680, 0 },
    { 17870, -2600, 2680, 0 },
    { 14000, -2600, 1140, 0 },
    { 14000, -2850, 1500, 0 },
    { 10000, -2850, 1500, 0 },
    { 10000, -2600, 1140, 0 },
};

WorldCoordRoomLighting gDryfieldNightBreezewayRoomLighting[1] = {
    { .lights = &_gDryfieldNightBreezewayRoomLights, .ambientTable = NULL },
};

WorldCollisionRoomResources gDryfieldNightBreezewayRoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldNightBreezewayCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightBreezewayViewBoundaries,
        .actionTriggers       = _gDryfieldNightBreezewayActionTriggers,
        .occluders            = NULL,
    },
};

u8* gDryfieldNightBreezewayViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldNightBreezewayViewCounts[1] = { ARRAY_SIZE(gDryfieldNightBreezewayViewCameras) };

// Optional door map-marker nibble IDs stored by these endpoints.
enum {
    DRYFIELD_NIGHT_BREEZEWAY_WATER_TOWER_MAP_FLAG = 476,
    DRYFIELD_NIGHT_BREEZEWAY_FACTORY_MAP_FLAG     = 475,
};

DirectionWarpEntry gDryfieldNightBreezewayWarpEntries[2] = {
    {
        .player         = { .yaw = { .word = (ACTOR_TRANSFORM_ANGLE_TURN / 4) }, .x = 6656, .y = 1, .z = 1568 },
        .unknown_10     = { 0 },
        .companion      = { .yaw = { .word = (ACTOR_TRANSFORM_ANGLE_TURN / 4) }, .x = 7940, .y = 1, .z = 1590 },
        .unknown_24     = { 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_BREEZEWAY_WATER_TOWER_MAP_FLAG,
    },
    {
        .player         = { .yaw = { .word = (ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4) }, .x = 0x44F3, .y = 1, .z = 2075 },
        .unknown_10     = { 0 },
        .companion      = { .yaw = { .word = (ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4) }, .x = 0x44F3, .y = 1, .z = 2075 },
        .unknown_24     = { 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY, 4),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY, 3),
        .blockedSound   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY, 5),
        .initialView    = 4,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_BREEZEWAY_FACTORY_MAP_FLAG,
    },
};

/// Collision-grid normal pool, with 4096 units for unit length.
static SVECTOR _gDryfieldNightBreezewayCollisionGridNormals[16] = {
#include "assets/dryfield_night_breezeway_collision_01604_normals.inc"
};

/// Collision-grid vertex pool in room-local integer game coordinates.
static SVECTOR _gDryfieldNightBreezewayCollisionGridVerts[48] = {
#include "assets/dryfield_night_breezeway_collision_01604_verts.inc"
};

/// Collision-grid quads indexing the vertex and normal pools; surface classes 0..2.
static WorldCollisionGridFace _gDryfieldNightBreezewayCollisionGridFaces[25] = {
#include "assets/dryfield_night_breezeway_collision_01604_faces.inc"
};

/// Eight terminated face-index lists in their serialized 130-halfword storage.
static s16 _gDryfieldNightBreezewayCollisionGridCells[130] = {
#include "assets/dryfield_night_breezeway_collision_01604_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldNightBreezewayCollisionGridCells` (130 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_NIGHT_BREEZEWAY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightBreezewayCollisionGridCells[(halfwordOffset)])

/// X-major 4-by-2 cell directory borrowing the face-index lists.
static s16* _gDryfieldNightBreezewayCollisionGridTable[8] = {
#include "assets/dryfield_night_breezeway_collision_01604_table.inc"
};
#undef DRYFIELD_NIGHT_BREEZEWAY_CELL_FACE_LIST_AT_OFFSET

/// Breezeway collision mesh and its 4-by-2 XZ cell index.
///
/// Cells span 4000 game units, with origin (5000, -1000). Room activation
/// binds the view transform and borrows this record and its mesh pools until
/// the room resources are replaced; the overlay must remain loaded throughout.
static WorldCollisionGrid _gDryfieldNightBreezewayCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightBreezewayCollisionGridNormals,
    .vertices    = _gDryfieldNightBreezewayCollisionGridVerts,
    .faces       = _gDryfieldNightBreezewayCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightBreezewayCollisionGridTable,
    .xBias       = -5000,
    .zBias       = 1000,
    .cellCountX  = 4,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightBreezewayCollisionGridFaces),
};

ViewCamera gDryfieldNightBreezewayViewCameras[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2EE0, 0x5DC0, 0 } }, 322 },
    { { { { 0, 0, 4096 }, { -1081, 3950, 0 }, { -3950, -1081, 0 } }, { -0x38A4, 200, -1600 } }, 257 },
    { { { { 0, 0, -4095 }, { 831, 4010, 0 }, { 4010, -831, 0 } }, { -9800, 300, -1600 } }, 257 },
    { { { { 1090, 0, -3948 }, { 702, 4030, 194 }, { 3885, -729, 1072 } }, { -0x35CA, 500, -1150 } }, 257 },
    { { { { 1589, 0, -3775 }, { -1365, 3818, -575 }, { 3519, 1481, 1481 } }, { -0x459C, 1450, -2780 } }, 257 },
    { { { { 0, 0, -4096 }, { 0, 4096, 0 }, { 4096, 0, 0 } }, { -0x41AC, 295, -3414 } }, 680 },
};

/// View 1's empty sprite list; the zero first count preserves background-image strips.
static SpriteBatch _gDryfieldNightBreezewayView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's 96 source rectangles, in view-centred pixels with unscaled OT depths.
///
/// Batches select elements 0..89 and 90..95. Cached packets snapshot texture
/// geometry while per-frame linking reads the source depths; storage stays
/// writable and loaded for the view's lifetime.
static SpriteSource _gDryfieldNightBreezewayView2SpriteSources[96] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -120, 426, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -120, 420, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -112, 431, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -112, 424, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -104, 435, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -104, 427, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -96, 439, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -96, 430, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -88, 442, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -88, 433, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -80, 446, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -80, 437, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -72, 453, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -72, 441, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -64, 463, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -64, 450, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -56, 474, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -56, 461, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -48, 488, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -48, 474, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -40, 503, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -40, 487, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -32, 505, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -32, 489, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -24, 488, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -24, 361, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -16, 366, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -16, 344, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -8, 350, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -8, 331, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 0, 344, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 0, 296, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 8, 301, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 8, 289, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 16, 299, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 16, 287, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 24, 297, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 24, 286, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 32, 276, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 32, 269, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 40, 1833, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 40, 271, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 40, 266, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 48, 267, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 48, 261, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 56, 261, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 56, 256, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 64, 253, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 64, 249, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 72, 236, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 72, 233, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 80, 230, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 80, 224, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 88, 227, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 88, 222, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 96, 225, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 96, 220, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 104, 223, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 104, 213, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 112, 221, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 112, 216, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, -120, 516, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, -40, 728, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, 40, 254, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, 40, 266, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, -40, 1002, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, -120, 566, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 64, -120, 687, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 64, -40, 1190, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 64, 40, 287, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 56, 40, 396, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 56, -40, 1253, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 56, -120, 1143, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 48, -120, 1251, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 48, -40, 1429, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, 40, 426, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 40, -40, 2500, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 40, -120, 1353, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 32, -112, 1483, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 32, -40, 2500, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 24, -40, 2500, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, -96, 1676, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -80, 1846, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 16, -40, 2500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, 40, 2500, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 40, 2069, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 72, 600, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 40, 2500, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, 56, 608, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 40, 48, 463, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 64, 1623, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 64, 652, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 64, 1167, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 72, 857, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 72, 634, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 72, 626, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
};

/// View 2's sprite ranges, preserving image strips and selecting all 96 sources.
///
/// The serialized `SpriteBatch.field_6` bytes are retained; their role is unproven.
static SpriteBatch _gDryfieldNightBreezewayView2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 90, 0, 0, { 1, 0 } },
    { 90, ARRAY_SIZE(_gDryfieldNightBreezewayView2SpriteSources) - 90, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's 46 source rectangles, in view-centred pixels with unscaled OT depths.
///
/// Batches select elements 0..31 and 32..45. Cached texture geometry and live
/// sorting depths borrow this writable storage while the overlay is loaded.
static SpriteSource _gDryfieldNightBreezewayView3SpriteSources[46] = {
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -80, -120, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -72, -120, 750, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -56, -120, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -64, -120, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, -120, 750, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -120, 750, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -16, 750, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -80, 48, 450, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, 48, 450, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -72, -16, 750, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -160, -120, 464, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -160, -32, 379, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, 48, 375, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -144, 48, 375, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -128, 48, 375, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -112, 48, 375, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -96, 48, 375, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -144, -32, 409, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -128, -32, 426, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -112, -32, 446, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -32, 625, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -16, 750, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -64, 24, 450, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -56, 32, 450, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -48, 48, 450, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -40, 48, 450, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -32, 56, 450, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 64, 450, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -144, -120, 490, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -120, 512, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, -120, 535, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -96, -120, 500, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 56, 250, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 80, 250, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 250, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -144, 64, 250, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -128, 64, 250, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, 32, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -96, 48, 250, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -80, 32, 250, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -72, 24, 250, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -64, 32, 250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -56, 40, 250, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 80, 250, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 80, 250, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -32, 96, 250, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

/// View 3's sprite ranges, preserving image strips and selecting all 46 sources.
///
/// The serialized `SpriteBatch.field_6` bytes are retained; their role is unproven.
static SpriteBatch _gDryfieldNightBreezewayView3SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 32, 0, 0, { 1, 0 } },
    { 32, ARRAY_SIZE(_gDryfieldNightBreezewayView3SpriteSources) - 32, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's empty sprite list; the zero first count preserves background-image strips.
static SpriteBatch _gDryfieldNightBreezewayView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's empty sprite list; the zero first count preserves background-image strips.
static SpriteBatch _gDryfieldNightBreezewayView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's 60 source rectangles, in view-centred pixels with unscaled OT depths.
///
/// Its single nonempty batch selects the entire array. Cached texture geometry
/// and live sorting depths borrow this writable storage while the overlay is loaded.
static SpriteSource _gDryfieldNightBreezewayView6SpriteSources[60] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 8, 298, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 8, 296, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 16, 298, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 16, 296, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 24, 298, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 24, 296, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 32, 298, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 32, 296, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 40, 298, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 40, 296, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 48, 298, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 48, 296, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 56, 298, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 56, 296, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 64, 298, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 64, 296, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 72, 296, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 72, 299, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 80, 297, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 80, 297, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -144, -96, 296, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, -160, -8, 269, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -144, 8, 260, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -128, 8, 282, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -112, 8, 271, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -96, 8, 296, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -80, 8, 298, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -64, 96, 244, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, -128, -88, 298, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -64, 8, 297, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -48, 8, 298, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -32, 8, 298, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -16, 8, 298, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 0, 8, 298, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, -112, -88, 298, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -96, -88, 298, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -80, -88, 298, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -64, -88, 298, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -48, -88, 298, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -32, -88, 298, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -16, -88, 298, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 0, -88, 298, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 16, -88, 298, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, 32, -88, 298, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, 48, -88, 298, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, 64, -88, 297, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, -88, 301, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, -88, 301, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, -88, 301, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 80, -72, 297, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, -56, 298, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, -32, 294, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 16, 8, 298, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 128, -96, 298, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 32, 8, 298, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 48, 8, 298, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 64, 8, 298, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 80, 8, 298, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 96, 8, 298, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 112, 8, 298, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
};

/// View 6's complete source range, preceded by the image-strip selection record.
static SpriteBatch _gDryfieldNightBreezewayView6SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightBreezewayView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightBreezewaySpriteViews[6] = {
    { { .empty = _gDryfieldNightBreezewayView1SpriteBatches }, _gDryfieldNightBreezewayView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightBreezewayView2SpriteSources }, _gDryfieldNightBreezewayView2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightBreezewayView3SpriteSources }, _gDryfieldNightBreezewayView3SpriteBatches, NULL },
    { { .empty = _gDryfieldNightBreezewayView4SpriteBatches }, _gDryfieldNightBreezewayView4SpriteBatches, NULL },
    { { .empty = _gDryfieldNightBreezewayView5SpriteBatches }, _gDryfieldNightBreezewayView5SpriteBatches, NULL },
    { { .elements = _gDryfieldNightBreezewayView6SpriteSources }, _gDryfieldNightBreezewayView6SpriteBatches, NULL },
};

/// Four directional model-shading lights shared by every nighttime breezeway view.
///
/// Local translations hold direction vectors, normalized when shading, with
/// equal RGB intensities of 750 in ONE-based fixed point. Coordinate updates
/// and lighting queries mutate the transform caches, parent and attenuation;
/// the room light collection borrows this array while the overlay is loaded.
static WorldCoordLight _gDryfieldNightBreezewayDirectionalLights[] = {
    {
        .transform = { .lighting = {
                           .composeStamp = GRAPHICS_COORD_DIRTY,
                           .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -1000, 0 } },
                           .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                           .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                       } },
        .color     = { 750, 750, 750 },
    },
    {
        .transform = { .lighting = {
                           .composeStamp = GRAPHICS_COORD_DIRTY,
                           .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -1000, 0 } },
                           .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                           .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                       } },
        .color     = { 750, 750, 750 },
    },
    {
        .transform = { .lighting = {
                           .composeStamp = GRAPHICS_COORD_DIRTY,
                           .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, 1000 } },
                           .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                           .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                       } },
        .color     = { 750, 750, 750 },
    },
    {
        .transform = { .lighting = {
                           .composeStamp = GRAPHICS_COORD_DIRTY,
                           .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, -1000 } },
                           .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                           .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                       } },
        .color     = { 750, 750, 750 },
    },
};

/// Seven point-light records for model shading and light queries in every nighttime breezeway view.
///
/// The first three start white at ONE-based RGB intensity; the remaining four
/// start black with inner/outer radii of 0/1. Positions and radii use integer
/// world units. Coordinate updates and lighting queries mutate the parents,
/// transform caches and attenuation. The room light collection borrows this
/// writable array while the overlay is loaded.
static WorldCoordPointLight _gDryfieldNightBreezewayPointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 10000, -1896, 1500 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1278,
        .outer = 2239,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 14000, -1896, 1500 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1278,
        .outer = 2239,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 17500, -2278, 2700 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 2001,
        .outer = 2203,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7500, -2000, 1000 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 0, 0, 0 },
        },
        .inner = 0,
        .outer = 1,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 12500, -2000, 1000 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 0, 0, 0 },
        },
        .inner = 0,
        .outer = 1,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 15000, -2000, 1000 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 0, 0, 0 },
        },
        .inner = 0,
        .outer = 1,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7500, -2000, 1000 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 0, 0, 0 },
        },
        .inner = 0,
        .outer = 1,
    },
};

/// The single room's mutable shading-light collection: four directions, seven points, no cones.
///
/// Coordinate updates and shading queries borrow and mutate the light arrays
/// while the breezeway overlay is loaded; this record owns no allocations.
static WorldCoordRoomLights _gDryfieldNightBreezewayRoomLights = {
    .directionalLightCount = ARRAY_SIZE(_gDryfieldNightBreezewayDirectionalLights),
    .directionalLights     = _gDryfieldNightBreezewayDirectionalLights,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightBreezewayPointLights),
    .pointLights           = _gDryfieldNightBreezewayPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Directed view-boundary quads for transitions 3->2, 2->3, 3->4 and 4->3.
///
/// Origins and corner offsets use integer room coordinates; normals use 4096
/// per unit. The final quad is live and LAST-marked. Room binding mutates the
/// coordinate, links and flags; keep the overlay loaded until they are unlinked.
static WorldCollisionTrigger _gDryfieldNightBreezewayViewBoundaries[4] = {
    { NULL, NULL, NULL, { 0x2E60, -2880, 1440, 0 }, { { 0, -3568, -1024, 0 }, { 0, -3568, 1024, 0 }, { 0, 3568, -1024, 0 }, { 0, 3568, 1024, 0 } }, { 4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3709, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2F20, -2816, 1472, 0 }, { { 0, -3280, 1024, 0 }, { 0, -3280, -1024, 0 }, { 0, 3280, 1024, 0 }, { 0, 3280, -1024, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3434, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3F8D, -2528, 2138, 0 }, { { -70, -3632, 1554, 0 }, { 56, -3632, -1563, 0 }, { -70, 3632, 1554, 0 }, { 56, 3632, -1563, 0 } }, { -4106, 0, -167, 0 }, { 0, 0, 4096, 0 }, 3949, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3ECD, -2576, 2122, 0 }, { { 78, -3392, -1520, 0 }, { -88, -3392, 1512, 0 }, { 78, 3392, -1520, 0 }, { -88, 3392, 1512, 0 } }, { 4099, 0, 223, 0 }, { 0, 0, 4096, 0 }, 3709, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

enum {
    // Warp bytes pack the 1-based departure slot above the destination arrival slot.
    DRYFIELD_NIGHT_BREEZEWAY_WATER_TOWER_WARP_SLOTS = (1 << 4) | 2,
    DRYFIELD_NIGHT_BREEZEWAY_FACTORY_WARP_SLOTS     = (2 << 4) | 2,
    DRYFIELD_NIGHT_BREEZEWAY_TRIGGER_CAP_COMMAND    = 1,
};

/// Facing-gated action quads for the water tower, factory and room CAP command 1.
///
/// The warps use departure slots 1/2 and destination arrival slot 2. The final
/// live quad sends a room command that requests CAP event 1 when idle.
/// Geometry uses room coordinates and 4096-based normals; links, coordinate,
/// flags and hit latches remain mutable until room teardown unlinks the array.
static WorldCollisionTrigger _gDryfieldNightBreezewayActionTriggers[3] = {
    {
        .origin       = { 6416, -48, 1680, 0 },
        .vertices     = { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } },
        .normal       = { 0, 4095, 0, 0 },
        .facingNormal = { 4096, 0, 0, 0 },
        .radius       = 807,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER,
        .parameter1   = DRYFIELD_NIGHT_BREEZEWAY_WATER_TOWER_WARP_SLOTS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { 0x4580, -64, 1760, 0 },
        .vertices     = { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } },
        .normal       = { 0, 4095, 0, 0 },
        .facingNormal = { -4091, 0, 201, 0 },
        .radius       = 807,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_DRYFIELD_NIGHT_FACTORY,
        .parameter1   = DRYFIELD_NIGHT_BREEZEWAY_FACTORY_WARP_SLOTS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { 0x45A0, -64, 2816, 0 },
        .vertices     = { { -368, 0, -528, 0 }, { 368, 0, -528, 0 }, { -368, 0, 528, 0 }, { 368, 0, 528, 0 } },
        .normal       = { 0, 4103, 0, 0 },
        .facingNormal = { -4096, 0, 0, 0 },
        .radius       = 643,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = DRYFIELD_NIGHT_BREEZEWAY_TRIGGER_CAP_COMMAND,
        .parameter1   = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST,
    },
};

/// Variant 1's bat resource, matched by placement entry 37 and ending with `AREA_PLACEMENT_END`.
///
/// Loads actor package 103700 and spawns descriptor 0. Placements and actors
/// borrow this table and its task descriptor while their overlays remain loaded.
static AreaResource _gDryfieldNightBreezewayVariant1Resources[2] = {
    { .entryId = 37, .fileNumber = 37, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor103700BatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's Desert Chaser and moth resource recipes, ending with `AREA_PLACEMENT_END`.
///
/// Entry 1 loads file 400100; entry 8 loads 300700. Each spawns descriptor 0
/// and must stay loaded with the placements and borrowed actor task descriptors.
static AreaResource _gDryfieldNightBreezewayVariant3Resources[3] = {
    { .entryId = 1, .fileNumber = 1, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor00100DesertChaserTaskDesc },
    { .entryId = 8, .fileNumber = 7, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .taskTable = &gActor300700MothTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 21's actor-102500 and Mind Suckler resource recipes, ending with `AREA_PLACEMENT_END`.
///
/// Placement entries 25/15 load files 102500/201500 and spawn descriptor 0.
/// The table and borrowed task descriptors remain loaded through their use.
static AreaResource _gDryfieldNightBreezewayVariant21Resources[3] = {
    { .entryId = 25, .fileNumber = 25, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor102500Tasks },
    { .entryId = 15, .fileNumber = 15, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = &gActor01500MindSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldNightBreezewayAreaVariants[22] = {
    [1]  = { .placements = gMapDryfieldFullBreezewayVariant1Placements, .resources = _gDryfieldNightBreezewayVariant1Resources },
    [3]  = { .placements = gMapDryfieldFullBreezewayVariant3Placements, .resources = _gDryfieldNightBreezewayVariant3Resources },
    [21] = { .placements = gMapDryfieldFullBreezewayVariant21Placements, .resources = _gDryfieldNightBreezewayVariant21Resources },
};

/// Class 2's paired footstep cues in the loaded type-1 sound-script bank.
///
/// Walk and scripted-jump cue 2 use entry 73; running uses entry 75. Cue 1
/// uses the following entry and companion playback adds 100. Surface lookup
/// borrows this record only while the breezeway overlay is loaded.
static WorldCollisionFootstepSounds _gDryfieldNightBreezewayClass2FootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | 73,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | 75,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | 73,
};

/// Shared policy for classes 0 and 3..7: blocking probes, weapon impacts, pushback and no footsteps.
static WorldCollisionSurfaceProperties _gDryfieldNightBreezewayBlockingSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1 policy: pass probes, omit weapon impacts, apply pushback and play no footsteps.
static WorldCollisionSurfaceProperties _gDryfieldNightBreezewayClass1SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 2 policy: blocking probes, weapon impacts and pushback with paired footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightBreezewayClass2SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightBreezewayClass2FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightBreezewaySurfacePropertiesByClass[8] = {
    &_gDryfieldNightBreezewayBlockingSurfaceProperties,
    &_gDryfieldNightBreezewayClass1SurfaceProperties,
    &_gDryfieldNightBreezewayClass2SurfaceProperties,
    &_gDryfieldNightBreezewayBlockingSurfaceProperties,
    &_gDryfieldNightBreezewayBlockingSurfaceProperties,
    &_gDryfieldNightBreezewayBlockingSurfaceProperties,
    &_gDryfieldNightBreezewayBlockingSurfaceProperties,
    &_gDryfieldNightBreezewayBlockingSurfaceProperties,
};

static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);
static void _glowDrawShaft(const SVECTOR worldPoints[2], s32 radiusScale);

/// Refuses every key-item use in the night breezeway.
///
/// All arguments are ignored. Returns 0 to select the item menu's refusal
/// notice, without consuming an item or changing the room.
static s32 _dryfieldNightBreezewayRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts the requested room transition without changing its destination.
///
/// Borrows a readable eight-byte `request` and a writable eight-byte `reply`
/// for synchronous dispatch; they may be the same record. Copies every field
/// for both query and execute requests, retains neither pointer and returns 1
/// to allow the transition. The task and message ID are unused.
static s32 _dryfieldNightBreezewayAcceptTransition(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_NIGHT_BREEZEWAY_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_NIGHT_BREEZEWAY_TRANSITION_ALLOWED;
}

/// Requests CAP event 1 for room command 1 when the interpreter is idle.
///
/// The event holds the player and pauses other actors until completion. Busy
/// requests are discarded. Other command IDs do nothing; every call returns
/// zero. The receiver, message ID and second payload word are unused.
static s32 _dryfieldNightBreezewayHandleRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArgument)
{
    enum {
        DRYFIELD_NIGHT_BREEZEWAY_COMMAND_CAP_EVENT = 1,
        DRYFIELD_NIGHT_BREEZEWAY_CAP_EVENT         = 1,
    };

    if (commandId == DRYFIELD_NIGHT_BREEZEWAY_COMMAND_CAP_EVENT) {
        capSpawnEventIfIdle(DRYFIELD_NIGHT_BREEZEWAY_CAP_EVENT, CAP_EVENT_PAUSE_ACTORS);
    }
    return 0;
}

/// Ignores room-specific action requests in the night breezeway.
///
/// The trigger dispatcher borrows a `DirectionActionRequest` and passes zero
/// as the second payload word. This handler reads no arguments, retains no
/// request and returns 0; the dispatcher does not use the result.
static s32 _dryfieldNightBreezewayIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Registers the night breezeway's room-message receiver and enters idle.
///
/// Requires state 0 and borrows the room's message table until teardown.
/// Keep the overlay loaded while the registered receiver can be messaged.
static void _dryfieldNightBreezewayInitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldNightBreezewayRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Leaves the initialized room task idle while its message table remains active.
///
/// State 1 performs no per-frame work and does not inspect `task` or advance
/// its state. Teardown is the separate state-2 callback.
static void _dryfieldNightBreezewayIdleRoomTask(Task* task)
{
}

void dryfieldNightBreezewayRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gDryfieldNightBreezewayRoomTaskStates;
    handlers.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_pulsing_star.inc.c"

#include "../../shared/effects/glow_draw_shaft.inc.c"

#include "../../shared/effects/glow_draw_flare.inc.c"

void dryfieldNightBreezewayDrawGlowsTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_BREEZEWAY_VIEW_FLARE_AND_SHAFT     = 2,
        DRYFIELD_NIGHT_BREEZEWAY_VIEW_STAR_AND_TWO_SHAFTS = 3,
        DRYFIELD_NIGHT_BREEZEWAY_VIEW_STAR_AND_SHAFT      = 4,
        DRYFIELD_NIGHT_BREEZEWAY_FLARE_TEXTURE_INDEX      = 2,
        DRYFIELD_NIGHT_BREEZEWAY_FLARE_RADIUS_SCALE       = 0x400,
        DRYFIELD_NIGHT_BREEZEWAY_SHAFT_RADIUS_SCALE       = 0x180,
        DRYFIELD_NIGHT_BREEZEWAY_STAR_PULSE_RATE          = 0x600, // Angle units per animation frame; 4096 per turn
        DRYFIELD_NIGHT_BREEZEWAY_STAR_RADIUS_SCALE        = 0x80,
    };

    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    switch (gGameSession->location.view) {
        case DRYFIELD_NIGHT_BREEZEWAY_VIEW_FLARE_AND_SHAFT:
            _glowDrawFlare(&_gDryfieldNightBreezewayGlowPoints[DRYFIELD_NIGHT_BREEZEWAY_GLOW_FLARE_POINT], DRYFIELD_NIGHT_BREEZEWAY_FLARE_TEXTURE_INDEX, DRYFIELD_NIGHT_BREEZEWAY_FLARE_RADIUS_SCALE);
            _glowDrawShaft(&_gDryfieldNightBreezewayGlowPoints[DRYFIELD_NIGHT_BREEZEWAY_GLOW_VIEW2_SHAFT_POINTS], DRYFIELD_NIGHT_BREEZEWAY_SHAFT_RADIUS_SCALE);
            break;
        case DRYFIELD_NIGHT_BREEZEWAY_VIEW_STAR_AND_TWO_SHAFTS:
            _glowDrawShaft(&_gDryfieldNightBreezewayGlowPoints[DRYFIELD_NIGHT_BREEZEWAY_GLOW_VIEW3_SHAFT_POINTS], DRYFIELD_NIGHT_BREEZEWAY_SHAFT_RADIUS_SCALE);
            // This view also shows the star and shaft drawn by view 4.
            /* fallthrough */
        case DRYFIELD_NIGHT_BREEZEWAY_VIEW_STAR_AND_SHAFT:
            glowDrawPulsingStar(&_gDryfieldNightBreezewayGlowPoints[DRYFIELD_NIGHT_BREEZEWAY_GLOW_STAR_POINT], DRYFIELD_NIGHT_BREEZEWAY_STAR_PULSE_RATE, DRYFIELD_NIGHT_BREEZEWAY_STAR_RADIUS_SCALE);
            _glowDrawShaft(&_gDryfieldNightBreezewayGlowPoints[DRYFIELD_NIGHT_BREEZEWAY_GLOW_STAR_SHAFT_POINTS], DRYFIELD_NIGHT_BREEZEWAY_SHAFT_RADIUS_SCALE);
            break;
    }
}
