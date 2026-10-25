#include "rooms/dryfield_cellar.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00700.h"
#include "actors/actor_03700.h"

#include "gameplay/display.h"
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
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/cellar.h"

static s32 _dryfieldCellarRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32 _dryfieldCellarIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* action, s32 unused);
static s32 _dryfieldCellarHandleSoundMessage(Task* task, s32 messageId, s32 soundSelector, s32 unused);

static WorldCollisionGrid     _gDryfieldCellarCollisionGrid;
static WorldCollisionOccluder _gDryfieldCellarOccluders[1];
static WorldCollisionTrigger  _gDryfieldCellarViewBoundaries[6];
static WorldCollisionTrigger  _gDryfieldCellarRoom1ActionTriggers[10];
static WorldCollisionTrigger  _gDryfieldCellarRoom2ActionTriggers[10];
static WorldCoordRoomLights   _gDryfieldCellarRoom1Lights;
static WorldCoordRoomLights   _gDryfieldCellarRoom2Lights;

/// The cellar receiver's transition, key-item, direction, CAP and sound messages.
///
/// Five handlers precede `TASK_MESSAGE_TABLE_END`; unsupported IDs return zero.
/// Never dispatch the reserved end ID, whose callback is NULL.
/// The registered room task borrows the table and callbacks until teardown.
/// Keep the room overlay loaded while it can receive messages.
static TaskMessageEntry _gDryfieldCellarMessageTable[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantResolveCellar },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldCellarRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldCellarIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, _cellarHandleCommand },
    { ROOM_MESSAGE_SOUND, _dryfieldCellarHandleSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// View 2's flare centres in the glow task coordinate's local game units.
///
/// Borrowed as a complete pair for drawing; the room overlay owns the storage.
static SVECTOR _gDryfieldCellarView2GlowLocalPoints[2] = {
    { 2000, -2330, 2200, 0 },
    { 5000, -2330, 2200, 0 },
};

/// View 3's flare centres in the glow task coordinate's local game units.
///
/// Borrowed as a complete pair for drawing; the room overlay owns the storage.
static SVECTOR _gDryfieldCellarView3GlowLocalPoints[2] = {
    { 8300, -2330, 3190, 0 },
    { 8300, -2330, 200, 0 },
};

WorldCollisionRoomResources gDryfieldCellarRoomCollisionResources[2] = {
    { .grid = &_gDryfieldCellarCollisionGrid, .viewBoundaryTriggers = _gDryfieldCellarViewBoundaries, .actionTriggers = _gDryfieldCellarRoom1ActionTriggers, .occluders = _gDryfieldCellarOccluders },
    { .grid = &_gDryfieldCellarCollisionGrid, .viewBoundaryTriggers = _gDryfieldCellarViewBoundaries, .actionTriggers = _gDryfieldCellarRoom2ActionTriggers, .occluders = _gDryfieldCellarOccluders },
};

/// Logical views 1..6 of cellar room 2 mapped to camera/image/sprite slots.
///
/// The two retained zero bytes lie outside the six-view lookup domain; they
/// are not terminators. Storage belongs to the loaded room overlay.
static u8 _gDryfieldCellarRoom2ViewMap[8] = {
    1,
    7,
    8,
    9,
    10,
    11,
    0,
    0,
};

u8* gDryfieldCellarRoomViewMaps[2] = {
    gViewIdentityMap,
    _gDryfieldCellarRoom2ViewMap,
};

ViewCount gDryfieldCellarRoomViewCounts[2] = { 11, 6 };

WorldCoordRoomLighting gDryfieldCellarRoomLighting[2] = {
    { &_gDryfieldCellarRoom1Lights, NULL },
    { &_gDryfieldCellarRoom2Lights, NULL },
};

/// Sounds and shared map marker for the cellar/underpass endpoint.
enum {
    DRYFIELD_CELLAR_WARP_ARRIVAL_SOUND   = 0x52220002,
    DRYFIELD_CELLAR_WARP_DEPARTURE_SOUND = 0x52220001,
    DRYFIELD_CELLAR_UNDERPASS_MAP_FLAG   = 463,
};

DirectionWarpEntry gDryfieldCellarWarpEntries[1] = {
    {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 5611, 3, 3945 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 5611, 3, 3945 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_CELLAR_WARP_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_CELLAR_WARP_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_CELLAR_UNDERPASS_MAP_FLAG,
    },
};

/// Unit-length grid normals in room axes, with 4096 representing one.
static SVECTOR _gDryfieldCellarCollisionGridNormals[8] = {
#include "assets/dryfield_cellar_collision_00994_normals.inc"
};

/// Collision-mesh vertices in signed room-coordinate units.
static SVECTOR _gDryfieldCellarCollisionGridVerts[39] = {
#include "assets/dryfield_cellar_collision_00994_verts.inc"
};

/// Indexed mesh faces using the cellar's normals, vertices and surface classes.
static WorldCollisionGridFace _gDryfieldCellarCollisionGridFaces[20] = {
#include "assets/dryfield_cellar_collision_00994_faces.inc"
};

/// Signed face-index lists ending at `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gDryfieldCellarCollisionGridCells[62] = {
#include "assets/dryfield_cellar_collision_00994_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldCellarCollisionGridCells` (62 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_CELLAR_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldCellarCollisionGridCells[(halfwordOffset)])

/// Six cell-list starts, indexed by X * 2 + Z in the 3-by-2 grid.
static s16* _gDryfieldCellarCollisionGridTable[6] = {
#include "assets/dryfield_cellar_collision_00994_table.inc"
};
#undef DRYFIELD_CELLAR_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh shared by both cellar rooms, with 4000-unit XZ cells.
///
/// Mesh pools and cell lists remain owned by the loaded room overlay. Room
/// activation binds the mutable view coordinate before publishing this grid.
static WorldCollisionGrid _gDryfieldCellarCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldCellarCollisionGridNormals,
    .vertices    = _gDryfieldCellarCollisionGridVerts,
    .faces       = _gDryfieldCellarCollisionGridFaces,
    .cellFaceIds = _gDryfieldCellarCollisionGridTable,
    .xBias       = 0,
    .zBias       = 0,
    .cellCountX  = 3,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldCellarCollisionGridFaces),
};

