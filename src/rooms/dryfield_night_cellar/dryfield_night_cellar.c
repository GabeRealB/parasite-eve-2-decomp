#include "rooms/dryfield_night_cellar.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00700.h"
#include "actors/actor_01600.h"
#include "actors/actor_03700.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/cellar.h"

static s32  _dryfieldNightCellarRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32  _dryfieldNightCellarIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* action, s32 unused);
static void _dryfieldNightCellarIdle(Task* task);

static WorldCollisionGrid     _gDryfieldNightCellarCollisionGrid;
static WorldCollisionOccluder _gDryfieldNightCellarSightOccluder;
static WorldCollisionTrigger  _gDryfieldNightCellarViewBoundaryTriggers[6];
static WorldCollisionTrigger  _gDryfieldNightCellarRoom1ActionTriggers[10];
static WorldCollisionTrigger  _gDryfieldNightCellarRoom2ActionTriggers[10];
static WorldCoordRoomLights   _gDryfieldNightCellarRoom1Lights;
static WorldCoordRoomLights   _gDryfieldNightCellarRoom2Lights;

/// Room-task message interface, borrowed from this overlay until receiver teardown.
///
/// Transition resolution, refused key-item use, direction actions and cellar CAP commands.
/// The final `TASK_MESSAGE_TABLE_END` record terminates dispatch; it is not a handler.
static const TaskMessageEntry _gDryfieldNightCellarMessageTable[5] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantResolveCellar },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightCellarRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightCellarIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, _cellarHandleCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// View 2's flare centres in signed integer world-coordinate units.
///
/// Borrowed as a complete pair for drawing while the room overlay is loaded.
static SVECTOR _gDryfieldNightCellarView2GlowWorldPoints[2] = {
    { 2000, -2330, 2200, 0 },
    { 5000, -2330, 2200, 0 },
};

/// View 3's flare centres in signed integer world-coordinate units.
///
/// Borrowed as a complete pair for drawing while the room overlay is loaded.
static SVECTOR _gDryfieldNightCellarView3GlowWorldPoints[2] = {
    { 8300, -2330, 3190, 0 },
    { 8300, -2330, 200, 0 },
};

WorldCoordRoomLighting gDryfieldNightCellarRoomLighting[2] = {
    { .lights = &_gDryfieldNightCellarRoom1Lights, .ambientTable = NULL },
    { .lights = &_gDryfieldNightCellarRoom2Lights, .ambientTable = NULL },
};

WorldCollisionRoomResources gDryfieldNightCellarRoomCollisionResources[2] = {
    {
        .grid                 = &_gDryfieldNightCellarCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightCellarViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightCellarRoom1ActionTriggers,
        .occluders            = &_gDryfieldNightCellarSightOccluder,
    },
    {
        .grid                 = &_gDryfieldNightCellarCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightCellarViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightCellarRoom2ActionTriggers,
        .occluders            = &_gDryfieldNightCellarSightOccluder,
    },
};

/// Maps room 2's six logical views to the shared camera, image and sprite indices.
///
/// Only entries 0..5 are active: {1, 7, 8, 9, 10, 11}. The final two zero
/// bytes are retained storage, not a terminator or selectable views. Borrowed
/// through `gDryfieldNightCellarViewMaps` while this overlay is loaded.
static u8 _gDryfieldNightCellarRoom2ViewMap[8] = {
    1,
    7,
    8,
    9,
    10,
    11,
    0,
    0,
};

u8* gDryfieldNightCellarViewMaps[2] = {
    gViewIdentityMap,
    _gDryfieldNightCellarRoom2ViewMap,
};

// Logical entries searched in each room map, independent of its storage capacity.
enum {
    DRYFIELD_NIGHT_CELLAR_ROOM_1_LOGICAL_VIEW_COUNT = 11,
    DRYFIELD_NIGHT_CELLAR_ROOM_2_LOGICAL_VIEW_COUNT = 6,
};

ViewCount gDryfieldNightCellarViewCounts[2] = { DRYFIELD_NIGHT_CELLAR_ROOM_1_LOGICAL_VIEW_COUNT, DRYFIELD_NIGHT_CELLAR_ROOM_2_LOGICAL_VIEW_COUNT };

// Optional door map-marker nibble IDs stored by these endpoints.
enum {
    DRYFIELD_NIGHT_CELLAR_UNDERPASS_MAP_FLAG = 463,
};

