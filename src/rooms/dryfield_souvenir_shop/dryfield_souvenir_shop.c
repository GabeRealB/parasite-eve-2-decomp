#include "rooms/dryfield_souvenir_shop.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_01500.h"
#include "actors/actor_04000.h"

#include "gameplay/display.h"
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
#include "main/display.h"
#include "main/display_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"

/// Prism corners in the space of the coordinate drawn under, eight per prism:
/// the lit ring, then the far ring. The room draws the prisms at `[0..7]` and
/// `[8..15]`.
extern SVECTOR gGlowPrismCorners[];

static s32  _dryfieldSouvenirShopRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32  _dryfieldSouvenirShopResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _dryfieldSouvenirShopIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32  _dryfieldSouvenirShopIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _dryfieldSouvenirShopIdleRoomTask(Task* task);
static void _dryfieldSouvenirShopInitRoomTask(Task* task);

static WorldCollisionGrid    _gDryfieldSouvenirShopCollisionGrid;
static WorldCollisionTrigger _gDryfieldSouvenirShopViewBoundaryTriggers[2];
static WorldCollisionTrigger _gDryfieldSouvenirShopActionTriggers[12];
static WorldCoordRoomLights  _gDryfieldSouvenirShopLights;

/// Synchronous room handlers for transitions, key items, direction actions and CAP commands.
///
/// The final record ends searches for unsupported IDs. The room task borrows
/// this table and its callbacks for its lifetime in the loaded room overlay.
static TaskMessageEntry _gDryfieldSouvenirShopMessageTable[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldSouvenirShopResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldSouvenirShopRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldSouvenirShopIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldSouvenirShopIgnoreRoomCommand },
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

WorldCollisionRoomResources gDryfieldSouvenirShopRoomCollisionResources[1] = {
    { &_gDryfieldSouvenirShopCollisionGrid, _gDryfieldSouvenirShopViewBoundaryTriggers, _gDryfieldSouvenirShopActionTriggers, NULL },
};

u8* gDryfieldSouvenirShopRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldSouvenirShopRoomViewCounts[1] = { ARRAY_SIZE(gDryfieldSouvenirShopSpriteViews) };

WorldCoordRoomLighting gDryfieldSouvenirShopRoomLighting[1] = {
    { &_gDryfieldSouvenirShopLights, NULL },
};

/// Door-script entries in this room's daytime area sound bank.
enum {
    DRYFIELD_SOUVENIR_SHOP_SOUND_ARRIVAL   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_SOUVENIR_SHOP, 2),
    DRYFIELD_SOUVENIR_SHOP_SOUND_DEPARTURE = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_SOUVENIR_SHOP, 1),
};

DirectionWarpEntry gDryfieldSouvenirShopWarpEntries[1] = {
    { { { .word = 0 }, 2000, 0, -3550 }, { 0, 0, 0, 0 }, { { .word = 512 }, 1381, 0, -2750 }, { 0, 0, 0, 0 }, DRYFIELD_SOUVENIR_SHOP_SOUND_ARRIVAL, DRYFIELD_SOUVENIR_SHOP_SOUND_DEPARTURE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 469 },
};

/// Unit-length collision normals in room space, with ONE (4096) per unit.
static SVECTOR _gDryfieldSouvenirShopCollisionGridNormals[11] = {
#include "assets/dryfield_souvenir_shop_collision_0101C_normals.inc"
};

/// Collision-mesh vertices in signed room-coordinate units.
static SVECTOR _gDryfieldSouvenirShopCollisionGridVerts[62] = {
#include "assets/dryfield_souvenir_shop_collision_0101C_verts.inc"
};

/// Indexed collision triangles and quads using room-local surface classes 0..2.
static WorldCollisionGridFace _gDryfieldSouvenirShopCollisionGridFaces[34] = {
#include "assets/dryfield_souvenir_shop_collision_0101C_faces.inc"
};

