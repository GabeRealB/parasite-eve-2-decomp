#include "rooms/dryfield_back_street.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00100.h"
#include "actors/actor_01500.h"
#include "actors/actor_02000.h"
#include "actors/actor_05600.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

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

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/back_street.h"

static void _dryfieldBackStreetAmbienceTask(Task* task);
static s32  _dryfieldBackStreetRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 secondArg);
static s32  _dryfieldBackStreetIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 executionMode);
static s32  _dryfieldBackStreetIgnoreRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg);

static WorldCollisionGrid    _gDryfieldBackStreetCollisionGrid;
static WorldCollisionTrigger _gDryfieldBackStreetViewBoundaries[6];
static WorldCollisionTrigger _gDryfieldBackStreetActionTriggers[11];
static WorldCoordRoomLights  _gDryfieldBackStreetRoomLights;

/// Back Street's transition, key-item, action and command message dispatch.
///
/// The room task borrows five records, including TASK_MESSAGE_TABLE_END,
/// while the overlay is loaded. Transition payloads are request/reply records;
/// actions borrow DirectionActionRequest, and the other messages carry integers.
static TaskMessageEntry _gDryfieldBackStreetRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantResolveBackStreet },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldBackStreetRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldBackStreetIgnoreRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, _dryfieldBackStreetIgnoreCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless view-dependent ambience recipe at slot 0, followed by TASK_DESC_END.
///
/// The room initializer spawns slot 0 with zero payloads. Spawning consumes the
/// descriptor; callback code stays loaded while the independent ambience runs.
static TaskDesc _gDryfieldBackStreetAmbienceTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _dryfieldBackStreetAmbienceTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources gDryfieldBackStreetRoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldBackStreetCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldBackStreetViewBoundaries,
        .actionTriggers       = _gDryfieldBackStreetActionTriggers,
        .occluders            = NULL,
    },
};

u8* gDryfieldBackStreetViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldBackStreetRoomViewCounts[1] = { ARRAY_SIZE(gDryfieldBackStreetViewCameras) };

WorldCoordRoomLighting gDryfieldBackStreetRoomLighting[1] = {
    { .lights = &_gDryfieldBackStreetRoomLights, .ambientTable = NULL },
};

enum {
    DRYFIELD_BACK_STREET_ARRIVAL_SOUND              = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BACK_STREET, 2),
    DRYFIELD_BACK_STREET_DEPARTURE_SOUND            = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BACK_STREET, 1),
    DRYFIELD_BACK_STREET_SHOP_ARRIVAL_SOUND         = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BACK_STREET, 4),
    DRYFIELD_BACK_STREET_SHOP_DEPARTURE_SOUND       = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BACK_STREET, 3),
    DRYFIELD_BACK_STREET_BLOCKED_WARP_SOUND         = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BACK_STREET, 5),
    DRYFIELD_BACK_STREET_GENERAL_STORE_MAP_FLAG     = 470,
    DRYFIELD_BACK_STREET_SOUVENIR_SHOP_MAP_FLAG     = 469,
    DRYFIELD_BACK_STREET_WAREHOUSE_MAP_FLAG         = 468,
    DRYFIELD_BACK_STREET_DILAPIDATED_HOUSE_MAP_FLAG = 467,
};

DirectionWarpEntry gDryfieldBackStreetWarpEntries[4] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -9597, 0, 4667 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -8696, 0, 5479 }, { 0, 0, 0, 0 }, DRYFIELD_BACK_STREET_ARRIVAL_SOUND, DRYFIELD_BACK_STREET_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DRYFIELD_BACK_STREET_GENERAL_STORE_MAP_FLAG },
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -7539, 0, 5536 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -8028, 0, 5175 }, { 0, 0, 0, 0 }, DRYFIELD_BACK_STREET_SHOP_ARRIVAL_SOUND, DRYFIELD_BACK_STREET_SHOP_DEPARTURE_SOUND, DRYFIELD_BACK_STREET_BLOCKED_WARP_SOUND, 2, DIRECTION_WARP_FLAG_NONE, DRYFIELD_BACK_STREET_SOUVENIR_SHOP_MAP_FLAG },
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -1423, 0, 5449 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -2346, 0, 5223 }, { 0, 0, 0, 0 }, DRYFIELD_BACK_STREET_ARRIVAL_SOUND, DRYFIELD_BACK_STREET_DEPARTURE_SOUND, DRYFIELD_BACK_STREET_BLOCKED_WARP_SOUND, 3, DIRECTION_WARP_FLAG_NONE, DRYFIELD_BACK_STREET_WAREHOUSE_MAP_FLAG },
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 9463, 2, 5532 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 9800, 2, 4668 }, { 0, 0, 0, 0 }, DRYFIELD_BACK_STREET_ARRIVAL_SOUND, DRYFIELD_BACK_STREET_DEPARTURE_SOUND, DRYFIELD_BACK_STREET_BLOCKED_WARP_SOUND, 5, DIRECTION_WARP_FLAG_NONE, DRYFIELD_BACK_STREET_DILAPIDATED_HOUSE_MAP_FLAG },
};

