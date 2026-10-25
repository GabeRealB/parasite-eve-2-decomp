#include "rooms/dryfield_night_back_street.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00100.h"
#include "actors/actor_01500.h"
#include "actors/actor_02500.h"
#include "actors/actor_03700.h"
#include "actors/actor_05700.h"
#include "actors/actor_300700.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
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
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/back_street.h"

// World-radius inputs to the flare and shaft projection formulas.
enum { DRYFIELD_NIGHT_BACK_STREET_FLARE_RADIUS_SCALE = 0x300,
       DRYFIELD_NIGHT_BACK_STREET_SHAFT_RADIUS_SCALE = 0x100 };

static s32 _dryfieldNightBackStreetRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 secondArg);
static s32 _dryfieldNightBackStreetIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 executionMode);
static s32 _dryfieldNightBackStreetIgnoreRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg);

static WorldCollisionGrid         _gDryfieldNightBackStreetCollisionGrid;
static WorldCollisionTrigger      _gDryfieldNightBackStreetViewBoundaries[6];
static WorldCollisionTrigger      _gDryfieldNightBackStreetActionTriggers[10];
static WorldCoordRoomAmbientEntry _gDryfieldNightBackStreetAmbientByView[6];
static WorldCoordRoomLights       _gDryfieldNightBackStreetRoomLights;

/// Room-task message interface, borrowed from this overlay until receiver teardown.
///
/// Transition resolution, refused key-item use, direction actions and CAP commands.
/// The final `TASK_MESSAGE_TABLE_END` record terminates dispatch; it is not a handler.
static const TaskMessageEntry _gDryfieldNightBackStreetMessageTable[5] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantResolveBackStreet },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightBackStreetRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightBackStreetIgnoreRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightBackStreetIgnoreCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// World-space shaft endpoints and flare positions for the night back street.
///
/// Integer game-coordinate units. Points 0/1 and 2/3 form the two shafts in
/// views 2/3; 4/5 are those views' texture-0 flares. Points 6/7 are view 3's
/// texture-1 flares, and 8/9 are the texture-1 flares shared by views 4/5.
/// Drawing borrows the points while this overlay is loaded.
static SVECTOR _gDryfieldNightBackStreetGlowPoints[10] = {
    { -9920, -2300, 5110, 0 },
    { -9920, -2300, 3900, 0 },
    { -9850, -2330, 5110, 0 },
    { -9850, -2330, 3900, 0 },
    { -8190, -1800, 5850, 0 },
    { -6800, -1800, 5850, 0 },
    { -2110, -1800, 6000, 0 },
    { -820, -1800, 6000, 0 },
    { 8870, -1800, 6000, 0 },
    { 10150, -1800, 6000, 0 },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources gDryfieldNightBackStreetRoomCollisionResources[1] = {
    { .grid = &_gDryfieldNightBackStreetCollisionGrid, .viewBoundaryTriggers = _gDryfieldNightBackStreetViewBoundaries, .actionTriggers = _gDryfieldNightBackStreetActionTriggers, .occluders = NULL },
};

WorldCoordRoomLighting gDryfieldNightBackStreetRoomLighting[1] = {
    { .lights = &_gDryfieldNightBackStreetRoomLights, .ambientTable = _gDryfieldNightBackStreetAmbientByView },
};

u8* gDryfieldNightBackStreetViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldNightBackStreetRoomViewCounts[1] = { ARRAY_SIZE(gDryfieldNightBackStreetViewCameras) };

/// Map-marker nibble IDs for these warp endpoints.
enum {
    DRYFIELD_NIGHT_BACK_STREET_MAP_FLAG_GENERAL_STORE     = 470,
    DRYFIELD_NIGHT_BACK_STREET_MAP_FLAG_SOUVENIR_SHOP     = 469,
    DRYFIELD_NIGHT_BACK_STREET_MAP_FLAG_WAREHOUSE         = 468,
    DRYFIELD_NIGHT_BACK_STREET_MAP_FLAG_DILAPIDATED_HOUSE = 467,
};

DirectionWarpEntry gDryfieldNightBackStreetWarpEntries[4] = {
    { .player = { { .word = 1024 }, -9597, 0, 4667 }, .unknown_10 = { 0, 0, 0, 0 }, .companion = { { .word = 1024 }, -8696, 0, 5479 }, .unknown_24 = { 0, 0, 0, 0 }, .arrivalSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 2), .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 1), .blockedSound = DIRECTION_WARP_SOUND_NONE, .initialView = 2, .flags = DIRECTION_WARP_FLAG_NONE, .mapFlagId = DRYFIELD_NIGHT_BACK_STREET_MAP_FLAG_GENERAL_STORE },
    { .player = { { .word = 2048 }, -7539, 0, 5536 }, .unknown_10 = { 0, 0, 0, 0 }, .companion = { { .word = 1024 }, -8028, 0, 5175 }, .unknown_24 = { 0, 0, 0, 0 }, .arrivalSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 4), .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 3), .blockedSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 5), .initialView = 2, .flags = DIRECTION_WARP_FLAG_NONE, .mapFlagId = DRYFIELD_NIGHT_BACK_STREET_MAP_FLAG_SOUVENIR_SHOP },
    { .player = { { .word = 2048 }, -1423, 0, 5449 }, .unknown_10 = { 0, 0, 0, 0 }, .companion = { { .word = 2048 }, -2346, 0, 5223 }, .unknown_24 = { 0, 0, 0, 0 }, .arrivalSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 2), .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 1), .blockedSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 5), .initialView = 3, .flags = DIRECTION_WARP_FLAG_NONE, .mapFlagId = DRYFIELD_NIGHT_BACK_STREET_MAP_FLAG_WAREHOUSE },
    { .player = { { .word = 2048 }, 9463, 2, 5532 }, .unknown_10 = { 0, 0, 0, 0 }, .companion = { { .word = 3072 }, 9800, 2, 4668 }, .unknown_24 = { 0, 0, 0, 0 }, .arrivalSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 2), .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 1), .blockedSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 5), .initialView = 5, .flags = DIRECTION_WARP_FLAG_NONE, .mapFlagId = DRYFIELD_NIGHT_BACK_STREET_MAP_FLAG_DILAPIDATED_HOUSE },
};