/// Face-index lists for the four XZ cells, each ending with WORLD_COLLISION_GRID_CELL_END.
///
/// The serialized pool retains two zero entries outside the selected lists.
static s16 _gDryfieldSouvenirShopCollisionGridCells[108] = {
#include "assets/dryfield_souvenir_shop_collision_0101C_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldSouvenirShopCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..107 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_SOUVENIR_SHOP_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldSouvenirShopCollisionGridCells[(halfwordOffset)])
/// X-major 2-by-2 directory of face-index lists within the collision-cell pool.
static s16* _gDryfieldSouvenirShopCollisionGridTable[4] = {
#include "assets/dryfield_souvenir_shop_collision_0101C_table.inc"
};
#undef DRYFIELD_SOUVENIR_SHOP_CELL_FACE_LIST_AT_OFFSET

/// The souvenir shop's room collision mesh and 4000-unit XZ cell index.
///
/// Room setup binds the view coordinate before activating this grid. The
/// descriptor and all five pools remain writable and live while the room is loaded.
static WorldCollisionGrid _gDryfieldSouvenirShopCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldSouvenirShopCollisionGridNormals,
    .vertices    = _gDryfieldSouvenirShopCollisionGridVerts,
    .faces       = _gDryfieldSouvenirShopCollisionGridFaces,
    .cellFaceIds = _gDryfieldSouvenirShopCollisionGridTable,
    .xBias       = 100,
    .zBias       = 4100,
    .cellCountX  = 2,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldSouvenirShopCollisionGridFaces),
};

ViewCamera gDryfieldSouvenirShopViewCameras[4] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -3000, 7500, 2000 } }, 230 },
    { { { { 1125, 0, 3938 }, { 1711, 3688, -489 }, { -3546, 1780, 1013 } }, { -4960, 2182, 3227 } }, 230 },
    { { { { 1194, 0, -3918 }, { -1520, 3775, -463 }, { 3611, 1589, 1100 } }, { -928, 2117, 3153 } }, 230 },
    { { { { 2278, 0, -3403 }, { -2520, 2752, -1687 }, { 2286, 3033, 1531 } }, { -4039, 2424, 1619 } }, 230 },
};

/// Empty mapped-view-1 sprite list, retaining decoded background image strips.
static SpriteBatch _gDryfieldSouvenirShopView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped-view-2 texture rectangles in screen pixels, with unscaled sorting depths.
static SpriteSource _gDryfieldSouvenirShopView2SpriteSources[45] = {
    { 143, 0x3FC0, { .fields = { 40, 96 } }, -144, 24, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -104, 72, 600, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 64 } }, -144, 56, 450, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -120, 64, 437, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -120, 88, 412, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 48, 450, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -16, -24, 875, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -16, -16, 875, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 8, -16, 775, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -8, -16, 875, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 24, 0, 675, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 8, 0, 837, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 48, 16, 537, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 64, 40, 487, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 48, 40, 375, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 80, 64, 425, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, 64, 375, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 80, 88, 371, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 104, 375, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 96, 88, 375, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 112, 104, 375, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 24, 72, 662, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, 16, 662, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 32, 16, 625, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, -128, -40, 625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, -136, -16, 625, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, -136, -80, 625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -152, -112, 575, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -152, -56, 575, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -144, -88, 600, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -144, -32, 600, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -160, -112, 550, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -160, -72, 550, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -128, 24, 625, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -120, -8, 625, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -120, 40, 625, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -112, 40, 625, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -104, 64, 625, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -112, 88, 625, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -120, 88, 625, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -128, 72, 625, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, -136, 48, 625, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 96 } }, -144, 24, 600, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 120 } }, -152, 0, 575, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 144 } }, -160, -24, 550, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

/// Five drawing ranges covering all 45 view-2 sources, preceded by an image-strip selector.
///
/// The final record ends the list; the first zero-count record retains the background.
static SpriteBatch _gDryfieldSouvenirShopView2SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 4, 0 } },
    { 2, 1, 0, 0, { 1, 0 } },
    { 3, 2, 0, 0, { 3, 0 } },
    { 5, 19, 0, 0, { 2, 0 } },
    { 24, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped-view-3 texture rectangles in screen pixels, with unscaled sorting depths.
static SpriteSource _gDryfieldSouvenirShopView3SpriteSources[53] = {
    { 143, 0x3FC0, { .fields = { 72, 96 } }, 72, 24, 625, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, -8, 925, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -32, -8, 925, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -48, -32, 925, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -32, -32, 900, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -112, -8, 1175, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 88 } }, 72, 8, 675, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 80, 48, 625, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -40, 1450, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, -32, 1375, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 16, -24, 1250, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 0, -40, 1450, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -8, -32, 1250, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, -24, 1125, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, 0, 1137, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -64, 0, 750, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -80, 16, 650, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -16, 16, 900, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 24 } }, -112, 32, 562, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -40, 32, 675, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -128, 56, 475, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 64 } }, -64, 56, 375, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 40 } }, -128, 80, 375, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -40, -16, 1050, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, -80, 912, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -120, -104, 912, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -104, -32, 1025, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -96, -8, 1050, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -136, -112, 912, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -128, -72, 912, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, -24, 1025, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, 0, 1025, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -160, -120, 875, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -160, -72, 875, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -152, -24, 900, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -144, 0, 1000, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, -24, 875, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -160, 0, 975, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, 152, -120, 675, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, 152, -48, 675, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, 144, -112, 675, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 88 } }, 144, -40, 675, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 136, -112, 675, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 128, -112, 700, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 120, -112, 725, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, 136, -80, 675, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, 136, -24, 675, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, 136, 32, 675, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 56 } }, 128, -48, 675, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 88 } }, 128, 8, 675, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 88 } }, 120, 8, 675, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, 112, 40, 675, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 104, 72, 675, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
};

