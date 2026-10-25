#include "rooms/dryfield_night_souvenir_shop.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00700.h"
#include "actors/actor_01600.h"
#include "actors/actor_02500.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/areas.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"

/// Prism corners in the space of the coordinate drawn under, eight per prism:
/// the lit ring, then the far ring. The room draws the prisms at `[0..7]` and
/// `[8..15]`.
extern SVECTOR gGlowPrismCorners[];

static s32  _dryfieldNightSouvenirShopRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32  _dryfieldNightSouvenirShopResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _dryfieldNightSouvenirShopIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32  _dryfieldNightSouvenirShopIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _dryfieldNightSouvenirShopInitRoomTask(Task* task);

/// Resource-array extent and the packed departure/arrival slots of the doorway.
enum {
    DRYFIELD_NIGHT_SOUVENIR_SHOP_ACTION_TRIGGER_COUNT     = 12,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_BACK_STREET_ARRIVAL_WARP = 2,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_BACK_STREET_WARP_SLOTS   = (DRYFIELD_NIGHT_SOUVENIR_SHOP_WARP_BACK_STREET << 4) | DRYFIELD_NIGHT_SOUVENIR_SHOP_BACK_STREET_ARRIVAL_WARP,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_CAP_PRESENTATION_FLAGS   = 0,
};

static WorldCollisionGrid         _gDryfieldNightSouvenirShopCollisionGrid;
static WorldCollisionTrigger      _gDryfieldNightSouvenirShopViewBoundaryTriggers[2];
static WorldCollisionTrigger      _gDryfieldNightSouvenirShopActionTriggers[DRYFIELD_NIGHT_SOUVENIR_SHOP_ACTION_TRIGGER_COUNT];
static WorldCoordRoomAmbientEntry _gDryfieldNightSouvenirShopAmbientByView[4];
static WorldCoordRoomLights       _gDryfieldNightSouvenirShopRoomLights;

/// Logical view IDs and the independent limit used by reverse view lookup.
enum {
    DRYFIELD_NIGHT_SOUVENIR_SHOP_VIEW_2             = 2,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_VIEW_3             = 3,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_LOGICAL_VIEW_COUNT = 3,
};

/// Camera projection distance in pixels and neutral sprite RGB modulation.
enum {
    DRYFIELD_NIGHT_SOUVENIR_SHOP_SCREEN_DISTANCE      = 230,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR = 128,
};

/// Room-event resolution and key-item, action and command message routing.
///
/// Four entries followed by TASK_MESSAGE_TABLE_END. Initialization publishes
/// this borrowed table on the live room task; its handlers permit unchanged
/// room transitions, reject key items and ignore room actions and commands.
/// Keep the room overlay loaded until the registered task is released.
static TaskMessageEntry _gDryfieldNightSouvenirShopRoomMessages[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldNightSouvenirShopResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightSouvenirShopRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightSouvenirShopIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightSouvenirShopIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR gGlowPrismCorners[16] = {
    { 2315, -1960, -3930, 0 },
    { 1685, -1960, -3930, 0 },
    { 1680, -1000, -3930, 0 },
    { 2315, -1000, -3930, 0 },
    { 1530, 0, -1970, 0 },
    { 900, 0, -1970, 0 },
    { 1290, 0, -2935, 0 },
    { 1920, 0, -2935, 0 },
    { 5255, -2085, -4020, 0 },
    { 3745, -2085, -4020, 0 },
    { 3745, -545, -4020, 0 },
    { 5255, -545, -4020, 0 },
    { 4420, 0, -1935, 0 },
    { 2910, 0, -1935, 0 },
    { 3530, 0, -3475, 0 },
    { 5040, 0, -3475, 0 },
};

WorldCoordRoomLighting gDryfieldNightSouvenirShopRoomLighting[1] = {
    { &_gDryfieldNightSouvenirShopRoomLights, _gDryfieldNightSouvenirShopAmbientByView },
};

WorldCollisionRoomResources gDryfieldNightSouvenirShopRoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldNightSouvenirShopCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightSouvenirShopViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightSouvenirShopActionTriggers,
        .occluders            = NULL,
    },
};

u8* gDryfieldNightSouvenirShopViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldNightSouvenirShopViewCounts[1] = { DRYFIELD_NIGHT_SOUVENIR_SHOP_LOGICAL_VIEW_COUNT };

