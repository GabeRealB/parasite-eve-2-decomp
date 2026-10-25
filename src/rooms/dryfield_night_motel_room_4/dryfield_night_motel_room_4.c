#include "rooms/dryfield_night_motel_room_4.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00700.h"
#include "actors/actor_04000.h"
#include "actors/actor_401800.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_variants.h"

static s32  _dryfieldNightMotelRoom4RejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32  _dryfieldNightMotelRoom4IgnoreCommand(Task* unusedTask, s32 messageId, s32 commandId, s32 commandMode);
static s32  _dryfieldNightMotelRoom4IgnoreActionRequest(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _dryfieldNightMotelRoom4IdleRoomTask(Task* unusedTask);
static void _dryfieldNightMotelRoom4InitRoomTask(Task* task);

static WorldCollisionGrid     _gDryfieldNightMotelRoom4CollisionGrid;
static WorldCollisionOccluder _gDryfieldNightMotelRoom4SightOccluders[2];
static WorldCollisionTrigger  _gDryfieldNightMotelRoom4ViewBoundaryTriggers[8];
static WorldCollisionTrigger  _gDryfieldNightMotelRoom4ActionTriggers[7];
static WorldCoordRoomLights   _gDryfieldNightMotelRoom4RoomLights;

/// Motel Room 4's synchronous room-message handlers, ending at `TASK_MESSAGE_TABLE_END`.
///
/// Resolves main-street room variants, refuses key-item use and ignores action
/// and command requests. The room task borrows this table while registered;
/// the table and callbacks require this overlay to remain loaded.
static TaskMessageEntry _gDryfieldNightMotelRoom4RoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantMainStreetMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightMotelRoom4RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightMotelRoom4IgnoreActionRequest },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightMotelRoom4IgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// World-space flare centres in signed game-coordinate units for views 2, 3 and 5.
///
/// Element 0 is shared by those views; element 1 is the larger flare in views
/// 2 and 3, and element 2 is the smaller secondary-texture flare in view 5.
static SVECTOR _gDryfieldNightMotelRoom4FlarePoints[3] = {
    { 420, -990, 3200, 0 },
    { 2480, -2240, 2400, 0 },
    { 5200, -1920, 2890, 0 },
};

/// World-space centre of view 4's sole flare, in signed game-coordinate units.
static SVECTOR _gDryfieldNightMotelRoom4View4FlarePoint = { 6550, -1020, 410, 0 };

WorldCoordRoomLighting gDryfieldNightMotelRoom4RoomLighting[1] = {
    { &_gDryfieldNightMotelRoom4RoomLights, NULL },
};

WorldCollisionRoomResources gDryfieldNightMotelRoom4RoomCollisionResources[1] = {
    { .grid = &_gDryfieldNightMotelRoom4CollisionGrid, .viewBoundaryTriggers = _gDryfieldNightMotelRoom4ViewBoundaryTriggers, .actionTriggers = _gDryfieldNightMotelRoom4ActionTriggers, .occluders = _gDryfieldNightMotelRoom4SightOccluders },
};

u8* gDryfieldNightMotelRoom4ViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldNightMotelRoom4ViewCounts[1] = { ARRAY_SIZE(gDryfieldNightMotelRoom4ViewCameras) };

/// Door endpoint selectors and stage/area sound-event keys.
enum {
    DRYFIELD_NIGHT_MOTEL_ROOM_4_DOOR_WARP                = 1,
    DRYFIELD_NIGHT_MOTEL_ROOM_4_MAIN_STREET_ARRIVAL_WARP = 6,
    DRYFIELD_NIGHT_MOTEL_ROOM_4_DOOR_ARRIVAL_SOUND       = 0x530E0002,
    DRYFIELD_NIGHT_MOTEL_ROOM_4_DOOR_DEPARTURE_SOUND     = 0x530E0001,
    DRYFIELD_NIGHT_MOTEL_ROOM_4_DOOR_MAP_FLAG            = 483,
};

DirectionWarpEntry gDryfieldNightMotelRoom4WarpEntries[1] = {
    {
        .player         = { { .word = 0 }, 3580, 0, 541 },
        .companion      = { { .word = 0 }, 3580, 0, 541 },
        .arrivalSound   = DRYFIELD_NIGHT_MOTEL_ROOM_4_DOOR_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_NIGHT_MOTEL_ROOM_4_DOOR_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_MOTEL_ROOM_4_DOOR_MAP_FLAG,
    },
};

