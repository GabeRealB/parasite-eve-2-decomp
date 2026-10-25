#include "rooms/dryfield_night_warehouse.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_01600.h"
#include "actors/actor_02500.h"
#include "actors/actor_04000.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"

static void _dryfieldNightWarehouseInitRoomTask(Task* task);
static void _dryfieldNightWarehouseRoomIdle(Task* task);
static void _dryfieldNightWarehouseDrawLightBeam(const GfxCoord* coord, s16 firstRing, s16 segmentCount);

/// Ring centres in the space of the coordinate drawn under, one per circle.
extern SVECTOR gGlowPrismCorners[];

static s32 _dryfieldNightWarehouseRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldNightWarehouseResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldNightWarehouseIgnoreCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _dryfieldNightWarehouseIgnoreDirectionAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static WorldCollisionGrid         _gDryfieldNightWarehouseCollisionGrid;
static WorldCollisionTrigger      _gDryfieldNightWarehouseViewBoundaryTriggers[4];
static WorldCollisionTrigger      _gDryfieldNightWarehouseActionTriggers[10];
static WorldCoordRoomAmbientEntry _gDryfieldNightWarehouseAmbientByView[5];
static WorldCoordRoomLights       _gDryfieldNightWarehouseRoomLights;

/// Texture page, palette row and neutral modulation shared by the view sprites.
enum {
    DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE         = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT          = getClut(0, 255),
    DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR = 128,
};

/// Placement keys and decimal file numbers for the warehouse's enemy resources.
enum {
    DRYFIELD_NIGHT_WAREHOUSE_SCORPION_ENTRY_ID         = 25,
    DRYFIELD_NIGHT_WAREHOUSE_SCORPION_FILE_NUMBER      = 25,
    DRYFIELD_NIGHT_WAREHOUSE_BLOOD_SUCKLER_ENTRY_ID    = 40,
    DRYFIELD_NIGHT_WAREHOUSE_BLOOD_SUCKLER_FILE_NUMBER = 40,
    DRYFIELD_NIGHT_WAREHOUSE_SCAVENGER_ENTRY_ID        = 16,
    DRYFIELD_NIGHT_WAREHOUSE_SCAVENGER_FILE_NUMBER     = 16,
};

/// Cue-2 footstep bases in the loaded type-1 sound bank; cue 1 adds one.
enum {
    DRYFIELD_NIGHT_WAREHOUSE_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x45,
    DRYFIELD_NIGHT_WAREHOUSE_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x47,
};

/// Night warehouse transition, key-item, direction-action and command handlers.
///
/// Borrowed by the live room task; keep this overlay loaded while it receives
/// messages. The final end marker makes unsupported IDs return zero.
static const TaskMessageEntry _gDryfieldNightWarehouseRoomMessages[5] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldNightWarehouseResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightWarehouseRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightWarehouseIgnoreDirectionAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightWarehouseIgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
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

/// Local XZ radii for four consecutive lit/dark beam-ring pairs, in game units.
///
/// Entries 0..7 correspond to the first eight `gGlowPrismCorners` positions.
static s16 _gDryfieldNightWarehouseBeamRadii[8] = {
    100,
    150,
    125,
    175,
    135,
    185,
    100,
    150,
};

WorldCoordRoomLighting gDryfieldNightWarehouseRoomLighting[3] = {
    { &_gDryfieldNightWarehouseRoomLights, _gDryfieldNightWarehouseAmbientByView },
    { &_gDryfieldNightWarehouseRoomLights, _gDryfieldNightWarehouseAmbientByView },
    { &_gDryfieldNightWarehouseRoomLights, _gDryfieldNightWarehouseAmbientByView },
};

WorldCollisionRoomResources gDryfieldNightWarehouseRoomCollisionResources[3] = {
    {
        .grid                 = &_gDryfieldNightWarehouseCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightWarehouseViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightWarehouseActionTriggers,
        .occluders            = NULL,
    },
    {
        .grid                 = &_gDryfieldNightWarehouseCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightWarehouseViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightWarehouseActionTriggers,
        .occluders            = NULL,
    },
    {
        .grid                 = &_gDryfieldNightWarehouseCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightWarehouseViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightWarehouseActionTriggers,
        .occluders            = NULL,
    },
};

u8* gDryfieldNightWarehouseViewMaps[3] = {
    gViewIdentityMap,
    gViewIdentityMap,
    gViewIdentityMap,
};