/// Door sound requests and the optional arrival map-marker nibble.
enum {
    DRYFIELD_NIGHT_SOUVENIR_SHOP_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP, 2),
    DRYFIELD_NIGHT_SOUVENIR_SHOP_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP, 1),
    DRYFIELD_NIGHT_SOUVENIR_SHOP_DOOR_MAP_FLAG   = 469,
};

DirectionWarpEntry gDryfieldNightSouvenirShopWarpEntries[DRYFIELD_NIGHT_SOUVENIR_SHOP_WARP_COUNT] = {
    [DRYFIELD_NIGHT_SOUVENIR_SHOP_WARP_BACK_STREET - 1] = {
        .player         = { { .word = 0 }, 2000, 0, -3550 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 512 }, 1381, 0, -2750 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_NIGHT_SOUVENIR_SHOP_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_NIGHT_SOUVENIR_SHOP_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = DRYFIELD_NIGHT_SOUVENIR_SHOP_VIEW_2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_SOUVENIR_SHOP_DOOR_MAP_FLAG,
    },
};

/// Grid-local plane normals, with ONE (4096) for unit length.
static SVECTOR _gDryfieldNightSouvenirShopCollisionGridNormals[11] = {
#include "assets/dryfield_night_souvenir_shop_collision_01044_normals.inc"
};

/// Collision-grid vertices in signed room-coordinate units.
static SVECTOR _gDryfieldNightSouvenirShopCollisionGridVerts[62] = {
#include "assets/dryfield_night_souvenir_shop_collision_01044_verts.inc"
};

/// Indexed triangles and quads selecting room-local surface classes 0, 1 and 2.
static WorldCollisionGridFace _gDryfieldNightSouvenirShopCollisionGridFaces[34] = {
#include "assets/dryfield_night_souvenir_shop_collision_01044_faces.inc"
};

/// Four signed face-index lists, each ending at WORLD_COLLISION_GRID_CELL_END.
///
/// The serialized extent also retains one zero after the final list.
static s16 _gDryfieldNightSouvenirShopCollisionGridCells[108] = {
#include "assets/dryfield_night_souvenir_shop_collision_01044_cells.inc"
};

/// Asset initializer binding: i is an in-bounds element offset into the cell lists.
/// Evaluates i once; defined only while including this grid's table initializer.
/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightSouvenirShopCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..107 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_SOUVENIR_SHOP_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightSouvenirShopCollisionGridCells[(halfwordOffset)])
/// X-major pointers for the grid's two-by-two cells; every cell is nonempty.
static s16* _gDryfieldNightSouvenirShopCollisionGridTable[4] = {
#include "assets/dryfield_night_souvenir_shop_collision_01044_table.inc"
};
#undef DRYFIELD_NIGHT_SOUVENIR_SHOP_CELL_FACE_LIST_AT_OFFSET

/// The shop's room collision mesh and two-by-two spatial index.
///
/// Cells are 4000 game units wide, starting at X=-100, Z=-4100. Activation
/// borrows the current view transform; the room owns all writable mesh storage.
static WorldCollisionGrid _gDryfieldNightSouvenirShopCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightSouvenirShopCollisionGridNormals,
    .vertices    = _gDryfieldNightSouvenirShopCollisionGridVerts,
    .faces       = _gDryfieldNightSouvenirShopCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightSouvenirShopCollisionGridTable,
    .xBias       = 100,
    .zBias       = 4100,
    .cellCountX  = 2,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightSouvenirShopCollisionGridFaces),
};