ViewCamera gDryfieldCellarViewCameras[11] = {
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

/// Background-only batch list for mapped cellar view 1.
///
/// The zero-count first record retains decoded image strips; the second
/// ends the list without selecting any source elements. The source slot
/// retains this list address, which is never read as sprites.
static SpriteBatch _gDryfieldCellarView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped cellar view 2.
///
/// Uses opaque 8-bit textures with neutral RGB modulation. Positions are
/// pixels relative to the view centre; depths are unscaled OT keys. The
/// room overlay owns the elements selected by `_gDryfieldCellarView2SpriteBatches`.
static SpriteSource _gDryfieldCellarView2SpriteSources[58] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 32, 56, 500, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, -24, 0, 900, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, 8, 0, 900, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 40, 0, 900, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, 72, 0, 900, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, -24, 16, 750, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, 8, 16, 750, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, 40, 16, 750, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, 72, 16, 750, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, 112, 16, 750, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -24, 32, 950, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, 8, 32, 625, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, 40, 32, 625, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 24 } }, 72, 32, 625, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 24 } }, 112, 32, 625, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, 40, 56, 500, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 24 } }, 72, 56, 469, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, 112, 56, 500, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 56, 80, 500, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, 72, 80, 500, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, 8, -16, 850, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 8, 16, 850, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, 16, 850, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, 72, 812, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, 88, -16, 875, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, 80, 0, 875, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, 56, 16, 875, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 56, 32, 875, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, 80, 16, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 80, 32, 875, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 112, 32, 750, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 144, 32, 800, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, 152, 16, 800, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 112, 64, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 144, 64, 800, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -40, -8, 875, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, -32, 16, 875, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -24, 16, 600, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 0, 24, 625, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -24, 32, 750, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, 0, 32, 625, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -24, 48, 750, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -32, 48, 875, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 0, 48, 625, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 24, 48, 625, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 56, 56, 500, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 24, 80, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, 96, 500, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 0, 80, 625, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -8, 80, 750, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, -72, -56, 1750, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, -32, -56, 1750, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 8, -56, 1750, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 40, -56, 1750, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -72, -24, 1750, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -32, -24, 1750, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 8, -24, 1750, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 40, -24, 1750, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
};

/// Visible source ranges for mapped cellar view 2, ending at `SPRITE_BATCH_END`.
///
/// The leading zero count retains decoded background strips. Ranges use
/// element indices into `_gDryfieldCellarView2SpriteSources`; authored metadata
/// bytes are retained without assigning an unproven meaning. Storage lives
/// with the room overlay.
static SpriteBatch _gDryfieldCellarView2SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { 20, 3, 0, 0, { 3, 0 } },
    { 23, 12, 0, 0, { 2, 0 } },
    { 35, 15, 0, 0, { 4, 0 } },
    { 50, ARRAY_SIZE(_gDryfieldCellarView2SpriteSources) - 50, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped cellar view 3.
///
/// Uses opaque 8-bit textures with neutral RGB modulation. Positions are
/// pixels relative to the view centre; depths are unscaled OT keys. The
/// room overlay owns the elements selected by `_gDryfieldCellarView3SpriteBatches`.
static SpriteSource _gDryfieldCellarView3SpriteSources[49] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -160, -64, 350, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -160, -32, 350, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -160, 0, 350, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -160, 32, 350, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -160, 64, 350, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -136, -24, 375, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -136, 0, 375, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -136, 32, 375, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -136, 64, 375, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -24, -16, 1250, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 8, 24, 1000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -72, 24, 1000, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -80, 8, 1000, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -48, 8, 1000, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -16, 8, 1000, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 24, 8, 1000, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 40, 24, 1250, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -72, 0, 1125, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -48, -8, 1125, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -16, -8, 1125, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 24, -8, 1125, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -16, -16, 1250, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, 24, -16, 1250, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -8, 24, 1000, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -64, -8, 1000, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -40, 0, 1000, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -56, 16, 1000, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -40, 16, 1000, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -56, 48, 1000, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -40, 48, 1000, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, 24, 1125, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, 32, 1125, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 8, 32, 850, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 24, 32, 925, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 40, 32, 1000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 8, 16, 850, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 24, 0, 925, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 40, -8, 1000, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 56, -16, 1125, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -72, 40, 1000, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -80, -16, 1125, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -64, -16, 1125, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -48, -24, 1250, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -32, -24, 1375, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, 0, 1500, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -48, 16, 1250, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -64, 16, 1125, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -32, 16, 1375, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, 16, 1500, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

/// Visible source ranges for mapped cellar view 3, ending at `SPRITE_BATCH_END`.
///
/// The leading zero count retains decoded background strips. Ranges use
/// element indices into `_gDryfieldCellarView3SpriteSources`; authored metadata
/// bytes are retained without assigning an unproven meaning. Storage lives
/// with the room overlay.
static SpriteBatch _gDryfieldCellarView3SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { 9, 14, 0, 0, { 3, 0 } },
    { 23, 7, 0, 0, { 2, 0 } },
    { 30, 9, 0, 0, { 4, 0 } },
    { 39, ARRAY_SIZE(_gDryfieldCellarView3SpriteSources) - 39, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped cellar view 4.
///
/// Uses opaque 8-bit textures with neutral RGB modulation. Positions are
/// pixels relative to the view centre; depths are unscaled OT keys. The
/// room overlay owns the elements selected by `_gDryfieldCellarView4SpriteBatches`.
static SpriteSource _gDryfieldCellarView4SpriteSources[33] = {
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, 80, 178, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -120, 80, 178, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 32 } }, -80, 88, 178, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 32 } }, -40, 88, 178, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, 0, 96, 178, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, 40, 96, 178, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 72, 104, 178, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -160, 56, 178, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -160, 32, 178, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -112, -104, 500, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -72, -96, 600, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 48 } }, -160, 72, 450, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 48 } }, -160, 24, 450, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 48 } }, -160, -24, 450, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 48 } }, -160, -72, 450, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 48 } }, -160, -120, 450, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, -112, 72, 525, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, -112, 24, 525, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, -112, -24, 525, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, -112, -72, 525, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 24 } }, -112, -96, 525, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, -72, 72, 650, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -72, 24, 650, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -72, -24, 650, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -72, -72, 650, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -72, -88, 650, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -40, 24, 750, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -40, -24, 750, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -40, -72, 750, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -24, 24, 800, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -24, -24, 800, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -24, -72, 800, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -40, -80, 750, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
};

