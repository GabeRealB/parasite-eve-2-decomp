#include "rooms/dryfield_motel_room_2.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor_01200.h"
#include "actors/actor_01500.h"
#include "actors/actor_04000.h"
#include "actors/actor_401800.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/session.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield.h"

enum {
    DRYFIELD_MOTEL_ROOM_2_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_2, 2),
    DRYFIELD_MOTEL_ROOM_2_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_2, 1),
    DRYFIELD_MOTEL_ROOM_2_MAP_FLAG        = 485,
    // This area's departure slot 1 and the main street's arrival slot 4.
    DRYFIELD_MOTEL_ROOM_2_MAIN_STREET_WARP_ENDPOINTS = 0x14,
};

static s32 _dryfieldMotelRoom2RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldMotelRoom2ResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldMotelRoom2IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument);
static s32 _dryfieldMotelRoom2IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static WorldCollisionGrid     _gDryfieldMotelRoom2CollisionGrid;
static WorldCollisionOccluder _gDryfieldMotelRoom2SightOccluders[2];
static WorldCollisionTrigger  _gDryfieldMotelRoom2ViewBoundaryTriggers[8];
static WorldCollisionTrigger  _gDryfieldMotelRoom2ActionTriggers[6];

static WorldCoordRoomLights _gDryfieldMotelRoom2RoomLights;

/// Synchronous room messages: allow transitions, refuse key items and ignore actions and commands.
///
/// The room task borrows the table and its callbacks while the overlay is loaded.
/// The final marker makes unsupported IDs return zero. Read-only authored storage
/// stays in the overlay's data section.
static const TaskMessageEntry _gDryfieldMotelRoom2RoomMessages[] __attribute__((section(".data"))) = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldMotelRoom2ResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldMotelRoom2RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldMotelRoom2IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldMotelRoom2IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8* gDryfieldMotelRoom2RoomViewMaps[1] = {
    gViewIdentityMap,
};

WorldCollisionRoomResources gDryfieldMotelRoom2RoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldMotelRoom2CollisionGrid,
        .viewBoundaryTriggers = _gDryfieldMotelRoom2ViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldMotelRoom2ActionTriggers,
        .occluders            = _gDryfieldMotelRoom2SightOccluders,
    },
};

ViewCount gDryfieldMotelRoom2RoomViewCounts[2] = { ARRAY_SIZE(gDryfieldMotelRoom2ViewCameras), 0 };

WorldCoordRoomLighting gDryfieldMotelRoom2RoomLighting[1] = {
    { &_gDryfieldMotelRoom2RoomLights, NULL },
};

DirectionWarpEntry gDryfieldMotelRoom2WarpEntries[1] = {
    {
        .player         = { { .word = 3 * ONE / 4 }, 4666, 0, 2610 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 3 * ONE / 4 }, 4666, 0, 2610 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_MOTEL_ROOM_2_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_MOTEL_ROOM_2_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_MOTEL_ROOM_2_MAP_FLAG,
    },
};

/// Grid-local normals, with 4096 units per unit length.
static SVECTOR _gDryfieldMotelRoom2CollisionGridNormals[13] = {
#include "assets/dryfield_motel_room_2_collision_0084C_normals.inc"
};

/// Collision-mesh vertices in signed grid-local game units.
static SVECTOR _gDryfieldMotelRoom2CollisionGridVerts[105] = {
#include "assets/dryfield_motel_room_2_collision_0084C_verts.inc"
};

/// Indexed room quads using surface classes 0..3.
static WorldCollisionGridFace _gDryfieldMotelRoom2CollisionGridFaces[43] = {
#include "assets/dryfield_motel_room_2_collision_0084C_faces.inc"
};

/// Four face-index lists ending at elements 43, 81, 112 and 132.
///
/// Element 113 repeats face 0 outside the lists; the final halfword is retained
/// authored storage, also outside every cell scan.
static s16 _gDryfieldMotelRoom2CollisionGridCells[134] = {
#include "assets/dryfield_motel_room_2_collision_0084C_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldMotelRoom2CollisionGridCells` (134 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_MOTEL_ROOM_2_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldMotelRoom2CollisionGridCells[(halfwordOffset)])

/// X-major 2-by-2 cell table, with list starts at elements 0, 44, 82 and 114.
static s16* _gDryfieldMotelRoom2CollisionGridTable[4] = {
#include "assets/dryfield_motel_room_2_collision_0084C_table.inc"
};
#undef DRYFIELD_MOTEL_ROOM_2_CELL_FACE_LIST_AT_OFFSET

/// Motel room 2's collision mesh and 2-by-2 XZ index of 4000-unit cells.
///
/// The room owns this mutable descriptor and its pools. Activation binds the
/// current view coordinate; keep the overlay loaded while the grid is active.
static WorldCollisionGrid _gDryfieldMotelRoom2CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldMotelRoom2CollisionGridNormals,
    .vertices    = _gDryfieldMotelRoom2CollisionGridVerts,
    .faces       = _gDryfieldMotelRoom2CollisionGridFaces,
    .cellFaceIds = _gDryfieldMotelRoom2CollisionGridTable,
    .xBias       = 0,
    .zBias       = 0,
    .cellCountX  = 2,
    .cellCountZ  = ARRAY_SIZE(_gDryfieldMotelRoom2CollisionGridTable) / 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldMotelRoom2CollisionGridFaces),
};