/// Room-space face normals for the room's collision grid, with 4096 per unit.
static SVECTOR _gDryfieldNightMotelRoom4CollisionGridNormals[11] = {
#include "assets/dryfield_night_motel_room_4_collision_00C10_normals.inc"
};

/// Room-space collision vertices in signed game-coordinate units.
static SVECTOR _gDryfieldNightMotelRoom4CollisionGridVerts[102] = {
#include "assets/dryfield_night_motel_room_4_collision_00C10_verts.inc"
};

/// Collision quads indexing the room's vertex and normal pools and surface classes 0..3.
static WorldCollisionGridFace _gDryfieldNightMotelRoom4CollisionGridFaces[45] = {
#include "assets/dryfield_night_motel_room_4_collision_00C10_faces.inc"
};

/// Packed signed face-index lists for the four cells, each ending at -1.
///
/// The cell table selects starts 0, 46, 84 and 124 in halfwords. The retained
/// halfword at index 45 follows the first terminator and belongs to no list.
static s16 _gDryfieldNightMotelRoom4CollisionGridCells[154] = {
#include "assets/dryfield_night_motel_room_4_collision_00C10_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightMotelRoom4CollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..153 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_MOTEL_ROOM_4_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightMotelRoom4CollisionGridCells[(halfwordOffset)])
/// X-major face-list pointers for the room's 2-by-2 collision grid.
static s16* _gDryfieldNightMotelRoom4CollisionGridTable[4] = {
#include "assets/dryfield_night_motel_room_4_collision_00C10_table.inc"
};
#undef DRYFIELD_NIGHT_MOTEL_ROOM_4_CELL_FACE_LIST_AT_OFFSET

/// The room's collision mesh, indexed by four 4000-unit XZ cells from origin (0, 0).
///
/// Room setup binds the view coordinate; the record and all pools stay live
/// while this room overlay is loaded and its grid is active.
static WorldCollisionGrid _gDryfieldNightMotelRoom4CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightMotelRoom4CollisionGridNormals,
    .vertices    = _gDryfieldNightMotelRoom4CollisionGridVerts,
    .faces       = _gDryfieldNightMotelRoom4CollisionGridFaces,
    .cellFaceIds = _gDryfieldNightMotelRoom4CollisionGridTable,
    .xBias       = 0,
    .zBias       = 0,
    .cellCountX  = 2,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightMotelRoom4CollisionGridFaces),
};

ViewCamera gDryfieldNightMotelRoom4ViewCameras[6] = {
    { .transform = { .m = { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, .t = { -3500, 0x2904, -2500 } }, .screenDistance = 289 },
    { .transform = { .m = { { 652, 0, 4043 }, { 208, 4090, -33 }, { -4038, 211, 651 } }, .t = { -6800, 1250, -950 } }, .screenDistance = 275 },
    { .transform = { .m = { { 3984, 0, -951 }, { -39, 4092, -166 }, { 950, 171, 3980 } }, .t = { -1200, 1450, -200 } }, .screenDistance = 240 },
    { .transform = { .m = { { 660, 0, -4042 }, { -57, 4095, -9 }, { 4042, 58, 660 } }, .t = { -1400, 1250, -1050 } }, .screenDistance = 275 },
    { .transform = { .m = { { 209, 0, 4090 }, { 1972, 3588, -100 }, { -3583, 1975, 183 } }, .t = { -6800, 2170, -3550 } }, .screenDistance = 263 },
    { .transform = { .m = { { 866, 0, -4003 }, { -172, 4092, -37 }, { 3999, 176, 865 } }, .t = { -2200, 1160, -3120 } }, .screenDistance = 269 },
};

/// View 1's empty sprite list; the zero first count retains decoded background strips.
static SpriteBatch _gDryfieldNightMotelRoom4View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's screen-space sprite rectangles and unscaled sorting depths.
static SpriteSource _gDryfieldNightMotelRoom4View2SpriteSources[37] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 72, 900, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 80, 900, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -120, 650, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -80, 650, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -40, 650, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 0, 650, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 40, 650, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 80, 650, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, 80, 750, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, 40, 750, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, 0, 750, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, -40, 750, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, -80, 750, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, -120, 750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, -120, 825, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, -80, 825, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, -40, 825, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, 0, 825, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 24, 900, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, 40, 825, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 80, 825, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 8, 1875, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 8, 1875, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 8, 1750, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 24, 1462, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 24, 1462, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, 24, 1462, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 24, 1462, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -16, 40, 1231, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 8, 40, 1231, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, 40, 1231, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 56, 40, 1231, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, 24, 1550, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 24, 1550, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, 0, 1600, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, 0, 1600, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, -16, 1600, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
};