ViewCount gDryfieldNightWarehouseRoomViewCounts[3] = {
    ARRAY_SIZE(gDryfieldNightWarehouseSpriteViews),
    ARRAY_SIZE(gDryfieldNightWarehouseSpriteViews),
    ARRAY_SIZE(gDryfieldNightWarehouseSpriteViews),
};

// Optional door map-marker nibble IDs stored by these endpoints.
enum {
    DRYFIELD_NIGHT_WAREHOUSE_BACK_STREET_MAP_FLAG = 468,
};

DirectionWarpEntry gDryfieldNightWarehouseWarpEntries[2] = {
    {
        .player         = { .yaw = { .word = 0 }, .x = 2631, .y = 0, .z = -3535 },
        .unknown_10     = { 0 },
        .companion      = { .yaw = { .word = (ACTOR_TRANSFORM_ANGLE_TURN / 4) }, .x = 1721, .y = 0, .z = -2640 },
        .unknown_24     = { 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_WAREHOUSE_BACK_STREET_MAP_FLAG,
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

/// Room-space collision face normals, with `ONE` representing unit length.
static SVECTOR _gDryfieldNightWarehouseCollisionGridNormals[10] = {
#include "assets/dryfield_night_warehouse_collision_01948_normals.inc"
};

/// Warehouse collision vertices in signed room-coordinate game units.
static SVECTOR _gDryfieldNightWarehouseCollisionGridVerts[74] = {
#include "assets/dryfield_night_warehouse_collision_01948_verts.inc"
};

/// Collision quads indexing the vertex/normal pools and surface classes 4..6.
static WorldCollisionGridFace _gDryfieldNightWarehouseCollisionGridFaces[39] = {
#include "assets/dryfield_night_warehouse_collision_01948_faces.inc"
};

/// Four packed signed face-index lists, each ending at -1; retained zeros separate lists.
static s16 _gDryfieldNightWarehouseCollisionGridCells[104] = {
#include "assets/dryfield_night_warehouse_collision_01948_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightWarehouseCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..103 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_WAREHOUSE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightWarehouseCollisionGridCells[(halfwordOffset)])
/// X-major cell pointers for the warehouse's 2-by-2 collision grid.
static s16* _gDryfieldNightWarehouseCollisionGridTable[4] = {
#include "assets/dryfield_night_warehouse_collision_01948_table.inc"
};
#undef DRYFIELD_NIGHT_WAREHOUSE_CELL_FACE_LIST_AT_OFFSET

/// Shared collision mesh for room slots 1..3, bound to the view while active.
///
/// Its 4000-unit XZ cells start at room X=0, Z=-4100 and borrow the pools above.
static WorldCollisionGrid _gDryfieldNightWarehouseCollisionGrid = {
    NULL,
    _gDryfieldNightWarehouseCollisionGridNormals,
    _gDryfieldNightWarehouseCollisionGridVerts,
    _gDryfieldNightWarehouseCollisionGridFaces,
    _gDryfieldNightWarehouseCollisionGridTable,
    0,
    4100,
    2,
    2,
    4000,
    ARRAY_SIZE(_gDryfieldNightWarehouseCollisionGridFaces),
};

ViewCamera gDryfieldNightWarehouseViewCameras[8] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -3000, 7700, 2000 } }, 230 },
    { { { { -981, 0, 3976 }, { -168, 4092, -41 }, { -3972, -173, -981 } }, { -5338, 1066, 1266 } }, 207 },
    { { { { -832, 0, -4010 }, { -256, 4087, 53 }, { 4002, 262, -831 } }, { -288, 1366, 916 } }, 207 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
    { { { { 2311, 0, -3381 }, { -2032, 3273, -1389 }, { 2702, 2461, 1847 } }, { -5038, 1666, 2016 } }, 207 },
    { { { { -832, 0, -4010 }, { -256, 4087, 53 }, { 4002, 262, -831 } }, { -288, 1366, 916 } }, 207 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
};

/// View 1's empty sprite list: retain decoded background strips, then end.
static SpriteBatch _gDryfieldNightWarehouseView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's depth-sorted texture rectangles, in pixels relative to the view centre.
static SpriteSource _gDryfieldNightWarehouseView2SpriteSources[23] = {
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 24, 48 } }, -8, 16, 1075, { .fields = { 104, 208 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 32 } }, 16, 24, 1025, { .fields = { 56, 32 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 24, 48 } }, -112, 64, 475, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 48 } }, -88, 64, 500, { .fields = { 96, 112 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 40 } }, -144, 80, 375, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 56 } }, -128, 64, 450, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 16 } }, -160, 64, 425, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, 80, 375, { .fields = { 80, 152 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 64, 675, { .fields = { 104, 48 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 32 } }, 88, 64, 675, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 40 } }, 96, 72, 625, { .fields = { 96, 160 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 48 } }, 144, 32, 500, { .fields = { 112, 160 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 8 } }, 112, 72, 625, { .fields = { 40, 144 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 24 } }, 112, 96, 575, { .fields = { 64, 88 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 40, 40 } }, 120, 80, 350, { .fields = { 64, 200 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 16 } }, 112, 80, 625, { .fields = { 80, 40 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 40, 24 } }, 80, 40, 675, { .fields = { 40, 176 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 8 } }, 96, 64, 625, { .fields = { 40, 152 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 32 } }, 120, 32, 650, { .fields = { 72, 144 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 40 } }, 128, 32, 600, { .fields = { 80, 112 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 56, 32 } }, -160, 0, 700, { .fields = { 24, 112 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 56, 24 } }, -160, 32, 700, { .fields = { 16, 64 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 56, 56 } }, -160, 56, 700, { .fields = { 72, 56 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 2's four source ranges (2, 6, 12, 3), following a background-strip selector.
///
/// All ranges start visible and participate in cached packet processing.
static SpriteBatch _gDryfieldNightWarehouseView2SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 6, 0, 0, { 0, 0 } },
    { 8, 12, 0, 0, { 2, 0 } },
    { 20, ARRAY_SIZE(_gDryfieldNightWarehouseView2SpriteSources) - 20, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's depth-sorted texture rectangles, in pixels relative to the view centre.
static SpriteSource _gDryfieldNightWarehouseView3SpriteSources[22] = {
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 32 } }, 56, 32, 975, { .fields = { 80, 128 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 32 } }, 72, 32, 925, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 32 } }, 88, 32, 875, { .fields = { 88, 160 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 32 } }, 48, 40, 825, { .fields = { 72, 192 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 24, 32 } }, 64, 40, 837, { .fields = { 64, 160 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 40 } }, -96, 24, 775, { .fields = { 104, 128 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 48 } }, -112, 24, 575, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 16 } }, -48, 72, 600, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 24 } }, -56, 72, 595, { .fields = { 104, 168 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 40, 48 } }, -96, 72, 475, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 48 } }, -128, 72, 475, { .fields = { 80, 80 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 32 } }, -128, 40, 475, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 32 } }, -88, 24, 787, { .fields = { 80, 32 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 24, 16 } }, -72, 40, 825, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 24, 8 } }, -72, 32, 812, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 8 } }, -64, 64, 625, { .fields = { 64, 8 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 8 } }, -96, 64, 625, { .fields = { 48, 16 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 8 } }, -64, 56, 825, { .fields = { 80, 64 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 24, 8 } }, -88, 56, 750, { .fields = { 72, 72 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 8, 32 } }, 96, 32, 1125, { .fields = { 96, 128 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 16, 64 } }, 104, 16, 1050, { .fields = { 112, 80 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 32, 80 } }, 120, 0, 1125, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 3's four source ranges (2, 3, 14, 3), following a background-strip selector.
///
/// All ranges start visible and participate in cached packet processing.
static SpriteBatch _gDryfieldNightWarehouseView3SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 3, 0, 0, { 0, 0 } },
    { 5, 14, 0, 0, { 2, 0 } },
    { 19, ARRAY_SIZE(_gDryfieldNightWarehouseView3SpriteSources) - 19, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's single 48-by-40-pixel texture rectangle at view-relative (-160, 80).
static SpriteSource _gDryfieldNightWarehouseView4SpriteSources[1] = {
    { DRYFIELD_NIGHT_WAREHOUSE_SPRITE_TPAGE, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_CLUT, { .fields = { 48, 40 } }, -160, 80, 500, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WAREHOUSE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 4's background-strip selector, one visible source range and end marker.
static SpriteBatch _gDryfieldNightWarehouseView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightWarehouseView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightWarehouseSpriteViews[4] = {
    { { .empty = _gDryfieldNightWarehouseView1SpriteBatches }, _gDryfieldNightWarehouseView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightWarehouseView2SpriteSources }, _gDryfieldNightWarehouseView2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightWarehouseView3SpriteSources }, _gDryfieldNightWarehouseView3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightWarehouseView4SpriteSources }, _gDryfieldNightWarehouseView4SpriteBatches, NULL },
};