ViewCamera gDryfieldMotelRoom2ViewCameras[6] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -2500, 10250, -3000 } }, 257 },
    { { { { 897, 0, -3996 }, { -928, 3983, -208 }, { 3887, 951, 872 } }, { -200, 1800, -1300 } }, 246 },
    { { { { 1464, 0, 3825 }, { 452, 4067, -173 }, { -3798, 484, 1454 } }, { -4800, 1500, -1000 } }, 225 },
    { { { { 4063, 0, -513 }, { -81, 4044, -640 }, { 507, 645, 4012 } }, { -600, 1660, -1300 } }, 240 },
    { { { { -888, 0, 3998 }, { 1562, 3770, 347 }, { -3680, 1600, -817 } }, { -4800, 1770, -5300 } }, 240 },
    { { { { -561, 0, -4057 }, { -1524, 3795, 211 }, { 3760, 1538, -520 } }, { -1750, 1770, -5300 } }, 235 },
};

/// View 1's empty sprite list, retaining decoded background strips.
///
/// The first zero-count record selects the background; the sentinel ends the
/// list. Its address also occupies the unused source slot and is never read as sprites.
static SpriteBatch _gDryfieldMotelRoom2View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's 72 textured rectangles, selected in source elements by its batches.
///
/// XY and sizes use view-centred pixels; depth is an unscaled sorting value.
/// Packet initialization snapshots geometry and texture data while this overlay is loaded.
static SpriteSource _gDryfieldMotelRoom2View2SpriteSources[72] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 80, 625, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 112, 640, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, 88, 600, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 80, 88, 600, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, 88, 600, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 0, 88, 600, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -40, 88, 600, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -72, 88, 600, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 120, 64, 612, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 80, 64, 612, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 40, 64, 612, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 0, 64, 612, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -40, 64, 612, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -64, 64, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, 48, 650, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 128, 32, 675, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 80, 40, 650, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 40, 40, 650, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 0, 40, 650, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 0, 32, 675, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -40, 40, 650, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -32, 32, 675, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -56, 48, 650, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 80, 500, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 80, 500, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 80, 500, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, -8, 1075, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 16, 1075, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, 16, 1075, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, 48, 1025, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 88, 48, 1025, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, 24, 1025, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, 0, 1025, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 88, 24, 1025, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 88, 0, 1025, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 80, 0, 1025, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 24, 1025, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 16, 875, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 48, 1000, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, 48, 875, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, 24, 875, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 24, 1000, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 8, 1000, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, 8, 875, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 40, 1137, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 887, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 887, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 887, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 887, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -152, 40, 887, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -120, 945, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -80, 945, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -40, 945, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 0, 945, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -120, 40, 945, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -104, 40, 1037, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, -112, 1037, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -80, 1037, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -40, 1037, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, 0, 1037, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, 0, 1137, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, -40, 1137, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, -80, 1137, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -112, 1137, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, -104, 1262, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, -80, 1262, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, -40, 1262, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -72, 0, 1262, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 0, 1287, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -40, 1287, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -80, 1287, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -96, 1287, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
};