/// View 2's three sprite ranges covering 37 sources, after a background-strip selector.
static SpriteBatch _gDryfieldNightMotelRoom4View2SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 1, 0 } },
    { 21, 11, 0, 0, { 2, 0 } },
    { 32, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's screen-space sprite rectangles and unscaled sorting depths.
static SpriteSource _gDryfieldNightMotelRoom4View3SpriteSources[83] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 88, 587, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -160, 96, 475, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -120, 96, 475, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -80, 96, 475, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -40, 96, 475, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 0, 96, 475, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 40, 96, 475, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, 96, 475, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 104, 112, 475, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, 80, 587, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 64, 600, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -120, 80, 587, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -80, 80, 587, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -40, 80, 587, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 0, 80, 587, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 40, 80, 587, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 72, 575, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 72, 575, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 40, 72, 575, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 104, 800, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -152, 104, 775, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -160, 80, 775, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -160, 56, 775, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -104, 80, 800, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -104, 56, 800, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -104, 40, 800, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -144, 40, 825, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -144, 16, 825, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 40, 1125, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 88, 1037, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -136, 64, 1037, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -96, 64, 1037, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -56, 64, 1037, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -16, 64, 956, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, 64, 956, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -136, 48, 1112, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -96, 48, 1112, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -56, 48, 1112, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -16, 48, 1006, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, 48, 1006, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -136, 40, 1175, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 32, 1200, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -96, 40, 1175, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -56, 40, 1175, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -16, 40, 1175, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 32, 1200, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -80, 1220, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 128, -72, 825, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 128, -40, 825, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 128, 0, 825, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 128, 40, 825, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 128, 80, 825, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 128, -112, 825, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 104, -104, 937, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 104, -72, 937, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, -40, 887, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 0, 887, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 40, 887, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 80, 887, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, -88, 1112, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, -72, 1112, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -40, 1125, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 0, 1125, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 40, 1125, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 40, 1225, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 0, 1225, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, -40, 1225, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -72, 1225, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, -72, 1400, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -40, 1400, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 0, 1400, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 40, 1400, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -112, 812, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -72, 812, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -32, 812, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 8, 812, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 48, 812, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 88, 812, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 40, 8, 1325, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 32, 32, 1325, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 88, 24, 1250, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 88, 40, 1237, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 40, 1237, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

/// View 3's seven sprite ranges covering 83 sources, after a background-strip selector.
static SpriteBatch _gDryfieldNightMotelRoom4View3SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 9, 0, 0, { 4, 0 } },
    { 28, 18, 0, 0, { 1, 0 } },
    { 46, 26, 0, 0, { 5, 0 } },
    { 72, 6, 0, 0, { 0, 0 } },
    { 78, 2, 0, 0, { 6, 0 } },
    { 80, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's screen-space sprite rectangles and unscaled sorting depths.
static SpriteSource _gDryfieldNightMotelRoom4View4SpriteSources[45] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -88, 1375, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -48, 1375, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -8, 1375, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, 32, 1375, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -72, -48, 1093, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -72, -8, 1093, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -72, 32, 1093, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 72, 1093, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, 72, 893, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, 32, 893, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, -8, 893, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, -48, 893, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -72, -88, 1093, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, -88, 893, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, -120, 925, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, -104, 1125, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, -120, 725, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, -88, 694, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, -48, 694, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, -8, 695, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, 32, 694, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 72, 695, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, 72, 672, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -120, 32, 647, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -120, -8, 656, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -120, -48, 656, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -120, -88, 656, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -120, 671, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -136, -120, 625, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -88, 640, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -48, 640, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -8, 641, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, 32, 641, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -128, 72, 640, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -136, 72, 608, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, 32, 612, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, -8, 612, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, -48, 611, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, -88, 611, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -120, 607, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -88, 608, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -48, 609, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -8, 611, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 32, 612, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 72, 613, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

/// View 4's whole-source sprite range, after a background-strip selector.
static SpriteBatch _gDryfieldNightMotelRoom4View4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightMotelRoom4View4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's screen-space sprite rectangles and unscaled sorting depths.
static SpriteSource _gDryfieldNightMotelRoom4View5SpriteSources[88] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 16, 32, 850, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -120, 850, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 120, -80, 850, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -40, 850, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 120, 0, 850, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -120, 850, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -80, 850, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -40, 850, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, 80, 0, 850, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, 40, 0, 850, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 40, -40, 850, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, -80, 850, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, -120, 850, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 16, -120, 850, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 16, -80, 850, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 8, -40, 850, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, 8, 0, 850, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -104, -120, 850, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -96, -88, 850, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -96, -48, 850, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -88, -16, 850, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -80, 24, 850, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, 80, 731, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 405, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 405, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 405, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 405, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 405, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 405, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 80, 556, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 40, 556, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 0, 556, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -40, 556, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -80, 556, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -120, 556, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, -120, 662, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, -80, 662, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, -40, 662, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, 0, 662, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -96, 40, 662, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -96, 80, 662, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, 40, 731, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, 0, 731, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, -40, 731, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, -80, -64, 731, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 550, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, -120, 550, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 550, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, -80, 550, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 550, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, -40, 550, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -96, -40, 550, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 550, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, 0, 550, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, 0, 550, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 550, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -120, 40, 550, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, 40, 550, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -144, 80, 550, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 80, 550, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, 80, 550, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, -72, 96, 396, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, 48, 529, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 72, 369, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -112, 80, 390, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -112, 56, 369, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, -72, 56, 498, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 40, 336, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -112, -120, 637, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 40 } }, -104, -80, 637, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -96, -40, 637, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -88, 0, 637, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, -80, 40, 637, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -72, 40, 681, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -72, 80, 681, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -72, 0, 681, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -48, 24, 700, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -72, -40, 681, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -72, -80, 681, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -72, -120, 681, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, 8, 825, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 48, 56, 775, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, 48, 32, 775, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 32, 32, 775, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 16, 825, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 48, 16, 825, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 80, 32, 775, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, 16, 825, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
};