ViewCamera gDryfieldNightSouvenirShopViewCameras[4] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -3000, 7500, 2000 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SCREEN_DISTANCE },
    { { { { 1125, 0, 3938 }, { 1711, 3688, -489 }, { -3546, 1780, 1013 } }, { -4960, 2182, 3227 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SCREEN_DISTANCE },
    { { { { 1194, 0, -3918 }, { -1520, 3775, -463 }, { 3611, 1589, 1100 } }, { -928, 2117, 3153 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SCREEN_DISTANCE },
    { { { { 2278, 0, -3403 }, { -2520, 2752, -1687 }, { 2286, 3033, 1531 } }, { -4039, 2424, 1619 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SCREEN_DISTANCE },
};

/// View 1's empty batch list; its zero first count selects decoded image strips.
static SpriteBatch _gDryfieldNightSouvenirShopView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's 45 texture rectangles and sorting depths, relative to view centre.
///
/// Positions and sizes are pixels; UV origins are texels on 8-bit texture pages.
static SpriteSource _gDryfieldNightSouvenirShopView2SpriteSources[45] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 96 } }, -144, 24, 500, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 48 } }, -104, 72, 600, { .fields = { 120, 96 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 88, 64 } }, -144, 56, 450, { .fields = { 8, 184 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 24 } }, -120, 64, 437, { .fields = { 72, 24 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 32 } }, -120, 88, 412, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 136, 48, 450, { .fields = { 16, 144 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -16, -24, 875, { .fields = { 40, 248 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -16, -16, 875, { .fields = { 40, 112 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, 8, -16, 775, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -8, -16, 875, { .fields = { 88, 96 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, 24, 0, 675, { .fields = { 80, 192 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 104 } }, 8, 0, 837, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 24 } }, 48, 16, 537, { .fields = { 80, 232 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 24 } }, 64, 40, 487, { .fields = { 80, 208 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, 48, 40, 375, { .fields = { 72, 96 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 24 } }, 80, 64, 425, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 64, 64, 375, { .fields = { 56, 128 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, 80, 88, 371, { .fields = { 120, 48 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 96, 104, 375, { .fields = { 104, 64 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, 96, 88, 375, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, 112, 104, 375, { .fields = { 88, 80 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, 24, 72, 662, { .fields = { 16, 48 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 24, 16, 662, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 32, 16, 625, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 64 } }, -128, -40, 625, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 64 } }, -136, -16, 625, { .fields = { 64, 64 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 64 } }, -136, -80, 625, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 56 } }, -152, -112, 575, { .fields = { 40, 56 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 56 } }, -152, -56, 575, { .fields = { 48, 112 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 56 } }, -144, -88, 600, { .fields = { 48, 56 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 56 } }, -144, -32, 600, { .fields = { 56, 56 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 40 } }, -160, -112, 550, { .fields = { 8, 144 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 48 } }, -160, -72, 550, { .fields = { 8, 48 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 48 } }, -128, 24, 625, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 48 } }, -120, -8, 625, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 48 } }, -120, 40, 625, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 48 } }, -112, 40, 625, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -104, 64, 625, { .fields = { 112, 120 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -112, 88, 625, { .fields = { 40, 160 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -120, 88, 625, { .fields = { 0, 176 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 48 } }, -128, 72, 625, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 72 } }, -136, 48, 625, { .fields = { 96, 184 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 96 } }, -144, 24, 600, { .fields = { 104, 96 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 120 } }, -152, 0, 575, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 144 } }, -160, -24, 550, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 2's five sprite ranges, preceded by image-strip selection and terminated.
///
/// Ranges cover all 45 sources once. Both runtime visibility bytes start clear;
/// the two serialized bytes after them retain their unproven roles.
static SpriteBatch _gDryfieldNightSouvenirShopView2SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 4, 0 } },
    { 2, 1, 0, 0, { 1, 0 } },
    { 3, 2, 0, 0, { 3, 0 } },
    { 5, 19, 0, 0, { 2, 0 } },
    { 24, ARRAY_SIZE(_gDryfieldNightSouvenirShopView2SpriteSources) - 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's 53 texture rectangles and sorting depths, relative to view centre.
///
/// Positions and sizes are pixels; UV origins are texels on 8-bit texture pages.
static SpriteSource _gDryfieldNightSouvenirShopView3SpriteSources[53] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 72, 96 } }, 72, 24, 625, { .fields = { 56, 104 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -48, -8, 925, { .fields = { 88, 200 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, -32, -8, 925, { .fields = { 56, 200 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -48, -32, 925, { .fields = { 64, 176 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, -32, -32, 900, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 40 } }, -112, -8, 1175, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 88 } }, 72, 8, 675, { .fields = { 112, 88 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 32 } }, 80, 48, 625, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 32, -40, 1450, { .fields = { 80, 200 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 24, -32, 1375, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 16, -24, 1250, { .fields = { 8, 176 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, 0, -40, 1450, { .fields = { 40, 248 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -8, -32, 1250, { .fields = { 32, 176 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -16, -24, 1125, { .fields = { 24, 248 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 0, 0, 1137, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, -64, 0, 750, { .fields = { 8, 232 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, -80, 16, 650, { .fields = { 16, 200 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 104 } }, -16, 16, 900, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 24 } }, -112, 32, 562, { .fields = { 8, 24 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 88 } }, -40, 32, 675, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 24 } }, -128, 56, 475, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 64 } }, -64, 56, 375, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 40 } }, -128, 80, 375, { .fields = { 32, 136 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 16 } }, -40, -16, 1050, { .fields = { 16, 216 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -112, -80, 912, { .fields = { 0, 176 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -120, -104, 912, { .fields = { 104, 144 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -104, -32, 1025, { .fields = { 112, 48 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 48 } }, -96, -8, 1050, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 40 } }, -136, -112, 912, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, -128, -72, 912, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -120, -24, 1025, { .fields = { 0, 64 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, -112, 0, 1025, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 48 } }, -160, -120, 875, { .fields = { 96, 176 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 48 } }, -160, -72, 875, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, -152, -24, 900, { .fields = { 48, 72 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 72 } }, -144, 0, 1000, { .fields = { 16, 176 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -160, -24, 875, { .fields = { 0, 232 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -160, 0, 975, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 72 } }, 152, -120, 675, { .fields = { 48, 176 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 72 } }, 152, -48, 675, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 72 } }, 144, -112, 675, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 88 } }, 144, -40, 675, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 136, -112, 675, { .fields = { 96, 88 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 128, -112, 700, { .fields = { 88, 88 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 120, -112, 725, { .fields = { 80, 88 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 56 } }, 136, -80, 675, { .fields = { 104, 200 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 56 } }, 136, -24, 675, { .fields = { 112, 200 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 8, 48 } }, 136, 32, 675, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 8, 56 } }, 128, -48, 675, { .fields = { 120, 176 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 88 } }, 128, 8, 675, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 88 } }, 120, 8, 675, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 8, 48 } }, 112, 40, 675, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 104, 72, 675, { .fields = { 104, 88 } }, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SOUVENIR_SHOP_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 3's eight sprite ranges, preceded by image-strip selection and terminated.
///
/// Ranges cover all 53 sources once. Both runtime visibility bytes start clear;
/// the two serialized bytes after them retain their unproven roles.
static SpriteBatch _gDryfieldNightSouvenirShopView3SpriteBatches[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 4, 0 } },
    { 1, 4, 0, 0, { 5, 0 } },
    { 5, 1, 0, 0, { 2, 0 } },
    { 6, 1, 0, 0, { 6, 0 } },
    { 7, 1, 0, 0, { 1, 0 } },
    { 8, 16, 0, 0, { 7, 0 } },
    { 24, 14, 0, 0, { 3, 0 } },
    { 38, ARRAY_SIZE(_gDryfieldNightSouvenirShopView3SpriteSources) - 38, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's empty batch list; its zero first count selects decoded image strips.
static SpriteBatch _gDryfieldNightSouvenirShopView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightSouvenirShopSpriteViews[4] = {
    { { .empty = _gDryfieldNightSouvenirShopView1SpriteBatches }, _gDryfieldNightSouvenirShopView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightSouvenirShopView2SpriteSources }, _gDryfieldNightSouvenirShopView2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightSouvenirShopView3SpriteSources }, _gDryfieldNightSouvenirShopView3SpriteBatches, NULL },
    { { .empty = _gDryfieldNightSouvenirShopView4SpriteBatches }, _gDryfieldNightSouvenirShopView4SpriteBatches, NULL },
};

/// Six point lights contributing in every view, with Q12 RGB and game-unit radii.
///
/// Room-light setup parents and composes these writable coordinate nodes;
/// subsequent lighting queries overwrite their attenuation scratch.
static WorldCoordPointLight _gDryfieldNightSouvenirShopPointLights[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4098, -1580, -3550 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1846, 2048, 2048 }, { 0, 0 } }, 1102, 1964 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2383, -1477, -3573 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1845, 2048, 2048 }, { 0, 0 } }, 1161, 2563 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1993, -1582, -21 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1844, 2048, 2049 }, { 0, 0 } }, 1486, 2463 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4049, -1580, -20 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1844, 2050, 2050 }, { 0, 0 } }, 1485, 2503 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 299, -393, -2198 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1025, 1230, 1229 }, { 0, 0 } }, 750, 1250 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3700, -120, -2840 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1556, 1474 }, { 0, 0 } }, 1001, 1800 },
};