/// Room-space collision normals, with 4096 for unit length.
static SVECTOR _gDryfieldBackStreetCollisionGridNormals[23] = {
#include "assets/dryfield_back_street_collision_02CC4_normals.inc"
};

/// Room-space mesh vertices in signed integer game-coordinate units.
static SVECTOR _gDryfieldBackStreetCollisionGridVerts[88] = {
#include "assets/dryfield_back_street_collision_02CC4_verts.inc"
};

/// Indexed collision quads using room-local surface classes 0..2.
static WorldCollisionGridFace _gDryfieldBackStreetCollisionGridFaces[39] = {
#include "assets/dryfield_back_street_collision_02CC4_faces.inc"
};

/// Packed signed face-index lists for the 28 grid cells, each terminated by -1.
static s16 _gDryfieldBackStreetCollisionGridCells[266] = {
#include "assets/dryfield_back_street_collision_02CC4_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldBackStreetCollisionGridCells` (266 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_BACK_STREET_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldBackStreetCollisionGridCells[(halfwordOffset)])

/// X-major cell pointers into the face-list pool for the 7 by 4 grid.
static s16* _gDryfieldBackStreetCollisionGridTable[28] = {
#include "assets/dryfield_back_street_collision_02CC4_table.inc"
};
#undef DRYFIELD_BACK_STREET_CELL_FACE_LIST_AT_OFFSET

/// Back Street's collision mesh and its 7 by 4 XZ cell index.
///
/// Cells are 4000 game units wide, with origin (-11006, -100) on XZ.
/// Setup binds the writable descriptor to the current view coordinate.
/// Mesh pools and cell lists remain borrowed while this overlay is loaded.
static WorldCollisionGrid _gDryfieldBackStreetCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldBackStreetCollisionGridNormals,
    .vertices    = _gDryfieldBackStreetCollisionGridVerts,
    .faces       = _gDryfieldBackStreetCollisionGridFaces,
    .cellFaceIds = _gDryfieldBackStreetCollisionGridTable,
    .xBias       = 0x2AFE,
    .zBias       = 100,
    .cellCountX  = 7,
    .cellCountZ  = 4,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldBackStreetCollisionGridFaces),
};

ViewCamera gDryfieldBackStreetViewCameras[5] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -850, 0x7148, -4850 } }, 329 },
    { { { { 696, 0, 4036 }, { -1070, 3949, 184 }, { -3892, -1085, 671 } }, { 4016, 385, -4128 } }, 230 },
    { { { { 681, 0, 4038 }, { 402, 4075, -67 }, { -4018, 408, 677 } }, { -2416, 1408, -4201 } }, 257 },
    { { { { 615, 0, -4049 }, { -873, 3999, -132 }, { 3954, 883, 600 } }, { 2448, 1600, -4230 } }, 230 },
    { { { { 173, 0, -4092 }, { -1464, 3824, -62 }, { 3821, 1465, 162 } }, { -2046, 2454, -4167 } }, 257 },
};