/// View 2's six contiguous sprite ranges, preceded by background selection.
///
/// Ranges cover all 72 sources; the final sentinel contributes no sprites.
/// Visibility remains writable while the loaded view borrows this list.
static SpriteBatch _gDryfieldMotelRoom2View2SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 3, 0 } },
    { 23, 3, 0, 0, { 0, 0 } },
    { 26, 3, 0, 0, { 5, 0 } },
    { 29, 8, 0, 0, { 1, 0 } },
    { 37, 7, 0, 0, { 4, 0 } },
    { 44, ARRAY_SIZE(_gDryfieldMotelRoom2View2SpriteSources) - 44, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's 84 textured rectangles, selected in source elements by its batches.
///
/// XY and sizes use view-centred pixels; depth is an unscaled sorting value.
/// Packet initialization snapshots geometry and texture data while this overlay is loaded.
static SpriteSource _gDryfieldMotelRoom2View3SpriteSources[84] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -8, 48, 525, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 64, 487, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 80, 80, 450, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 88, 435, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -120, 88, 435, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -80, 88, 435, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -40, 88, 435, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 0, 88, 435, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 40, 88, 435, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 80, 88, 435, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, 72, 450, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -120, 72, 450, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -80, 72, 450, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -40, 72, 450, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 0, 72, 450, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 40, 72, 450, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -120, 64, 487, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -80, 56, 487, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -40, 56, 487, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 0, 56, 487, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 40, 56, 487, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 48, 525, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -160, 96, 550, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 96, 425, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, 64, 550, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -128, 64, 550, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, 32, 550, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -128, 32, 550, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -128, 0, 550, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -144, 0, 550, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 16, 1075, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 56, 1031, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -152, 56, 1025, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -120, 56, 1025, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -88, 56, 1025, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -56, 56, 1025, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -152, 24, 1055, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -120, 24, 1050, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -88, 24, 1055, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -56, 24, 1037, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, 16, 1075, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 16, 1075, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -24, 24, 1037, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -8, 24, 1037, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, 56, 1031, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 56, 1031, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 40, 1037, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, 104, 637, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 136, 72, 650, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 88, 675, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 136, 40, 650, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 136, 32, 650, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, -120, 937, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 72, 32, 1000, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 72, 0, 1000, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 80, -32, 1000, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 80, -56, 1000, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 80, -96, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, -104, 1000, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 112, -96, 962, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 112, -112, 962, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 136, -120, 962, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -96, 937, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 56, 75, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 16, 75, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -24, 75, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -56, 750, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, -112, 937, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -96, 1000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -96, 1000, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -56, 1000, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -56, 1000, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -16, 1000, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -16, 1000, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, 24, 1000, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 120, 24, 1000, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, 24, 1000, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 96, 24, 987, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 96, -16, 987, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 96, -56, 987, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 104, -64, 987, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 136, -56, 987, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 136, -16, 987, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 24, 987, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
};

/// View 3's seven contiguous sprite ranges, preceded by background selection.
///
/// Ranges cover all 84 sources and end at SPRITE_BATCH_END. Visibility remains
/// writable while the loaded view borrows this list.
static SpriteBatch _gDryfieldMotelRoom2View3SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 3, 0 } },
    { 22, 8, 0, 0, { 4, 0 } },
    { 30, 17, 0, 0, { 1, 0 } },
    { 47, 5, 0, 0, { 5, 0 } },
    { 52, 16, 0, 0, { 0, 0 } },
    { 68, 9, 0, 0, { 6, 0 } },
    { 77, ARRAY_SIZE(_gDryfieldMotelRoom2View3SpriteSources) - 77, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's 47 textured rectangles, selected in source elements by its batches.
///
/// XY and sizes use view-centred pixels; depth is an unscaled sorting value.
/// Packet initialization snapshots geometry and texture data while this overlay is loaded.
static SpriteSource _gDryfieldMotelRoom2View4SpriteSources[47] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -88, 1250, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 80, 950, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, -120, 725, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -80, 725, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -40, 725, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 0, 725, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 40, 725, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, 80, 725, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, 80, 800, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, 40, 800, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, 0, 800, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -40, 800, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -80, 800, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -120, 800, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -112, 950, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -80, 950, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -40, 950, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, 0, 950, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, 40, 950, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 40, 1125, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 0, 1125, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -40, 1125, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -80, 1125, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -96, 1125, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, -80, 1250, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, -40, 1250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, 0, 1250, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 56, -120, 725, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 96, -120, 725, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -120, 725, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, -80, 725, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 96, -80, 725, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 56, -80, 725, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 56, -40, 725, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 56, 0, 725, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, 40, 725, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 56, 80, 725, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 80, 725, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, 40, 725, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, 0, 725, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -40, 725, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -64, 725, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -64, 725, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -40, 725, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 0, 725, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 40, 725, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 80, 725, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
};