/// Visible source ranges for mapped cellar view 4, ending at `SPRITE_BATCH_END`.
///
/// The leading zero count retains decoded background strips. Ranges use
/// element indices into `_gDryfieldCellarView4SpriteSources`; authored metadata
/// bytes are retained without assigning an unproven meaning. Storage lives
/// with the room overlay.
static SpriteBatch _gDryfieldCellarView4SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, ARRAY_SIZE(_gDryfieldCellarView4SpriteSources) - 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped cellar view 5.
///
/// Uses opaque 8-bit textures with neutral RGB modulation. Positions are
/// pixels relative to the view centre; depths are unscaled OT keys. The
/// room overlay owns the elements selected by `_gDryfieldCellarView5SpriteBatches`.
static SpriteSource _gDryfieldCellarView5SpriteSources[21] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -72, 512, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 40, 32, 875, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 88 } }, 128, 32, 512, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 96 } }, 128, -64, 512, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 96 } }, 104, -64, 525, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 88 } }, 104, 32, 525, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 88, 32, 550, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 88, -64, 550, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 72, 32, 575, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 72, -64, 575, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 72, -80, 575, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 88, -80, 550, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 104, -72, 525, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 112 } }, 64, -80, 625, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 64, 32, 625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 112 } }, 56, -80, 675, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 112 } }, 48, -80, 775, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 56, 32, 675, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 48, 32, 775, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 112 } }, 40, -80, 875, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, 32, -64, 950, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
};

/// Visible source ranges for mapped cellar view 5, ending at `SPRITE_BATCH_END`.
///
/// The leading zero count retains decoded background strips. Ranges use
/// element indices into `_gDryfieldCellarView5SpriteSources`; authored metadata
/// bytes are retained without assigning an unproven meaning. Storage lives
/// with the room overlay.
static SpriteBatch _gDryfieldCellarView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldCellarView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped cellar view 6.
///
/// The zero-count first record retains decoded image strips; the second
/// ends the list without selecting any source elements. The source slot
/// retains this list address, which is never read as sprites.
static SpriteBatch _gDryfieldCellarView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped cellar view 7.
///
/// Uses opaque 8-bit textures with neutral RGB modulation. Positions are
/// pixels relative to the view centre; depths are unscaled OT keys. The
/// room overlay owns the elements selected by `_gDryfieldCellarView7SpriteBatches`.
static SpriteSource _gDryfieldCellarView7SpriteSources[58] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 32, 56, 500, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, 112, 16, 750, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 24 } }, 112, 32, 625, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, 112, 56, 500, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, 72, 0, 900, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, 72, 16, 750, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, 72, 32, 625, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, 72, 56, 469, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, 40, 0, 900, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 40, 16, 750, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, 40, 32, 625, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, 40, 56, 500, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 56, 80, 500, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, 72, 80, 500, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, 8, 0, 900, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, 8, 16, 750, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, 8, 32, 625, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, -24, 0, 900, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, -24, 16, 750, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -24, 32, 950, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, 8, -16, 850, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 8, 16, 850, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, 16, 850, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, 72, 834, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 144, 64, 800, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 144, 32, 800, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, 152, 16, 800, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 112, 64, 750, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 112, 32, 750, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 80, 32, 875, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, 80, 16, 875, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, 80, 0, 875, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, 88, -16, 875, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, 56, 16, 728, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 56, 32, 875, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, 96, 500, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 56, 56, 500, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 24, 80, 625, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 24, 48, 625, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 0, 80, 625, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 0, 48, 625, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 0, 32, 625, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 0, 24, 625, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -8, 80, 0x9E59, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -24, 48, 750, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -32, 48, 875, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -24, 32, 750, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -24, 16, 600, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -32, 16, 875, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -40, -8, 875, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 40, -56, 1750, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 40, -24, 1750, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 8, -56, 1750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 8, -24, 1750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, -32, -56, 1750, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -32, -24, 1750, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, -72, -56, 1750, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -72, -24, 1750, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
};