/// View 5's six sprite ranges covering 88 sources, after a background-strip selector.
static SpriteBatch _gDryfieldNightMotelRoom4View5SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 3, 0 } },
    { 22, 23, 0, 0, { 0, 0 } },
    { 45, 17, 0, 0, { 5, 0 } },
    { 62, 6, 0, 0, { 1, 0 } },
    { 68, 12, 0, 0, { 4, 0 } },
    { 80, 8, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's screen-space sprite rectangles and unscaled sorting depths.
static SpriteSource _gDryfieldNightMotelRoom4View6SpriteSources[50] = {
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 500, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 500, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 500, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 500, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 500, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, 40, 500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, 80, 500, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, 40, 500, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, 80, 500, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, 0, 500, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, 0, 500, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, -40, 500, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, -40, 500, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, -80, 500, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, -80, 500, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, -120, 500, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, -120, 500, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 80, 500, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 40, 500, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 0, 500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -40, 500, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -80, 500, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -120, 500, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -120, 500, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -80, 500, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -40, 500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, 0, 500, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, 40, 500, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, 80, 500, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 48, -112, 850, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 48, -72, 850, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 48, -32, 850, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 48, 8, 850, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 48, 48, 850, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 56, 16, 750, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 56, 24, 750, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 64, 40, 750, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, -120, 512, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, -80, 512, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, -40, 512, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, 0, 512, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, 40, 512, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, 80, 380, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, 64, 637, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, 80, 625, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -80, 96, 637, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -112, 64, 637, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -112, 80, 637, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -112, 96, 637, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
};

/// View 6's five sprite ranges covering 50 sources, after a background-strip selector.
static SpriteBatch _gDryfieldNightMotelRoom4View6SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 0, 0 } },
    { 30, 5, 0, 0, { 3, 0 } },
    { 35, 3, 0, 0, { 2, 0 } },
    { 38, 6, 0, 0, { 4, 0 } },
    { 44, 6, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightMotelRoom4SpriteViews[6] = {
    { { .empty = _gDryfieldNightMotelRoom4View1SpriteBatches }, _gDryfieldNightMotelRoom4View1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom4View2SpriteSources }, _gDryfieldNightMotelRoom4View2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom4View3SpriteSources }, _gDryfieldNightMotelRoom4View3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom4View4SpriteSources }, _gDryfieldNightMotelRoom4View4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom4View5SpriteSources }, _gDryfieldNightMotelRoom4View5SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom4View6SpriteSources }, _gDryfieldNightMotelRoom4View6SpriteBatches, NULL },
};