/// The shop's six point lights, with no directional or cone lights.
static WorldCoordRoomLights _gDryfieldNightSouvenirShopRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightSouvenirShopPointLights),
    .pointLights           = _gDryfieldNightSouvenirShopPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Opposing boundaries switching views 3 to 2 and 2 to 3.
///
/// Setup links and enables both writable quads, including the LAST-marked second
/// record. Positions are room units; normals use ONE (4096) for unit length.
static WorldCollisionTrigger _gDryfieldNightSouvenirShopViewBoundaryTriggers[2] = {
    { NULL, NULL, NULL, { 2879, -1440, -1969, 0 }, { { 147, -1824, -2190, 0 }, { -146, -1824, 2191, 0 }, { 147, 1824, -2190, 0 }, { -146, 1824, 2191, 0 } }, { 4087, 0, 272, 0 }, { 0, 0, ONE, 0 }, 2850, 0, DRYFIELD_NIGHT_SOUVENIR_SHOP_VIEW_3, DRYFIELD_NIGHT_SOUVENIR_SHOP_VIEW_2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3039, -1536, -2017, 0 }, { { -146, -1888, 2191, 0 }, { 147, -1888, -2190, 0 }, { -146, 1888, 2191, 0 }, { 147, 1888, -2190, 0 } }, { -4087, 0, -275, 0 }, { 0, 0, ONE, 0 }, 2884, 0, DRYFIELD_NIGHT_SOUVENIR_SHOP_VIEW_2, DRYFIELD_NIGHT_SOUVENIR_SHOP_VIEW_3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Ambient minima for views 1..3, following their count header.
///
/// RGB uses 16 units per colour level; each fourth halfword stores weighted luma.
static WorldCoordRoomAmbientEntry _gDryfieldNightSouvenirShopAmbientByView[4] = {
    { .viewCount = ARRAY_SIZE(_gDryfieldNightSouvenirShopAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 410, 615, 615, 538 } },
    { .color = { 410, 615, 615, 538 } },
};

/// The shop's doorway and eleven CAP interaction quads.
///
/// The doorway requests back-street area 5, departure slot 1 and arrival slot 2.
/// CAP indices 1..7 and 9 use presentation flags 0. The final active quad carries
/// LAST and accepts proximity or facing; the others require facing. Setup binds
/// the writable records to the view coordinate and links all twelve, including
/// LAST. Keep this overlay loaded until the action list is replaced and unlinked.
static WorldCollisionTrigger _gDryfieldNightSouvenirShopActionTriggers[DRYFIELD_NIGHT_SOUVENIR_SHOP_ACTION_TRIGGER_COUNT] = {
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 1920, -57, -3760, 0 },
        .vertices     = { { -544, 0, -240, 0 }, { 544, 0, -240, 0 }, { -544, 0, 240, 0 }, { 544, 0, 240, 0 } },
        .normal       = { 0, 4095, 0, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 593,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_DRYFIELD_NIGHT_BACK_STREET,
        .parameter1   = DRYFIELD_NIGHT_SOUVENIR_SHOP_BACK_STREET_WARP_SLOTS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 2832, -64, -704, 0 },
        .vertices     = { { -1088, 0, -272, 0 }, { 1088, 0, -272, 0 }, { -1088, 0, 272, 0 }, { 1088, 0, 272, 0 } },
        .normal       = { 0, 4109, 0, 0 },
        .facingNormal = { -201, 0, -4091, 0 },
        .radius       = 1115,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 4,
        .parameter1   = DRYFIELD_NIGHT_SOUVENIR_SHOP_CAP_PRESENTATION_FLAGS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 4176, -64, -3296, 0 },
        .vertices     = { { -1520, 0, -272, 0 }, { 1520, 0, -272, 0 }, { -1520, 0, 272, 0 }, { 1520, 0, 272, 0 } },
        .normal       = { 0, 4102, 0, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 1541,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 1,
        .parameter1   = DRYFIELD_NIGHT_SOUVENIR_SHOP_CAP_PRESENTATION_FLAGS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 5312, -64, -752, 0 },
        .vertices     = { { -192, 0, -1024, 0 }, { 192, 0, -1024, 0 }, { -192, 0, 1024, 0 }, { 192, 0, 1024, 0 } },
        .normal       = { 0, 4095, 0, 0 },
        .facingNormal = { -4091, 0, 201, 0 },
        .radius       = 1039,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 3,
        .parameter1   = DRYFIELD_NIGHT_SOUVENIR_SHOP_CAP_PRESENTATION_FLAGS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 1023, -64, -1728, 0 },
        .vertices     = { { -336, 0, -640, 0 }, { 336, 0, -640, 0 }, { -336, 0, 640, 0 }, { 336, 0, 640, 0 } },
        .normal       = { 0, 4098, 0, 0 },
        .facingNormal = { 4095, 0, 0, 0 },
        .radius       = 721,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 2,
        .parameter1   = DRYFIELD_NIGHT_SOUVENIR_SHOP_CAP_PRESENTATION_FLAGS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 4656, -64, -1200, 0 },
        .vertices     = { { -880, 0, -384, 0 }, { 880, 0, -384, 0 }, { -880, 0, 384, 0 }, { 880, 0, 384, 0 } },
        .normal       = { 0, 4101, 0, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 960,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 9,
        .parameter1   = DRYFIELD_NIGHT_SOUVENIR_SHOP_CAP_PRESENTATION_FLAGS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 2736, -64, -1696, 0 },
        .vertices     = { { -848, 0, -176, 0 }, { 848, 0, -176, 0 }, { -848, 0, 176, 0 }, { 848, 0, 176, 0 } },
        .normal       = { 0, 4098, 0, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 865,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 5,
        .parameter1   = DRYFIELD_NIGHT_SOUVENIR_SHOP_CAP_PRESENTATION_FLAGS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 2720, -64, -2624, 0 },
        .vertices     = { { -848, 0, -176, 0 }, { 848, 0, -176, 0 }, { -848, 0, 176, 0 }, { 848, 0, 176, 0 } },
        .normal       = { 0, 4098, 0, 0 },
        .facingNormal = { -201, 0, -4091, 0 },
        .radius       = 865,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 5,
        .parameter1   = DRYFIELD_NIGHT_SOUVENIR_SHOP_CAP_PRESENTATION_FLAGS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 1056, -64, -3104, 0 },
        .vertices     = { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } },
        .normal       = { 0, 4098, 0, 0 },
        .facingNormal = { 3973, 0, 995, 0 },
        .radius       = 561,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 6,
        .parameter1   = DRYFIELD_NIGHT_SOUVENIR_SHOP_CAP_PRESENTATION_FLAGS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 1392, -64, -736, 0 },
        .vertices     = { { -576, 0, -176, 0 }, { 576, 0, -176, 0 }, { -576, 0, 176, 0 }, { 576, 0, 176, 0 } },
        .normal       = { 0, 4097, 0, 0 },
        .facingNormal = { -201, 0, -4091, 0 },
        .radius       = 600,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 7,
        .parameter1   = DRYFIELD_NIGHT_SOUVENIR_SHOP_CAP_PRESENTATION_FLAGS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 4720, -64, -448, 0 },
        .vertices     = { { -784, 0, -176, 0 }, { 784, 0, -176, 0 }, { -784, 0, 176, 0 }, { 784, 0, 176, 0 } },
        .normal       = { 0, 4099, 0, 0 },
        .facingNormal = { 200, 0, -4091, 0 },
        .radius       = 801,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 7,
        .parameter1   = DRYFIELD_NIGHT_SOUVENIR_SHOP_CAP_PRESENTATION_FLAGS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 4352, -64, -1952, 0 },
        .vertices     = { { -1360, 0, 32, 0 }, { 240, 0, 32, 0 }, { -752, 0, 896, 0 }, { 240, 0, 896, 0 } },
        .normal       = { 0, 4112, 0, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 1354,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 9,
        .parameter1   = DRYFIELD_NIGHT_SOUVENIR_SHOP_CAP_PRESENTATION_FLAGS,
        .flags        = WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST,
        .hit          = 0,
    },
};