/// Visible source ranges for mapped cellar view 7, ending at `SPRITE_BATCH_END`.
///
/// The leading zero count retains decoded background strips. Ranges use
/// element indices into `_gDryfieldCellarView7SpriteSources`; authored metadata
/// bytes are retained without assigning an unproven meaning. Storage lives
/// with the room overlay.
static SpriteBatch _gDryfieldCellarView7SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { 20, 3, 0, 0, { 3, 0 } },
    { 23, 12, 0, 0, { 2, 0 } },
    { 35, 15, 0, 0, { 4, 0 } },
    { 50, ARRAY_SIZE(_gDryfieldCellarView7SpriteSources) - 50, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped cellar view 8.
///
/// Uses opaque 8-bit textures with neutral RGB modulation. Positions are
/// pixels relative to the view centre; depths are unscaled OT keys. The
/// room overlay owns the elements selected by `_gDryfieldCellarView8SpriteBatches`.
static SpriteSource _gDryfieldCellarView8SpriteSources[49] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -136, -24, 212, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -136, 0, 375, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -136, 32, 375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -136, 64, 375, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -160, -64, 350, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -160, -32, 350, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -160, 0, 350, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -160, 32, 350, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -160, 64, 350, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -24, -16, 1250, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 40, 24, 1250, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 24, 8, 1000, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 24, -8, 1125, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, 24, -16, 1250, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -16, -16, 1250, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -16, -8, 1125, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -16, 8, 1000, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -48, -8, 1125, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -48, 8, 1000, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -72, 0, 1000, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -80, 8, 1000, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 8, 24, 1000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -72, 24, 1000, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -8, 24, 987, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -40, 0, 1000, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -40, 16, 1000, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -40, 48, 1000, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -64, -8, 1000, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -56, 16, 1000, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -56, 48, 1000, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, 24, 1125, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, 32, 1125, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 8, 16, 850, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 8, 32, 850, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 24, 0, 925, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 24, 32, 925, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 40, 32, 1000, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 40, -8, 1000, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 56, -16, 1125, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -72, 40, 1000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, 16, 1500, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, 0, 1500, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -32, -24, 1375, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -32, 16, 1375, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -48, -24, 1250, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -48, 16, 1250, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -64, -16, 1125, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -64, 16, 1125, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -80, -16, 1125, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
};

/// Visible source ranges for mapped cellar view 8, ending at `SPRITE_BATCH_END`.
///
/// The leading zero count retains decoded background strips. Ranges use
/// element indices into `_gDryfieldCellarView8SpriteSources`; authored metadata
/// bytes are retained without assigning an unproven meaning. Storage lives
/// with the room overlay.
static SpriteBatch _gDryfieldCellarView8SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { 9, 14, 0, 0, { 3, 0 } },
    { 23, 7, 0, 0, { 2, 0 } },
    { 30, 9, 0, 0, { 4, 0 } },
    { 39, ARRAY_SIZE(_gDryfieldCellarView8SpriteSources) - 39, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped cellar view 9.
///
/// Uses opaque 8-bit textures with neutral RGB modulation. Positions are
/// pixels relative to the view centre; depths are unscaled OT keys. The
/// room overlay owns the elements selected by `_gDryfieldCellarView9SpriteBatches`.
static SpriteSource _gDryfieldCellarView9SpriteSources[66] = {
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 104, 172, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, 104, 172, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, 104, 171, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, 104, 171, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 112, 168, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, 112, 167, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, 112, 163, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, 112, 166, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, 48, 104, 350, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, 8, 104, 350, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 8, 96, 350, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, -40, 104, 350, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 16 } }, -40, 88, 350, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, -72, 104, 350, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, -72, 88, 350, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, -104, 104, 350, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -104, 88, 350, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, -136, 104, 350, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, -136, 88, 350, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -136, 80, 350, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -160, 104, 350, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -160, 88, 350, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 80, 350, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -160, 32, 350, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -32, -80, 850, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -48, 72, 750, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -160, 72, 350, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -160, 24, 437, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -160, -24, 437, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -160, -72, 437, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -160, -120, 437, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -128, 72, 450, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -128, 24, 450, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -128, -24, 450, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -128, -72, 450, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -128, -120, 450, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -112, 72, 500, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -112, 24, 500, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -112, -24, 500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -112, -72, 500, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -112, -104, 500, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -96, 72, 525, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -96, 24, 525, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -96, -24, 525, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -96, -72, 525, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -96, -96, 525, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -80, 72, 587, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -80, 24, 587, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -80, -24, 587, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -80, -72, 587, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -80, -96, 587, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -64, 24, 650, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -64, -24, 650, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -64, -72, 650, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -64, -88, 650, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -48, 24, 750, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -48, -24, 750, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -48, -72, 750, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -48, -80, 750, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -32, 24, 850, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -32, -24, 850, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -32, -72, 850, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -24, 24, 925, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -24, -24, 925, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -24, -72, 925, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -64, 72, 650, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
};

/// Visible source ranges for mapped cellar view 9, ending at `SPRITE_BATCH_END`.
///
/// The leading zero count retains decoded background strips. Ranges use
/// element indices into `_gDryfieldCellarView9SpriteSources`; authored metadata
/// bytes are retained without assigning an unproven meaning. Storage lives
/// with the room overlay.
static SpriteBatch _gDryfieldCellarView9SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 1, 0 } },
    { 24, ARRAY_SIZE(_gDryfieldCellarView9SpriteSources) - 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped cellar view 10.
///
/// Uses opaque 8-bit textures with neutral RGB modulation. Positions are
/// pixels relative to the view centre; depths are unscaled OT keys. The
/// room overlay owns the elements selected by `_gDryfieldCellarView10SpriteBatches`.
static SpriteSource _gDryfieldCellarView10SpriteSources[21] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -72, 512, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 40, 32, 875, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 88 } }, 128, 32, 512, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 96 } }, 128, -64, 512, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 96 } }, 104, -64, 525, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 104, -72, 525, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 88, 32, 550, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 88, -64, 550, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 88, -80, 550, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 72, 32, 575, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 72, -64, 575, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 72, -80, 575, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 64, 32, 625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 112 } }, 64, -80, 625, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 56, 32, 675, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 112 } }, 56, -80, 675, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 48, 32, 775, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 112 } }, 48, -80, 775, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 112 } }, 40, -80, 875, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, 32, -64, 950, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 88 } }, 104, 32, 525, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
};