/// Eight directed view boundaries connecting views 2/3, 2/4, 3/5 and 5/6.
///
/// Each pair has opposing source/destination views. Setup binds the quads to
/// the active view and writes their links and enable flags; the last included
/// record carries `WORLD_COLLISION_TRIGGER_LAST`. Positions and radii use game
/// units, normals use `ONE` per unit. Keep the room loaded until unlinked.
static WorldCollisionTrigger _gDryfieldNightMotelRoom4ViewBoundaryTriggers[8] = {
    { .origin = { 1872, -1136, 2736, 0 }, .vertices = { { -1808, -2160, 16, 0 }, { 1808, -2160, -16, 0 }, { -1808, 2160, 16, 0 }, { 1808, 2160, -16, 0 } }, .normal = { -37, 0, -4105, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2816, .control = 0, .parameter0 = 2, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { 1888, -1088, 2656, 0 }, .vertices = { { 1808, -2112, -16, 0 }, { -1808, -2112, 16, 0 }, { 1808, 2112, -16, 0 }, { -1808, 2112, 16, 0 } }, .normal = { 36, 0, ONE, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2769, .control = 0, .parameter0 = 3, .parameter1 = 2, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { 4511, -1152, 1519, 0 }, .vertices = { { -17, -2176, -1316, 0 }, { 14, -2176, 1313, 0 }, { -17, 2176, -1316, 0 }, { 14, 2176, 1313, 0 } }, .normal = { 4095, 0, -49, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2534, .control = 0, .parameter0 = 4, .parameter1 = 2, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { 4639, -1120, 1471, 0 }, .vertices = { { 15, -2144, 1314, 0 }, { -16, -2144, -1315, 0 }, { 15, 2144, 1314, 0 }, { -16, 2144, -1315, 0 } }, .normal = { -4107, 0, 47, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2508, .control = 0, .parameter0 = 2, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { 3647, -1120, 3502, 0 }, .vertices = { { 0, -2144, -738, 0 }, { 0, -2144, 738, 0 }, { 0, 2144, -738, 0 }, { 0, 2144, 738, 0 } }, .normal = { 4105, 0, 0, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2260, .control = 0, .parameter0 = 5, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { 3776, -1136, 3488, 0 }, .vertices = { { 0, -2160, 738, 0 }, { 0, -2160, -738, 0 }, { 0, 2160, 738, 0 }, { 0, 2160, -738, 0 } }, .normal = { -4109, 0, 0, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2275, .control = 0, .parameter0 = 3, .parameter1 = 5, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { 5408, -1088, 3920, 0 }, .vertices = { { 96, -2144, -1074, 0 }, { -96, -2144, 1074, 0 }, { 96, 2144, -1074, 0 }, { -96, 2144, 1074, 0 } }, .normal = { 4098, 0, 366, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2387, .control = 0, .parameter0 = 6, .parameter1 = 5, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { 5536, -1152, 3904, 0 }, .vertices = { { -96, -2144, 1074, 0 }, { 96, -2144, -1074, 0 }, { -96, 2144, 1074, 0 }, { 96, 2144, -1074, 0 } }, .normal = { -4101, 0, -367, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2387, .control = 0, .parameter0 = 5, .parameter1 = 6, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST },
};

/// The main-street door warp and six facing-gated CAP interaction quads.
///
/// The door uses this area's departure slot 1 and main street's arrival slot 6.
/// Other quads select CAP commands 1, 4, 5, 7, 6 and 8 with default playback
/// flags. Setup writes coordinate/list state; the final quad carries LAST.
/// Positions and radii use game units, normals use `ONE` per unit. The room
/// overlay must remain loaded while the quads are linked.
static WorldCollisionTrigger _gDryfieldNightMotelRoom4ActionTriggers[7] = {
    { .origin = { 3136, -57, 512, 0 }, .vertices = { { -832, 0, -320, 0 }, { 832, 0, -320, 0 }, { -832, 0, 320, 0 }, { 832, 0, 320, 0 } }, .normal = { 0, ONE, 0, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 891, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP, .parameter0 = GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, .parameter1 = (DRYFIELD_NIGHT_MOTEL_ROOM_4_DOOR_WARP << 4) | DRYFIELD_NIGHT_MOTEL_ROOM_4_MAIN_STREET_ARRIVAL_WARP, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 5824, -64, 1984, 0 }, .vertices = { { -544, 0, -544, 0 }, { 512, 0, -544, 0 }, { -480, 0, 544, 0 }, { 512, 0, 544, 0 } }, .normal = { 0, 4110, 0, 0 }, .facingNormal = { -ONE, 0, 0, 0 }, .radius = 768, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 1, .parameter1 = CAP_EVENT_NO_FLAGS, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 5680, -64, 768, 0 }, .vertices = { { -464, 0, -544, 0 }, { 464, 0, -544, 0 }, { -464, 0, 544, 0 }, { 464, 0, 544, 0 } }, .normal = { 0, 4099, 0, 0 }, .facingNormal = { -ONE, 0, 0, 0 }, .radius = 712, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 4, .parameter1 = CAP_EVENT_NO_FLAGS, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 2912, -64, 3680, 0 }, .vertices = { { -576, 0, -320, 0 }, { 576, 0, -96, 0 }, { -576, 0, 96, 0 }, { 576, 0, 320, 0 } }, .normal = { 0, 4104, 0, 0 }, .facingNormal = { 0, 0, -ONE, 0 }, .radius = 658, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 5, .parameter1 = CAP_EVENT_NO_FLAGS, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 4336, -64, 3840, 0 }, .vertices = { { -448, 0, -272, 0 }, { 448, 0, -144, 0 }, { -448, 0, 144, 0 }, { 448, 0, 272, 0 } }, .normal = { 0, 4102, 0, 0 }, .facingNormal = { 201, 0, -4091, 0 }, .radius = 523, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 7, .parameter1 = CAP_EVENT_NO_FLAGS, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 4352, -64, 3328, 0 }, .vertices = { { -448, 0, -272, 0 }, { 448, 0, -144, 0 }, { -448, 0, 144, 0 }, { 448, 0, 272, 0 } }, .normal = { 0, 4102, 0, 0 }, .facingNormal = { -201, 0, 4091, 0 }, .radius = 523, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 6, .parameter1 = CAP_EVENT_NO_FLAGS, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 4624, -64, 2320, 0 }, .vertices = { { -672, 0, -224, 0 }, { 672, 0, -224, 0 }, { -672, 0, 224, 0 }, { 672, 0, 224, 0 } }, .normal = { 0, 4105, 0, 0 }, .facingNormal = { 0, 0, -ONE, 0 }, .radius = 706, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 8, .parameter1 = CAP_EVENT_NO_FLAGS, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST },
};

/// Two room-space sight-blocking quads for Motel Room 4.
///
/// Setup writes links and enable flags through the final LAST-marked quad;
/// keep the overlay loaded until unlinked. Positions/radii use game units and
/// normals use `ONE` per unit. The authored low flag bit is retained without
/// a proven interpretation; the sight test does not consume the radii.
static WorldCollisionOccluder _gDryfieldNightMotelRoom4SightOccluders[2] = {
    { .origin = { 5200, -1520, 2656, 0 }, .vertices = { { -1520, 2064, 0, 0 }, { 1520, 2064, 0, 0 }, { -1520, -2064, 0, 0 }, { 1520, -2064, 0, 0 } }, .normal = { 0, 0, 4097, 0 }, .radius = 2560, .flags = 1 },
    { .origin = { 3712, -1568, 4800, 0 }, .vertices = { { 0, 2064, 720, 0 }, { 0, 2064, -720, 0 }, { 0, -2064, 720, 0 }, { 0, -2064, -720, 0 } }, .normal = { 4103, 0, 0, 0 }, .radius = 2172, .flags = 1 | WORLD_COLLISION_OCCLUDER_LAST },
};

/// Four authored point lights contributing in every view, with world-unit falloff radii.
///
/// Room setup parents and composes their transforms; lighting queries overwrite
/// attenuation. RGB intensities have 12 fractional bits.
static WorldCoordPointLight _gDryfieldNightMotelRoom4PointLights[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 445, -1064, 3258 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 6144, 5816, 5652 }, { 0, 0 } }, 527, 1437 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5010, -1967, 3187 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2400, 2800, 3200 }, { 0, 0 } }, 690, 3545 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2512, -1113, 1911 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2900, 2800, 2700 }, { 0, 0 } }, 1140, 3771 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2510, -1531, 2390 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1100, 1200, 1200 }, { 0, 0 } }, 572, 5081 },
};