/// Eight drawing ranges covering all 53 view-3 sources, preceded by an image-strip selector.
///
/// The final record ends the list; the first zero-count record retains the background.
static SpriteBatch _gDryfieldSouvenirShopView3SpriteBatches[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 4, 0 } },
    { 1, 4, 0, 0, { 5, 0 } },
    { 5, 1, 0, 0, { 2, 0 } },
    { 6, 1, 0, 0, { 6, 0 } },
    { 7, 1, 0, 0, { 1, 0 } },
    { 8, 16, 0, 0, { 7, 0 } },
    { 24, 14, 0, 0, { 3, 0 } },
    { 38, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldSouvenirShopSpriteViews[3] = {
    { { .empty = _gDryfieldSouvenirShopView1SpriteBatches }, _gDryfieldSouvenirShopView1SpriteBatches, NULL },
    { { .elements = _gDryfieldSouvenirShopView2SpriteSources }, _gDryfieldSouvenirShopView2SpriteBatches, NULL },
    { { .elements = _gDryfieldSouvenirShopView3SpriteSources }, _gDryfieldSouvenirShopView3SpriteBatches, NULL },
};

/// Directed boundaries switching logical views 3 to 2 and 2 to 3.
///
/// Room setup binds and links both mutable quads, including the final LAST record.
static WorldCollisionTrigger _gDryfieldSouvenirShopViewBoundaryTriggers[2] = {
    { NULL, NULL, NULL, { 2879, -1440, -1969, 0 }, { { 147, -1824, -2190, 0 }, { -146, -1824, 2191, 0 }, { 147, 1824, -2190, 0 }, { -146, 1824, 2191, 0 } }, { 4087, 0, 272, 0 }, { 0, 0, ONE, 0 }, 2850, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3039, -1536, -2017, 0 }, { { -146, -1888, 2191, 0 }, { 147, -1888, -2190, 0 }, { -146, 1888, 2191, 0 }, { 147, 1888, -2190, 0 } }, { -4087, 0, -275, 0 }, { 0, 0, ONE, 0 }, 2884, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Door warp to the back street and eleven CAP interaction quads.
///
/// The door uses departure slot 1 and destination slot 2. CAP indices are
/// zero-based; the final near-or-facing quad carries the array's LAST marker.
static WorldCollisionTrigger _gDryfieldSouvenirShopActionTriggers[12] = {
    { NULL, NULL, NULL, { 1920, -57, -3760, 0 }, { { -544, 0, -240, 0 }, { 544, 0, -240, 0 }, { -544, 0, 240, 0 }, { 544, 0, 240, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 593, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_BACK_STREET, (1 << 4) | 2, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2832, -64, -704, 0 }, { { -1088, 0, -272, 0 }, { 1088, 0, -272, 0 }, { -1088, 0, 272, 0 }, { 1088, 0, 272, 0 } }, { 0, 4109, 0, 0 }, { -201, 0, -4091, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4176, -64, -3296, 0 }, { { -1520, 0, -272, 0 }, { 1520, 0, -272, 0 }, { -1520, 0, 272, 0 }, { 1520, 0, 272, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, ONE, 0 }, 1541, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5296, -64, -752, 0 }, { { -208, 0, -1024, 0 }, { 208, 0, -1024, 0 }, { -208, 0, 1024, 0 }, { 208, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4091, 0, 201, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1023, -64, -1728, 0 }, { { -336, 0, -640, 0 }, { 336, 0, -640, 0 }, { -336, 0, 640, 0 }, { 336, 0, 640, 0 } }, { 0, 4098, 0, 0 }, { 4095, 0, 0, 0 }, 721, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4560, -64, -1232, 0 }, { { -880, 0, -352, 0 }, { 880, 0, -352, 0 }, { -880, 0, 352, 0 }, { 880, 0, 352, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, ONE, 0 }, 947, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2736, -64, -1696, 0 }, { { -848, 0, -176, 0 }, { 848, 0, -176, 0 }, { -848, 0, 176, 0 }, { 848, 0, 176, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, ONE, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2720, -64, -2624, 0 }, { { -848, 0, -176, 0 }, { 848, 0, -176, 0 }, { -848, 0, 176, 0 }, { 848, 0, 176, 0 } }, { 0, 4098, 0, 0 }, { -201, 0, -4091, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1056, -64, -3104, 0 }, { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 3973, 0, 995, 0 }, 561, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1392, -64, -736, 0 }, { { -576, 0, -176, 0 }, { 576, 0, -176, 0 }, { -576, 0, 176, 0 }, { 576, 0, 176, 0 } }, { 0, 4097, 0, 0 }, { -201, 0, -4091, 0 }, 600, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4720, -64, -448, 0 }, { { -784, 0, -176, 0 }, { 784, 0, -176, 0 }, { -784, 0, 176, 0 }, { 784, 0, 176, 0 } }, { 0, 4099, 0, 0 }, { 200, 0, -4091, 0 }, 801, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4192, -64, -1824, 0 }, { { -1072, 0, 0, 0 }, { 880, 0, -32, 0 }, { -656, 0, 672, 0 }, { 880, 0, 672, 0 } }, { 0, 4113, 0, 0 }, { 0, 0, ONE, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Six room-space point lights contributing in every view.
///
/// Intensities use ONE (4096) for unity; radii use game-coordinate units.
/// Coordinate binding, composition and attenuation keep these records mutable.
static WorldCoordPointLight _gDryfieldSouvenirShopPointLights[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4500, -1580, -3620 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3485, 3485, 3485 }, { 0, 0 } }, 1923, 3384 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2001, -1477, -3631 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3482, 3482, 3482 }, { 0, 0 } }, 1899, 3404 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1395, -1582, 5 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1764, 2626 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4491, -1580, -16 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1759, 2543 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 921, -393, -2198 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1556, 1474 }, { 0, 0 } }, 749, 1009 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3700, -120, -2840 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 641, 879 },
};