/// Visible source ranges for mapped cellar view 10, ending at `SPRITE_BATCH_END`.
///
/// The leading zero count retains decoded background strips. Ranges use
/// element indices into `_gDryfieldCellarView10SpriteSources`; authored metadata
/// bytes are retained without assigning an unproven meaning. Storage lives
/// with the room overlay.
static SpriteBatch _gDryfieldCellarView10SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldCellarView10SpriteSources), 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 11's background-only sprite list.
///
/// A zero-count selector precedes `SPRITE_BATCH_END`, retaining decoded image
/// strips. The same address occupies `sources.empty` and supplies no sprites.
/// Keep this distinct authored storage live while the room overlay is loaded.
static SpriteBatch _gDryfieldCellarView11SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

SpriteView gDryfieldCellarSpriteViews[11] = {
    { { .empty = _gDryfieldCellarView1SpriteBatches }, _gDryfieldCellarView1SpriteBatches, NULL },
    { { .elements = _gDryfieldCellarView2SpriteSources }, _gDryfieldCellarView2SpriteBatches, NULL },
    { { .elements = _gDryfieldCellarView3SpriteSources }, _gDryfieldCellarView3SpriteBatches, NULL },
    { { .elements = _gDryfieldCellarView4SpriteSources }, _gDryfieldCellarView4SpriteBatches, NULL },
    { { .elements = _gDryfieldCellarView5SpriteSources }, _gDryfieldCellarView5SpriteBatches, NULL },
    { { .empty = _gDryfieldCellarView6SpriteBatches }, _gDryfieldCellarView6SpriteBatches, NULL },
    { { .elements = _gDryfieldCellarView7SpriteSources }, _gDryfieldCellarView7SpriteBatches, NULL },
    { { .elements = _gDryfieldCellarView8SpriteSources }, _gDryfieldCellarView8SpriteBatches, NULL },
    { { .elements = _gDryfieldCellarView9SpriteSources }, _gDryfieldCellarView9SpriteBatches, NULL },
    { { .elements = _gDryfieldCellarView10SpriteSources }, _gDryfieldCellarView10SpriteBatches, NULL },
    { { .empty = _gDryfieldCellarView11SpriteBatches }, _gDryfieldCellarView11SpriteBatches, NULL },
};

/// Six directed view boundaries shared by both cellar room slots.
///
/// Source/destination pairs are 2->3, 3->2, 4->5, 5->4, 4->2 and 2->4.
/// They select logical views in the active room.
/// Geometry uses room-coordinate units and Q12 normals. Setup binds coordinates
/// and mutates links/flags; the final included quad carries LAST. Keep the
/// overlay loaded until the collision list is unlinked.
static WorldCollisionTrigger _gDryfieldCellarViewBoundaries[6] = {
    { NULL, NULL, NULL, { 6576, -1472, 2096, 0 }, { { -318, -1904, 2593, 0 }, { 311, -1904, -2603, 0 }, { -318, 1904, 2593, 0 }, { 311, 1904, -2603, 0 } }, { -4074, 0, -494, 0 }, { 0, 0, ONE, 0 }, 3228, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6463, -1440, 2080, 0 }, { { 312, -1904, -2602, 0 }, { -317, -1904, 2594, 0 }, { 312, 1904, -2602, 0 }, { -317, 1904, 2594, 0 } }, { 4072, 0, 492, 0 }, { 0, 0, ONE, 0 }, 3228, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1727, -1056, 2096, 0 }, { { -1261, -1904, 95, 0 }, { 1256, -1904, -100, 0 }, { -1261, 1904, 95, 0 }, { 1256, 1904, -100, 0 } }, { -318, 0, -4087, 0 }, { 0, 0, ONE, 0 }, 2275, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1663, -1376, 2047, 0 }, { { 1253, -1904, -102, 0 }, { -1264, -1904, 94, 0 }, { 1253, 1904, -102, 0 }, { -1264, 1904, 94, 0 } }, { 317, 0, 4084, 0 }, { 0, 0, ONE, 0 }, 2275, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2878, -1344, 670, 0 }, { { -87, -1904, 1261, 0 }, { 87, -1904, -1260, 0 }, { -87, 1904, 1261, 0 }, { 87, 1904, -1260, 0 } }, { -4092, 0, -283, 0 }, { 0, 0, ONE, 0 }, 2275, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2718, -1312, 670, 0 }, { { 86, -1904, -1262, 0 }, { -88, -1904, 1259, 0 }, { 86, 1904, -1262, 0 }, { -88, 1904, 1259, 0 } }, { 4089, 0, 281, 0 }, { 0, 0, ONE, 0 }, 2275, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Authored CAP table indices used by the cellar interaction quads.
enum {
    DRYFIELD_CELLAR_CAP_COMMAND_3        = 3,
    DRYFIELD_CELLAR_CAP_COMMAND_5        = 5,
    DRYFIELD_CELLAR_CAP_COMMAND_6        = 6,
    DRYFIELD_CELLAR_CAP_COMMAND_7        = 7,
    DRYFIELD_CELLAR_CAP_COMMAND_8        = 8,
    DRYFIELD_CELLAR_CAP_COMMAND_9        = 9,
    DRYFIELD_CELLAR_CAP_COMMAND_10       = 10,
    DRYFIELD_CELLAR_CAP_COMMAND_11       = 11,
    DRYFIELD_CELLAR_CAP_COMMAND_12       = 12,
    DRYFIELD_CELLAR_COMMAND_P08_DIALOGUE = 13,
};

/// Packed departure slot 1 and destination arrival slot 1 in a warp trigger.
enum { DRYFIELD_CELLAR_WARP_ENDPOINTS = (1 << 4) | 1 };