/// The room's authored cone light, contributing in every view.
///
/// Placement and falloff radii use world units; the axis uses 4096 per unit
/// and the full opening angle uses 4096 per turn. Transform and attenuation
/// remain writable for room setup and lighting queries.
static WorldCoordSpotLight _gDryfieldNightMotelRoom4ConeLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2512, -2274, 2143 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1311, 1269, 1208 }, { 0, 0 } }, { -238, 4082, -226, 0 }, 2320, 3500, 921 },
};

/// The room's four point lights and one cone light, without directional lights.
static WorldCoordRoomLights _gDryfieldNightMotelRoom4RoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightMotelRoom4PointLights),
    .pointLights           = _gDryfieldNightMotelRoom4PointLights,
    .coneLightCount        = ARRAY_SIZE(_gDryfieldNightMotelRoom4ConeLights),
    .coneLights            = _gDryfieldNightMotelRoom4ConeLights,
};

/// Placement-entry keys used by this room's resource recipes.
enum {
    DRYFIELD_NIGHT_MOTEL_ROOM_4_ODD_STRANGER_RESOURCE_ENTRY  = 18,
    DRYFIELD_NIGHT_MOTEL_ROOM_4_MOTH_RESOURCE_ENTRY          = 8,
    DRYFIELD_NIGHT_MOTEL_ROOM_4_BLOOD_SUCKLER_RESOURCE_ENTRY = 40,
};