/// View 4's two contiguous sprite ranges, preceded by background selection.
///
/// Ranges cover all 47 sources and end at SPRITE_BATCH_END. Visibility remains
/// writable while the loaded view borrows this list.
static SpriteBatch _gDryfieldMotelRoom2View4SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 1, 0 } },
    { 27, ARRAY_SIZE(_gDryfieldMotelRoom2View4SpriteSources) - 27, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's 83 textured rectangles, selected in source elements by its batches.
///
/// XY and sizes use view-centred pixels; depth is an unscaled sorting value.
/// Packet initialization snapshots geometry and texture data while this overlay is loaded.
static SpriteSource _gDryfieldMotelRoom2View5SpriteSources[83] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -40, 875, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 487, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 487, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 487, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 487, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 487, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 487, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 80, 537, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 40, 537, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 0, 537, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, -40, 537, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -80, 537, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -120, 537, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, -96, -72, 587, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, -40, 587, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, 0, 587, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, 40, 587, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 80, 587, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, 40, 625, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -120, 850, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -72, -120, 850, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -72, -80, 850, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -64, 0, 887, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -64, -40, 862, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -8, -120, 830, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -8, -72, 830, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -8, -24, 830, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -8, 24, 830, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, -24, 843, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 24, 842, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, -72, 843, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, -120, 843, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, -64, -120, 850, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, 40, 56, 664, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, 40, 779, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, 40, 40, 609, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 72, 40, 331, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, 24, 652, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 40, 24, 699, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 96, 24, 323, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 72, 24, 323, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 16, 316, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -120, 362, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -80, 362, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 96, -120, 362, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, -120, 362, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, -80, 362, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 96, -80, 362, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -40, 362, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 96, -40, 362, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, -40, 362, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 72, -40, 362, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 120, 0, 362, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, 96, 0, 314, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, 0, 316, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 120, 40, 362, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 96, 40, 362, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, 40, 362, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 64, 40, 362, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 64, 0, 362, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, 56, 56, 362, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 120, 80, 375, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 96, 80, 375, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, 80, 375, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 80, 375, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 80, 375, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 48, 104, 362, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -112, 48, 625, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -128, 32, 625, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -120, 16, 650, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -88, 32, 637, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -88, 16, 650, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -120, 88, 675, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -96, 88, 675, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -120, 72, 675, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -96, 72, 675, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -40, 875, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -56, -120, 875, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, -120, 875, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -64, -80, 875, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -56, -80, 875, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -56, -40, 900, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -56, 0, 887, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
};