/// The room's six point lights, with no directional or cone lights.
static WorldCoordRoomLights _gDryfieldSouvenirShopLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldSouvenirShopPointLights),
    .pointLights           = _gDryfieldSouvenirShopPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Default-layout actor recipes for Blood Suckler entry 40 and Mind Suckler entry 15.
///
/// Placement entries select these IDs; AREA_PLACEMENT_END ends the resource list.
static AreaResource _gDryfieldSouvenirShopDefaultActorResources[3] = {
    { 40, 40, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor04000BloodSucklerTaskDesc },
    { 15, 15, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor01500MindSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldSouvenirShopAreaVariants[DRYFIELD_SOUVENIR_SHOP_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapDryfieldSouvenirShopDefaultPlacements, .resources = _gDryfieldSouvenirShopDefaultActorResources },
};

/// Cue-2 footstep bases in the loaded type-1 bank; cue 1 uses the following entry.
enum {
    DRYFIELD_SOUVENIR_SHOP_SURFACE_CLASS_1_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_SOUVENIR_SHOP_SURFACE_CLASS_1_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_SOUVENIR_SHOP_SURFACE_CLASS_2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x31,
    DRYFIELD_SOUVENIR_SHOP_SURFACE_CLASS_2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x33,
};

/// Surface-class-1 footstep requests, sharing the walking pair with scripted jumps.
static WorldCollisionFootstepSounds _gDryfieldSouvenirShopSurfaceClass1FootstepSounds = {
    .walk         = DRYFIELD_SOUVENIR_SHOP_SURFACE_CLASS_1_WALK_SOUND,
    .run          = DRYFIELD_SOUVENIR_SHOP_SURFACE_CLASS_1_RUN_SOUND,
    .scriptedJump = DRYFIELD_SOUVENIR_SHOP_SURFACE_CLASS_1_WALK_SOUND,
};

/// Surface-class-2 footstep requests, sharing the walking pair with scripted jumps.
static WorldCollisionFootstepSounds _gDryfieldSouvenirShopSurfaceClass2FootstepSounds = {
    .walk         = DRYFIELD_SOUVENIR_SHOP_SURFACE_CLASS_2_WALK_SOUND,
    .run          = DRYFIELD_SOUVENIR_SHOP_SURFACE_CLASS_2_RUN_SOUND,
    .scriptedJump = DRYFIELD_SOUVENIR_SHOP_SURFACE_CLASS_2_WALK_SOUND,
};

/// Silent blocking surface shared by classes 0 and 5..7, allowing impacts and pushback.
static WorldCollisionSurfaceProperties _gDryfieldSouvenirShopDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Silent class-3 policy passing probes and ignoring impacts while retaining pushback.
///
/// Published for class 3, which the room's initial grid faces do not select.
static WorldCollisionSurfaceProperties _gDryfieldSouvenirShopSurfaceClass3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Separate silent class-4 blocking policy, allowing impacts and pushback.
///
/// Published for class 4, which the room's initial grid faces do not select.
static WorldCollisionSurfaceProperties _gDryfieldSouvenirShopSurfaceClass4Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class-1 blocking policy with its footstep pair, allowing impacts and pushback.
static WorldCollisionSurfaceProperties _gDryfieldSouvenirShopSurfaceClass1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldSouvenirShopSurfaceClass1FootstepSounds,
};