/// Odd Stranger recipe for night Motel Room 4 variant 1.
///
/// Two placements use entry 18, base-group-40 file 18 and descriptor 0.
/// The final record is `AREA_PLACEMENT_END`. Keep this room, its map-owned
/// placements and the selected actor overlays loaded during loading, spawning
/// and model-flag updates.
static AreaResource _gDryfieldNightMotelRoom4Variant1Resources[2] = {
    { .entryId = DRYFIELD_NIGHT_MOTEL_ROOM_4_ODD_STRANGER_RESOURCE_ENTRY, .fileNumber = 18, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor401800OddStrangerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Moth and Blood Suckler recipes for night Motel Room 4 variant 2.
///
/// Three Moths use entry 8, base-group-10 file 7; four Blood Sucklers use
/// entry 40, base-group-20 file 40. Both spawn descriptor 0.
/// The final record is `AREA_PLACEMENT_END`. Keep this room, its map-owned
/// placements and the selected actor overlays loaded during loading, spawning
/// and model-flag updates.
static AreaResource _gDryfieldNightMotelRoom4Variant2Resources[3] = {
    { .entryId = DRYFIELD_NIGHT_MOTEL_ROOM_4_MOTH_RESOURCE_ENTRY, .fileNumber = 7, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor00700MothTaskDesc },
    { .entryId = DRYFIELD_NIGHT_MOTEL_ROOM_4_BLOOD_SUCKLER_RESOURCE_ENTRY, .fileNumber = 40, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = &gActor204000BloodSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldNightMotelRoom4AreaVariants[13] = {
    [0] = { NULL, NULL },
    [1] = { .placements = gMapDryfieldFullMotelRoom4Variant1Placements, .resources = _gDryfieldNightMotelRoom4Variant1Resources },
    [2] = { .placements = gMapDryfieldFullMotelRoom4Variant2Placements, .resources = _gDryfieldNightMotelRoom4Variant2Resources },
};

/// Cue-2 footstep bases in the currently loaded type-1 sound bank.
///
/// Cue 1 adds one; companion playback adds 100. Scripted jumps reuse walking.
enum {
    DRYFIELD_NIGHT_MOTEL_ROOM_4_SURFACE_1_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_NIGHT_MOTEL_ROOM_4_SURFACE_1_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_NIGHT_MOTEL_ROOM_4_SURFACE_2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x01,
    DRYFIELD_NIGHT_MOTEL_ROOM_4_SURFACE_2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x03,
};

/// Surface class 1's footstep cue bases; scripted jumps reuse the walking pair.
static WorldCollisionFootstepSounds _gDryfieldNightMotelRoom4Surface1FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_MOTEL_ROOM_4_SURFACE_1_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_MOTEL_ROOM_4_SURFACE_1_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_MOTEL_ROOM_4_SURFACE_1_WALK_SOUND,
};

/// Surface class 2's footstep cue bases; scripted jumps reuse the walking pair.
static WorldCollisionFootstepSounds _gDryfieldNightMotelRoom4Surface2FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_MOTEL_ROOM_4_SURFACE_2_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_MOTEL_ROOM_4_SURFACE_2_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_MOTEL_ROOM_4_SURFACE_2_WALK_SOUND,
};

/// Blocking policy without footstep cues, shared by surface classes 0 and 4..7.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom4SilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface class 1 blocks probes, allows weapon impacts and applies pushback with its footstep pair.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom4Surface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelRoom4Surface1FootstepSounds,
};

/// Surface class 2 blocks probes, allows weapon impacts and applies pushback with its footstep pair.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom4Surface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelRoom4Surface2FootstepSounds,
};