/// Room-space collision normals, with 4096 representing unit length.
static SVECTOR _gDryfieldNightBackStreetCollisionGridNormals[19] = {
#include "assets/dryfield_night_back_street_collision_03574_normals.inc"
};

/// Collision-mesh vertices in signed integer game-coordinate units.
static SVECTOR _gDryfieldNightBackStreetCollisionGridVerts[72] = {
#include "assets/dryfield_night_back_street_collision_03574_verts.inc"
};

/// Indexed collision quads selecting room-local surface classes 0..2.
static WorldCollisionGridFace _gDryfieldNightBackStreetCollisionGridFaces[32] = {
#include "assets/dryfield_night_back_street_collision_03574_faces.inc"
};

/// Packed signed face-index lists for the grid cells, each ending with -1.
static s16 _gDryfieldNightBackStreetCollisionGridCells[224] = {
#include "assets/dryfield_night_back_street_collision_03574_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldNightBackStreetCollisionGridCells` (224 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_NIGHT_BACK_STREET_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightBackStreetCollisionGridCells[(halfwordOffset)])

/// X-major pointers into the face-list pool for the 7 by 4 collision grid.
static s16* _gDryfieldNightBackStreetCollisionGridTable[28] = {
#include "assets/dryfield_night_back_street_collision_03574_table.inc"
};
#undef DRYFIELD_NIGHT_BACK_STREET_CELL_FACE_LIST_AT_OFFSET

/// The night back street's collision mesh and 7 by 4 XZ cell index.
///
/// Cells span 4000 game units from origin (-11006, -100) on XZ. Setup binds
/// the writable descriptor to the current view. Keep this overlay loaded
/// while the descriptor, mesh pools and cell lists are borrowed.
static WorldCollisionGrid _gDryfieldNightBackStreetCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightBackStreetCollisionGridNormals,
    .vertices    = _gDryfieldNightBackStreetCollisionGridVerts,
    .faces       = _gDryfieldNightBackStreetCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightBackStreetCollisionGridTable,
    .xBias       = 0x2AFE,
    .zBias       = 100,
    .cellCountX  = 7,
    .cellCountZ  = 4,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightBackStreetCollisionGridFaces),
};