/// View 5's seven contiguous sprite ranges, preceded by background selection.
///
/// Ranges cover all 83 sources and end at SPRITE_BATCH_END. Visibility remains
/// writable while the loaded view borrows this list.
static SpriteBatch _gDryfieldMotelRoom2View5SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 3, 0 } },
    { 24, 9, 0, 0, { 4, 0 } },
    { 33, 9, 0, 0, { 1, 0 } },
    { 42, 25, 0, 0, { 5, 0 } },
    { 67, 5, 0, 0, { 0, 0 } },
    { 72, 4, 0, 0, { 6, 0 } },
    { 76, ARRAY_SIZE(_gDryfieldMotelRoom2View5SpriteSources) - 76, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's 179-by-239-pixel draw-buffer clip, with full-screen restore at depth 800.
///
/// Coordinates start at the buffer's upper-left corner. The sentinel ends the
/// list; clipping also affects scene objects drawn before the restore command.
static SpriteDrawArea _gDryfieldMotelRoom2View5SpriteDrawAreas[2] = {
    { { 0, 0, 179, 239 }, 800 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// View 6's 79 textured rectangles, selected in source elements by its batches.
///
/// XY and sizes use view-centred pixels; depth is an unscaled sorting value.
/// Packet initialization snapshots geometry and texture data while this overlay is loaded.
static SpriteSource _gDryfieldMotelRoom2View6SpriteSources[79] = {
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 96, 80, 907, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, -120, 941, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 40, 0, 937, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -40, 937, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -80, 937, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -120, 937, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -80, 829, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -40, 883, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 0, 916, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 8, 853, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 24, 786, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -16, 782, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -56, 752, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 64, -40, 834, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, -80, 816, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 72, -80, 757, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 80, 40, 637, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, 40, 639, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 40, 622, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 40, 589, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 0, 720, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, 0, 674, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 0, 651, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 0, 583, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -40, 707, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -40, 663, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -40, 625, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, -40, 575, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -80, 695, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -80, 653, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -80, 616, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, -80, 567, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -120, 664, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 96, -120, 610, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 120, -120, 534, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -120, 443, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -80, 514, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 136, -80, 457, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -40, 521, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -40, 462, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 0, 527, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 0, 471, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 40, 531, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 40, 476, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 80, 541, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 80, 482, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -120, 375, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -88, -120, 375, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -112, -120, 375, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -80, 375, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -88, -80, 375, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -80, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -40, 375, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -88, -40, 375, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -40, 375, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 0, 375, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -88, 0, 375, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, 0, 375, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, 0, 375, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -48, -32, 375, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -88, 40, 375, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -80, 80, 375, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, 40, 375, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -64, 80, 375, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -48, 40, 375, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -48, 80, 375, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 80, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -40, 40, 375, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 72, 72, 500, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 96, 72, 500, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 72, 48, 500, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 56, 500, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 32, 487, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 24, 500, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 104, 24, 500, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 40, 487, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 104, 40, 487, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 136, 40, 487, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 104, 56, 450, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
};

/// View 6's four contiguous sprite ranges, preceded by background selection.
///
/// Ranges cover all 79 sources and end at SPRITE_BATCH_END. Visibility remains
/// writable while the loaded view borrows this list.
static SpriteBatch _gDryfieldMotelRoom2View6SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 46, 0, 0, { 3, 0 } },
    { 46, 22, 0, 0, { 0, 0 } },
    { 68, 4, 0, 0, { 2, 0 } },
    { 72, ARRAY_SIZE(_gDryfieldMotelRoom2View6SpriteSources) - 72, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldMotelRoom2SpriteViews[6] = {
    { { .empty = _gDryfieldMotelRoom2View1SpriteBatches }, _gDryfieldMotelRoom2View1SpriteBatches, NULL },
    { { .elements = _gDryfieldMotelRoom2View2SpriteSources }, _gDryfieldMotelRoom2View2SpriteBatches, NULL },
    { { .elements = _gDryfieldMotelRoom2View3SpriteSources }, _gDryfieldMotelRoom2View3SpriteBatches, NULL },
    { { .elements = _gDryfieldMotelRoom2View4SpriteSources }, _gDryfieldMotelRoom2View4SpriteBatches, NULL },
    { { .elements = _gDryfieldMotelRoom2View5SpriteSources }, _gDryfieldMotelRoom2View5SpriteBatches, _gDryfieldMotelRoom2View5SpriteDrawAreas },
    { { .elements = _gDryfieldMotelRoom2View6SpriteSources }, _gDryfieldMotelRoom2View6SpriteBatches, NULL },
};

/// Directed view boundaries joining views 2/3, 3/4, 3/5 and 5/6 in both directions.
///
/// Coordinates and radii use room game units; normals use 4096 per unit.
/// Room setup binds, links and enables every record through the included LAST
/// record. The overlay owns the mutable links and hit latches until unlinked.
static WorldCollisionTrigger _gDryfieldMotelRoom2ViewBoundaryTriggers[8] = {
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 2590, -1168, 2061, 0 },
        .vertices     = { { 783, -2192, 2008, 0 }, { -789, -2192, -2014, 0 }, { 783, 2192, 2008, 0 }, { -789, 2192, -2014, 0 } },
        .normal       = { -3826, 0, 1494, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 3072,
        .control      = 0,
        .parameter0   = 3,
        .parameter1   = 2,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 2495, -1280, 2079, 0 },
        .vertices     = { { -791, -2192, -2016, 0 }, { 781, -2192, 2006, 0 }, { -791, 2192, -2016, 0 }, { 781, 2192, 2006, 0 } },
        .normal       = { 3824, 0, -1496, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 3072,
        .control      = 0,
        .parameter0   = 2,
        .parameter1   = 3,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 847, -1184, 3966, 0 },
        .vertices     = { { -838, -2192, 18, 0 }, { 839, -2192, -17, 0 }, { -838, 2192, 18, 0 }, { 839, 2192, -17, 0 } },
        .normal       = { -87, 0, -4102, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 2346,
        .control      = 0,
        .parameter0   = 3,
        .parameter1   = 4,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 895, -1152, 3887, 0 },
        .vertices     = { { 846, -2192, -36, 0 }, { -846, -2192, 36, 0 }, { 846, 2192, -36, 0 }, { -846, 2192, 36, 0 } },
        .normal       = { 174, 0, 4095, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 2346,
        .control      = 0,
        .parameter0   = 4,
        .parameter1   = 3,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 2464, -1184, 4000, 0 },
        .vertices     = { { -838, -2192, 18, 0 }, { 839, -2192, -17, 0 }, { -838, 2192, 18, 0 }, { 839, 2192, -17, 0 } },
        .normal       = { -87, 0, -4102, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 2346,
        .control      = 0,
        .parameter0   = 3,
        .parameter1   = 5,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 2431, -1120, 3871, 0 },
        .vertices     = { { 839, -2192, -17, 0 }, { -838, -2192, 18, 0 }, { 839, 2192, -17, 0 }, { -838, 2192, 18, 0 } },
        .normal       = { 84, 0, 4099, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 2346,
        .control      = 0,
        .parameter0   = 5,
        .parameter1   = 3,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 3568, -1312, 4975, 0 },
        .vertices     = { { -267, -2192, 1058, 0 }, { 267, -2192, -1057, 0 }, { -267, 2192, 1058, 0 }, { 267, 2192, -1057, 0 } },
        .normal       = { -3976, 0, -1005, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 2442,
        .control      = 0,
        .parameter0   = 5,
        .parameter1   = 6,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 3424, -1248, 4927, 0 },
        .vertices     = { { 267, -2192, -1057, 0 }, { -267, -2192, 1058, 0 }, { 267, 2192, -1057, 0 }, { -267, 2192, 1058, 0 } },
        .normal       = { 3973, 0, 1002, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 2442,
        .control      = 0,
        .parameter0   = 6,
        .parameter1   = 5,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST,
        .hit          = 0,
    },
};

/// Six interaction quads: the main-street warp and CAP commands 5, 8, 10, 11 and 1.
///
/// All require facing except command 11, which also accepts nearby activation.
/// Positions and radii use room game units; normals use 4096 per unit. Setup
/// binds, links and enables the included LAST record. Keep this mutable storage
/// loaded until its links and latched hits have been retired.
static WorldCollisionTrigger _gDryfieldMotelRoom2ActionTriggers[6] = {
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 4560, -80, 2608, 0 },
        .vertices     = { { -304, 0, -528, 0 }, { 304, 0, -528, 0 }, { -304, 0, 528, 0 }, { 304, 0, 528, 0 } },
        .normal       = { 0, ONE, 0, 0 },
        .facingNormal = { -ONE, 0, 0, 0 },
        .radius       = 607,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_DRYFIELD_MAIN_STREET,
        .parameter1   = DRYFIELD_MOTEL_ROOM_2_MAIN_STREET_WARP_ENDPOINTS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 400, -64, 3024, 0 },
        .vertices     = { { -336, 0, -480, 0 }, { 336, 0, -480, 0 }, { -336, 0, 480, 0 }, { 336, 0, 480, 0 } },
        .normal       = { 0, 4101, 0, 0 },
        .facingNormal = { ONE, 0, 0, 0 },
        .radius       = 583,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 5,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 2288, -64, 4864, 0 },
        .vertices     = { { -512, 0, -336, 0 }, { 512, 0, -336, 0 }, { -512, 0, 336, 0 }, { 512, 0, 336, 0 } },
        .normal       = { 0, 4100, 0, 0 },
        .facingNormal = { 0, 0, -ONE, 0 },
        .radius       = 610,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 8,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 4640, -64, 4976, 0 },
        .vertices     = { { -288, 0, -480, 0 }, { 288, 0, -480, 0 }, { -288, 0, 480, 0 }, { 288, 0, 480, 0 } },
        .normal       = { 0, 4100, 0, 0 },
        .facingNormal = { -ONE, 0, 0, 0 },
        .radius       = 557,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 10,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 3408, -64, 3872, 0 },
        .vertices     = { { -1088, 0, 80, 0 }, { 224, 0, 80, 0 }, { -736, 0, 912, 0 }, { 224, 0, 912, 0 } },
        .normal       = { 0, 4098, 0, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 1166,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 11,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 704, -64, 5216, 0 },
        .vertices     = { { -544, 0, -240, 0 }, { 544, 0, -240, 0 }, { -544, 0, 240, 0 }, { 544, 0, 240, 0 } },
        .normal       = { 0, 4095, 0, 0 },
        .facingNormal = { 201, 0, -4091, 0 },
        .radius       = 593,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 1,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST,
        .hit          = 0,
    },
};