/// Six all-view point lights, with Q12 RGB intensities and game-unit falloff radii.
///
/// View binding and light queries update their coordinates and attenuation in place.
static WorldCoordPointLight _gDryfieldNightWarehousePointLights[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2731, -1953, 17 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2051, 2460, 2460 }, { 0, 0 } }, 2039, 2301 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 839, -710, -1699 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1640, 2052, 2052 }, { 0, 0 } }, 951, 1382 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3317, -270, -901 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1436, 1642, 1642 }, { 0, 0 } }, 561, 755 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3416, -158, -3078 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1130, 1230, 1230 }, { 0, 0 } }, 1163, 1602 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4169, -1528, 161 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2050, 2460, 2460 }, { 0, 0 } }, 1319, 1642 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3902, -1641, -3609 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2458, 2870, 2871 }, { 0, 0 } }, 1420, 4454 },
};

/// The six point lights shared by room slots 1..3; no directional or cone lights.
static WorldCoordRoomLights _gDryfieldNightWarehouseRoomLights = {
    0,
    NULL,
    ARRAY_SIZE(_gDryfieldNightWarehousePointLights),
    _gDryfieldNightWarehousePointLights,
    0,
    NULL,
};

/// Directed view boundaries 3->2, 2->3, 3->4 and 4->3, ending with LAST.
///
/// Shared by room slots 1..3; linking and collision scans mutate the list state.
static WorldCollisionTrigger _gDryfieldNightWarehouseViewBoundaryTriggers[4] = {
    { NULL, NULL, NULL, { 2927, -1584, -2065, 0 }, { { -487, 1744, 1952, 0 }, { 487, 1744, -1951, 0 }, { -487, -1744, 1952, 0 }, { 487, -1744, -1951, 0 } }, { 3985, 0, 994, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3072, -960, -2114, 0 }, { { 487, 1984, -1951, 0 }, { -487, 1984, 1952, 0 }, { 487, -1984, -1951, 0 }, { -487, -1984, 1952, 0 } }, { -3980, 0, -994, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4703, -1104, -1858, 0 }, { { 1097, 2128, -1688, 0 }, { -1133, 2128, 1658, 0 }, { 1097, -2128, -1688, 0 }, { -1133, -2128, 1658, 0 } }, { -3424, 0, -2283, 0 }, { 0, 0, 4096, 0 }, 2918, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4607, -1168, -1922, 0 }, { { -1133, 2192, 1655, 0 }, { 1097, 2192, -1692, 0 }, { -1133, -2192, 1655, 0 }, { 1097, -2192, -1692, 0 } }, { 3424, 0, 2281, 0 }, { 0, 0, 4096, 0 }, 2974, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Four view ambient minima after the count header, at 16 units per colour level.
///
/// Views 1, 2 and 4 have red-only minima; view 3 has equal RGB minima of 400.
static WorldCoordRoomAmbientEntry _gDryfieldNightWarehouseAmbientByView[5] = {
    { .viewCount = ARRAY_SIZE(_gDryfieldNightWarehouseAmbientByView) - 1 },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 400, 400, 400, 400 } },
    { .color = { 16, 0, 0, 6 } },
};