ViewCamera gDryfieldNightBackStreetViewCameras[5] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -850, 0x7148, -4850 } }, 329 },
    { { { { 696, 0, 4036 }, { -1070, 3949, 184 }, { -3892, -1085, 671 } }, { 4016, 385, -4128 } }, 230 },
    { { { { 681, 0, 4038 }, { 402, 4075, -67 }, { -4018, 408, 677 } }, { -2416, 1408, -4201 } }, 257 },
    { { { { 615, 0, -4049 }, { -873, 3999, -132 }, { 3954, 883, 600 } }, { 2448, 1600, -4230 } }, 230 },
    { { { { 173, 0, -4092 }, { -1464, 3824, -62 }, { 3821, 1465, 162 } }, { -2046, 2454, -4167 } }, 257 },
};

/// View 1's empty sprite list, retaining decoded background strips.
///
/// The zero-count batch precedes the terminator; its address also fills the
/// view's empty source slot. Writable storage lives with this room overlay.
static SpriteBatch _gDryfieldNightBackStreetView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's empty sprite list, retaining decoded background strips.
///
/// The zero-count batch precedes the terminator; its address also fills the
/// view's empty source slot. Writable storage lives with this room overlay.
static SpriteBatch _gDryfieldNightBackStreetView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's empty sprite list, retaining decoded background strips.
///
/// The zero-count batch precedes the terminator; its address also fills the
/// view's empty source slot. Writable storage lives with this room overlay.
static SpriteBatch _gDryfieldNightBackStreetView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's five textured foreground rectangles and sorting depths.
///
/// Positions and dimensions are pixels relative to the view centre; UVs are
/// texels and depths use the renderer's unscaled sorting domain. The batch
/// selects every element; writable source storage lives with this overlay.
static SpriteSource _gDryfieldNightBackStreetView4SpriteSources[5] = {
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -64, 1295, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 72, -72, 1258, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 152 } }, 80, -72, 1125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, 96, -88, 1022, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 216 } }, 120, -96, 785, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

/// View 4's background-strip selector and complete foreground-sprite range.
///
/// A zero-count first batch retains the background, followed by the five
/// source elements and the terminator. Writable storage lives with this overlay.
static SpriteBatch _gDryfieldNightBackStreetView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightBackStreetView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's five textured foreground rectangles and sorting depths.
///
/// Positions and dimensions are pixels relative to the view centre; UVs are
/// texels and depths use the renderer's unscaled sorting domain. The batch
/// selects every element; writable source storage lives with this overlay.
static SpriteSource _gDryfieldNightBackStreetView5SpriteSources[5] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -16, 1714, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -16, 1680, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -24, 1678, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, -16, 1669, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -8, 1663, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
};

/// View 5's background-strip selector and complete foreground-sprite range.
///
/// A zero-count first batch retains the background, followed by the five
/// source elements and the terminator. Writable storage lives with this overlay.
static SpriteBatch _gDryfieldNightBackStreetView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightBackStreetView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightBackStreetSpriteViews[5] = {
    { { .empty = _gDryfieldNightBackStreetView1SpriteBatches }, _gDryfieldNightBackStreetView1SpriteBatches, NULL },
    { { .empty = _gDryfieldNightBackStreetView2SpriteBatches }, _gDryfieldNightBackStreetView2SpriteBatches, NULL },
    { { .empty = _gDryfieldNightBackStreetView3SpriteBatches }, _gDryfieldNightBackStreetView3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightBackStreetView4SpriteSources }, _gDryfieldNightBackStreetView4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightBackStreetView5SpriteSources }, _gDryfieldNightBackStreetView5SpriteBatches, NULL },
};