/// Room 1 interaction quads: the underpass exit and nine CAP/dialogue actions.
///
/// The exit uses departure slot 1 and underpass arrival slot 1; the P08
/// interaction sends room command 13 instead of selecting CAP command 13.
/// Positions/radii use room units and normals use Q12. Setup mutates links,
/// coordinates and flags; the last included quad carries LAST. Keep the room
/// overlay and selected CAP commands loaded until unlinked.
static WorldCollisionTrigger _gDryfieldCellarRoom1ActionTriggers[10] = {
    { NULL, NULL, NULL, { 5056, -48, 4080, 0 }, { { -864, 0, -432, 0 }, { 864, 0, -432, 0 }, { -864, 0, 432, 0 }, { 864, 0, 432, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -ONE, 0 }, 964, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_UNDERPASS, DRYFIELD_CELLAR_WARP_ENDPOINTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1728, -64, 3488, 0 }, { { -864, 0, -432, 0 }, { 864, 0, -432, 0 }, { -864, 0, 432, 0 }, { 864, 0, 432, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -ONE, 0 }, 964, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7168, -64, 2368, 0 }, { { -1888, 0, -1520, 0 }, { 1888, 0, -1520, 0 }, { -1888, 0, 1520, 0 }, { 1888, 0, 1520, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, -ONE, 0 }, 2415, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x27B0, -64, 2224, 0 }, { { -400, 0, -960, 0 }, { 400, 0, -960, 0 }, { -400, 0, 960, 0 }, { 400, 0, 960, 0 } }, { 0, 4100, 0, 0 }, { -ONE, 0, 0, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_COMMAND_P08_DIALOGUE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9696, -64, 656, 0 }, { { -400, 0, -688, 0 }, { 400, 0, -688, 0 }, { -400, 0, 688, 0 }, { 400, 0, 688, 0 } }, { 0, 4099, 0, 0 }, { -ONE, 0, 0, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1216, -64, 1744, 0 }, { { -400, 0, -1664, 0 }, { 400, 0, -1664, 0 }, { -400, 0, 1664, 0 }, { 400, 0, 1664, 0 } }, { 0, 4098, 0, 0 }, { ONE, 0, 0, 0 }, 1707, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3840, -64, 2720, 0 }, { { -400, 0, -1552, 0 }, { 400, 0, -1552, 0 }, { -400, 0, 1552, 0 }, { 400, 0, 1552, 0 } }, { 0, 4110, 0, 0 }, { ONE, 0, 0, 0 }, 1598, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2048, -64, 2112, 0 }, { { -400, 0, -400, 0 }, { 400, 0, -400, 0 }, { -400, 0, 400, 0 }, { 400, 0, 400, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 565, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2112, -64, 2192, 0 }, { { -400, 0, -1216, 0 }, { 400, 0, -1216, 0 }, { -400, 0, 1216, 0 }, { 400, 0, 1216, 0 } }, { 0, 4098, 0, 0 }, { -ONE, 0, 0, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7264, 0, 320, 0 }, { { -2112, 0, -368, 0 }, { 2112, 0, -368, 0 }, { -2112, 0, 368, 0 }, { 2112, 0, 368, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, ONE, 0 }, 2141, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room 2 interaction quads: the underpass exit and nine direct CAP actions.
///
/// The exit uses departure slot 1 and underpass arrival slot 1. Other quads
/// select CAP commands directly, with room-specific positions and command IDs.
/// Positions/radii use room units and normals use Q12. Setup mutates links,
/// coordinates and flags; the last included quad carries LAST. Keep the room
/// overlay and selected CAP commands loaded until unlinked.
static WorldCollisionTrigger _gDryfieldCellarRoom2ActionTriggers[10] = {
    { NULL, NULL, NULL, { 5056, -48, 4080, 0 }, { { -864, 0, -432, 0 }, { 864, 0, -432, 0 }, { -864, 0, 432, 0 }, { 864, 0, 432, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -ONE, 0 }, 964, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_UNDERPASS, DRYFIELD_CELLAR_WARP_ENDPOINTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1728, -64, 3552, 0 }, { { -864, 0, -432, 0 }, { 864, 0, -432, 0 }, { -864, 0, 432, 0 }, { 864, 0, 432, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -ONE, 0 }, 964, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7168, -64, 2368, 0 }, { { -1888, 0, -1520, 0 }, { 1888, 0, -1520, 0 }, { -1888, 0, 1520, 0 }, { 1888, 0, 1520, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, -ONE, 0 }, 2415, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x27B0, -64, 2224, 0 }, { { -400, 0, -960, 0 }, { 400, 0, -960, 0 }, { -400, 0, 960, 0 }, { 400, 0, 960, 0 } }, { 0, 4100, 0, 0 }, { -ONE, 0, 0, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9696, -64, 656, 0 }, { { -400, 0, -688, 0 }, { 400, 0, -688, 0 }, { -400, 0, 688, 0 }, { 400, 0, 688, 0 } }, { 0, 4099, 0, 0 }, { -ONE, 0, 0, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1216, -64, 1792, 0 }, { { -400, 0, -1712, 0 }, { 400, 0, -1712, 0 }, { -400, 0, 1712, 0 }, { 400, 0, 1712, 0 } }, { 0, 4113, 0, 0 }, { ONE, 0, 0, 0 }, 1755, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3840, -64, 2720, 0 }, { { -400, 0, -1552, 0 }, { 400, 0, -1552, 0 }, { -400, 0, 1552, 0 }, { 400, 0, 1552, 0 } }, { 0, 4110, 0, 0 }, { ONE, 0, 0, 0 }, 1598, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2048, -64, 2336, 0 }, { { -400, 0, -400, 0 }, { 400, 0, -400, 0 }, { -400, 0, 400, 0 }, { 400, 0, 400, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 565, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2112, -64, 2224, 0 }, { { -400, 0, -1248, 0 }, { 400, 0, -1248, 0 }, { -400, 0, 1248, 0 }, { 400, 0, 1248, 0 } }, { 0, 4099, 0, 0 }, { -ONE, 0, 0, 0 }, 1305, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7472, -64, 256, 0 }, { { -2256, 0, -336, 0 }, { 2256, 0, -336, 0 }, { -2256, 0, 336, 0 }, { 2256, 0, 336, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, ONE, 0 }, 2275, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_CELLAR_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// The sight-blocking quad shared by both cellar room slots.
///
/// Its plane is at room X=3040; vertices are offsets in room game units and
/// the normal uses Q12. Setup mutates links/flags. This sole included record
/// carries LAST; keep the room overlay loaded until it is unlinked. The role
/// of `flags` bit 0 is unproven.
static WorldCollisionOccluder _gDryfieldCellarOccluders[1] = {
    { NULL, NULL, { 3040, -960, 2832, 0 }, { { 0, -1984, -1744, 0 }, { 0, -1984, 1744, 0 }, { 0, 1984, -1744, 0 }, { 0, 1984, 1744, 0 } }, { 4098, 0, 0, 0 }, 2635, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Five warm point lights for cellar room variant 1, contributing in every view.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits. Each light is full strength through 2700 units and
/// fades to zero at 3500 units. The loaded room overlay owns the mutable array:
/// coordinate updates parent and compose its transforms, and shading queries
/// overwrite attenuation. Borrowed pointers must not outlive the overlay.
static WorldCoordPointLight _gDryfieldCellarRoom1PointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1700, -2500, 2300 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4915, 4505, ONE },
        },
        .inner = 2700,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4000, -2500, 2300 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4915, 4505, ONE },
        },
        .inner = 2700,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6700, -2500, 3200 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4915, 4505, ONE },
        },
        .inner = 2700,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6700, -2500, 900 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4915, 4505, ONE },
        },
        .inner = 2700,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9095, -2500, 2200 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4915, 4505, ONE },
        },
        .inner = 2700,
        .outer = 3500,
    },
};