/// Class-2 blocking policy with its footstep pair, allowing impacts and pushback.
static WorldCollisionSurfaceProperties _gDryfieldSouvenirShopSurfaceClass2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldSouvenirShopSurfaceClass2FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldSouvenirShopSurfacePropertiesByClass[8] = {
    &_gDryfieldSouvenirShopDefaultSurfaceProperties,
    &_gDryfieldSouvenirShopSurfaceClass1Properties,
    &_gDryfieldSouvenirShopSurfaceClass2Properties,
    &_gDryfieldSouvenirShopSurfaceClass3Properties,
    &_gDryfieldSouvenirShopSurfaceClass4Properties,
    &_gDryfieldSouvenirShopDefaultSurfaceProperties,
    &_gDryfieldSouvenirShopDefaultSurfaceProperties,
    &_gDryfieldSouvenirShopDefaultSurfaceProperties,
};

/// Refuses every key-item use in this room without consuming the item.
///
/// Returns 0 so inventory displays its "No use now" notice. `itemId` is the
/// inventory item's integer ID; all callback arguments are ignored.
static s32 _dryfieldSouvenirShopRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Permits a room transition with the requested destination unchanged.
///
/// Copies the complete eight-byte request into the reply and returns 1 in both
/// query and execute modes. Borrows non-null, readable request and writable
/// reply records through synchronous dispatch; they may be the same object.
/// Retains neither pointer and performs no transition effects itself.
static s32 _dryfieldSouvenirShopResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_SOUVENIR_SHOP_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_SOUVENIR_SHOP_TRANSITION_ALLOWED;
}

/// Ignores room commands without starting an action.
///
/// Both integer command payload words are unused; the result is always 0.
static s32 _dryfieldSouvenirShopIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores direction-trigger room actions and returns 0.
///
/// The borrowed action request is neither read nor retained. The sender passes
/// zero as the second payload word and ignores the result.
static s32 _dryfieldSouvenirShopIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the room task and its message handlers, then enters its idle state.
///
/// Called in state 0 with a live task. The loaded room's table is borrowed
/// for subsequent synchronous messages while the registered task remains alive.
static void _dryfieldSouvenirShopInitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldSouvenirShopMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the initialized room task idle until its state changes externally.
///
/// State 1 preserves the task and its message table; state 2 releases the task.
static void _dryfieldSouvenirShopIdleRoomTask(Task* task)
{
}

/// Room-task states 0 initialize, 1 remain available for messages, and 2 release the task.
///
/// Copied whole before unchecked state dispatch in 0..2; all three callback
/// addresses are borrowed from loaded code. There is no terminator.
static const TaskFuncTable3 _gDryfieldSouvenirShopRoomTaskStates = {
    { _dryfieldSouvenirShopInitRoomTask, _dryfieldSouvenirShopIdleRoomTask, taskKill },
};

void dryfieldSouvenirShopRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gDryfieldSouvenirShopRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_grey_prism.inc.c"

void dryfieldSouvenirShopLightPrismsTask(Task* task)
{
    enum { DRYFIELD_SOUVENIR_SHOP_PRISM_CORNER_COUNT = 8 };

    const GfxCoord* coord = task->extra.coordBody->coord;

    _glowDrawGreyPrism(coord, 0);
    _glowDrawGreyPrism(coord, DRYFIELD_SOUVENIR_SHOP_PRISM_CORNER_COUNT);
}