/// Placement keys and decimal actor file numbers used by the shop's encounters.
enum {
    DRYFIELD_NIGHT_SOUVENIR_SHOP_RESOURCE_MOTH         = 8,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_RESOURCE_SCAVENGER    = 16,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_RESOURCE_SCORPION     = 25,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_MOTH_FILE_NUMBER      = 7,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_SCAVENGER_FILE_NUMBER = 16,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_SCORPION_FILE_NUMBER  = 25,
};

/// Variant 1's moth loading/spawn recipe, followed by AREA_PLACEMENT_END.
///
/// Placement key 8 selects descriptor 0 from actor file 100700; the night map
/// supplies four placements. The room and actor overlays stay loaded during use.
static AreaResource _gDryfieldNightSouvenirShopVariant1Resources[2] = {
    { DRYFIELD_NIGHT_SOUVENIR_SHOP_RESOURCE_MOTH, DRYFIELD_NIGHT_SOUVENIR_SHOP_MOTH_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor00700MothTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's scavenger loading/spawn recipe, followed by AREA_PLACEMENT_END.
///
/// Placement key 16 selects descriptor 0 from actor file 101600; the night map
/// supplies four placements. The room and actor overlays stay loaded during use.
static AreaResource _gDryfieldNightSouvenirShopVariant3Resources[2] = {
    { DRYFIELD_NIGHT_SOUVENIR_SHOP_RESOURCE_SCAVENGER, DRYFIELD_NIGHT_SOUVENIR_SHOP_SCAVENGER_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor101600ScavengerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 11's moth and scorpion recipes, followed by AREA_PLACEMENT_END.
///
/// Placement keys 8/25 select descriptor 0 from files 100700/202500, loaded
/// in that order. The night map supplies seven moths and three scorpions.
/// The room and both actor overlays stay loaded during loading and spawning.
static AreaResource _gDryfieldNightSouvenirShopVariant11Resources[3] = {
    { DRYFIELD_NIGHT_SOUVENIR_SHOP_RESOURCE_MOTH, DRYFIELD_NIGHT_SOUVENIR_SHOP_MOTH_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor00700MothTaskDesc },
    { DRYFIELD_NIGHT_SOUVENIR_SHOP_RESOURCE_SCORPION, DRYFIELD_NIGHT_SOUVENIR_SHOP_SCORPION_FILE_NUMBER, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor02500Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldNightSouvenirShopAreaVariants[DRYFIELD_NIGHT_SOUVENIR_SHOP_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapDryfieldFullSouvenirShopVariant1Placements, .resources = _gDryfieldNightSouvenirShopVariant1Resources },
    [3]                    = { .placements = gMapDryfieldFullSouvenirShopVariant3Placements, .resources = _gDryfieldNightSouvenirShopVariant3Resources },
    [11]                   = { .placements = gMapDryfieldFullSouvenirShopVariant11Placements, .resources = _gDryfieldNightSouvenirShopVariant11Resources },
};

/// Cue-2 bases in the currently loaded type-1 sound bank; cue 1 uses base + 1.
enum {
    DRYFIELD_NIGHT_SOUVENIR_SHOP_SURFACE1_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_SURFACE1_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_SURFACE2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x31,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_SURFACE2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x33,
};

/// Surface class 1's paired walking/running cues; scripted jumps reuse walking.
static WorldCollisionFootstepSounds _gDryfieldNightSouvenirShopSurface1FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_SOUVENIR_SHOP_SURFACE1_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_SOUVENIR_SHOP_SURFACE1_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_SOUVENIR_SHOP_SURFACE1_WALK_SOUND,
};

/// Surface class 2's paired walking/running cues; scripted jumps reuse walking.
static WorldCollisionFootstepSounds _gDryfieldNightSouvenirShopSurface2FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_SOUVENIR_SHOP_SURFACE2_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_SOUVENIR_SHOP_SURFACE2_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_SOUVENIR_SHOP_SURFACE2_WALK_SOUND,
};

/// Blocking, weapon-impact-enabled properties shared by silent classes 0 and 5..7.
static WorldCollisionSurfaceProperties _gDryfieldNightSouvenirShopSilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 3 passes probes and ignores weapon impacts, while retaining pushback.
///
/// It has no footstep cues; the shop's authored grid contains no class-3 faces.
static WorldCollisionSurfaceProperties _gDryfieldNightSouvenirShopSurface3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 4 blocks probes, enables weapon impacts and pushback, and has no cues.
///
/// Retains its own record; the shop's authored grid contains no class-4 faces.
static WorldCollisionSurfaceProperties _gDryfieldNightSouvenirShopSurface4Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1 blocks probes, enables weapon impacts and pushback, and selects its cues.
static WorldCollisionSurfaceProperties _gDryfieldNightSouvenirShopSurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightSouvenirShopSurface1FootstepSounds,
};

/// Class 2 blocks probes, enables weapon impacts and pushback, and selects its cues.
static WorldCollisionSurfaceProperties _gDryfieldNightSouvenirShopSurface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightSouvenirShopSurface2FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightSouvenirShopSurfacePropertiesByClass[8] = {
    &_gDryfieldNightSouvenirShopSilentSurfaceProperties,
    &_gDryfieldNightSouvenirShopSurface1Properties,
    &_gDryfieldNightSouvenirShopSurface2Properties,
    &_gDryfieldNightSouvenirShopSurface3Properties,
    &_gDryfieldNightSouvenirShopSurface4Properties,
    &_gDryfieldNightSouvenirShopSilentSurfaceProperties,
    &_gDryfieldNightSouvenirShopSilentSurfaceProperties,
    &_gDryfieldNightSouvenirShopSilentSurfaceProperties,
};

/// Rejects every key-item use in this room, returning 0 without consuming the item.
///
/// `itemId` is the inventory item's integer ID; the second payload word is unused.
static s32 _dryfieldNightSouvenirShopRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Permits a room transition with the requested destination unchanged.
///
/// Borrows a complete readable request and writable eight-byte reply through
/// synchronous dispatch; they may be the same record. Copies every field and
/// returns 1 in both query and execute modes, retaining neither pointer and
/// changing no game flags.
static s32 _dryfieldNightSouvenirShopResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_NIGHT_SOUVENIR_SHOP_ROOM_EVENT_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_NIGHT_SOUVENIR_SHOP_ROOM_EVENT_ALLOWED;
}

/// Ignores every room command and returns 0 without changing room or task state.
///
/// Both command payload words are unused.
static s32 _dryfieldNightSouvenirShopIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores direction-trigger room actions and returns 0.
///
/// Neither the borrowed action request nor the second payload word is accessed
/// or retained; no room or task state changes.
static s32 _dryfieldNightSouvenirShopIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the room task and its message handlers, then enters its idle state.
///
/// Called in state 0 with a live task. The loaded room's table is borrowed
/// for subsequent synchronous messages while the registered task remains alive.
static void _dryfieldNightSouvenirShopInitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldNightSouvenirShopRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the initialized room task idle until its state changes externally.
///
/// State 1 leaves the task and its installed message table alive; state 2 is
/// handled separately by `taskKill`.
static void _dryfieldNightSouvenirShopIdleRoomTask(Task* task)
{
}

/// Unchecked selectors for the room task's three callback slots.
enum {
    DRYFIELD_NIGHT_SOUVENIR_SHOP_ROOM_STATE_INITIALIZE = 0,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_ROOM_STATE_IDLE       = 1,
    DRYFIELD_NIGHT_SOUVENIR_SHOP_ROOM_STATE_RELEASE    = 2,
};

/// The room task's initialize, persistent idle and release states (0..2).
///
/// The dispatcher copies all three callbacks by value and indexes task->state
/// without a bounds check. The callbacks require this room overlay to remain
/// loaded; the release callback ends the task and its message-table lifetime.
static const TaskFuncTable3 _gDryfieldNightSouvenirShopRoomTaskStates = {
    .funcs = {
        [DRYFIELD_NIGHT_SOUVENIR_SHOP_ROOM_STATE_INITIALIZE] = _dryfieldNightSouvenirShopInitRoomTask,
        [DRYFIELD_NIGHT_SOUVENIR_SHOP_ROOM_STATE_IDLE]       = _dryfieldNightSouvenirShopIdleRoomTask,
        [DRYFIELD_NIGHT_SOUVENIR_SHOP_ROOM_STATE_RELEASE]    = taskKill,
    },
};

void dryfieldNightSouvenirShopRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gDryfieldNightSouvenirShopRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_prism.inc.c"

void dryfieldNightSouvenirShopPrismLightTask(Task* task)
{
    enum { DRYFIELD_NIGHT_SOUVENIR_SHOP_CORNERS_PER_PRISM = 8 };

    GfxCoord* coord;

    coord = task->extra.coordBody->coord;
    actorRenderComposeCoord(coord);
    _glowDrawPrism(coord, 0);
    _glowDrawPrism(coord, DRYFIELD_NIGHT_SOUVENIR_SHOP_CORNERS_PER_PRISM);
}