/// IDs stored in action records; warp pairs pack departure then arrival in four-bit nibbles.
enum {
    DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_WARP_TO_BACK_STREET       = (1 << 4) | 3,
    DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_WARP_TO_DILAPIDATED_HOUSE = (2 << 4) | 2,
    DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_CAP_COMMAND_9             = 9,
    DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_CAP_COMMAND_1             = 1,
    DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_CAP_COMMAND_10            = 10,
    DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_CAP_COMMAND_7             = 7,
    DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_CAP_COMMAND_2             = 2,
    DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_CAP_COMMAND_8             = 8,
};

/// 10 interaction quads: 2 warps, 8 CAP commands.
///
/// Room-local origins/corners and radii use game units; normals use 4096 per unit.
/// Warp bytes select destination area and departure/arrival endpoint nibbles;
/// CAP IDs index the loaded command table or route through the room-message sentinel.
/// Room binding enables all records through LAST; keep them live until unlinking.
static WorldCollisionTrigger _gDryfieldNightWarehouseActionTriggers[10] = {
    { NULL, NULL, NULL, { 2303, -62, -3664, 0 }, { { -543, 0, -208, 0 }, { 544, 0, -208, 0 }, { -543, 0, 208, 0 }, { 544, 0, 208, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 579, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_WARP_TO_BACK_STREET, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5600, -65, -864, 0 }, { { 352, 0, -671, 0 }, { 352, 0, 672, 0 }, { -352, 0, -671, 0 }, { -352, 0, 672, 0 } }, { 0, 4102, 0, 0 }, { -ONE, 0, 0, 0 }, 757, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE, DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_WARP_TO_DILAPIDATED_HOUSE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5040, -64, -2224, 0 }, { { 272, 0, -783, 0 }, { 272, 0, 784, 0 }, { -272, 0, -783, 0 }, { -272, 0, 784, 0 } }, { 0, 4095, 0, 0 }, { -ONE, 0, 0, 0 }, 829, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_CAP_COMMAND_9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 784, -64, -2592, 0 }, { { -751, 0, -208, 0 }, { 752, 0, -208, 0 }, { -751, 0, 208, 0 }, { 752, 0, 208, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -ONE, 0 }, 778, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_CAP_COMMAND_1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1504, -64, -1408, 0 }, { { -271, 0, -1296, 0 }, { 272, 0, -1296, 0 }, { -271, 0, 1296, 0 }, { 272, 0, 1296, 0 } }, { 0, 4111, 0, 0 }, { 4091, 0, 201, 0 }, 1324, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_CAP_COMMAND_1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1744, -64, -368, 0 }, { { -543, 0, -256, 0 }, { 544, 0, -256, 0 }, { -543, 0, 256, 0 }, { 544, 0, 256, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -ONE, 0 }, 600, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_CAP_COMMAND_10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4512, -64, -416, 0 }, { { -543, 0, -256, 0 }, { 544, 0, -256, 0 }, { -543, 0, 256, 0 }, { 544, 0, 256, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -ONE, 0 }, 600, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_CAP_COMMAND_10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3040, -64, -2880, 0 }, { { -351, 0, -416, 0 }, { 352, 0, -416, 0 }, { -351, 0, 416, 0 }, { 352, 0, 416, 0 } }, { 0, 4115, 0, 0 }, { -601, 0, 4052, 0 }, 543, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3008, -64, -416, 0 }, { { -1327, 0, -848, 0 }, { 1328, 0, -848, 0 }, { -1327, 0, 240, 0 }, { 1328, 0, 240, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -ONE, 0 }, 1572, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_CAP_COMMAND_2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 672, -64, -3200, 0 }, { { -703, 0, -208, 0 }, { 704, 0, -208, 0 }, { -703, 0, 208, 0 }, { 704, 0, 208, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, ONE, 0 }, 732, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_WAREHOUSE_TRIGGER_CAP_COMMAND_8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Variant 1's Scorpion resource (file 102500), followed by AREA_PLACEMENT_END.
static AreaResource _gDryfieldNightWarehouseVariant1Resources[2] = {
    { DRYFIELD_NIGHT_WAREHOUSE_SCORPION_ENTRY_ID, DRYFIELD_NIGHT_WAREHOUSE_SCORPION_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102500Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's Blood Suckler resource (file 104000), followed by AREA_PLACEMENT_END.
static AreaResource _gDryfieldNightWarehouseVariant3Resources[2] = {
    { DRYFIELD_NIGHT_WAREHOUSE_BLOOD_SUCKLER_ENTRY_ID, DRYFIELD_NIGHT_WAREHOUSE_BLOOD_SUCKLER_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor04000BloodSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 11's Scavenger (101600) and Scorpion (202500) resources, then END.
static AreaResource _gDryfieldNightWarehouseVariant11Resources[3] = {
    { DRYFIELD_NIGHT_WAREHOUSE_SCAVENGER_ENTRY_ID, DRYFIELD_NIGHT_WAREHOUSE_SCAVENGER_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor101600ScavengerTaskDesc },
    { DRYFIELD_NIGHT_WAREHOUSE_SCORPION_ENTRY_ID, DRYFIELD_NIGHT_WAREHOUSE_SCORPION_FILE_NUMBER, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor02500Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldNightWarehouseAreaVariants[DRYFIELD_NIGHT_WAREHOUSE_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapDryfieldFullWarehouseVariant1Placements, .resources = _gDryfieldNightWarehouseVariant1Resources },
    [3]                    = { .placements = gMapDryfieldFullWarehouseVariant3Placements, .resources = _gDryfieldNightWarehouseVariant3Resources },
    [11]                   = { .placements = gMapDryfieldFullWarehouseVariant11Placements, .resources = _gDryfieldNightWarehouseVariant11Resources },
};

/// Surface class 6's walk, run and scripted-jump cue bases in the loaded type-1 bank.
///
/// Cue 1 adds one; companion playback adds 100. Scripted jumps use the walk base.
static WorldCollisionFootstepSounds _gDryfieldNightWarehouseClass6FootstepSounds = {
    DRYFIELD_NIGHT_WAREHOUSE_WALK_SOUND,
    DRYFIELD_NIGHT_WAREHOUSE_RUN_SOUND,
    DRYFIELD_NIGHT_WAREHOUSE_WALK_SOUND,
};

/// Classes 0..3 and 7: block probes, allow weapon impacts and pushback, with no footsteps.
static WorldCollisionSurfaceProperties _gDryfieldNightWarehouseDefaultSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 4: block probes, allow weapon impacts and pushback, with no footsteps.
static WorldCollisionSurfaceProperties _gDryfieldNightWarehouseClass4SurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 5: pass probes, suppress weapon impacts and apply pushback, with no footsteps.
static WorldCollisionSurfaceProperties _gDryfieldNightWarehouseClass5SurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_PASS_PROBES,
    WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 6: block probes, allow weapon impacts and pushback, and sound footsteps.
static WorldCollisionSurfaceProperties _gDryfieldNightWarehouseClass6SurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldNightWarehouseClass6FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightWarehouseSurfacePropertiesByClass[8] = {
    &_gDryfieldNightWarehouseDefaultSurfaceProperties,
    &_gDryfieldNightWarehouseDefaultSurfaceProperties,
    &_gDryfieldNightWarehouseDefaultSurfaceProperties,
    &_gDryfieldNightWarehouseDefaultSurfaceProperties,
    &_gDryfieldNightWarehouseClass4SurfaceProperties,
    &_gDryfieldNightWarehouseClass5SurfaceProperties,
    &_gDryfieldNightWarehouseClass6SurfaceProperties,
    &_gDryfieldNightWarehouseDefaultSurfaceProperties,
};

/// Refuses every key-item use in the warehouse, returning 0 without consuming it.
///
/// `ROOM_MESSAGE_USE_KEY_ITEM` supplies an inventory item ID
/// and an unused second word.
static s32 _dryfieldNightWarehouseRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return 0;
}

/// Permits a room transition with the requested destination unchanged.
///
/// Borrows a complete readable request and writable eight-byte reply through
/// synchronous dispatch; they may be the same record. Copies all fields,
/// including the query choice and flag ID, and returns 1 in query and execute
/// modes. Keeps neither pointer and changes no game flags.
static s32 _dryfieldNightWarehouseResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    return 1;
}

/// Ignores every warehouse room command and returns 0.
///
/// Both command payload words are unused; no task or scene state changes.
static s32 _dryfieldNightWarehouseIgnoreCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores room actions from direction triggers and returns 0.
///
/// The borrowed action request and second payload word are never accessed.
static s32 _dryfieldNightWarehouseIgnoreDirectionAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the room task and its message handlers, then enters its idle state.
///
/// Called in state 0 with a live task. The loaded room's table is borrowed
/// for subsequent synchronous messages while the registered task remains alive.
static void _dryfieldNightWarehouseInitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldNightWarehouseRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the initialized warehouse room task idle until its state changes externally.
static void _dryfieldNightWarehouseRoomIdle(Task* task)
{
}

/// Room-task states 0 initialize, 1 remain available for messages, and 2 release the task.
///
/// Copied whole before unchecked state dispatch in 0..2; all three callback
/// addresses are borrowed from loaded code. There is no terminator.
static const TaskFuncTable3 _gDryfieldNightWarehouseRoomTaskStates = {
    { _dryfieldNightWarehouseInitRoomTask, _dryfieldNightWarehouseRoomIdle, taskKill },
};

void dryfieldNightWarehouseRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gDryfieldNightWarehouseRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_prism.inc.c"

/// Draws a rotating, pulsing light beam between two circular local-space rings.
///
/// `firstRing` selects a lit centre/radius and the following dark centre/radius;
/// callers select 0, 2, 4 or 6. Distances use local coordinate units. The
/// borrowed `coord->workm` must already map these positions into world space.
/// `segmentCount` is 1..4096 (all callers use 8); angles use 4096 units per
/// turn with a truncated integer step, so counts not dividing 4096 leave a gap.
/// World positions narrow to signed 16 bits before view projection.
///
/// Requires initialized scratch storage, view matrices, ordering table and
/// packet arena. Queues one Gouraud quad and additive blend command per segment,
/// sorted at the last corner's camera Z / 4. Does not clip or test GTE flags.
static void _dryfieldNightWarehouseDrawLightBeam(const GfxCoord* coord, s16 firstRing, s16 segmentCount)
{
    enum {
        DRYFIELD_NIGHT_WAREHOUSE_BEAM_PULSE_PHASE_SHIFT = 10,
        DRYFIELD_NIGHT_WAREHOUSE_BEAM_BRIGHTNESS_BASE   = 16,
    };

    EffectQuadCornersScratch* block;
    POLY_G4*                  prim;
    s16                       red;
    s16                       blue;
    s16                       green;
    s16                       angleStep;
    s16                       startAngle;
    s16                       brightness;
    s32                       angle;
    s32                       nextAngle;

    brightness = (rsin(gDisplayState.animFrame << DRYFIELD_NIGHT_WAREHOUSE_BEAM_PULSE_PHASE_SHIFT) >> GLOW_TRIG_SHIFT) + DRYFIELD_NIGHT_WAREHOUSE_BEAM_BRIGHTNESS_BASE;
    SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    block      = SCRATCH_STACK_CURSOR(EffectQuadCornersScratch);
    startAngle = gDisplayState.animFrame & (GLOW_FULL_TURN - 1);
    angleStep  = GLOW_FULL_TURN / segmentCount;
    gte_SetTransMatrix(&GsWSMATRIX);
    red   = brightness * 3 / 4;
    green = brightness;
    blue  = brightness;
    /// Rotates one scratch corner in place with the coordinate's composed matrix.
    ///
    /// Captures `coord` (const GfxCoord*) and `block` (EffectQuadCornersScratch*).
    /// `vertexIndex` is evaluated twice and must be a pure integer in 0..3.
    /// Loads the Q12 rotation and stores signed-halfword XYZ; translation
    /// remains for the caller. Expands to statements inside braces, and is
    /// undefined before this drawer returns.
#define DRYFIELD_NIGHT_WAREHOUSE_ROTATE_BEAM_CORNER(vertexIndex) \
    gte_SetRotMatrix(&coord->workm);                             \
    gte_ldv0(&block->vertices[(vertexIndex)]);                   \
    gte_rtv0();                                                  \
    gte_stsv(&block->vertices[(vertexIndex)]);

    // Generate each ring pair in local XZ, then rotate and translate into world space.
    for (angle = startAngle; angle < startAngle + angleStep * segmentCount; angle = nextAngle) {
        block->vertices[0].vx = gGlowPrismCorners[firstRing].vx +
                                ((rsin(angle) * _gDryfieldNightWarehouseBeamRadii[firstRing]) >> GLOW_TRIG_SHIFT);
        block->vertices[0].vy = gGlowPrismCorners[firstRing].vy;
        block->vertices[0].vz = gGlowPrismCorners[firstRing].vz +
                                ((rcos(angle) * _gDryfieldNightWarehouseBeamRadii[firstRing]) >> GLOW_TRIG_SHIFT);
        DRYFIELD_NIGHT_WAREHOUSE_ROTATE_BEAM_CORNER(0);
        block->vertices[0].vx += coord->workm.t[0];
        block->vertices[0].vy += coord->workm.t[1];
        nextAngle              = angle + angleStep;
        block->vertices[0].vz += coord->workm.t[2];

        block->vertices[1].vx = gGlowPrismCorners[firstRing].vx +
                                ((rsin(nextAngle) * _gDryfieldNightWarehouseBeamRadii[firstRing]) >> GLOW_TRIG_SHIFT);
        block->vertices[1].vy = gGlowPrismCorners[firstRing].vy;
        block->vertices[1].vz = gGlowPrismCorners[firstRing].vz +
                                ((rcos(nextAngle) * _gDryfieldNightWarehouseBeamRadii[firstRing]) >> GLOW_TRIG_SHIFT);
        DRYFIELD_NIGHT_WAREHOUSE_ROTATE_BEAM_CORNER(1);
        block->vertices[1].vx += coord->workm.t[0];
        block->vertices[1].vy += coord->workm.t[1];
        block->vertices[1].vz += coord->workm.t[2];

        block->vertices[2].vx = gGlowPrismCorners[firstRing + 1].vx +
                                ((rsin(angle) * _gDryfieldNightWarehouseBeamRadii[firstRing + 1]) >> GLOW_TRIG_SHIFT);
        block->vertices[2].vy = gGlowPrismCorners[firstRing + 1].vy;
        block->vertices[2].vz = gGlowPrismCorners[firstRing + 1].vz +
                                ((rcos(angle) * _gDryfieldNightWarehouseBeamRadii[firstRing + 1]) >> GLOW_TRIG_SHIFT);
        DRYFIELD_NIGHT_WAREHOUSE_ROTATE_BEAM_CORNER(2);
        block->vertices[2].vx += coord->workm.t[0];
        block->vertices[2].vy += coord->workm.t[1];
        block->vertices[2].vz += coord->workm.t[2];

        block->vertices[3].vx = gGlowPrismCorners[firstRing + 1].vx +
                                ((rsin(nextAngle) * _gDryfieldNightWarehouseBeamRadii[firstRing + 1]) >> GLOW_TRIG_SHIFT);
        block->vertices[3].vy = gGlowPrismCorners[firstRing + 1].vy;
        block->vertices[3].vz = gGlowPrismCorners[firstRing + 1].vz +
                                ((rcos(nextAngle) * _gDryfieldNightWarehouseBeamRadii[firstRing + 1]) >> GLOW_TRIG_SHIFT);
        DRYFIELD_NIGHT_WAREHOUSE_ROTATE_BEAM_CORNER(3);
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
        setRGB0(prim, red, green, blue);
        setRGB1(prim, red, green, blue);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
    }
#undef DRYFIELD_NIGHT_WAREHOUSE_ROTATE_BEAM_CORNER
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
}

void dryfieldNightWarehouseDrawGlowsTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_WAREHOUSE_BEAM_SEGMENTS               = 8,
        DRYFIELD_NIGHT_WAREHOUSE_FIRST_BEAM_RING             = 0,
        DRYFIELD_NIGHT_WAREHOUSE_SECOND_BEAM_RING            = 2,
        DRYFIELD_NIGHT_WAREHOUSE_THIRD_BEAM_RING             = 4,
        DRYFIELD_NIGHT_WAREHOUSE_FOURTH_BEAM_RING            = 6,
        DRYFIELD_NIGHT_WAREHOUSE_PRISM_FIRST_CORNER          = 8,
        DRYFIELD_NIGHT_WAREHOUSE_FIRST_BEAM_VIEWS            = 1 << 2,
        DRYFIELD_NIGHT_WAREHOUSE_PRISM_AND_SECOND_BEAM_VIEWS = (1 << 2) | (1 << 3) | (1 << 6) | (1 << 9),
        DRYFIELD_NIGHT_WAREHOUSE_LAST_BEAMS_VIEWS            = (1 << 2) | (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9),
    };

    GfxCoord* coord;
    s32       viewMask;

    coord    = task->extra.coordBody->coord;
    viewMask = 1 << gGameSession->location.view;
    actorRenderComposeCoord(coord);
    // Select visible lights with the room-local view ID, before camera remapping.
    if (viewMask & DRYFIELD_NIGHT_WAREHOUSE_PRISM_AND_SECOND_BEAM_VIEWS) {
        _glowDrawPrism(coord, DRYFIELD_NIGHT_WAREHOUSE_PRISM_FIRST_CORNER);
    }
    if (viewMask & DRYFIELD_NIGHT_WAREHOUSE_FIRST_BEAM_VIEWS) {
        _dryfieldNightWarehouseDrawLightBeam(coord, DRYFIELD_NIGHT_WAREHOUSE_FIRST_BEAM_RING, DRYFIELD_NIGHT_WAREHOUSE_BEAM_SEGMENTS);
    }
    if (viewMask & DRYFIELD_NIGHT_WAREHOUSE_PRISM_AND_SECOND_BEAM_VIEWS) {
        _dryfieldNightWarehouseDrawLightBeam(coord, DRYFIELD_NIGHT_WAREHOUSE_SECOND_BEAM_RING, DRYFIELD_NIGHT_WAREHOUSE_BEAM_SEGMENTS);
    }
    if (viewMask & DRYFIELD_NIGHT_WAREHOUSE_LAST_BEAMS_VIEWS) {
        _dryfieldNightWarehouseDrawLightBeam(coord, DRYFIELD_NIGHT_WAREHOUSE_THIRD_BEAM_RING, DRYFIELD_NIGHT_WAREHOUSE_BEAM_SEGMENTS);
        _dryfieldNightWarehouseDrawLightBeam(coord, DRYFIELD_NIGHT_WAREHOUSE_FOURTH_BEAM_RING, DRYFIELD_NIGHT_WAREHOUSE_BEAM_SEGMENTS);
    }
}