DirectionWarpEntry gDryfieldNightCellarWarpEntries[1] = {
    {
        .player         = { .yaw = { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, .x = 5611, .y = 3, .z = 3945 },
        .unknown_10     = { 0 },
        .companion      = { .yaw = { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, .x = 5611, .y = 3, .z = 3945 },
        .unknown_24     = { 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_CELLAR, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_CELLAR, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_CELLAR_UNDERPASS_MAP_FLAG,
    },
};

/// Room-axis collision normals, with 4096 representing unit length.
static SVECTOR _gDryfieldNightCellarCollisionGridNormals[7] = {
#include "assets/dryfield_night_cellar_collision_008A0_normals.inc"
};

/// Collision vertices in signed integer room-coordinate units.
static SVECTOR _gDryfieldNightCellarCollisionGridVerts[39] = {
#include "assets/dryfield_night_cellar_collision_008A0_verts.inc"
};

/// Indexed collision quads using room-local surface classes 0 and 1.
static WorldCollisionGridFace _gDryfieldNightCellarCollisionGridFaces[20] = {
#include "assets/dryfield_night_cellar_collision_008A0_faces.inc"
};

/// Six face-index lists, each ending with `WORLD_COLLISION_GRID_CELL_END`.
///
/// Storage between lists is retained from the asset and is outside traversal.
static s16 _gDryfieldNightCellarCollisionGridCells[62] = {
#include "assets/dryfield_night_cellar_collision_008A0_cells.inc"
};

enum {
    DRYFIELD_NIGHT_CELLAR_COLLISION_GRID_CELLS_X   = 3,
    DRYFIELD_NIGHT_CELLAR_COLLISION_GRID_CELLS_Z   = 2,
    DRYFIELD_NIGHT_CELLAR_COLLISION_GRID_CELL_SIZE = 4000
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldNightCellarCollisionGridCells` (62 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_NIGHT_CELLAR_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightCellarCollisionGridCells[(halfwordOffset)])

/// X-major cell directory for the cellar's 3 by 2 collision grid.
static s16* _gDryfieldNightCellarCollisionGridTable[DRYFIELD_NIGHT_CELLAR_COLLISION_GRID_CELLS_X * DRYFIELD_NIGHT_CELLAR_COLLISION_GRID_CELLS_Z] = {
#include "assets/dryfield_night_cellar_collision_008A0_table.inc"
};
#undef DRYFIELD_NIGHT_CELLAR_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh shared by both nighttime cellar rooms.
///
/// The XZ grid starts at (0, 0), with 4000-unit square cells. Its 20 faces
/// reference 39 vertices and seven normals; each cell list ends with -1.
/// Room activation binds the view transform. The descriptor and pools remain
/// writable and must stay loaded while borrowed by collision queries.
static WorldCollisionGrid _gDryfieldNightCellarCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightCellarCollisionGridNormals,
    .vertices    = _gDryfieldNightCellarCollisionGridVerts,
    .faces       = _gDryfieldNightCellarCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightCellarCollisionGridTable,
    .xBias       = 0,
    .zBias       = 0,
    .cellCountX  = DRYFIELD_NIGHT_CELLAR_COLLISION_GRID_CELLS_X,
    .cellCountZ  = DRYFIELD_NIGHT_CELLAR_COLLISION_GRID_CELLS_Z,
    .cellSize    = DRYFIELD_NIGHT_CELLAR_COLLISION_GRID_CELL_SIZE,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightCellarCollisionGridFaces),
};

ViewCamera gDryfieldNightCellarViewCameras[11] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -5416, 0x3922, -1666 } }, 316 },
    { { { { 1292, 0, 3886 }, { 693, 4030, -230 }, { -3824, 730, 1272 } }, { -9942, 1542, -572 } }, 263 },
    { { { { 1498, 0, -3812 }, { -671, 4031, -264 }, { 3752, 721, 1474 } }, { -2892, 1542, -572 } }, 263 },
    { { { { -3904, 0, -1236 }, { -120, 4076, 379 }, { 1231, 397, -3886 } }, { -1302, 1102, -4422 } }, 263 },
    { { { { 4075, 0, -407 }, { -188, 3631, -1885 }, { 361, 1894, 3613 } }, { -1545, 2214, -754 } }, 207 },
    { { { { 1498, 0, -3812 }, { -671, 4031, -264 }, { 3752, 721, 1474 } }, { -9257, 1612, -1963 } }, 263 },
    { { { { 1292, 0, 3886 }, { 693, 4030, -230 }, { -3824, 730, 1272 } }, { -9942, 1542, -572 } }, 263 },
    { { { { 1498, 0, -3812 }, { -671, 4031, -264 }, { 3752, 721, 1474 } }, { -2892, 1542, -572 } }, 263 },
    { { { { -3904, 0, -1236 }, { -120, 4076, 379 }, { 1231, 397, -3886 } }, { -1302, 1102, -4422 } }, 263 },
    { { { { 4075, 0, -407 }, { -188, 3631, -1885 }, { 361, 1894, 3613 } }, { -1545, 2214, -754 } }, 207 },
    { { { { 1498, 0, -3812 }, { -671, 4031, -264 }, { 3752, 721, 1474 } }, { -9257, 1612, -1963 } }, 263 },
};

/// Empty sprite list for mapped view 1, retaining the decoded background image.
///
/// The zero-count first record is skipped before `SPRITE_BATCH_END` is tested.
/// The source slot retains this list's address but is never read as sprites.
static SpriteBatch _gDryfieldNightCellarView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Encoded 8-bit sprite pages at VRAM (896, 0)/(960, 0) and palette at (0, 255).
///
/// Texture UVs remain local to the selected page; opaque sprites do not use
/// the pages' average semitransparency mode.
enum {
    DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0 = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1 = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT           = getClut(0, 255)
};

/// Foreground texture rectangles and sorting depths for mapped view 2.
///
/// Contains 58 mutable sources, in pixels relative to the view centre with
/// texture origins in texels and unscaled OT depths. Borrowed by this view's
/// batches only while the room overlay is loaded.
static SpriteSource _gDryfieldNightCellarView2SpriteSources[58] = {
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 56, 500, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, -24, 0, 900, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 8, 0, 900, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 40, 0, 900, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 16 } }, 72, 0, 900, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, -24, 16, 750, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 8, 16, 750, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 40, 16, 750, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 16 } }, 72, 16, 750, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 16 } }, 112, 16, 750, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, -24, 32, 950, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 24 } }, 8, 32, 625, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 24 } }, 40, 32, 625, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 24 } }, 72, 32, 625, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 24 } }, 112, 32, 625, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 24 } }, 40, 56, 500, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 24 } }, 72, 56, 469, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 16 } }, 112, 56, 500, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 40 } }, 56, 80, 500, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 16 } }, 72, 80, 500, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 32 } }, 8, -16, 850, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 40 } }, 8, 16, 850, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 16 } }, 48, 16, 850, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 72, 812, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 88, -16, 875, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 16 } }, 80, 0, 875, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 16 } }, 56, 16, 875, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, 56, 32, 875, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 80, 16, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 32 } }, 80, 32, 875, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 32 } }, 112, 32, 750, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, 144, 32, 800, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 16 } }, 152, 16, 800, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 32 } }, 112, 64, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, 144, 64, 800, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, -40, -8, 875, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 16 } }, -32, 16, 875, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 16 } }, -24, 16, 600, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 8 } }, 0, 24, 625, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 16 } }, -24, 32, 750, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 16 } }, 0, 32, 625, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, -24, 48, 750, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 32 } }, -32, 48, 875, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, 0, 48, 625, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 32 } }, 24, 48, 625, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, 56, 56, 500, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 40 } }, 24, 80, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 24 } }, 56, 96, 500, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 40 } }, 0, 80, 625, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 32 } }, -8, 80, 750, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 32 } }, -72, -56, 1750, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 32 } }, -32, -56, 1750, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 32 } }, 8, -56, 1750, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, 40, -56, 1750, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 40 } }, -72, -24, 1750, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 40 } }, -32, -24, 1750, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 40 } }, 8, -24, 1750, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, 40, -24, 1750, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
};

/// Five foreground ranges covering all 58 sources for mapped view 2.
///
/// The first zero-count record retains the background; the final record ends
/// traversal. Visibility and cached-packet exclusion remain mutable.
static SpriteBatch _gDryfieldNightCellarView2SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { 20, 3, 0, 0, { 3, 0 } },
    { 23, 12, 0, 0, { 2, 0 } },
    { 35, 15, 0, 0, { 4, 0 } },
    { 50, ARRAY_SIZE(_gDryfieldNightCellarView2SpriteSources) - 50, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles and sorting depths for mapped view 3.
///
/// Contains 49 mutable sources. Positions and sizes use view-centred pixels,
/// texture origins use texels, and depths are unscaled OT values. Storage
/// remains borrowed only while the room overlay is loaded.
static SpriteSource _gDryfieldNightCellarView3SpriteSources[49] = {
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, -160, -64, 350, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, -160, -32, 350, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, -160, 0, 350, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, -160, 32, 350, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 40 } }, -160, 64, 350, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, -136, -24, 375, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -136, 0, 375, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -136, 32, 375, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 16 } }, -136, 64, 375, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, -24, -16, 1250, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 48 } }, 8, 24, 1000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -72, 24, 1000, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, -80, 8, 1000, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, -48, 8, 1000, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 16 } }, -16, 8, 1000, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 24, 8, 1000, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 8 } }, 40, 24, 1250, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 8 } }, -72, 0, 1125, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, -48, -8, 1125, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 16 } }, -16, -8, 1125, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 24, -8, 1125, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 8 } }, -16, -16, 1250, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 8 } }, 24, -16, 1250, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, -8, 24, 1000, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 24 } }, -64, -8, 1000, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 16 } }, -40, 0, 1000, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -56, 16, 1000, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 32 } }, -40, 16, 1000, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 16 } }, -56, 48, 1000, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 24 } }, -40, 48, 1000, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, 56, 24, 1125, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, 56, 32, 1125, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, 8, 32, 850, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, 24, 32, 925, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, 40, 32, 1000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 16 } }, 8, 16, 850, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, 24, 0, 925, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 40 } }, 40, -8, 1000, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, 56, -16, 1125, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, -72, 40, 1000, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -80, -16, 1125, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -64, -16, 1125, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 40 } }, -48, -24, 1250, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 40 } }, -32, -24, 1375, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 16 } }, -16, 0, 1500, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -48, 16, 1250, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -64, 16, 1125, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, -32, 16, 1375, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 16 } }, -16, 16, 1500, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

/// Five foreground ranges covering all 49 sources for mapped view 3.
///
/// A zero-count background record precedes the ranges; `SPRITE_BATCH_END`
/// follows them. Visibility and cached-packet exclusion remain mutable.
static SpriteBatch _gDryfieldNightCellarView3SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { 9, 14, 0, 0, { 3, 0 } },
    { 23, 7, 0, 0, { 2, 0 } },
    { 30, 9, 0, 0, { 4, 0 } },
    { 39, ARRAY_SIZE(_gDryfieldNightCellarView3SpriteSources) - 39, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles and sorting depths for mapped view 4.
///
/// Contains 33 mutable sources in view-centred pixels, with texture origins
/// in texels and unscaled OT depths. The room overlay owns their lifetime.
static SpriteSource _gDryfieldNightCellarView4SpriteSources[33] = {
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 40 } }, -160, 80, 178, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 40 } }, -120, 80, 178, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 32 } }, -80, 88, 178, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 32 } }, -40, 88, 178, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 24 } }, 0, 96, 178, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 24 } }, 40, 96, 178, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 72, 104, 178, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, -160, 56, 178, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, -160, 32, 178, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, -112, -104, 500, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, -72, -96, 600, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, 72, 450, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, 24, 450, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -24, 450, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -72, 450, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -120, 450, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 48 } }, -112, 72, 525, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 48 } }, -112, 24, 525, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 48 } }, -112, -24, 525, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 48 } }, -112, -72, 525, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 24 } }, -112, -96, 525, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 24 } }, -72, 72, 650, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 48 } }, -72, 24, 650, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 48 } }, -72, -24, 650, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 48 } }, -72, -72, 650, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, -72, -88, 650, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 48 } }, -40, 24, 750, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 48 } }, -40, -24, 750, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 48 } }, -40, -72, 750, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 40 } }, -24, 24, 800, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 48 } }, -24, -24, 800, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 48 } }, -24, -72, 800, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 8 } }, -40, -80, 750, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
};

/// Two foreground ranges covering all 33 sources for mapped view 4.
///
/// The leading zero-count record retains the background; the sentinel ends
/// traversal. Both ranges initially participate in cached packet drawing.
static SpriteBatch _gDryfieldNightCellarView4SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, ARRAY_SIZE(_gDryfieldNightCellarView4SpriteSources) - 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles and sorting depths for mapped view 5.
///
/// Contains 21 mutable sources in view-centred pixels, with texture origins
/// in texels and unscaled OT depths. Retains storage separate from view 10
/// even where the authored values agree; pointers expire with the overlay.
static SpriteSource _gDryfieldNightCellarView5SpriteSources[21] = {
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, -72, 512, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, 40, 32, 875, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 88 } }, 128, 32, 512, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 96 } }, 128, -64, 512, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 96 } }, 104, -64, 525, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 88 } }, 104, 32, 525, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 88 } }, 88, 32, 550, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 96 } }, 88, -64, 550, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 88 } }, 72, 32, 575, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 96 } }, 72, -64, 575, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, -80, 575, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, -80, 550, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 8 } }, 104, -72, 525, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 112 } }, 64, -80, 625, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 64 } }, 64, 32, 625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 112 } }, 56, -80, 675, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 112 } }, 48, -80, 775, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 40 } }, 56, 32, 675, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 24 } }, 48, 32, 775, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 112 } }, 40, -80, 875, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 88 } }, 32, -64, 950, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
};

/// One foreground range covering all 21 sources for mapped view 5.
///
/// The zero-count first record retains the background, followed by the range
/// and `SPRITE_BATCH_END`. The range's visibility remains mutable.
static SpriteBatch _gDryfieldNightCellarView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightCellarView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 6, retaining the decoded background image.
///
/// The zero-count first record is skipped before `SPRITE_BATCH_END` is tested.
/// The source slot retains this list's address but is never read as sprites.
static SpriteBatch _gDryfieldNightCellarView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles and sorting depths for mapped view 7.
///
/// Room 2's logical view 2 selects these 58 mutable sources. Positions and
/// sizes use view-centred pixels, texture origins use texels, and depths are
/// unscaled OT values. Storage remains live only with the room overlay.
static SpriteSource _gDryfieldNightCellarView7SpriteSources[58] = {
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 56, 500, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 16 } }, 112, 16, 750, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 24 } }, 112, 32, 625, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 16 } }, 112, 56, 500, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 16 } }, 72, 0, 900, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 16 } }, 72, 16, 750, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 24 } }, 72, 32, 625, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 24 } }, 72, 56, 469, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 40, 0, 900, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 40, 16, 750, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 24 } }, 40, 32, 625, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 24 } }, 40, 56, 500, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 40 } }, 56, 80, 500, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 16 } }, 72, 80, 500, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 8, 0, 900, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 8, 16, 750, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 24 } }, 8, 32, 625, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, -24, 0, 900, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, -24, 16, 750, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, -24, 32, 950, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 32 } }, 8, -16, 850, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 40 } }, 8, 16, 850, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 16 } }, 48, 16, 850, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 72, 834, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, 144, 64, 800, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, 144, 32, 800, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 16 } }, 152, 16, 800, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 32 } }, 112, 64, 750, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 32 } }, 112, 32, 750, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 32 } }, 80, 32, 875, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 80, 16, 875, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 16 } }, 80, 0, 875, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 88, -16, 875, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 16 } }, 56, 16, 728, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, 56, 32, 875, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 24 } }, 56, 96, 500, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, 56, 56, 500, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 40 } }, 24, 80, 625, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 32 } }, 24, 48, 625, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 40 } }, 0, 80, 625, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, 0, 48, 625, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 16 } }, 0, 32, 625, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 8 } }, 0, 24, 625, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 32 } }, -8, 80, 0x9E59, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, -24, 48, 750, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 32 } }, -32, 48, 875, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 16 } }, -24, 32, 750, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 16 } }, -24, 16, 600, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 16 } }, -32, 16, 875, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, -40, -8, 875, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, 40, -56, 1750, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, 40, -24, 1750, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 32 } }, 8, -56, 1750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 40 } }, 8, -24, 1750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 32 } }, -32, -56, 1750, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 40 } }, -32, -24, 1750, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 32 } }, -72, -56, 1750, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 40 } }, -72, -24, 1750, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
};

/// Five foreground ranges covering all 58 sources for mapped view 7.
///
/// Room 2's logical view 2 selects this list. A zero-count background record
/// precedes the mutable ranges; `SPRITE_BATCH_END` follows them.
static SpriteBatch _gDryfieldNightCellarView7SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { 20, 3, 0, 0, { 3, 0 } },
    { 23, 12, 0, 0, { 2, 0 } },
    { 35, 15, 0, 0, { 4, 0 } },
    { 50, ARRAY_SIZE(_gDryfieldNightCellarView7SpriteSources) - 50, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles and sorting depths for mapped view 8.
///
/// Room 2's logical view 3 selects these 49 mutable sources, in view-centred
/// pixels with texel origins and unscaled OT depths. The room overlay owns
/// their storage and lifetime.
static SpriteSource _gDryfieldNightCellarView8SpriteSources[49] = {
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, -136, -24, 212, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -136, 0, 375, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -136, 32, 375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 16 } }, -136, 64, 375, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, -160, -64, 350, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, -160, -32, 350, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, -160, 0, 350, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 32 } }, -160, 32, 350, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 40 } }, -160, 64, 350, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, -24, -16, 1250, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 8 } }, 40, 24, 1250, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 24, 8, 1000, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, 24, -8, 1125, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 8 } }, 24, -16, 1250, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 8 } }, -16, -16, 1250, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 16 } }, -16, -8, 1125, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 16 } }, -16, 8, 1000, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, -48, -8, 1125, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, -48, 8, 1000, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 8 } }, -72, 0, 1000, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, -80, 8, 1000, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 48 } }, 8, 24, 1000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -72, 24, 1000, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, -8, 24, 987, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 16 } }, -40, 0, 1000, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 32 } }, -40, 16, 1000, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 24 } }, -40, 48, 1000, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 24 } }, -64, -8, 1000, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -56, 16, 1000, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 16 } }, -56, 48, 1000, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, 56, 24, 1125, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, 56, 32, 1125, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 16 } }, 8, 16, 850, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, 8, 32, 850, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, 24, 0, 925, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, 24, 32, 925, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, 40, 32, 1000, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 40 } }, 40, -8, 1000, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, 56, -16, 1125, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, -72, 40, 1000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 16 } }, -16, 16, 1500, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 16 } }, -16, 0, 1500, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 40 } }, -32, -24, 1375, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 24 } }, -32, 16, 1375, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 40 } }, -48, -24, 1250, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -48, 16, 1250, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -64, -16, 1125, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -64, 16, 1125, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -80, -16, 1125, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
};

/// Five foreground ranges covering all 49 sources for mapped view 8.
///
/// Room 2's logical view 3 selects this list. A zero-count background record
/// precedes the mutable ranges; `SPRITE_BATCH_END` follows them.
static SpriteBatch _gDryfieldNightCellarView8SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { 9, 14, 0, 0, { 3, 0 } },
    { 23, 7, 0, 0, { 2, 0 } },
    { 30, 9, 0, 0, { 4, 0 } },
    { 39, ARRAY_SIZE(_gDryfieldNightCellarView8SpriteSources) - 39, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles and sorting depths for mapped view 9.
///
/// Room 2's logical view 4 selects these 33 mutable sources, in view-centred
/// pixels with texel origins and unscaled OT depths. The room overlay owns
/// their storage and lifetime.
static SpriteSource _gDryfieldNightCellarView9SpriteSources[33] = {
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, -112, 80, 178, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 32 } }, -160, 88, 178, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 32 } }, -112, 88, 178, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 32 } }, -64, 88, 178, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 24 } }, -16, 96, 178, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 24 } }, 32, 96, 178, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 16 } }, 80, 104, 178, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 24 } }, -160, 64, 178, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 32 } }, -160, 32, 178, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, -112, -104, 500, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, -72, -96, 600, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, 72, 450, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, 24, 450, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -24, 450, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -72, 450, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -120, 450, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 48 } }, -112, 72, 525, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 48 } }, -112, 24, 525, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 48 } }, -112, -24, 525, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 48 } }, -112, -72, 525, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 40, 24 } }, -112, -96, 525, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_0, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 24 } }, -72, 72, 650, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 48 } }, -72, 24, 650, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 48 } }, -72, -24, 650, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 48 } }, -72, -72, 650, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 16 } }, -72, -88, 650, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 48 } }, -40, 24, 750, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 48 } }, -40, -24, 750, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 48 } }, -40, -72, 750, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 40 } }, -24, 24, 800, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 48 } }, -24, -24, 800, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 48 } }, -24, -72, 800, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 8 } }, -40, -80, 750, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
};

/// Two foreground ranges covering all 33 sources for mapped view 9.
///
/// Room 2's logical view 4 selects this list. A zero-count background record
/// precedes the mutable ranges; `SPRITE_BATCH_END` follows them.
static SpriteBatch _gDryfieldNightCellarView9SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, ARRAY_SIZE(_gDryfieldNightCellarView9SpriteSources) - 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles and sorting depths for mapped view 10.
///
/// Room 2's logical view 5 selects these 21 mutable sources, in view-centred
/// pixels with texel origins and unscaled OT depths. Storage is separate from
/// view 5's sources and stays live only while the room overlay is loaded.
static SpriteSource _gDryfieldNightCellarView10SpriteSources[21] = {
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, -72, 512, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 8 } }, 40, 32, 875, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 88 } }, 128, 32, 512, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 32, 96 } }, 128, -64, 512, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 96 } }, 104, -64, 525, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 8 } }, 104, -72, 525, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 88 } }, 88, 32, 550, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 96 } }, 88, -64, 550, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, -80, 550, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 88 } }, 72, 32, 575, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 96 } }, 72, -64, 575, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, -80, 575, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 64 } }, 64, 32, 625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 112 } }, 64, -80, 625, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 40 } }, 56, 32, 675, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 112 } }, 56, -80, 675, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 24 } }, 48, 32, 775, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 112 } }, 48, -80, 775, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 112 } }, 40, -80, 875, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 8, 88 } }, 32, -64, 950, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { DRYFIELD_NIGHT_CELLAR_SPRITE_TEXTURE_PAGE_1, DRYFIELD_NIGHT_CELLAR_SPRITE_CLUT, { .fields = { 24, 88 } }, 104, 32, 525, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
};

/// One foreground range covering all 21 sources for mapped view 10.
///
/// Room 2's logical view 5 selects this list. A zero-count background record
/// precedes the mutable range; `SPRITE_BATCH_END` follows it.
static SpriteBatch _gDryfieldNightCellarView10SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightCellarView10SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 11's background-only sprite list.
///
/// A zero-count selector precedes `SPRITE_BATCH_END`, retaining decoded image
/// strips. The same address occupies `sources.empty` and supplies no sprites.
/// Keep this distinct authored storage live while the room overlay is loaded.
static SpriteBatch _gDryfieldNightCellarView11SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

SpriteView gDryfieldNightCellarSpriteViews[11] = {
    { { .empty = _gDryfieldNightCellarView1SpriteBatches }, _gDryfieldNightCellarView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightCellarView2SpriteSources }, _gDryfieldNightCellarView2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightCellarView3SpriteSources }, _gDryfieldNightCellarView3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightCellarView4SpriteSources }, _gDryfieldNightCellarView4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightCellarView5SpriteSources }, _gDryfieldNightCellarView5SpriteBatches, NULL },
    { { .empty = _gDryfieldNightCellarView6SpriteBatches }, _gDryfieldNightCellarView6SpriteBatches, NULL },
    { { .elements = _gDryfieldNightCellarView7SpriteSources }, _gDryfieldNightCellarView7SpriteBatches, NULL },
    { { .elements = _gDryfieldNightCellarView8SpriteSources }, _gDryfieldNightCellarView8SpriteBatches, NULL },
    { { .elements = _gDryfieldNightCellarView9SpriteSources }, _gDryfieldNightCellarView9SpriteBatches, NULL },
    { { .elements = _gDryfieldNightCellarView10SpriteSources }, _gDryfieldNightCellarView10SpriteBatches, NULL },
    { { .empty = _gDryfieldNightCellarView11SpriteBatches }, _gDryfieldNightCellarView11SpriteBatches, NULL },
};

/// 6 directed camera boundaries in room-local game units.
///
/// Source/destination views are 1-based: 2->3, 3->2, 4->5, 5->4, 4->2, 2->4.
/// Room binding installs the view coordinate and enables all records through LAST.
/// Normals use 4096 per unit; keep the writable list live until it is unlinked.
static WorldCollisionTrigger _gDryfieldNightCellarViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { 6576, -1472, 2096, 0 }, { { -318, -1904, 2593, 0 }, { 311, -1904, -2603, 0 }, { -318, 1904, 2593, 0 }, { 311, 1904, -2603, 0 } }, { -4074, 0, -494, 0 }, { 0, 0, ONE, 0 }, 3228, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6463, -1440, 2080, 0 }, { { 312, -1904, -2602, 0 }, { -317, -1904, 2594, 0 }, { 312, 1904, -2602, 0 }, { -317, 1904, 2594, 0 } }, { 4072, 0, 492, 0 }, { 0, 0, ONE, 0 }, 3228, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1727, -1056, 2096, 0 }, { { -1261, -1904, 95, 0 }, { 1256, -1904, -100, 0 }, { -1261, 1904, 95, 0 }, { 1256, 1904, -100, 0 } }, { -318, 0, -4087, 0 }, { 0, 0, ONE, 0 }, 2275, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1663, -1376, 2047, 0 }, { { 1253, -1904, -102, 0 }, { -1264, -1904, 94, 0 }, { 1253, 1904, -102, 0 }, { -1264, 1904, 94, 0 } }, { 317, 0, 4084, 0 }, { 0, 0, ONE, 0 }, 2275, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2878, -1344, 670, 0 }, { { -87, -1904, 1261, 0 }, { 87, -1904, -1260, 0 }, { -87, 1904, 1261, 0 }, { 87, 1904, -1260, 0 } }, { -4092, 0, -283, 0 }, { 0, 0, ONE, 0 }, 2275, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2718, -1312, 670, 0 }, { { 86, -1904, -1262, 0 }, { -88, -1904, 1259, 0 }, { 86, 1904, -1262, 0 }, { -88, 1904, 1259, 0 } }, { 4089, 0, 281, 0 }, { 0, 0, ONE, 0 }, 2275, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// IDs stored in action records; warp pairs pack departure then arrival in four-bit nibbles.
enum {
    DRYFIELD_NIGHT_CELLAR_TRIGGER_WARP_TO_UNDERPASS    = (1 << 4) | 1,
    DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_12       = 12,
    DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_3        = 3,
    DRYFIELD_NIGHT_CELLAR_TRIGGER_COMMAND_P08_DIALOGUE = 13,
    DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_5        = 5,
    DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_7        = 7,
    DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_6        = 6,
    DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_11       = 11,
    DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_10       = 10,
    DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_9        = 9,
    DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_8        = 8,
};

/// 10 interaction quads for cellar room 1: 1 warp, 8 CAP commands, 1 room command.
///
/// Room-local origins/corners and radii use game units; normals use 4096 per unit.
/// Warp bytes select destination area and departure/arrival endpoint nibbles;
/// CAP IDs index the loaded command table or route through the room-message sentinel.
/// Room binding enables all records through LAST; keep them live until unlinking.
static WorldCollisionTrigger _gDryfieldNightCellarRoom1ActionTriggers[10] = {
    { NULL, NULL, NULL, { 5056, -48, 4080, 0 }, { { -864, 0, -432, 0 }, { 864, 0, -432, 0 }, { -864, 0, 432, 0 }, { 864, 0, 432, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -ONE, 0 }, 964, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, DRYFIELD_NIGHT_CELLAR_TRIGGER_WARP_TO_UNDERPASS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1728, -64, 3680, 0 }, { { -864, 0, -432, 0 }, { 864, 0, -432, 0 }, { -864, 0, 432, 0 }, { 864, 0, 432, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -ONE, 0 }, 964, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7168, -64, 2368, 0 }, { { -1888, 0, -1520, 0 }, { 1888, 0, -1520, 0 }, { -1888, 0, 1520, 0 }, { 1888, 0, 1520, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, -ONE, 0 }, 2415, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x27B0, -64, 2224, 0 }, { { -400, 0, -960, 0 }, { 400, 0, -960, 0 }, { -400, 0, 960, 0 }, { 400, 0, 960, 0 } }, { 0, 4100, 0, 0 }, { -ONE, 0, 0, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_COMMAND_P08_DIALOGUE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9696, -64, 656, 0 }, { { -400, 0, -688, 0 }, { 400, 0, -688, 0 }, { -400, 0, 688, 0 }, { 400, 0, 688, 0 } }, { 0, 4099, 0, 0 }, { -ONE, 0, 0, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1216, -64, 1632, 0 }, { { -400, 0, -1552, 0 }, { 400, 0, -1552, 0 }, { -400, 0, 1552, 0 }, { 400, 0, 1552, 0 } }, { 0, 4110, 0, 0 }, { ONE, 0, 0, 0 }, 1598, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3840, -64, 2720, 0 }, { { -400, 0, -1552, 0 }, { 400, 0, -1552, 0 }, { -400, 0, 1552, 0 }, { 400, 0, 1552, 0 } }, { 0, 4110, 0, 0 }, { ONE, 0, 0, 0 }, 1598, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2048, -64, 2112, 0 }, { { -400, 0, -400, 0 }, { 400, 0, -400, 0 }, { -400, 0, 400, 0 }, { 400, 0, 400, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 565, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2112, -64, 2096, 0 }, { { -400, 0, -1120, 0 }, { 400, 0, -1120, 0 }, { -400, 0, 1120, 0 }, { 400, 0, 1120, 0 } }, { 0, 4100, 0, 0 }, { -ONE, 0, 0, 0 }, 1187, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7120, -64, 192, 0 }, { { -1904, 0, -432, 0 }, { 1904, 0, -432, 0 }, { -1904, 0, 432, 0 }, { 1904, 0, 432, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, ONE, 0 }, 1949, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// 10 interaction quads for cellar room 2: 1 warp, 9 CAP commands.
///
/// Room-local origins/corners and radii use game units; normals use 4096 per unit.
/// Warp bytes select destination area and departure/arrival endpoint nibbles;
/// CAP IDs index the loaded command table or route through the room-message sentinel.
/// Room binding enables all records through LAST; keep them live until unlinking.
static WorldCollisionTrigger _gDryfieldNightCellarRoom2ActionTriggers[10] = {
    { NULL, NULL, NULL, { 5056, -48, 4080, 0 }, { { -864, 0, -432, 0 }, { 864, 0, -432, 0 }, { -864, 0, 432, 0 }, { 864, 0, 432, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -ONE, 0 }, 964, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, DRYFIELD_NIGHT_CELLAR_TRIGGER_WARP_TO_UNDERPASS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1728, -64, 3680, 0 }, { { -864, 0, -432, 0 }, { 864, 0, -432, 0 }, { -864, 0, 432, 0 }, { 864, 0, 432, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -ONE, 0 }, 964, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7168, -64, 2368, 0 }, { { -1888, 0, -1520, 0 }, { 1888, 0, -1520, 0 }, { -1888, 0, 1520, 0 }, { 1888, 0, 1520, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, -ONE, 0 }, 2415, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x27B0, -64, 2224, 0 }, { { -400, 0, -960, 0 }, { 400, 0, -960, 0 }, { -400, 0, 960, 0 }, { 400, 0, 960, 0 } }, { 0, 4100, 0, 0 }, { -ONE, 0, 0, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9696, -64, 656, 0 }, { { -400, 0, -688, 0 }, { 400, 0, -688, 0 }, { -400, 0, 688, 0 }, { 400, 0, 688, 0 } }, { 0, 4099, 0, 0 }, { -ONE, 0, 0, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1216, -64, 1632, 0 }, { { -400, 0, -1552, 0 }, { 400, 0, -1552, 0 }, { -400, 0, 1552, 0 }, { 400, 0, 1552, 0 } }, { 0, 4110, 0, 0 }, { ONE, 0, 0, 0 }, 1598, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3680, -64, 2720, 0 }, { { -400, 0, -1552, 0 }, { 400, 0, -1552, 0 }, { -400, 0, 1552, 0 }, { 400, 0, 1552, 0 } }, { 0, 4110, 0, 0 }, { ONE, 0, 0, 0 }, 1598, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2048, -64, 2096, 0 }, { { -400, 0, -416, 0 }, { 400, 0, -416, 0 }, { -400, 0, 416, 0 }, { 400, 0, 416, 0 } }, { 0, 4105, 0, 0 }, { -ONE, 0, 0, 0 }, 576, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2368, -64, 2096, 0 }, { { -400, 0, -1120, 0 }, { 400, 0, -1120, 0 }, { -400, 0, 1120, 0 }, { 400, 0, 1120, 0 } }, { 0, 4100, 0, 0 }, { -ONE, 0, 0, 0 }, 1187, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7376, -64, 192, 0 }, { { -2192, 0, -303, 0 }, { 2192, 0, -303, 0 }, { -2192, 0, 304, 0 }, { 2192, 0, 304, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, ONE, 0 }, 2202, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_CELLAR_TRIGGER_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Sight-blocking room quad.
///
/// Origin and corner offsets use room-local game units; normals use 4096 per unit.
/// Room binding links and enables every record through LAST; storage stays live
/// until unlinking or room/view rebinding. The authored low flag bit is unproven.
static WorldCollisionOccluder _gDryfieldNightCellarSightOccluder = { NULL, NULL, { 3040, -960, 2832, 0 }, { { 0, -1984, -1744, 0 }, { 0, -1984, 1744, 0 }, { 0, 1984, -1744, 0 }, { 0, 1984, 1744, 0 } }, { 4098, 0, 0, 0 }, 2635, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 };

/// Five warm point lights for nighttime cellar room variant 1, active in every view.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits. All five are full strength through 100 units; the first
/// four fade to zero at 3000 units and the fifth at 1500. The loaded room overlay
/// owns this mutable array: coordinate updates parent and compose its transforms,
/// and shading queries overwrite attenuation. Borrowed pointers must not outlive
/// the overlay.
static WorldCoordPointLight _gDryfieldNightCellarRoom1PointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2000, -2000, 2200 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4915, 4505, ONE },
        },
        .inner = 100,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5000, -2000, 2200 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4915, 4505, ONE },
        },
        .inner = 100,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8300, -2000, 3200 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4915, 4505, ONE },
        },
        .inner = 100,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8300, -2000, 200 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4915, 4505, ONE },
        },
        .inner = 100,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9500, -1800, 2200 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3072, 2662, 2048 },
        },
        .inner = 100,
        .outer = 1500,
    },
};

/// Five point lights for nighttime cellar room 1, with no directional or cone lights.
///
/// All views use this separate writable set. Coordinates and attenuation are
/// updated in place; borrowed pointers expire when the room overlay unloads.
static WorldCoordRoomLights _gDryfieldNightCellarRoom1Lights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightCellarRoom1PointLights),
    .pointLights           = _gDryfieldNightCellarRoom1PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Five warm point lights for nighttime cellar room variant 2.
///
/// All views use this set. Positions and falloff radii are in integer world
/// units; RGB intensities have 12 fractional bits. Full strength extends to
/// 100 units, fading to zero at 3000 for the first four lights and 1500 for the
/// fifth. Coordinate composition and lighting queries mutate these records,
/// so the room overlay owns a separate writable set for this variant. Borrowed
/// pointers remain valid only while the overlay is loaded.
static WorldCoordPointLight _gDryfieldNightCellarRoom2PointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2000, -2000, 2200 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4915, 4505, ONE },
        },
        .inner = 100,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5000, -2000, 2200 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4915, 4505, ONE },
        },
        .inner = 100,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8300, -2000, 3200 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4915, 4505, ONE },
        },
        .inner = 100,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8300, -2000, 200 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4915, 4505, ONE },
        },
        .inner = 100,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9500, -1800, 2200 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3072, 2662, 2048 },
        },
        .inner = 100,
        .outer = 1500,
    },
};

/// Five point lights for nighttime cellar room 2, with no directional or cone lights.
///
/// All views use this separate writable set. Coordinates and attenuation are
/// updated in place; borrowed pointers expire when the room overlay unloads.
static WorldCoordRoomLights _gDryfieldNightCellarRoom2Lights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightCellarRoom2PointLights),
    .pointLights           = _gDryfieldNightCellarRoom2PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Placement-entry keys used by this room's resource recipes.
enum {
    DRYFIELD_NIGHT_CELLAR_SCAVENGER_RESOURCE_ENTRY = 16,
    DRYFIELD_NIGHT_CELLAR_BAT_RESOURCE_ENTRY       = 37,
    DRYFIELD_NIGHT_CELLAR_RAT_RESOURCE_ENTRY       = 7,
};

/// Scavenger loading/spawn recipe for night cellar variant 1.
///
/// Five placements use entry 16, base-group-10 file 16 and descriptor 0.
/// The final record is `AREA_PLACEMENT_END`. Keep this room, its map-owned
/// placements and the selected actor overlays loaded during loading, spawning
/// and model-flag updates.
static AreaResource _gDryfieldNightCellarVariant1Resources[2] = {
    { .entryId = DRYFIELD_NIGHT_CELLAR_SCAVENGER_RESOURCE_ENTRY, .fileNumber = 16, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor101600ScavengerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Bat and Rat loading/spawn recipes for night cellar variant 3.
///
/// Six Bats use entry 37, base-group-10 file 37; five Rats use entry 7,
/// base-group-20 file 7. Both spawn descriptor 0.
/// The final record is `AREA_PLACEMENT_END`. Keep this room, its map-owned
/// placements and the selected actor overlays loaded during loading, spawning
/// and model-flag updates.
static AreaResource _gDryfieldNightCellarVariant3Resources[3] = {
    { .entryId = DRYFIELD_NIGHT_CELLAR_BAT_RESOURCE_ENTRY, .fileNumber = 37, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor103700BatTaskDesc },
    { .entryId = DRYFIELD_NIGHT_CELLAR_RAT_RESOURCE_ENTRY, .fileNumber = 7, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = &gActor200700RatTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Scavenger and Bat loading/spawn recipes for night cellar variant 11.
///
/// Two Scavengers use entry 16, base-group-10 file 16; eight Bats use entry
/// 37, base-group-20 file 37. Both spawn descriptor 0.
/// The final record is `AREA_PLACEMENT_END`. Keep this room, its map-owned
/// placements and the selected actor overlays loaded during loading, spawning
/// and model-flag updates.
static AreaResource _gDryfieldNightCellarVariant11Resources[3] = {
    { .entryId = DRYFIELD_NIGHT_CELLAR_SCAVENGER_RESOURCE_ENTRY, .fileNumber = 16, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor101600ScavengerTaskDesc },
    { .entryId = DRYFIELD_NIGHT_CELLAR_BAT_RESOURCE_ENTRY, .fileNumber = 37, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = &gActor203700BatTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldNightCellarAreaVariants[12] = {
    [1]  = { .placements = gMapDryfieldFullCellarVariant1Placements, .resources = _gDryfieldNightCellarVariant1Resources },
    [3]  = { .placements = gMapDryfieldFullCellarVariant3Placements, .resources = _gDryfieldNightCellarVariant3Resources },
    [11] = { .placements = gMapDryfieldFullCellarVariant11Placements, .resources = _gDryfieldNightCellarVariant11Resources },
};

/// Cue-2 script bases in the loaded type-1 room bank; cue 1 uses the next entry.
enum {
    DRYFIELD_NIGHT_CELLAR_WALK_FOOTSTEP_SCRIPT = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x51,
    DRYFIELD_NIGHT_CELLAR_RUN_FOOTSTEP_SCRIPT  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x53
};

/// Paired walk, run and scripted-jump footstep cues for every cellar surface class.
///
/// Walking and scripted jumps share entries 0x51/0x52; running uses 0x53/0x54
/// in the currently loaded type-1 bank. Surface policies borrow this record
/// while the room overlay is loaded.
static WorldCollisionFootstepSounds _gDryfieldNightCellarFootstepSounds = {
    .walk         = DRYFIELD_NIGHT_CELLAR_WALK_FOOTSTEP_SCRIPT,
    .run          = DRYFIELD_NIGHT_CELLAR_RUN_FOOTSTEP_SCRIPT,
    .scriptedJump = DRYFIELD_NIGHT_CELLAR_WALK_FOOTSTEP_SCRIPT,
};

/// Blocking collision and footstep policy shared by surface classes 0..7.
///
/// Enables weapon impacts and pushback, with the cellar's paired footsteps.
/// The first byte is retained as zero; its role remains unproven. Borrowed
/// through the class directory only while this room overlay is loaded.
static WorldCollisionSurfaceProperties _gDryfieldNightCellarSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightCellarFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightCellarSurfacePropertiesByClass[8] = {
    &_gDryfieldNightCellarSurfaceProperties,
    &_gDryfieldNightCellarSurfaceProperties,
    &_gDryfieldNightCellarSurfaceProperties,
    &_gDryfieldNightCellarSurfaceProperties,
    &_gDryfieldNightCellarSurfaceProperties,
    &_gDryfieldNightCellarSurfaceProperties,
    &_gDryfieldNightCellarSurfaceProperties,
    &_gDryfieldNightCellarSurfaceProperties,
};

static void _dryfieldNightCellarInitRoomTask(Task* task);

#include "../../shared/cellar_cap_msg.inc.c"

static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);

/// Rejects key-item use in the nighttime cellar.
///
/// The inventory sends a collected item ID and a zero second argument.
/// Returns zero so its use prompt reports that the item cannot be used here.
/// All arguments are ignored; no item or room state is changed.
static s32 _dryfieldNightCellarRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

#include "../../shared/cellar_door_msg.inc.c"

/// Ignores room-action requests in the nighttime cellar and returns zero.
///
/// `action` is borrowed for synchronous dispatch and is never read or retained.
/// The sender supplies a zero second argument and ignores the return value.
static s32 _dryfieldNightCellarIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* action, s32 unused)
{
    return 0;
}

/// Registers the nighttime cellar's room-message receiver and enters idle.
///
/// Requires state 0 and borrows the room's message table until teardown.
/// Keep the overlay loaded while the registered receiver can be messaged.
static void _dryfieldNightCellarInitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldNightCellarMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the initialized room task alive between message dispatches.
///
/// State 1 performs no per-frame work, leaving the message table installed
/// and the state selector at 1 until teardown.
static void _dryfieldNightCellarIdle(Task* task)
{
}

/// Room-task lifecycle: state 0 initializes, state 1 waits or updates, state 2 releases.
///
/// The dispatcher copies all three callback pointers before indexing by `Task::state`.
/// The selector must be in 0..2; there is no terminator or bounds check.
/// Keep this overlay loaded until the room task is released.
static const TaskFuncTable3 _gDryfieldNightCellarRoomTaskStates = {
    { _dryfieldNightCellarInitRoomTask, _dryfieldNightCellarIdle, taskKill },
};

void dryfieldNightCellarRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gDryfieldNightCellarRoomTaskStates;
    handlers.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_flare.inc.c"

/// Draws the nighttime cellar's pair of flickering textured flares in array order.
///
/// Borrows `worldPoints`, a complete two-position array in integer world units,
/// for this call. Texture column 1 and scale 640 give a pixel half-extent of
/// `640 * 39 / (camera Z / 4)`. Each point with nonnegative GTE flags queues
/// one semitransparent quad; rejected points reserve no packet. Accepted depths
/// must be nonzero. The current view transform, scratch stack, ordering table
/// and packet arena must be ready; queued packets live until GPU completion.
static inline void _dryfieldNightCellarDrawGlowPair(const SVECTOR (*worldPoints)[2])
{
    enum {
        DRYFIELD_NIGHT_CELLAR_GLOW_TEXTURE      = 1,
        DRYFIELD_NIGHT_CELLAR_GLOW_RADIUS_SCALE = 640,
    };

    _glowDrawFlare(&(*worldPoints)[0], DRYFIELD_NIGHT_CELLAR_GLOW_TEXTURE, DRYFIELD_NIGHT_CELLAR_GLOW_RADIUS_SCALE);
    _glowDrawFlare(&(*worldPoints)[1], DRYFIELD_NIGHT_CELLAR_GLOW_TEXTURE, DRYFIELD_NIGHT_CELLAR_GLOW_RADIUS_SCALE);
}

void dryfieldNightCellarDrawGlowsTask(Task* unused)
{
    enum {
        DRYFIELD_NIGHT_CELLAR_GLOW_SWITCH_ON = 1,
        DRYFIELD_NIGHT_CELLAR_GLOW_VIEW_2    = 2,
        DRYFIELD_NIGHT_CELLAR_GLOW_VIEW_3    = 3,
    };

    u8 viewId;

    if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_2) == DRYFIELD_NIGHT_CELLAR_GLOW_SWITCH_ON) {
        viewId = gGameSession->location.view;
        if (viewId == DRYFIELD_NIGHT_CELLAR_GLOW_VIEW_2) {
            _dryfieldNightCellarDrawGlowPair(&_gDryfieldNightCellarView2GlowWorldPoints);
        } else if (viewId == DRYFIELD_NIGHT_CELLAR_GLOW_VIEW_3) {
            _dryfieldNightCellarDrawGlowPair(&_gDryfieldNightCellarView3GlowWorldPoints);
        }
    }
}