/// Room 1's five warm point lights, with no directional or cone lights.
///
/// Borrows writable light elements for the room overlay's loaded lifetime.
static WorldCoordRoomLights _gDryfieldCellarRoom1Lights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldCellarRoom1PointLights),
    .pointLights           = _gDryfieldCellarRoom1PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Five blue point lights for cellar room variant 2, contributing in every view.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits. Every light fades to zero at 3500 units. The loaded room
/// overlay owns this mutable array: coordinate updates parent and compose its
/// transforms, and shading queries overwrite attenuation. Borrowed pointers
/// must not outlive the overlay.
static WorldCoordPointLight _gDryfieldCellarRoom2PointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1700, -2500, 2300 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1843, 1843, 2662 },
        },
        .inner = 2250,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4000, -2500, 2300 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1843, 1843, 2662 },
        },
        .inner = 2250,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6700, -2500, 3200 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1843, 1843, 2662 },
        },
        .inner = 2500,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6700, -2500, 900 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1843, 1843, 2662 },
        },
        .inner = 2500,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9095, -2500, 2200 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1843, 1843, 2662 },
        },
        .inner = 2700,
        .outer = 3500,
    },
};

/// Room 2's five dim blue point lights, with no directional or cone lights.
///
/// Borrows writable light elements for the room overlay's loaded lifetime.
static WorldCoordRoomLights _gDryfieldCellarRoom2Lights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldCellarRoom2PointLights),
    .pointLights           = _gDryfieldCellarRoom2PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Placement-entry keys used by this room's resource recipes.
enum {
    DRYFIELD_CELLAR_BAT_RESOURCE_ENTRY = 37,
    DRYFIELD_CELLAR_RAT_RESOURCE_ENTRY = 7,
};