/// Surface class 3 passes probes and ignores weapon impacts, with pushback and no footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom4Surface3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gDryfieldNightMotelRoom4SurfaceProperties[8] = {
    &_gDryfieldNightMotelRoom4SilentSurfaceProperties,
    &_gDryfieldNightMotelRoom4Surface1Properties,
    &_gDryfieldNightMotelRoom4Surface2Properties,
    &_gDryfieldNightMotelRoom4Surface3Properties,
    &_gDryfieldNightMotelRoom4SilentSurfaceProperties,
    &_gDryfieldNightMotelRoom4SilentSurfaceProperties,
    &_gDryfieldNightMotelRoom4SilentSurfaceProperties,
    &_gDryfieldNightMotelRoom4SilentSurfaceProperties,
};

/// Refuses every key-item use request in Dryfield Night Motel Room 4.
///
/// Returns 0 so the inventory shows its "No use now" notice. All arguments
/// are ignored; no item is consumed and no room action starts.
static s32 _dryfieldNightMotelRoom4RejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

#include "../../shared/room/room_variants_main_street.inc.c"

/// Ignores room commands from CAP playback and direction triggers.
///
/// All arguments are ignored and no room state changes. Returns 0;
/// the senders discard the result.
static s32 _dryfieldNightMotelRoom4IgnoreCommand(Task* unusedTask, s32 messageId, s32 commandId, s32 commandMode)
{
    return 0;
}

/// Ignores room-action requests from direction triggers.
///
/// The request is borrowed during synchronous dispatch but never accessed or
/// retained. Returns 0; the sender discards the result. All arguments are ignored.
static s32 _dryfieldNightMotelRoom4IgnoreActionRequest(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the room task and its message handlers, then enters its idle state.
///
/// Called in state 0 with a live task. The loaded room's table is borrowed
/// for subsequent synchronous messages while the registered task remains alive.
static void _dryfieldNightMotelRoom4InitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldNightMotelRoom4RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the registered room task idle between message dispatches.
///
/// Leaves the task and its state unchanged, keeping its message table
/// available without per-frame room work.
static void _dryfieldNightMotelRoom4IdleRoomTask(Task* unusedTask)
{
}

/// Motel Room 4 task handlers indexed by state 0..2.
///
/// State 0 registers the room and its message table, state 1 stays idle, and
/// state 2 releases the task. The dispatcher copies all three handlers before
/// selecting a state.
static const TaskFuncTable3 _gDryfieldNightMotelRoom4RoomStates = {
    .funcs = { _dryfieldNightMotelRoom4InitRoomTask, _dryfieldNightMotelRoom4IdleRoomTask, taskKill },
};

void dryfieldNightMotelRoom4RoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gDryfieldNightMotelRoom4RoomStates;
    states.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_flare_clipped.inc.c"

void dryfieldNightMotelRoom4DrawFlaresTask(Task* unusedTask)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_TEXTURE_PRIMARY   = 1,
        DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_TEXTURE_SECONDARY = 2,
        // Pixel half-extent is this scale * 39 / (camera Z / 4).
        DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_RADIUS_SCALE_STANDARD = 0x200,
        DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_RADIUS_SCALE_LARGE    = 0x240,
        DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_RADIUS_SCALE_SMALL    = 0x180,
    };

    // Keep each view's draw order; even a depth-rejected flare reserves a packet.
    switch (gGameSession->location.view) {
        case 2:
        case 3: {
            glowDrawFlareClipped(&_gDryfieldNightMotelRoom4FlarePoints[0], DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_TEXTURE_PRIMARY, DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_RADIUS_SCALE_STANDARD);
            glowDrawFlareClipped(&_gDryfieldNightMotelRoom4FlarePoints[1], DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_TEXTURE_PRIMARY, DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_RADIUS_SCALE_LARGE);
            break;
        }
        case 4: {
            glowDrawFlareClipped(&_gDryfieldNightMotelRoom4View4FlarePoint, DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_TEXTURE_PRIMARY, DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_RADIUS_SCALE_STANDARD);
            break;
        }
        case 5: {
            glowDrawFlareClipped(&_gDryfieldNightMotelRoom4FlarePoints[0], DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_TEXTURE_PRIMARY, DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_RADIUS_SCALE_STANDARD);
            glowDrawFlareClipped(&_gDryfieldNightMotelRoom4FlarePoints[2], DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_TEXTURE_SECONDARY, DRYFIELD_NIGHT_MOTEL_ROOM_4_FLARE_RADIUS_SCALE_SMALL);
            break;
        }
    }
}