/// Directed camera boundaries between views 2/3, 3/4 and 4/5.
///
/// Each pair covers both crossing directions. The final LAST-marked quad is
/// included when setup links the array and binds its view coordinate. Positions
/// and radii use game units; normals use 4096 per unit. Keep storage loaded
/// until the runtime trigger list is cleared.
static WorldCollisionTrigger _gDryfieldNightBackStreetViewBoundaries[6] = {
    { NULL, NULL, NULL, { -6272, -3296, 4752, 0 }, { { 0, -4320, -1584, 0 }, { 0, -4320, 1584, 0 }, { 0, 4320, -1584, 0 }, { 0, 4320, 1584, 0 } }, { 4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5888, -3504, 4672, 0 }, { { 0, -4528, 1584, 0 }, { 0, -4528, -1584, 0 }, { 0, 4528, 1584, 0 }, { 0, 4528, -1584, 0 } }, { -4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -192, -4160, 4576, 0 }, { { -236, -5184, 1562, 0 }, { 228, -5184, -1572, 0 }, { -236, 5184, 1562, 0 }, { 228, 5184, -1572, 0 } }, { -4054, 0, -601, 0 }, { 0, 0, 4096, 0 }, 5418, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -320, -4128, 4608, 0 }, { { 229, -5152, -1571, 0 }, { -235, -5152, 1563, 0 }, { 229, 5152, -1571, 0 }, { -235, 5152, 1563, 0 } }, { 4053, 0, 600, 0 }, { 0, 0, 4096, 0 }, 5369, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4256, -4096, 4704, 0 }, { { 0, -5120, -1584, 0 }, { 0, -5120, 1584, 0 }, { 0, 5120, -1584, 0 }, { 0, 5120, 1584, 0 } }, { 4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 5345, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4416, -4352, 4736, 0 }, { { 0, -5376, 1584, 0 }, { 0, -5376, -1584, 0 }, { 0, 5376, 1584, 0 }, { 0, 5376, -1584, 0 } }, { -4126, 0, 0, 0 }, { 0, 0, 4096, 0 }, 5585, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// IDs stored in action records; warp pairs pack departure then arrival in four-bit nibbles.
enum {
    DRYFIELD_NIGHT_BACK_STREET_TRIGGER_WARP_TO_GENERAL_STORE     = (1 << 4) | 2,
    DRYFIELD_NIGHT_BACK_STREET_TRIGGER_WARP_TO_SOUVENIR_SHOP     = (2 << 4) | 1,
    DRYFIELD_NIGHT_BACK_STREET_TRIGGER_WARP_TO_WAREHOUSE         = (3 << 4) | 1,
    DRYFIELD_NIGHT_BACK_STREET_TRIGGER_WARP_TO_DILAPIDATED_HOUSE = (4 << 4) | 1,
    DRYFIELD_NIGHT_BACK_STREET_TRIGGER_CAP_COMMAND_10            = 10,
    DRYFIELD_NIGHT_BACK_STREET_TRIGGER_CAP_COMMAND_8             = 8,
    DRYFIELD_NIGHT_BACK_STREET_TRIGGER_CAP_COMMAND_4             = 4,
    DRYFIELD_NIGHT_BACK_STREET_TRIGGER_CAP_COMMAND_1             = 1,
    DRYFIELD_NIGHT_BACK_STREET_TRIGGER_CAP_COMMAND_6             = 6,
};