/// View 1's mutable empty sprite list, retaining decoded background strips.
///
/// The zero-count first record is followed by SPRITE_BATCH_END; no source
/// elements are read. Keep this view's own list live with the room overlay.
static SpriteBatch _gDryfieldBackStreetView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's mutable empty sprite list, retaining decoded background strips.
///
/// The zero-count first record is followed by SPRITE_BATCH_END; no source
/// elements are read. Keep this view's own list live with the room overlay.
static SpriteBatch _gDryfieldBackStreetView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's mutable empty sprite list, retaining decoded background strips.
///
/// The zero-count first record is followed by SPRITE_BATCH_END; no source
/// elements are read. Keep this view's own list live with the room overlay.
static SpriteBatch _gDryfieldBackStreetView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Encoded 8-bit sprite page at (960, 0), palette row 255 and neutral Q7 RGB.
enum {
    DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION = 128,
    DRYFIELD_BACK_STREET_SPRITE_TEXTURE_PAGE       = getTPage(1, 0, 960, 0),
    DRYFIELD_BACK_STREET_SPRITE_CLUT               = getClut(0, 255),
};

/// View 4's five mutable textured rectangles for depth-sorted scene drawing.
///
/// Positions and dimensions are draw pixels, UVs are texels and depth is an
/// unscaled sorting value. RGB is neutral; command flags select opaque modulation.
/// Keep the sources live with the room while packets are built and linked.
static SpriteSource _gDryfieldBackStreetView4SpriteSources[5] = {
    { DRYFIELD_BACK_STREET_SPRITE_TEXTURE_PAGE, DRYFIELD_BACK_STREET_SPRITE_CLUT, { .fields = { 8, 96 } }, 64, -64, 1295, { .fields = { 48, 152 } }, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_BACK_STREET_SPRITE_TEXTURE_PAGE, DRYFIELD_BACK_STREET_SPRITE_CLUT, { .fields = { 8, 104 } }, 72, -72, 1258, { .fields = { 56, 152 } }, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_BACK_STREET_SPRITE_TEXTURE_PAGE, DRYFIELD_BACK_STREET_SPRITE_CLUT, { .fields = { 16, 152 } }, 80, -72, 1125, { .fields = { 48, 0 } }, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_BACK_STREET_SPRITE_TEXTURE_PAGE, DRYFIELD_BACK_STREET_SPRITE_CLUT, { .fields = { 24, 200 } }, 96, -88, 1022, { .fields = { 64, 0 } }, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_BACK_STREET_SPRITE_TEXTURE_PAGE, DRYFIELD_BACK_STREET_SPRITE_CLUT, { .fields = { 40, 216 } }, 120, -96, 785, { .fields = { 88, 0 } }, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 4's mutable range list: decoded background strips, then all five sprites.
///
/// The first count is zero; the next range uses source elements, followed by
/// SPRITE_BATCH_END. Sprites start visible and included in cached packet processing.
static SpriteBatch _gDryfieldBackStreetView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldBackStreetView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's five mutable textured rectangles for depth-sorted scene drawing.
///
/// Positions and dimensions are draw pixels, UVs are texels and depth is an
/// unscaled sorting value. RGB is neutral; command flags select opaque modulation.
/// Keep the sources live with the room while packets are built and linked.
static SpriteSource _gDryfieldBackStreetView5SpriteSources[5] = {
    { DRYFIELD_BACK_STREET_SPRITE_TEXTURE_PAGE, DRYFIELD_BACK_STREET_SPRITE_CLUT, { .fields = { 24, 24 } }, 80, -8, 1663, { .fields = { 104, 64 } }, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_BACK_STREET_SPRITE_TEXTURE_PAGE, DRYFIELD_BACK_STREET_SPRITE_CLUT, { .fields = { 8, 32 } }, 72, -16, 1680, { .fields = { 120, 0 } }, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_BACK_STREET_SPRITE_TEXTURE_PAGE, DRYFIELD_BACK_STREET_SPRITE_CLUT, { .fields = { 8, 32 } }, 64, -16, 1714, { .fields = { 120, 32 } }, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_BACK_STREET_SPRITE_TEXTURE_PAGE, DRYFIELD_BACK_STREET_SPRITE_CLUT, { .fields = { 24, 8 } }, 72, -24, 1678, { .fields = { 104, 88 } }, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
    { DRYFIELD_BACK_STREET_SPRITE_TEXTURE_PAGE, DRYFIELD_BACK_STREET_SPRITE_CLUT, { .fields = { 32, 8 } }, 80, -16, 1669, { .fields = { 96, 96 } }, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, DRYFIELD_BACK_STREET_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 5's mutable range list: decoded background strips, then all five sprites.
///
/// The first count is zero; the next range uses source elements, followed by
/// SPRITE_BATCH_END. Sprites start visible and included in cached packet processing.
static SpriteBatch _gDryfieldBackStreetView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldBackStreetView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldBackStreetSpriteViews[5] = {
    { { .empty = _gDryfieldBackStreetView1SpriteBatches }, _gDryfieldBackStreetView1SpriteBatches, NULL },
    { { .empty = _gDryfieldBackStreetView2SpriteBatches }, _gDryfieldBackStreetView2SpriteBatches, NULL },
    { { .empty = _gDryfieldBackStreetView3SpriteBatches }, _gDryfieldBackStreetView3SpriteBatches, NULL },
    { { .elements = _gDryfieldBackStreetView4SpriteSources }, _gDryfieldBackStreetView4SpriteBatches, NULL },
    { { .elements = _gDryfieldBackStreetView5SpriteSources }, _gDryfieldBackStreetView5SpriteBatches, NULL },
};

/// Six directed camera boundaries between views 2/3, 3/4 and 4/5.
///
/// Each pair covers both directions. The final record carries LAST and is
/// included in setup. Positions and radii use game units, normals 4096 per
/// unit; setup and collision mutate links, coordinate bindings, flags and hits.
/// Keep these records loaded until the view-boundary list is cleared.
static WorldCollisionTrigger _gDryfieldBackStreetViewBoundaries[6] = {
    { NULL, NULL, NULL, { -6272, -3296, 4752, 0 }, { { 0, -4320, -1584, 0 }, { 0, -4320, 1584, 0 }, { 0, 4320, -1584, 0 }, { 0, 4320, 1584, 0 } }, { 4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5888, -3504, 4672, 0 }, { { 0, -4528, 1584, 0 }, { 0, -4528, -1584, 0 }, { 0, 4528, 1584, 0 }, { 0, 4528, -1584, 0 } }, { -4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -192, -4160, 4576, 0 }, { { -236, -5184, 1562, 0 }, { 228, -5184, -1572, 0 }, { -236, 5184, 1562, 0 }, { 228, 5184, -1572, 0 } }, { -4054, 0, -601, 0 }, { 0, 0, 4096, 0 }, 5418, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -320, -4128, 4608, 0 }, { { 229, -5152, -1571, 0 }, { -235, -5152, 1563, 0 }, { 229, 5152, -1571, 0 }, { -235, 5152, 1563, 0 } }, { 4053, 0, 600, 0 }, { 0, 0, 4096, 0 }, 5369, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4256, -4096, 4704, 0 }, { { 0, -5120, -1584, 0 }, { 0, -5120, 1584, 0 }, { 0, 5120, -1584, 0 }, { 0, 5120, 1584, 0 } }, { 4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 5345, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4416, -4352, 4736, 0 }, { { 0, -5376, 1584, 0 }, { 0, -5376, -1584, 0 }, { 0, 5376, 1584, 0 }, { 0, 5376, -1584, 0 } }, { -4126, 0, 0, 0 }, { 0, 0, 4096, 0 }, 5585, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

// One-based indices in this area's warp table; triggers pack them in the high nibble.
enum {
    DRYFIELD_BACK_STREET_WARP_GENERAL_STORE     = 1,
    DRYFIELD_BACK_STREET_WARP_SOUVENIR_SHOP     = 2,
    DRYFIELD_BACK_STREET_WARP_WAREHOUSE         = 3,
    DRYFIELD_BACK_STREET_WARP_DILAPIDATED_HOUSE = 4,
};

// CAP command slots for examination dialogue; command 10 branches to placed object 40's pickup.
enum {
    DRYFIELD_BACK_STREET_CAP_EXAMINE_1                = 1,
    DRYFIELD_BACK_STREET_CAP_EXAMINE_4                = 4,
    DRYFIELD_BACK_STREET_CAP_EXAMINE_6                = 6,
    DRYFIELD_BACK_STREET_CAP_EXAMINE_7                = 7,
    DRYFIELD_BACK_STREET_CAP_EXAMINE_8                = 8,
    DRYFIELD_BACK_STREET_CAP_EXAMINE_PLACED_OBJECT_40 = 10,
};

/// Four departure quads and seven CAP interaction quads for daytime Back Street.
///
/// The warps pack current endpoint 1..4 in the high nibble and destination
/// endpoint 2/1/1/1 in the low nibble. Six CAP dialogue commands cover seven
/// spots; the two command-10 quads share placed object 40's pickup dialogue. The other
/// spots' narrative roles are unproven. Geometry uses game units and Q12 normals.
/// Setup links and enables every record, including the LAST-marked final quad.
/// Keep the writable records loaded until the action list is cleared.
static WorldCollisionTrigger _gDryfieldBackStreetActionTriggers[11] = {
    { NULL, NULL, NULL, { -9648, -55, 4496, 0 }, { { -400, 0, -560, 0 }, { 400, 0, -560, 0 }, { -400, 0, 560, 0 }, { 400, 0, 560, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 686, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_GENERAL_STORE, (DRYFIELD_BACK_STREET_WARP_GENERAL_STORE << 4) | 2, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7872, -55, 5776, 0 }, { { 832, 0, -352, 0 }, { 832, 0, 352, 0 }, { -832, 0, -352, 0 }, { -832, 0, 352, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, -4096, 0 }, 902, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_SOUVENIR_SHOP, (DRYFIELD_BACK_STREET_WARP_SOUVENIR_SHOP << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1616, -48, 5744, 0 }, { { 688, 0, -352, 0 }, { 688, 0, 352, 0 }, { -688, 0, -352, 0 }, { -688, 0, 352, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 770, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_WAREHOUSE, (DRYFIELD_BACK_STREET_WARP_WAREHOUSE << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9424, -48, 5712, 0 }, { { 784, 0, -384, 0 }, { 784, 0, 384, 0 }, { -784, 0, -384, 0 }, { -784, 0, 384, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE, (DRYFIELD_BACK_STREET_WARP_DILAPIDATED_HOUSE << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2CA0, -64, 5024, 0 }, { { -1072, 0, -416, 0 }, { 1072, 0, -416, 0 }, { -1072, 0, 416, 0 }, { 1072, 0, 416, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_BACK_STREET_CAP_EXAMINE_PLACED_OBJECT_40, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6113, -64, 5569, 0 }, { { -752, 0, -432, 0 }, { 752, 0, -432, 0 }, { -752, 0, 432, 0 }, { 752, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, -4092, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_BACK_STREET_CAP_EXAMINE_8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8288, -64, 2144, 0 }, { { -1696, 0, -208, 0 }, { 1760, 0, -208, 0 }, { -800, 0, 1008, 0 }, { 736, 0, 1008, 0 } }, { 0, 4104, 0, 0 }, { 201, 0, -4092, 0 }, 1768, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_BACK_STREET_CAP_EXAMINE_4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7456, -64, 5696, 0 }, { { -1248, 0, -368, 0 }, { 1248, 0, -368, 0 }, { -1248, 0, 368, 0 }, { 1248, 0, 368, 0 } }, { 0, 4095, 0, 0 }, { 200, 0, -4093, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_BACK_STREET_CAP_EXAMINE_7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4576, -64, 5536, 0 }, { { -752, 0, -432, 0 }, { 752, 0, -432, 0 }, { -752, 0, 432, 0 }, { 752, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, -4092, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_BACK_STREET_CAP_EXAMINE_1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5152, -64, 5664, 0 }, { { 1056, 0, -384, 0 }, { 1056, 0, 384, 0 }, { -1056, 0, -384, 0 }, { -1056, 0, 384, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_BACK_STREET_CAP_EXAMINE_6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2790, -64, 5520, 0 }, { { -224, 0, -560, 0 }, { 448, 0, -560, 0 }, { -448, 0, 560, 0 }, { 224, 0, 560, 0 } }, { 0, 4107, 0, 0 }, { -4076, 0, 401, 0 }, 715, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_BACK_STREET_CAP_EXAMINE_PLACED_OBJECT_40, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Variant 1's Desert Chaser resource recipe, followed by AREA_PLACEMENT_END.
///
/// Placement entry 1 selects file 400100 and descriptor 0. The two placements
/// live in the stage map; loading and spawning borrow both overlays' storage.
static AreaResource _gDryfieldBackStreetDesertChaserResources[2] = {
    { .entryId = 1, .fileNumber = 1, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor00100DesertChaserTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 7's two Pawn GOLEM resource recipes, followed by AREA_PLACEMENT_END.
///
/// Entries 20/56 select files 102000/205600 and each table's descriptor 0.
/// The paired placements live in the stage map; loading and spawning borrow
/// the map, room and actor overlays while using their records.
static AreaResource _gDryfieldBackStreetPawnGolemResources[3] = {
    { .entryId = 20, .fileNumber = 20, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor102000PawnGolemTasks },
    { .entryId = 56, .fileNumber = 56, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor205600PawnGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

// Actor variant 0 reads mode bit 0 as wall/ceiling and leaves wall settling enabled.
enum {
    DRYFIELD_BACK_STREET_MIND_SUCKLER_ENTRY_ID      = 15,
    DRYFIELD_BACK_STREET_MIND_SUCKLER_PERCHED       = 0,
    DRYFIELD_BACK_STREET_MIND_SUCKLER_WALL_PERCH    = 0,
    DRYFIELD_BACK_STREET_MIND_SUCKLER_CEILING_PERCH = 1,
};

/// Variant 10's Mind Suckler resource recipe, followed by AREA_PLACEMENT_END.
///
/// Placement entry 15 selects file 101500 and descriptor 0. Room placements
/// and the actor's task descriptor remain borrowed while loading and spawning.
static AreaResource _gDryfieldBackStreetMindSucklerResources[2] = {
    { .entryId = DRYFIELD_BACK_STREET_MIND_SUCKLER_ENTRY_ID, .fileNumber = 15, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor01500MindSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 10's two Mind Suckler placements and AREA_PLACEMENT_END record.
///
/// Both select resource entry 15 and the perched actor variant, with wall
/// and ceiling modes respectively; later wall settling remains enabled.
/// Positions use world units, yaw uses 4096 units per turn. The base resource
/// is reused with no page shift and a two-row CLUT shift. Actors borrow these
/// room-owned records through their placement-dependent work; rowIndex is unused.
static AreaPlacement _gDryfieldBackStreetMindSucklerPlacements[3] = {
    { .entryId = DRYFIELD_BACK_STREET_MIND_SUCKLER_ENTRY_ID, .variant = DRYFIELD_BACK_STREET_MIND_SUCKLER_PERCHED, .mode = DRYFIELD_BACK_STREET_MIND_SUCKLER_WALL_PERCH, .x = 6900, .y = -2000, .z = 6050, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = DRYFIELD_BACK_STREET_MIND_SUCKLER_ENTRY_ID, .variant = DRYFIELD_BACK_STREET_MIND_SUCKLER_PERCHED, .mode = DRYFIELD_BACK_STREET_MIND_SUCKLER_CEILING_PERCH, .x = 8000, .y = -2800, .z = 3600, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gDryfieldBackStreetAreaVariants[13] = {
    { NULL, NULL },
    { gMapDryfieldBackStreetDesertChaserPlacements, _gDryfieldBackStreetDesertChaserResources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { gMapDryfieldBackStreetPawnGolemPlacements, _gDryfieldBackStreetPawnGolemResources },
    { NULL, NULL },
    { NULL, NULL },
    { _gDryfieldBackStreetMindSucklerPlacements, _gDryfieldBackStreetMindSucklerResources },
    { NULL, NULL },
    { NULL, NULL },
};

/// Authored white point lights for model shading in every daytime Back Street view.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits (`ONE` is full strength). The room overlay owns this
/// writable storage: coordinate updates set its view parent and cached
/// transforms, and lighting queries overwrite attenuation. Borrowed pointers
/// expire when the room overlay unloads.
static WorldCoordPointLight _gDryfieldBackStreetPointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2985, 5488 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2990, -2985, 5955 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 5000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2985, 5955 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 5000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -2985, 5955 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 5000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6000, -2985, 5955 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9000, -2985, 5955 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3000, -2985, 5062 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6020, -2985, 5065 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -9000, -2985, 5488 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2980, -2985, 5488 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5990, -3500, 5452 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 4000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2941, -3500, 5065 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 4000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -9000, -2985, 5955 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 5000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6000, -2985, 5955 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 5000,
    },
};

/// Fourteen authored white point lights for model shading in every Back Street view.
///
/// No directional or cone lights are present. Coordinates and attenuation
/// are mutable; the collection and its arrays live with this room overlay.
static WorldCoordRoomLights _gDryfieldBackStreetRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldBackStreetPointLights),
    .pointLights           = _gDryfieldBackStreetPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

// Type-1 cue bases select scripts from the currently loaded area bank.
enum {
    DRYFIELD_BACK_STREET_WALK_FOOTSTEP_SOUND = 0x1000001D,
    DRYFIELD_BACK_STREET_RUN_FOOTSTEP_SOUND  = 0x1000001F,
};

/// Paired walking, running and scripted-jump cue bases for surface classes 1/2.
///
/// Walking and scripted jumps share base 29; running uses base 31. Animation
/// cue 1 adds one and companions add 100. Surface records borrow this table
/// without modifying it; keep the room overlay and current sound bank loaded.
static WorldCollisionFootstepSounds _gDryfieldBackStreetFootstepSounds = {
    .walk         = DRYFIELD_BACK_STREET_WALK_FOOTSTEP_SOUND,
    .run          = DRYFIELD_BACK_STREET_RUN_FOOTSTEP_SOUND,
    .scriptedJump = DRYFIELD_BACK_STREET_WALK_FOOTSTEP_SOUND,
};

/// Classes 0 and 3..7: blocked probes, enabled weapon impacts and silent footsteps.
///
/// Applies pushback. The unused first byte retains its authored zero; its
/// meaning is unproven. The room overlay owns this shared policy record.
static WorldCollisionSurfaceProperties _gDryfieldBackStreetBlockingSilentSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1: blocked probes, enabled weapon impacts and walking/running footstep cues.
///
/// Applies pushback. The unused first byte retains its authored zero; its
/// meaning is unproven. Both policy and sound table live with this room overlay.
static WorldCollisionSurfaceProperties _gDryfieldBackStreetBlockingFootstepSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldBackStreetFootstepSounds,
};

/// Class 2: probe passage and suppressed weapon impacts, with walking/running cues.
///
/// Applies pushback independently of probe passage. The unused first byte
/// retains its authored zero; its meaning is unproven. Storage lives with the room.
static WorldCollisionSurfaceProperties _gDryfieldBackStreetPassThroughFootstepSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldBackStreetFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldBackStreetSurfaceProperties[8] = {
    &_gDryfieldBackStreetBlockingSilentSurface,
    &_gDryfieldBackStreetBlockingFootstepSurface,
    &_gDryfieldBackStreetPassThroughFootstepSurface,
    &_gDryfieldBackStreetBlockingSilentSurface,
    &_gDryfieldBackStreetBlockingSilentSurface,
    &_gDryfieldBackStreetBlockingSilentSurface,
    &_gDryfieldBackStreetBlockingSilentSurface,
    &_gDryfieldBackStreetBlockingSilentSurface,
};

/// Last requested Back Street ambience volume, in percent (0, 30, 50 or 100).
///
/// Reset to zero at ambience-task initialization; subsequent changes are
/// cached even when sound admission fails. Zero denotes a silence request,
/// not confirmed playback state. Retained for the loaded room's lifetime.
static s32 _gDryfieldBackStreetRequestedAmbienceVolumePercent = 0;

static void _dryfieldBackStreetInitializeRoomTask(Task* task);

/// Requests Back Street's ambience mix as the logical camera view changes.
///
/// State 0 clears the requested-volume cache and advances to state 1; other
/// states do nothing. Views 3/4/5 request 30/50/100 percent with pan offsets
/// +4/-8/-12; other views request silence. Only a volume change queues a start,
/// 30-audio-update fade-out, or mix change. Pan uses three SPU steps per unit.
/// The cache records requests even if the queue rejects them. Requires a live
/// bodyless task and the loaded room sound bank through playback.
static void _dryfieldBackStreetAmbienceTask(Task* task)
{
    enum { DRYFIELD_BACK_STREET_AMBIENCE_INITIALIZE       = 0,
           DRYFIELD_BACK_STREET_AMBIENCE_UPDATE           = 1,
           DRYFIELD_BACK_STREET_AMBIENCE_VOLUME_FULL      = 100,
           DRYFIELD_BACK_STREET_AMBIENCE_ATTENUATION_FULL = 127,
           DRYFIELD_BACK_STREET_AMBIENCE_FADE_TICKS       = 30 };
    s32 volumePercent;
    s32 panOffset;

    switch (task->state) {
        case DRYFIELD_BACK_STREET_AMBIENCE_INITIALIZE:
            _gDryfieldBackStreetRequestedAmbienceVolumePercent = 0;
            task->state                                        = task->state + 1;
            return;
        case DRYFIELD_BACK_STREET_AMBIENCE_UPDATE:
            break;
        default:
            return;
    }

    switch (viewGetMappedIndex()) {
        case 3:
            volumePercent = 30;
            panOffset     = 4;
            break;
        case 4:
            volumePercent = 50;
            panOffset     = -8;
            break;
        case 5:
            volumePercent = DRYFIELD_BACK_STREET_AMBIENCE_VOLUME_FULL;
            panOffset     = -12;
            break;
        default:
            panOffset     = 0;
            volumePercent = 0;
            break;
    }

    if (volumePercent == _gDryfieldBackStreetRequestedAmbienceVolumePercent) {
        return;
    }
    if (_gDryfieldBackStreetRequestedAmbienceVolumePercent == 0) {
        sndEvtRequestScriptStart(SOUND_BACK_STREET_AMBIENCE, panOffset, (s8)(((DRYFIELD_BACK_STREET_AMBIENCE_VOLUME_FULL - volumePercent) * DRYFIELD_BACK_STREET_AMBIENCE_ATTENUATION_FULL) / DRYFIELD_BACK_STREET_AMBIENCE_VOLUME_FULL));
    } else if (volumePercent == 0) {
        sndEvtRequestScriptStop(SOUND_BACK_STREET_AMBIENCE, DRYFIELD_BACK_STREET_AMBIENCE_FADE_TICKS);
    } else {
        sndEvtRequestScriptMix(SOUND_BACK_STREET_AMBIENCE, panOffset, (s8)(((DRYFIELD_BACK_STREET_AMBIENCE_VOLUME_FULL - volumePercent) * DRYFIELD_BACK_STREET_AMBIENCE_ATTENUATION_FULL) / DRYFIELD_BACK_STREET_AMBIENCE_VOLUME_FULL));
    }
    _gDryfieldBackStreetRequestedAmbienceVolumePercent = volumePercent;
}

#include "../../shared/back_street_event_msg.inc.c"

/// Refuses every key-item use in Back Street, returning the menu's unusable result.
///
/// The item ID and zero second payload are ignored; no item is consumed.
static s32 _dryfieldBackStreetRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Ignores room commands and their execution modes, returning zero.
static s32 _dryfieldBackStreetIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 executionMode)
{
    return 0;
}

/// Ignores room-action requests and returns zero.
///
/// The request is borrowed for synchronous dispatch; no payload is read or retained.
static s32 _dryfieldBackStreetIgnoreRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg)
{
    return 0;
}

/// Publishes Back Street's message receiver and starts its ambience before idling.
///
/// Requires a live state-0 room task. The receiver occupies
/// `GAME_TASK_SLOT_ROOM` until teardown; the spawned ambience runs independently.
/// A failed ambience spawn is not retried. Keep the overlay loaded while either
/// task can be dispatched.
static void _dryfieldBackStreetInitializeRoomTask(Task* task)
{
    task->msgTable = _gDryfieldBackStreetRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(_gDryfieldBackStreetAmbienceTaskDescs, 0, 0, 0);
    task->state = task->state + 1;
}

/// Keeps the initialized room task available for messages until its state changes.
static void _dryfieldBackStreetIdleState(Task* task)
{
}

/// Room-task lifecycle: register the receiver and ambience, idle, release.
///
/// Copied by value for unchecked state 0..2 dispatch; callbacks require this overlay loaded.
static const TaskFuncTable3 _gDryfieldBackStreetRoomTaskStates = {
    { _dryfieldBackStreetInitializeRoomTask, _dryfieldBackStreetIdleState, taskKill },
};

void dryfieldBackStreetRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gDryfieldBackStreetRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

void dryfieldBackStreetConfigureEffectsTask(Task* task)
{
    enum { DRYFIELD_BACK_STREET_EFFECTS_INITIALIZE,
           DRYFIELD_BACK_STREET_EFFECTS_ACTIVE };

    if (task->state == DRYFIELD_BACK_STREET_EFFECTS_INITIALIZE) {
        gRoomEffectFlashId      = EFFECT_DRYFIELD_BACK_STREET_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_DRYFIELD_BACK_STREET_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_DRYFIELD_BACK_STREET_SPARK_BURST;
        task->state             = DRYFIELD_BACK_STREET_EFFECTS_ACTIVE;
    }
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void dryfieldBackStreetRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void dryfieldBackStreetRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void dryfieldBackStreetRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