/// Two room-space quads blocking actor line-of-sight queries.
///
/// Positions and radii use game units; normals use 4096 per unit. Room setup
/// links and enables both records, including the final LAST record. Links are
/// mutable and must be retired before unloading this room. The stored low flag
/// bit is retained; no meaning is established for it.
static WorldCollisionOccluder _gDryfieldMotelRoom2SightOccluders[2] = {
    {
        .next       = NULL,
        .prevLink   = NULL,
        .origin     = { 4000, -1280, 3904, 0 },
        .vertices   = { { -1024, -1952, 0, 0 }, { 1024, -1952, 0, 0 }, { -1024, 1952, 0, 0 }, { 1024, 1952, 0, 0 } },
        .normal     = { 0, 0, -4101, 0 },
        .radius     = 2202,
        .flags      = 1,
        .unknown_3B = 0,
    },
    {
        .next       = NULL,
        .prevLink   = NULL,
        .origin     = { 1600, -1312, 4896, 0 },
        .vertices   = { { 0, -1952, 1024, 0 }, { 0, -1952, -1024, 0 }, { 0, 1952, 1024, 0 }, { 0, 1952, -1024, 0 } },
        .normal     = { -4101, 0, 0, 0 },
        .radius     = 2202,
        .flags      = 1 | WORLD_COLLISION_OCCLUDER_LAST,
        .unknown_3B = 0,
    },
};

/// Motel room 2's five point lights, shared by every view.
///
/// Positions and falloff radii use integer world units; RGB intensities use
/// 12 fractional bits (ONE is full intensity). The loaded room overlay owns
/// this writable array: coordinate updates set its parent and composed matrices,
/// and lighting queries overwrite attenuation. Borrowed light pointers remain
/// valid only while the overlay is loaded.
static WorldCoordPointLight _gDryfieldMotelRoom2PointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2853, -1418, 419 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1800, 1800, 1800 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 544,
        .outer = 2890,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2471, -1350, 1397 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1800, 1800, 1800 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 866,
        .outer = 6189,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 663, -1418, 429 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2300, 2300, 2300 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 838,
        .outer = 3082,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4432, -1590, 5383 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3000, 3000, 3000 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1098,
        .outer = 3287,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4606, -1570, 908 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3100, 3100, 3100 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 841,
        .outer = 1490,
    },
};