/// Bat and Rat loading/spawn recipes for daytime cellar variant 1.
///
/// Eight Bat placements use entry 37, base-group-10 file 37; two Rats use
/// entry 7, base-group-20 file 7. Both spawn descriptor 0.
/// The final record is `AREA_PLACEMENT_END`. Keep this room, its map-owned
/// placements and the selected actor overlays loaded during loading, spawning
/// and model-flag updates.
static AreaResource _gDryfieldCellarVariant1Resources[3] = {
    { .entryId = DRYFIELD_CELLAR_BAT_RESOURCE_ENTRY, .fileNumber = 37, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor103700BatTaskDesc },
    { .entryId = DRYFIELD_CELLAR_RAT_RESOURCE_ENTRY, .fileNumber = 7, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = &gActor200700RatTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldCellarAreaVariants[12] = {
    [1] = { .placements = gMapDryfieldCellarVariant1Placements, .resources = _gDryfieldCellarVariant1Resources },
};

/// Cue-2 footstep bases in the currently loaded type-1 sound bank.
enum {
    DRYFIELD_CELLAR_FOOTSTEP_WALK          = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x51,
    DRYFIELD_CELLAR_FOOTSTEP_RUN           = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x53,
    DRYFIELD_CELLAR_FOOTSTEP_SCRIPTED_JUMP = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x55,
};

/// Paired walking, running and scripted-jump cues shared by all surface classes.
///
/// Playback adds one for animation cue 1 and 100 for a companion. The room
/// overlay owns these request bases; surface records borrow them while loaded.
static WorldCollisionFootstepSounds _gDryfieldCellarFootstepSounds = {
    .walk         = DRYFIELD_CELLAR_FOOTSTEP_WALK,
    .run          = DRYFIELD_CELLAR_FOOTSTEP_RUN,
    .scriptedJump = DRYFIELD_CELLAR_FOOTSTEP_SCRIPTED_JUMP,
};

/// Shared blocking, weapon-impact and pushback policy for every cellar surface.
static WorldCollisionSurfaceProperties _gDryfieldCellarSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldCellarFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldCellarSurfacePropertiesByClass[8] = {
    &_gDryfieldCellarSurfaceProperties,
    &_gDryfieldCellarSurfaceProperties,
    &_gDryfieldCellarSurfaceProperties,
    &_gDryfieldCellarSurfaceProperties,
    &_gDryfieldCellarSurfaceProperties,
    &_gDryfieldCellarSurfaceProperties,
    &_gDryfieldCellarSurfaceProperties,
    &_gDryfieldCellarSurfaceProperties,
};

static void _dryfieldCellarInitRoomTask(Task* task);
static void _dryfieldCellarIdle(Task* task);

#include "../../shared/cellar_cap_msg.inc.c"

static void _glowDrawFlareLocal(const GfxCoord* coord, const SVECTOR* localPoint, s32 textureIndex, s32 radiusScale);

/// Rejects every key-item use in the cellar.
///
/// Returns 0 for any `itemId`, making the inventory prompt report that the item
/// cannot be used here. All arguments are ignored; no item is consumed.
static s32 _dryfieldCellarRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

#include "../../shared/cellar_door_msg.inc.c"

/// Ignores room-specific direction actions in the cellar.
///
/// The borrowed `action` payload is never read. Returns 0 without changing the
/// room or receiver; the second payload word is unused.
static s32 _dryfieldCellarIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* action, s32 unused)
{
    return 0;
}

/// Handles the cellar's room-sound selector 3; other selectors do nothing.
///
/// Requests bank 0x5222, instance 0, entry 3 with unchanged pan and gain.
/// Returns 0 even if the sound request is refused; no arguments are retained.
static s32 _dryfieldCellarHandleSoundMessage(Task* task, s32 messageId, s32 soundSelector, s32 unused)
{
    enum {
        DRYFIELD_CELLAR_SOUND_SELECTOR_3 = 3,
        DRYFIELD_CELLAR_SOUND_SCRIPT_3   = 0x52220003,
    };

    if (soundSelector == DRYFIELD_CELLAR_SOUND_SELECTOR_3) {
        sndEvtRequestScriptStart(DRYFIELD_CELLAR_SOUND_SCRIPT_3, 0, 0);
    }
    return 0;
}

/// Registers the cellar's room receiver and permits post-CAP sound messages.
///
/// Requires the live room task in state 0. Borrows the room's message table
/// for its lifetime and advances to the idle state 1.
static void _dryfieldCellarInitRoomTask(Task* task)
{
    enum { DRYFIELD_CELLAR_POST_CAP_SOUND_ENABLED = 1 };

    task->msgTable = _gDryfieldCellarMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state                    += 1;
    gCapRoomCompletionSoundsEnabled = DRYFIELD_CELLAR_POST_CAP_SOUND_ENABLED;
}

/// Leaves the initialized room task alive without per-frame updates.
///
/// State 1 preserves the installed message table and task state until an
/// external request ends the task.
static void _dryfieldCellarIdle(Task* task)
{
}

/// Cellar room-task handlers indexed by `Task.state`: 0 register, 1 idle, 2 release.
///
/// Dispatch copies the complete three-handler value onto the stack before
/// indexing it. The task state must be in 0..2 and the room overlay loaded.
static const TaskFuncTable3 _gDryfieldCellarRoomTaskStates = {
    { _dryfieldCellarInitRoomTask, _dryfieldCellarIdle, taskKill },
};

void dryfieldCellarRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldCellarRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_flare_local.inc.c"

/// Draws the cellar's pair of local-point flares with a shared texture and size.
///
/// Borrows `localPoints`, a complete two-position array in local game units,
/// and `coord` for this call. `coord->workm` must be current and map into
/// `GsWSMATRIX`'s input space; the projection, scratch stack, ordering table
/// and packet arena must be ready. The signed low halfword of `textureIndex`
/// selects a flare column (0..2); that of `radiusScale` gives a pixel half-extent
/// of `radiusScale * 39 / (camera Z / 4)` after transformed coordinates narrow
/// to s16.
/// Draws element 0 before element 1, reserving two packets even when depth
/// clipping rejects either point. Queued packets live until GPU completion.
static inline void _dryfieldCellarDrawGlowPair(const GfxCoord* coord, const SVECTOR (*localPoints)[2], s32 textureIndex, s32 radiusScale)
{
    _glowDrawFlareLocal(coord, &(*localPoints)[0], textureIndex, radiusScale);
    _glowDrawFlareLocal(coord, &(*localPoints)[1], textureIndex, radiusScale);
}

void dryfieldCellarDrawGlowsTask(Task* task)
{
    enum {
        DRYFIELD_CELLAR_GLOW_SWITCH_ON    = 1,
        DRYFIELD_CELLAR_GLOW_VIEW_2       = 2,
        DRYFIELD_CELLAR_GLOW_VIEW_3       = 3,
        DRYFIELD_CELLAR_GLOW_TEXTURE      = 1,
        DRYFIELD_CELLAR_GLOW_RADIUS_SCALE = 640, // Pixel half-extent = scale * 39 / (camera Z / 4)
    };

    const GfxCoord* coord;

    coord = task->extra.coordBody->coord;
    if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_2) == DRYFIELD_CELLAR_GLOW_SWITCH_ON) {
        if (gGameSession->location.view == DRYFIELD_CELLAR_GLOW_VIEW_2) {
            _dryfieldCellarDrawGlowPair(coord, &_gDryfieldCellarView2GlowLocalPoints, DRYFIELD_CELLAR_GLOW_TEXTURE, DRYFIELD_CELLAR_GLOW_RADIUS_SCALE);
        } else if (gGameSession->location.view == DRYFIELD_CELLAR_GLOW_VIEW_3) {
            _dryfieldCellarDrawGlowPair(coord, &_gDryfieldCellarView3GlowLocalPoints, DRYFIELD_CELLAR_GLOW_TEXTURE, DRYFIELD_CELLAR_GLOW_RADIUS_SCALE);
        }
    }
}