/// 10 interaction quads: 4 warps, 6 CAP commands.
///
/// Room-local origins/corners and radii use game units; normals use 4096 per unit.
/// Warp bytes select destination area and departure/arrival endpoint nibbles;
/// CAP IDs index the loaded command table or route through the room-message sentinel.
/// Room binding enables all records through LAST; keep them live until unlinking.
static WorldCollisionTrigger _gDryfieldNightBackStreetActionTriggers[10] = {
    { NULL, NULL, NULL, { -9648, -55, 4496, 0 }, { { -400, 0, -560, 0 }, { 400, 0, -560, 0 }, { -400, 0, 560, 0 }, { 400, 0, 560, 0 } }, { 0, 4102, 0, 0 }, { ONE, 0, 0, 0 }, 686, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE, DRYFIELD_NIGHT_BACK_STREET_TRIGGER_WARP_TO_GENERAL_STORE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7872, -55, 5776, 0 }, { { 832, 0, -352, 0 }, { 832, 0, 352, 0 }, { -832, 0, -352, 0 }, { -832, 0, 352, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, -ONE, 0 }, 902, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP, DRYFIELD_NIGHT_BACK_STREET_TRIGGER_WARP_TO_SOUVENIR_SHOP, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1616, -48, 5744, 0 }, { { 688, 0, -352, 0 }, { 688, 0, 352, 0 }, { -688, 0, -352, 0 }, { -688, 0, 352, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -ONE, 0 }, 770, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE, DRYFIELD_NIGHT_BACK_STREET_TRIGGER_WARP_TO_WAREHOUSE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9424, -48, 5712, 0 }, { { 784, 0, -384, 0 }, { 784, 0, 384, 0 }, { -784, 0, -384, 0 }, { -784, 0, 384, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -ONE, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE, DRYFIELD_NIGHT_BACK_STREET_TRIGGER_WARP_TO_DILAPIDATED_HOUSE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2CA0, -64, 5024, 0 }, { { -1072, 0, -416, 0 }, { 1072, 0, -416, 0 }, { -1072, 0, 416, 0 }, { 1072, 0, 416, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -ONE, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_BACK_STREET_TRIGGER_CAP_COMMAND_10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6113, -64, 5569, 0 }, { { -752, 0, -432, 0 }, { 752, 0, -432, 0 }, { -752, 0, 432, 0 }, { 752, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, -4092, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_BACK_STREET_TRIGGER_CAP_COMMAND_8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8288, -64, 2144, 0 }, { { -1696, 0, -208, 0 }, { 1760, 0, -208, 0 }, { -800, 0, 1008, 0 }, { 736, 0, 1008, 0 } }, { 0, 4104, 0, 0 }, { 201, 0, -4092, 0 }, 1768, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_BACK_STREET_TRIGGER_CAP_COMMAND_4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4576, -64, 5536, 0 }, { { -752, 0, -432, 0 }, { 752, 0, -432, 0 }, { -752, 0, 432, 0 }, { 752, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, -4092, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_BACK_STREET_TRIGGER_CAP_COMMAND_1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5152, -64, 5664, 0 }, { { 1056, 0, -384, 0 }, { 1056, 0, 384, 0 }, { -1056, 0, -384, 0 }, { -1056, 0, 384, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -ONE, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_BACK_STREET_TRIGGER_CAP_COMMAND_6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2740, -64, 5360, 0 }, { { -304, 0, -720, 0 }, { 656, 0, -720, 0 }, { -656, 0, 720, 0 }, { 304, 0, 720, 0 } }, { 0, 4112, 0, 0 }, { -4091, 0, 200, 0 }, 972, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_BACK_STREET_TRIGGER_CAP_COMMAND_10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// The night back street's six point lights for model shading in every view.
///
/// Positions and falloff radii use integer world units: full strength within
/// 1000 units, fading with squared distance to zero at 3000. RGB intensities use
/// 12 fractional bits (`ONE` is full intensity). The loaded room overlay owns
/// these writable records; coordinate updates parent and compose their
/// transforms, and lighting queries overwrite attenuation. Borrowed pointers
/// must not survive unloading the overlay.
static WorldCoordPointLight _gDryfieldNightBackStreetPointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6797, -1800, 5711 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3522, 3522, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8205, -1800, 5708 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3522, 3522, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -807, -1800, 5862 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3522, 3522, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8855, -1800, 5862 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3522, 3522, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 10160, -1800, 5862 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3522, 3522, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -9762, -2100, 4476 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3522, 3522, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 3000,
    },
};

/// The night back street's six point lights, with no directional or cone lights.
///
/// Model-light queries borrow this collection independently of the view.
/// Coordinates and attenuation in its point-light array remain writable;
/// neither collection nor light pointers may outlive the loaded room overlay.
static WorldCoordRoomLights _gDryfieldNightBackStreetRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightBackStreetPointLights),
    .pointLights           = _gDryfieldNightBackStreetPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Variant 1's scorpion file and spawning recipe, followed by the resource terminator.
///
/// Placement ID 25 selects actor-slot-1 file 25 and task descriptor 0.
/// The room table and actor overlay must stay loaded while loading or spawning.
static AreaResource _gDryfieldNightBackStreetVariant1Resources[2] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102500Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's Desert Chaser and Mind Suckler resource recipes and terminator.
///
/// Placement IDs 1/15 select files 1/15 in actor slots 4/3, respectively,
/// each using descriptor 0. Borrow the room and actor overlays while spawning.
static AreaResource _gDryfieldNightBackStreetVariant3Resources[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor00100DesertChaserTaskDesc },
    { 15, 15, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor01500MindSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 4's Desert Chaser and Moth resource recipes and terminator.
///
/// Placement IDs 1/8 select files 1/7 in actor slots 4/3, respectively,
/// each using descriptor 0. Borrow the room and actor overlays while spawning.
static AreaResource _gDryfieldNightBackStreetVariant4Resources[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor00100DesertChaserTaskDesc },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor300700MothTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 7's Rook Golem file and spawning recipe, followed by the resource terminator.
///
/// Placement ID 57 selects actor-slot-1 file 57 and task descriptor 0.
/// The room table and actor overlay must stay loaded while loading or spawning.
static AreaResource _gDryfieldNightBackStreetVariant7Resources[2] = {
    { 57, 57, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor05700RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 21's Bat file and spawning recipe, followed by the resource terminator.
///
/// Placement ID 37 selects actor-slot-1 file 37 and task descriptor 0.
/// The room table and actor overlay must stay loaded while loading or spawning.
static AreaResource _gDryfieldNightBackStreetVariant21Resources[2] = {
    { 37, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor103700BatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldNightBackStreetAreaVariants[22] = {
    [1]  = { .placements = gMapDryfieldFullBackStreetVariant1Placements, .resources = _gDryfieldNightBackStreetVariant1Resources },
    [3]  = { .placements = gMapDryfieldFullBackStreetVariant3Placements, .resources = _gDryfieldNightBackStreetVariant3Resources },
    [4]  = { .placements = gMapDryfieldFullBackStreetVariant4Placements, .resources = _gDryfieldNightBackStreetVariant4Resources },
    [7]  = { .placements = gMapDryfieldFullBackStreetVariant7Placements, .resources = _gDryfieldNightBackStreetVariant7Resources },
    [21] = { .placements = gMapDryfieldFullBackStreetVariant21Placements, .resources = _gDryfieldNightBackStreetVariant21Resources },
};

/// Minimum ambient RGB levels for the night back street's five views.
///
/// Entry zero holds the view count; entries 1..5 hold RGB and stored luma in
/// units of 1/16 of an 8-bit colour level. Lighting reads this overlay-owned
/// table without modifying it; pointers must not survive unloading the room.
static WorldCoordRoomAmbientEntry _gDryfieldNightBackStreetAmbientByView[6] = {
    { .viewCount = ARRAY_SIZE(_gDryfieldNightBackStreetAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 618, 618, 615, 617 } },
    { .color = { 616, 618, 617, 617 } },
    { .color = { 616, 618, 618, 617 } },
    { .color = { 616, 617, 617, 616 } },
};

// Type-1 sound requests select entries in the current area bank.
enum {
    DRYFIELD_NIGHT_BACK_STREET_WALK_FOOTSTEP_SOUND = 0x1000001D,
    DRYFIELD_NIGHT_BACK_STREET_RUN_FOOTSTEP_SOUND  = 0x1000001F,
};

/// Walking, running and scripted-jump cue bases for surface classes 1/2.
///
/// Walking and jumps use base 29; running uses base 31. Animation cue 1 adds
/// one and companions add 100. Surface policies borrow this table while the
/// room overlay is loaded; playback also requires the current sound bank.
static WorldCollisionFootstepSounds _gDryfieldNightBackStreetFootstepSounds = {
    .walk         = DRYFIELD_NIGHT_BACK_STREET_WALK_FOOTSTEP_SOUND,
    .run          = DRYFIELD_NIGHT_BACK_STREET_RUN_FOOTSTEP_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_BACK_STREET_WALK_FOOTSTEP_SOUND,
};

/// Classes 0 and 3..7: blocked probes, enabled weapon impacts and silent footsteps.
///
/// Applies pushback. The first byte retains its authored zero; its role is
/// unproven. The class table borrows this record while the room is loaded.
static WorldCollisionSurfaceProperties _gDryfieldNightBackStreetBlockingSilentSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1: blocked probes, enabled weapon impacts and walking/running footstep cues.
///
/// Applies pushback. The first byte retains its authored zero; its role is
/// unproven. Both policy and borrowed sound table live with this room overlay.
static WorldCollisionSurfaceProperties _gDryfieldNightBackStreetBlockingFootstepSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightBackStreetFootstepSounds,
};

/// Class 2: probe passage and suppressed weapon impacts, with walking/running cues.
///
/// Applies pushback independently of probe passage. The first byte's role is
/// unproven and its zero is retained. Policy and sounds live with this overlay.
static WorldCollisionSurfaceProperties _gDryfieldNightBackStreetPassThroughFootstepSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightBackStreetFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightBackStreetSurfacePropertiesByClass[8] = {
    &_gDryfieldNightBackStreetBlockingSilentSurface,
    &_gDryfieldNightBackStreetBlockingFootstepSurface,
    &_gDryfieldNightBackStreetPassThroughFootstepSurface,
    &_gDryfieldNightBackStreetBlockingSilentSurface,
    &_gDryfieldNightBackStreetBlockingSilentSurface,
    &_gDryfieldNightBackStreetBlockingSilentSurface,
    &_gDryfieldNightBackStreetBlockingSilentSurface,
    &_gDryfieldNightBackStreetBlockingSilentSurface,
};

static void _dryfieldNightBackStreetInitRoomTask(Task* task);
static void _dryfieldNightBackStreetIdleState(Task* task);

#include "../../shared/back_street_event_msg.inc.c"

static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);
static void _glowDrawShaft(const SVECTOR worldPoints[2], s32 radiusScale);

/// Rejects key-item use in the night back street, returning the menu's unusable result.
///
/// Neither payload is read; the room consumes no item and retains no arguments.
static s32 _dryfieldNightBackStreetRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Ignores room commands and their execution modes, returning zero.
static s32 _dryfieldNightBackStreetIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 executionMode)
{
    return 0;
}

/// Ignores room-action requests and returns zero.
///
/// The request is borrowed during synchronous dispatch; no payload is read or retained.
static s32 _dryfieldNightBackStreetIgnoreRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg)
{
    return 0;
}

/// Registers the night back street's room receiver and advances it to idle state 1.
///
/// Requires the live room task in state 0. The installed message table is
/// borrowed for the room task's lifetime.
static void _dryfieldNightBackStreetInitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldNightBackStreetMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state += 1;
}

/// Keeps the initialized room task available for messages until its state changes.
static void _dryfieldNightBackStreetIdleState(Task* task)
{
}

/// Room-task lifecycle: state 0 initializes, state 1 waits or updates, state 2 releases.
///
/// The dispatcher copies all three callback pointers before indexing by `Task::state`.
/// The selector must be in 0..2; there is no terminator or bounds check.
/// Keep this overlay loaded until the room task is released.
static const TaskFuncTable3 _gDryfieldNightBackStreetRoomTaskStates = {
    { _dryfieldNightBackStreetInitRoomTask, _dryfieldNightBackStreetIdleState, taskKill },
};

void dryfieldNightBackStreetRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gDryfieldNightBackStreetRoomTaskStates;
    handlers.funcs[task->state](task);
}

/// Draws view 2's two textured flares and two grey light shafts, also used by view 3.
///
/// Requires the active view transform, flare textures, an initialized scratch
/// stack and space in the current frame's primitive arena and ordering table.
/// Accepted points must have nonzero camera-Z / 4 depths. Points are borrowed
/// only during drawing; queued packets use the frame's arena until GPU completion.
static inline void _dryfieldNightBackStreetDrawView2Glows(void)
{
    enum {
        DRYFIELD_NIGHT_BACK_STREET_VIEW_2_FIRST_SHAFT_START  = 0,
        DRYFIELD_NIGHT_BACK_STREET_VIEW_2_SECOND_SHAFT_START = 2,
        DRYFIELD_NIGHT_BACK_STREET_VIEW_2_FIRST_FLARE_POINT  = 4,
        DRYFIELD_NIGHT_BACK_STREET_VIEW_2_SECOND_FLARE_POINT = 5,
        DRYFIELD_NIGHT_BACK_STREET_VIEW_2_FLARE_TEXTURE      = 0,
    };

    // Each shaft borrows its start point and the next point as its two ends.
    _glowDrawFlare(&_gDryfieldNightBackStreetGlowPoints[DRYFIELD_NIGHT_BACK_STREET_VIEW_2_FIRST_FLARE_POINT],
                   DRYFIELD_NIGHT_BACK_STREET_VIEW_2_FLARE_TEXTURE, DRYFIELD_NIGHT_BACK_STREET_FLARE_RADIUS_SCALE);
    _glowDrawFlare(&_gDryfieldNightBackStreetGlowPoints[DRYFIELD_NIGHT_BACK_STREET_VIEW_2_SECOND_FLARE_POINT],
                   DRYFIELD_NIGHT_BACK_STREET_VIEW_2_FLARE_TEXTURE, DRYFIELD_NIGHT_BACK_STREET_FLARE_RADIUS_SCALE);
    _glowDrawShaft(&_gDryfieldNightBackStreetGlowPoints[DRYFIELD_NIGHT_BACK_STREET_VIEW_2_FIRST_SHAFT_START],
                   DRYFIELD_NIGHT_BACK_STREET_SHAFT_RADIUS_SCALE);
    _glowDrawShaft(&_gDryfieldNightBackStreetGlowPoints[DRYFIELD_NIGHT_BACK_STREET_VIEW_2_SECOND_SHAFT_START],
                   DRYFIELD_NIGHT_BACK_STREET_SHAFT_RADIUS_SCALE);
}

void dryfieldNightBackStreetDrawGlowsTask(Task* task)
{
    enum { DRYFIELD_NIGHT_BACK_STREET_EFFECTS_INITIALIZE = 0 };
    enum {
        DRYFIELD_NIGHT_BACK_STREET_VIEW_3_FIRST_FLARE_POINT    = 6,
        DRYFIELD_NIGHT_BACK_STREET_VIEW_3_SECOND_FLARE_POINT   = 7,
        DRYFIELD_NIGHT_BACK_STREET_VIEW_4_5_FIRST_FLARE_POINT  = 8,
        DRYFIELD_NIGHT_BACK_STREET_VIEW_4_5_SECOND_FLARE_POINT = 9,
    };

    if (task->state == DRYFIELD_NIGHT_BACK_STREET_EFFECTS_INITIALIZE) {
        gRoomEffectFlashId      = EFFECT_DRYFIELD_NIGHT_BACK_STREET_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_DRYFIELD_NIGHT_BACK_STREET_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_DRYFIELD_NIGHT_BACK_STREET_SPARK_BURST;
    }
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    // View 3 includes view 2's lights; the first four points are paired shaft ends.
    switch (gGameSession->location.view) {
        case 3:
            _glowDrawFlare(&_gDryfieldNightBackStreetGlowPoints[DRYFIELD_NIGHT_BACK_STREET_VIEW_3_FIRST_FLARE_POINT], 1, DRYFIELD_NIGHT_BACK_STREET_FLARE_RADIUS_SCALE);
            _glowDrawFlare(&_gDryfieldNightBackStreetGlowPoints[DRYFIELD_NIGHT_BACK_STREET_VIEW_3_SECOND_FLARE_POINT], 1, DRYFIELD_NIGHT_BACK_STREET_FLARE_RADIUS_SCALE);
            /* fallthrough */
        case 2:
            _dryfieldNightBackStreetDrawView2Glows();
            break;
        case 4:
        case 5:
            _glowDrawFlare(&_gDryfieldNightBackStreetGlowPoints[DRYFIELD_NIGHT_BACK_STREET_VIEW_4_5_FIRST_FLARE_POINT], 1, DRYFIELD_NIGHT_BACK_STREET_FLARE_RADIUS_SCALE);
            _glowDrawFlare(&_gDryfieldNightBackStreetGlowPoints[DRYFIELD_NIGHT_BACK_STREET_VIEW_4_5_SECOND_FLARE_POINT], 1, DRYFIELD_NIGHT_BACK_STREET_FLARE_RADIUS_SCALE);
            break;
    }
}

#include "../../shared/effects/glow_draw_shaft.inc.c"

#include "../../shared/effects/glow_draw_flare.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void dryfieldNightBackStreetRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void dryfieldNightBackStreetRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void dryfieldNightBackStreetRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