/// Room 1's five point lights, with no directional or cone-light arrays.
///
/// The collection borrows this overlay's writable lights; composing their
/// transforms and sampling attenuation require the room overlay to remain loaded.
static WorldCoordRoomLights _gDryfieldMotelRoom2RoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldMotelRoom2PointLights),
    .pointLights           = _gDryfieldMotelRoom2PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Placement-entry keys used by this room's encounter resource tables.
enum {
    DRYFIELD_MOTEL_ROOM_2_ODD_STRANGER_ENTRY  = 18,
    DRYFIELD_MOTEL_ROOM_2_BONE_SUCKLER_ENTRY  = 12,
    DRYFIELD_MOTEL_ROOM_2_MIND_SUCKLER_ENTRY  = 15,
    DRYFIELD_MOTEL_ROOM_2_BLOOD_SUCKLER_ENTRY = 40,
};

/// Variant 1's Odd Stranger loading and spawn recipe for placement entry 18.
///
/// File group base 40 and number 18 select actor file 401800; descriptor 0
/// serves both placements. AREA_PLACEMENT_END terminates the borrowed list.
static AreaResource _gDryfieldMotelRoom2Variant1Resources[2] = {
    { .entryId = DRYFIELD_MOTEL_ROOM_2_ODD_STRANGER_ENTRY, .fileNumber = 18, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor401800OddStrangerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 2's Bone Suckler and Mind Suckler loading and spawn recipes.
///
/// Entries 12/15 select files 101200/201500 and descriptor 0 of each actor,
/// serving four Bone Suckler and two Mind Suckler placements. The sentinel
/// terminates the list; keep its descriptors loaded through spawning.
static AreaResource _gDryfieldMotelRoom2Variant2Resources[3] = {
    { .entryId = DRYFIELD_MOTEL_ROOM_2_BONE_SUCKLER_ENTRY, .fileNumber = 12, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor101200BoneSucklerTaskDesc },
    { .entryId = DRYFIELD_MOTEL_ROOM_2_MIND_SUCKLER_ENTRY, .fileNumber = 15, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor01500MindSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 7's Blood Suckler and Mind Suckler loading and spawn recipes.
///
/// Entries 40/15 select files 104000/201500 and descriptor 0 of each actor,
/// serving four Blood Suckler and two Mind Suckler placements. The sentinel
/// terminates the list; keep its descriptors loaded through spawning.
static AreaResource _gDryfieldMotelRoom2Variant7Resources[3] = {
    { .entryId = DRYFIELD_MOTEL_ROOM_2_BLOOD_SUCKLER_ENTRY, .fileNumber = 40, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor04000BloodSucklerTaskDesc },
    { .entryId = DRYFIELD_MOTEL_ROOM_2_MIND_SUCKLER_ENTRY, .fileNumber = 15, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor01500MindSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldMotelRoom2AreaVariants[12] = {
    [1] = { .placements = gMapDryfieldMotelRoom2Variant1Placements, .resources = _gDryfieldMotelRoom2Variant1Resources },
    [2] = { .placements = gMapDryfieldMotelRoom2Variant2Placements, .resources = _gDryfieldMotelRoom2Variant2Resources },
    [7] = { .placements = gMapDryfieldMotelRoom2Variant7Placements, .resources = _gDryfieldMotelRoom2Variant7Resources },
};

/// Cue-2 bases in the currently loaded type-1 sound bank; cue 1 uses base + 1.
enum {
    DRYFIELD_MOTEL_ROOM_2_CLASS_1_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_MOTEL_ROOM_2_CLASS_1_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_MOTEL_ROOM_2_CLASS_2_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x01,
    DRYFIELD_MOTEL_ROOM_2_CLASS_2_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x03,
};

/// Class 1's walking, running and scripted-jump footstep cue bases.
///
/// Walking and scripted jumps share entries 0x35/0x36; running uses 0x37/0x38
/// in the loaded type-1 bank. The paired animation cues select base/base + 1.
static WorldCollisionFootstepSounds _gDryfieldMotelRoom2Class1FootstepSounds = {
    .walk         = DRYFIELD_MOTEL_ROOM_2_CLASS_1_WALK_CUE_BASE,
    .run          = DRYFIELD_MOTEL_ROOM_2_CLASS_1_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_MOTEL_ROOM_2_CLASS_1_WALK_CUE_BASE,
};

/// Classes 2 and 4's shared walking, running and scripted-jump footstep cue bases.
///
/// Walking and scripted jumps share entries 1/2; running uses 3/4 in the
/// loaded type-1 bank. Class 4 is retained in the directory but absent from this mesh.
static WorldCollisionFootstepSounds _gDryfieldMotelRoom2Classes2And4FootstepSounds = {
    .walk         = DRYFIELD_MOTEL_ROOM_2_CLASS_2_WALK_CUE_BASE,
    .run          = DRYFIELD_MOTEL_ROOM_2_CLASS_2_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_MOTEL_ROOM_2_CLASS_2_WALK_CUE_BASE,
};

/// Shared properties for silent, probe-blocking surface classes 0, 5, 6 and 7.
///
/// Weapon impacts and pushback remain enabled. All four directory entries
/// borrow this single record from the loaded room overlay.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom2SolidSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1's blocking and impact-enabled surface with its own footstep cues.
///
/// Applies pushback and borrows the cue record for the room overlay's lifetime.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom2Class1SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMotelRoom2Class1FootstepSounds,
};

/// Class 2's blocking and impact-enabled surface with cues shared by class 4.
///
/// Applies pushback and borrows the cue record for the room overlay's lifetime.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom2Class2SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMotelRoom2Classes2And4FootstepSounds,
};

/// Class 3's silent surface, passing probes and suppressing weapon impacts.
///
/// Pushback remains enabled independently of probe and impact policy.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom2Class3SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 4's probe-passing, impact-enabled surface with class 2's footstep cues.
///
/// Pushback remains enabled. This retained directory entry is absent from the
/// current mesh; setup still reads its policy with the other seven entries.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom2Class4SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMotelRoom2Classes2And4FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldMotelRoom2SurfacePropertiesByClass[8] = {
    &_gDryfieldMotelRoom2SolidSurfaceProperties,
    &_gDryfieldMotelRoom2Class1SurfaceProperties,
    &_gDryfieldMotelRoom2Class2SurfaceProperties,
    &_gDryfieldMotelRoom2Class3SurfaceProperties,
    &_gDryfieldMotelRoom2Class4SurfaceProperties,
    &_gDryfieldMotelRoom2SolidSurfaceProperties,
    &_gDryfieldMotelRoom2SolidSurfaceProperties,
    &_gDryfieldMotelRoom2SolidSurfaceProperties,
};

static void _dryfieldMotelRoom2InitRoomTask(Task* task);

/// Refuses every key-item use request in Dryfield Motel Room 2.
///
/// Returns 0 so the inventory shows its "No use now" notice. The item ID and
/// the other arguments are ignored; no item is consumed or room event started.
static s32 _dryfieldMotelRoom2RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Allows a room transition with the requested destination unchanged.
///
/// Copies the complete eight-byte record and returns 1 for both query and
/// execution requests. `request` and `reply` must be live, non-null records
/// during synchronous dispatch; they may be the same object. The reply must
/// be writable. No payload is retained and no transition side effects occur.
static s32 _dryfieldMotelRoom2ResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_MOTEL_ROOM_2_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_MOTEL_ROOM_2_TRANSITION_ALLOWED;
}

/// Ignores room commands without starting an action.
///
/// Both command words are ignored and the message result is always 0.
static s32 _dryfieldMotelRoom2IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument)
{
    enum { DRYFIELD_MOTEL_ROOM_2_COMMAND_IGNORED = 0 };

    return DRYFIELD_MOTEL_ROOM_2_COMMAND_IGNORED;
}

/// Ignores room actions selected by direction triggers.
///
/// The borrowed request is neither read nor retained. The sender passes zero
/// as the second payload word and ignores the result, which is always 0.
static s32 _dryfieldMotelRoom2IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { DRYFIELD_MOTEL_ROOM_2_ACTION_IGNORED = 0 };

    return DRYFIELD_MOTEL_ROOM_2_ACTION_IGNORED;
}

/// Registers motel room 2's room-message receiver and advances to idle state 1.
///
/// Runs in room-task state 0. Borrows this overlay's message table and registers
/// the live task in `GAME_TASK_SLOT_ROOM`; keep the overlay loaded while it can
/// receive messages. Registration does not retain the task or clear on teardown.
static void _dryfieldMotelRoom2InitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldMotelRoom2RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the initialized room task idle between messages.
///
/// Leaves its state, message table and resources intact so synchronous room
/// messages remain available. Task teardown is handled by the separate kill state.
static void _dryfieldMotelRoom2IdleRoomTask(Task* unusedTask)
{
}

/// Room-task states 0 initialize the receiver, 1 idle, and 2 release the task.
///
/// The dispatcher copies all three callbacks by value before its unchecked
/// state lookup. Callback code must remain loaded through dispatch.
static const TaskFuncTable3 _gDryfieldMotelRoom2RoomStateHandlers = {
    { _dryfieldMotelRoom2InitRoomTask, _dryfieldMotelRoom2IdleRoomTask, taskKill },
};

void dryfieldMotelRoom2Task(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldMotelRoom2RoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

void dryfieldMotelRoom2EffectNoopTaskCA(Task* unusedTask)
{
}
