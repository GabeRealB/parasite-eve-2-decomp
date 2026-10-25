#include "rooms/mine_gorge.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00700.h"
#include "actors/actor_01500.h"
#include "actors/actor_03700.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task_types.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"

static WorldCollisionGrid         _gMineGorgeRoom1Collision;
static WorldCollisionGrid         _gMineGorgeRoom2Collision;
static WorldCollisionTrigger      _gMineGorgeViewBoundaryTriggers[8];
static WorldCollisionTrigger      _gMineGorgeRoom1ActionTriggers[8];
static WorldCollisionTrigger      _gMineGorgeRoom2ActionTriggers[5];
static WorldCoordRoomAmbientEntry _gMineGorgeViewAmbientTable[12];
static WorldCoordRoomLights       _gMineGorgeRoomLights;

enum {
    MINE_GORGE_FLARE_TEXTURE_COLUMN = 1,
    MINE_GORGE_FLARE_RADIUS_SCALE   = 0x300 // Pixel half-extent = scale * 39 / (camera Z / 4).
};

/// Ambient-effect gates for mapped gorge views 1..11.
///
/// Indexed by mapped view minus one; views 2 and 6 enable movement dust.
/// The trailing disabled entry is retained storage outside the valid views.
/// Unsigned halfwords are copied into the resident room-effect mode each frame.
static u16 _gMineGorgeViewEffectModes[MINE_GORGE_VIEW_COUNT + 1] = {
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
};

/// Six fixed world-space flare centres, borrowed by the current view's drawer.
///
/// Mapped view 2 uses indices 4..5; views 3 and 7 use 0..4; views 4, 5 and 9
/// use 2..5; view 6 uses 0..1; view 8 uses 2, 4 and 5; views 10 and 11 use 3.
/// View 1 selects none. Components use signed game-coordinate units. Storage
/// remains live while the gorge is loaded.
static SVECTOR _gMineGorgeFlarePositions[6] = {
    { 980, -2030, 3720, 0 },
    { 2950, -2150, 260, 0 },
    { 8960, -420, 930, 0 },
    { 0x2ADA, -460, 2080, 0 },
    { 0x3B42, -1970, 450, 0 },
    { 0x4628, -1670, 3650, 0 },
};
STATIC_ASSERT_SIZEOF(_gMineGorgeFlarePositions, 0x30);

WorldCoordRoomLighting gMineGorgeRoomLighting[2] = {
    { &_gMineGorgeRoomLights, _gMineGorgeViewAmbientTable },
    { &_gMineGorgeRoomLights, _gMineGorgeViewAmbientTable },
};

WorldCollisionRoomResources gMineGorgeRoomCollisionResources[MINE_GORGE_ROOM_COUNT] = {
    { .grid = &_gMineGorgeRoom1Collision, .viewBoundaryTriggers = _gMineGorgeViewBoundaryTriggers, .actionTriggers = _gMineGorgeRoom1ActionTriggers, .occluders = NULL },
    { .grid = &_gMineGorgeRoom2Collision, .viewBoundaryTriggers = _gMineGorgeViewBoundaryTriggers, .actionTriggers = _gMineGorgeRoom2ActionTriggers, .occluders = NULL },
};

/// Logical-view to camera/image/sprite indices after the oak board is laid.
///
/// Logical views 1..11 index this byte array at view minus one; the final zero
/// is retained storage outside that range. Views 3..5 exchange resources with
/// 7..9. The loaded gorge owns the map borrowed by `gMineGorgeRoomViewMaps`.
static u8 _gMineGorgeRoom2ViewMap[12] = {
    1,
    2,
    7,
    8,
    9,
    6,
    3,
    4,
    5,
    10,
    11,
    0,
};

u8* gMineGorgeRoomViewMaps[2] = {
    gViewIdentityMap,
    _gMineGorgeRoom2ViewMap,
};

ViewCount gMineGorgeRoomViewCounts[MINE_GORGE_ROOM_COUNT] = { MINE_GORGE_VIEW_COUNT, MINE_GORGE_VIEW_COUNT };

/// Gorge endpoint IDs, map marker and room/CAP action selectors.
///
/// Warp IDs are 1-based; CAP slots are zero-based. Room action 1 starts the
/// encounter. The three CAP interaction subjects are not established here.
enum {
    MINE_GORGE_WARP_TUNNEL           = 1,
    MINE_GORGE_WARP_REFUGE           = 2,
    MINE_GORGE_WARP_CAVERN           = 3,
    MINE_GORGE_CAVERN_MAP_FLAG       = 448,
    MINE_GORGE_ROOM_ACTION_ENCOUNTER = 1,
    MINE_GORGE_CAP_INTERACTION_1     = 1,
    MINE_GORGE_CAP_INTERACTION_6     = 6,
    MINE_GORGE_CAP_INTERACTION_7     = 7
};

DirectionWarpEntry gMineGorgeWarpEntries[3] = {
    { .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 0x4588, 0, 1600 },
      .unknown_10     = { 0, 0, 0, 0 },
      .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 0x4588, 0, 1600 },
      .unknown_24     = { 0, 0, 0, 0 },
      .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_GORGE, 2),
      .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_GORGE, 1),
      .blockedSound   = DIRECTION_WARP_SOUND_NONE,
      .initialView    = 2,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE },
    { .player         = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 1731, 0, 3487 },
      .unknown_10     = { 0, 0, 0, 0 },
      .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 1731, 0, 3487 },
      .unknown_24     = { 0, 0, 0, 0 },
      .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_GORGE, 4),
      .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_GORGE, 3),
      .blockedSound   = DIRECTION_WARP_SOUND_NONE,
      .initialView    = 6,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE },
    { .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 300, 0, 1500 },
      .unknown_10     = { 0, 0, 0, 0 },
      .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 300, 0, 1500 },
      .unknown_24     = { 0, 0, 0, 0 },
      .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_GORGE, 6),
      .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_GORGE, 5),
      .blockedSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_GORGE, 8),
      .initialView    = 6,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = MINE_GORGE_CAVERN_MAP_FLAG },
};

/// Room-1 mesh normals, with 4096 representing unit length.
static SVECTOR _gMineGorgeRoom1CollisionNormals[30] = {
#include "assets/mine_gorge_collision_01BC4_normals.inc"
};

/// Room-1 mesh vertices in signed room-local game coordinates.
static SVECTOR _gMineGorgeRoom1CollisionVerts[120] = {
#include "assets/mine_gorge_collision_01BC4_verts.inc"
};

/// Room-1 indexed collision faces using the room's surface classes 0..4.
static WorldCollisionGridFace _gMineGorgeRoom1CollisionFaces[49] = {
#include "assets/mine_gorge_collision_01BC4_faces.inc"
};

/// Room-1 cell face-index lists, each ending in WORLD_COLLISION_GRID_CELL_END.
static s16 _gMineGorgeRoom1CollisionCells[224] = {
#include "assets/mine_gorge_collision_01BC4_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gMineGorgeRoom1CollisionCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define MINE_GORGE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gMineGorgeRoom1CollisionCells[(halfwordOffset)])
/// Room-1 X-major cell directory, indexed as x * 2 + z for x in 0..5 and z in 0..1.
static s16* _gMineGorgeRoom1CollisionTable[12] = {
#include "assets/mine_gorge_collision_01BC4_table.inc"
};
#undef MINE_GORGE_CELL_FACE_LIST_AT_OFFSET

/// Room-1 collision mesh before the oak board is laid.
///
/// Owns 49 faces and a 6-by-2 X-major cell table. Cells span 4000 game units,
/// with grid origin (-1244, -2085) in XZ; vertices use game units and normals
/// use 4096 per unit. Activation borrows the mesh and binds its view coordinate.
static WorldCollisionGrid _gMineGorgeRoom1Collision = {
    .viewCoord   = NULL,
    .normals     = _gMineGorgeRoom1CollisionNormals,
    .vertices    = _gMineGorgeRoom1CollisionVerts,
    .faces       = _gMineGorgeRoom1CollisionFaces,
    .cellFaceIds = _gMineGorgeRoom1CollisionTable,
    .xBias       = 1244,
    .zBias       = 2085,
    .cellCountX  = 6,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gMineGorgeRoom1CollisionFaces),
};

/// Room-2 mesh normals, with 4096 representing unit length.
static SVECTOR _gMineGorgeRoom2CollisionNormals[28] = {
#include "assets/mine_gorge_collision_02430_normals.inc"
};

/// Room-2 mesh vertices in signed room-local game coordinates.
static SVECTOR _gMineGorgeRoom2CollisionVerts[108] = {
#include "assets/mine_gorge_collision_02430_verts.inc"
};

/// Room-2 indexed collision faces using the room's surface classes 0..4.
static WorldCollisionGridFace _gMineGorgeRoom2CollisionFaces[46] = {
#include "assets/mine_gorge_collision_02430_faces.inc"
};

/// Room-2 cell face-index lists, each ending in WORLD_COLLISION_GRID_CELL_END.
static s16 _gMineGorgeRoom2CollisionCells[216] = {
#include "assets/mine_gorge_collision_02430_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gMineGorgeRoom2CollisionCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define MINE_GORGE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gMineGorgeRoom2CollisionCells[(halfwordOffset)])
/// Room-2 X-major cell directory, indexed as x * 2 + z for x in 0..5 and z in 0..1.
static s16* _gMineGorgeRoom2CollisionTable[12] = {
#include "assets/mine_gorge_collision_02430_table.inc"
};
#undef MINE_GORGE_CELL_FACE_LIST_AT_OFFSET

/// Room-2 collision mesh after the oak board is laid.
///
/// Owns 46 faces and a 6-by-2 X-major cell table. Cells span 4000 game units,
/// with grid origin (-1244, -2085) in XZ; vertices use game units and normals
/// use 4096 per unit. Activation borrows the mesh and binds its view coordinate.
static WorldCollisionGrid _gMineGorgeRoom2Collision = {
    .viewCoord   = NULL,
    .normals     = _gMineGorgeRoom2CollisionNormals,
    .vertices    = _gMineGorgeRoom2CollisionVerts,
    .faces       = _gMineGorgeRoom2CollisionFaces,
    .cellFaceIds = _gMineGorgeRoom2CollisionTable,
    .xBias       = 1244,
    .zBias       = 2085,
    .cellCountX  = 6,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gMineGorgeRoom2CollisionFaces),
};

ViewCamera gMineGorgeViewCameras[MINE_GORGE_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -9000, 0x4B64, -2000 } }, 289 },
    { { { { 718, 0, -4032 }, { -1123, 3933, -200 }, { 3872, 1141, 689 } }, { -0x2DF4, 2237, -1133 } }, 257 },
    { { { { 844, 0, 4007 }, { 1410, 3834, -297 }, { -3751, 1441, 790 } }, { -0x4405, 2242, -988 } }, 246 },
    { { { { 2333, 0, -3365 }, { 1255, 3800, 870 }, { 3123, -1527, 2165 } }, { -5868, -921, 827 } }, 269 },
    { { { { 1366, 0, -3861 }, { -676, 4032, -239 }, { 3801, 717, 1345 } }, { -1704, 1804, -615 } }, 269 },
    { { { { 1510, 0, 3807 }, { 245, 4087, -97 }, { -3799, 264, 1506 } }, { -6638, 1162, -391 } }, 257 },
    { { { { 844, 0, 4007 }, { 1410, 3834, -297 }, { -3751, 1441, 790 } }, { -0x4405, 2242, -988 } }, 246 },
    { { { { 2333, 0, -3365 }, { 1255, 3800, 870 }, { 3123, -1527, 2165 } }, { -5868, -921, 827 } }, 269 },
    { { { { 1366, 0, -3861 }, { -676, 4032, -239 }, { 3801, 717, 1345 } }, { -1704, 1804, -615 } }, 269 },
    { { { { 186, 0, 4091 }, { 3209, 2541, -146 }, { -2538, 3212, 115 } }, { -0x2E48, 1696, -1474 } }, 230 },
    { { { { 186, 0, 4091 }, { 3209, 2541, -146 }, { -2538, 3212, 115 } }, { -0x2E48, 1696, -1474 } }, 230 },
};

// Room sprites use 8-bit texture pages at VRAM Y=0 and the palette at (0, 255).
enum {
    MINE_GORGE_SPRITE_TEXTURE_PAGE_X896 = getTPage(1, 0, 896, 0),
    MINE_GORGE_SPRITE_TEXTURE_PAGE_X960 = getTPage(1, 0, 960, 0),
    MINE_GORGE_SPRITE_CLUT              = getClut(0, 255),
    MINE_GORGE_SPRITE_NEUTRAL_COLOR     = 128
};

/// Empty sprite batch list for mapped gorge view 1.
///
/// The zero-count first record selects decoded background strips, followed by
/// `SPRITE_BATCH_END`. Its retained source-slot address supplies no sprites.
/// The loaded gorge owns this writable list.
static SpriteBatch _gMineGorgeView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped gorge view 2.
///
/// The view's batches cover all 86 elements. Positions and dimensions are
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// The loaded gorge owns these writable source records.
static SpriteSource _gMineGorgeView2SpriteSources[86] = {
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -32, 16, 901, { .fields = { 72, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -32, 32, 922, { .fields = { 96, 72 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -32, 56, 950, { .fields = { 104, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, 48, 16, 855, { .fields = { 88, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, 48, 40, 876, { .fields = { 96, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, 48, 64, 901, { .fields = { 104, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, -88, 112, 527, { .fields = { 56, 224 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -80, 104, 543, { .fields = { 64, 200 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, -80, 96, 567, { .fields = { 56, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -72, 88, 589, { .fields = { 64, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -64, 80, 648, { .fields = { 64, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -64, 72, 643, { .fields = { 56, 216 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -56, 64, 685, { .fields = { 48, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -56, 56, 714, { .fields = { 48, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -48, 48, 755, { .fields = { 48, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -40, 40, 812, { .fields = { 48, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -40, 32, 846, { .fields = { 48, 152 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -32, 24, 905, { .fields = { 56, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -48, 72, 951, { .fields = { 104, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -48, 96, 759, { .fields = { 104, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 56, 24, 855, { .fields = { 80, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 56, 40, 774, { .fields = { 80, 224 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 56, 56, 690, { .fields = { 72, 80 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 56, 72, 647, { .fields = { 64, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 56, 88, 562, { .fields = { 64, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 56, 104, 524, { .fields = { 64, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -136, -72, 1149, { .fields = { 56, 8 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -128, -64, 1104, { .fields = { 96, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -120, -48, 1064, { .fields = { 112, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -112, -16, 1043, { .fields = { 96, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -104, 0, 1022, { .fields = { 96, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, 16, 995, { .fields = { 104, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -88, 24, 974, { .fields = { 120, 240 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -64, 40, 888, { .fields = { 56, 56 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -48, 40, 855, { .fields = { 80, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -80, 32, 931, { .fields = { 72, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 64, 80, 767, { .fields = { 56, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 80, 72, 795, { .fields = { 80, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, 64, 815, { .fields = { 80, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 88, 56, 824, { .fields = { 56, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 96, 40, 837, { .fields = { 80, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 96, 24, 858, { .fields = { 80, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 96, 16, 882, { .fields = { 64, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 104, -8, 901, { .fields = { 88, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 104, -32, 915, { .fields = { 88, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 104, -56, 935, { .fields = { 88, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, 112, -80, 947, { .fields = { 112, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 40, 745, { .fields = { 88, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, 64, 32, 759, { .fields = { 120, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 64, 72, 796, { .fields = { 56, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 88, 72, 790, { .fields = { 64, 240 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, 112, 64, 779, { .fields = { 80, 72 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, 64, 88, 826, { .fields = { 112, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, 112, 88, 807, { .fields = { 120, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 120, 40, 756, { .fields = { 88, 24 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, 32, 759, { .fields = { 64, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, 32, 755, { .fields = { 64, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 104, 40, 752, { .fields = { 56, 88 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, -128, 8, 999, { .fields = { 112, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -120, 40, 1037, { .fields = { 120, 80 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, -112, 40, 1004, { .fields = { 48, 80 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, -112, 8, 965, { .fields = { 56, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, -88, 40, 962, { .fields = { 56, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, -88, 8, 944, { .fields = { 112, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 24 } }, -88, 56, 995, { .fields = { 80, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 8 } }, -72, 16, 919, { .fields = { 32, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -40, 16, 899, { .fields = { 120, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, -64, 48, 962, { .fields = { 40, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, -40, 48, 954, { .fields = { 120, 40 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -96, -8, 1299, { .fields = { 88, 216 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -96, 16, 1393, { .fields = { 88, 240 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -80, -8, 1255, { .fields = { 88, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -80, 16, 1285, { .fields = { 72, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -64, -8, 1266, { .fields = { 104, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -64, 16, 1350, { .fields = { 104, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -56, -8, 1319, { .fields = { 112, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -56, 8, 1391, { .fields = { 112, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -48, -16, 1426, { .fields = { 104, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -48, 8, 1526, { .fields = { 80, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -40, -24, 1547, { .fields = { 96, 24 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -40, 0, 1567, { .fields = { 104, 24 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, -96, -16, 1294, { .fields = { 40, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, -72, -16, 1441, { .fields = { 40, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, -88, -24, 1552, { .fields = { 40, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, -64, -24, 1591, { .fields = { 40, 136 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -72, -32, 1591, { .fields = { 48, 104 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing ranges for mapped gorge view 2.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover the complete source array, then `SPRITE_BATCH_END` ends the
/// list. The loaded gorge owns these writable records.
static SpriteBatch _gMineGorgeView2SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { 6, 20, 0, 0, { 3, 0 } },
    { 26, 21, 0, 0, { 2, 0 } },
    { 47, 22, 0, 0, { 4, 0 } },
    { 69, ARRAY_SIZE(_gMineGorgeView2SpriteSources) - 69, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped gorge view 3.
///
/// The view's batches cover all 73 elements. Positions and dimensions are
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// The loaded gorge owns these writable source records.
static SpriteSource _gMineGorgeView3SpriteSources[73] = {
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -104, 96, 691, { .fields = { 88, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, -112, 80, 615, { .fields = { 112, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -144, -48, 475, { .fields = { 80, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, -136, -88, 483, { .fields = { 120, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -128, -120, 453, { .fields = { 120, 224 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, -144, -120, 412, { .fields = { 96, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, -88, 432, { .fields = { 112, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, -160, -24, 432, { .fields = { 112, 88 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -160, 88, 435, { .fields = { 112, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -152, -24, 459, { .fields = { 112, 200 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 48 } }, -152, 8, 454, { .fields = { 120, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, -160, 16, 436, { .fields = { 120, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -160, 56, 438, { .fields = { 104, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -152, 56, 462, { .fields = { 104, 80 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -152, 88, 467, { .fields = { 104, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, -144, 56, 510, { .fields = { 96, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, -144, 88, 503, { .fields = { 96, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -128, 56, 586, { .fields = { 96, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -120, 64, 621, { .fields = { 88, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -120, 88, 568, { .fields = { 96, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -128, 88, 538, { .fields = { 96, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 48 } }, -144, 8, 535, { .fields = { 120, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -136, 40, 569, { .fields = { 96, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, -160, -120, 355, { .fields = { 80, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, -160, -88, 389, { .fields = { 80, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -160, -56, 408, { .fields = { 88, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -152, -56, 431, { .fields = { 96, 80 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 32 } }, -128, -72, 501, { .fields = { 72, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -136, -96, 490, { .fields = { 72, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -72, 3502, { .fields = { 56, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -64, 2829, { .fields = { 56, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -56, 2486, { .fields = { 56, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -48, 2112, { .fields = { 56, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -40, 1878, { .fields = { 64, 24 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -32, 1560, { .fields = { 72, 56 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -24, 1383, { .fields = { 56, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -16, 1225, { .fields = { 64, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -8, 1097, { .fields = { 72, 88 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, 0, 980, { .fields = { 72, 200 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, 8, 909, { .fields = { 80, 88 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, 16, 830, { .fields = { 48, 8 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, 24, 777, { .fields = { 48, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, 32, 726, { .fields = { 56, 136 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -56, 40, 619, { .fields = { 56, 104 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -56, 56, 637, { .fields = { 80, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -56, 80, 664, { .fields = { 80, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -56, 104, 687, { .fields = { 96, 240 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 40, 24, 706, { .fields = { 40, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 40, 32, 674, { .fields = { 48, 56 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 40, 48, 691, { .fields = { 48, 152 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 40, 64, 712, { .fields = { 48, 88 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, 40, 80, 735, { .fields = { 64, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 16 } }, -128, 24, 691, { .fields = { 40, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, -96, 24, 710, { .fields = { 48, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -72, 24, 733, { .fields = { 56, 200 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -72, 56, 769, { .fields = { 56, 216 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -120, 64, 738, { .fields = { 72, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 8 } }, -104, 64, 750, { .fields = { 40, 248 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -64, 40, 753, { .fields = { 56, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -64, 72, 786, { .fields = { 64, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -112, 88, 759, { .fields = { 72, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -120, 40, 709, { .fields = { 64, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 64, 24, 693, { .fields = { 40, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 64, 40, 724, { .fields = { 72, 152 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 64, 64, 754, { .fields = { 48, 72 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, 56, 761, { .fields = { 48, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, 56, 80, 775, { .fields = { 64, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, 88, 24, 727, { .fields = { 32, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 48 } }, 104, 56, 804, { .fields = { 104, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 32 } }, 112, 24, 738, { .fields = { 72, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, 16, 768, { .fields = { 40, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 8 } }, 128, 56, 786, { .fields = { 24, 104 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 152, 72, 814, { .fields = { 112, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing ranges for mapped gorge view 3.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover the complete source array, then `SPRITE_BATCH_END` ends the
/// list. The loaded gorge owns these writable records.
static SpriteBatch _gMineGorgeView3SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 3, 0 } },
    { 27, 2, 0, 0, { 0, 0 } },
    { 29, 23, 0, 0, { 2, 0 } },
    { 52, ARRAY_SIZE(_gMineGorgeView3SpriteSources) - 52, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped gorge view 4.
///
/// The view's batches cover all 66 elements. Positions and dimensions are
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// The loaded gorge owns these writable source records.
static SpriteSource _gMineGorgeView4SpriteSources[66] = {
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 32, 1581, { .fields = { 88, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 40, 1902, { .fields = { 8, 24 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 72, 48, 1948, { .fields = { 32, 40 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 64, 1994, { .fields = { 80, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, 112, 40, 2124, { .fields = { 0, 136 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, 112, 72, 1905, { .fields = { 0, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 128, 80, 1839, { .fields = { 120, 88 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 24 } }, 128, 40, 1914, { .fields = { 8, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 144, 64, 1914, { .fields = { 0, 72 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, 16, -16, 946, { .fields = { 16, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 48 } }, 24, 16, 946, { .fields = { 64, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 56 } }, 56, 8, 1182, { .fields = { 120, 200 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, 48, 0, 1125, { .fields = { 72, 224 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 32, -8, 1040, { .fields = { 0, 56 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 40, 0, 1080, { .fields = { 0, 104 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 64, 16, 1262, { .fields = { 120, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 72, 16, 1375, { .fields = { 0, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 80, 24, 1490, { .fields = { 0, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 96, 32, 1727, { .fields = { 80, 240 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 8, -24, 909, { .fields = { 88, 240 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 0, -32, 855, { .fields = { 96, 240 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -8, -32, 834, { .fields = { 0, 40 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -16, -40, 802, { .fields = { 0, 24 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 72 } }, -24, -48, 770, { .fields = { 96, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -32, -48, 742, { .fields = { 56, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -40, -56, 728, { .fields = { 112, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -48, -64, 701, { .fields = { 112, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -56, -64, 689, { .fields = { 120, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -64, -72, 662, { .fields = { 120, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -72, -80, 639, { .fields = { 120, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, -80, 630, { .fields = { 120, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -88, -88, 607, { .fields = { 112, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, -96, 586, { .fields = { 120, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 64 } }, -104, -104, 564, { .fields = { 72, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, -112, -56, 587, { .fields = { 48, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -112, -104, 545, { .fields = { 120, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -120, -112, 539, { .fields = { 120, 136 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -128, -120, 525, { .fields = { 48, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -136, -120, 521, { .fields = { 120, 104 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -152, -120, 508, { .fields = { 104, 248 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, 24, 64, 1432, { .fields = { 32, 224 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 32 } }, 40, 64, 1432, { .fields = { 8, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 40 } }, 64, 56, 1457, { .fields = { 32, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 32 } }, 88, 64, 1598, { .fields = { 8, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 48, 96, 1606, { .fields = { 104, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, 56, 112, 1640, { .fields = { 16, 136 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, 96, 1422, { .fields = { 112, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 96, 112, 1464, { .fields = { 40, 40 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 8, 40, 827, { .fields = { 120, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, 24, 733, { .fields = { 104, 200 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 72 } }, -80, 0, 602, { .fields = { 80, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 48, 24 } }, -80, 72, 913, { .fields = { 112, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 16 } }, -56, 96, 915, { .fields = { 112, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 24 } }, -160, -40, 455, { .fields = { 0, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 96 } }, -160, -16, 455, { .fields = { 72, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 96 } }, -128, -16, 510, { .fields = { 104, 104 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 104 } }, -104, -16, 557, { .fields = { 104, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 40 } }, -8, 24, 787, { .fields = { 32, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 64 } }, -56, 8, 660, { .fields = { 64, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 40, 48 } }, 120, 72, 1814, { .fields = { 32, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, 72, 2134, { .fields = { 40, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 48 } }, 72, 72, 2044, { .fields = { 56, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 48 } }, 88, 72, 1900, { .fields = { 48, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 48 } }, 104, 72, 1911, { .fields = { 56, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 40 } }, 16, 80, 2367, { .fields = { 8, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 40 } }, -16, 80, 2629, { .fields = { 16, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing ranges for mapped gorge view 4.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover the complete source array, then `SPRITE_BATCH_END` ends the
/// list. The loaded gorge owns these writable records.
static SpriteBatch _gMineGorgeView4SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 59, 0, 0, { 1, 0 } },
    { 59, ARRAY_SIZE(_gMineGorgeView4SpriteSources) - 59, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped gorge view 5.
///
/// The zero-count first record selects decoded background strips, followed by
/// `SPRITE_BATCH_END`. Its retained source-slot address supplies no sprites.
/// The loaded gorge owns this writable list.
static SpriteBatch _gMineGorgeView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped gorge view 6.
///
/// The view's batches cover all 83 elements. Positions and dimensions are
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// The loaded gorge owns these writable source records.
static SpriteSource _gMineGorgeView6SpriteSources[83] = {
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 40 } }, -72, -8, 979, { .fields = { 104, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, -72, 32, 987, { .fields = { 80, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, -8, -8, 1119, { .fields = { 80, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, -8, 24, 1128, { .fields = { 80, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -72, 0, 975, { .fields = { 120, 248 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -72, 24, 815, { .fields = { 112, 248 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -128, -80, 782, { .fields = { 96, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -128, -48, 786, { .fields = { 32, 80 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -120, -32, 791, { .fields = { 88, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -120, 0, 791, { .fields = { 56, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -112, 24, 809, { .fields = { 40, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 8 } }, -96, 32, 815, { .fields = { 80, 248 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -64, 0, 820, { .fields = { 88, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 56 } }, -56, 8, 738, { .fields = { 120, 80 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 56 } }, -56, 64, 750, { .fields = { 120, 136 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -48, 16, 606, { .fields = { 64, 72 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -40, 24, 543, { .fields = { 72, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -32, 32, 493, { .fields = { 72, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 56 } }, -24, 32, 460, { .fields = { 120, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -24, 88, 465, { .fields = { 96, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -16, 40, 406, { .fields = { 72, 200 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -8, 48, 370, { .fields = { 72, 224 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 0, 56, 342, { .fields = { 64, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, 8, 56, 322, { .fields = { 104, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 16, 64, 306, { .fields = { 72, 152 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, 24, 64, 294, { .fields = { 104, 88 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, 32, 72, 273, { .fields = { 104, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, 40, 80, 256, { .fields = { 96, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, 48, 80, 248, { .fields = { 112, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, 56, 88, 237, { .fields = { 96, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 64, 96, 226, { .fields = { 64, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, 104, 216, { .fields = { 40, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, -128, -120, 778, { .fields = { 112, 88 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 0, 0, 1081, { .fields = { 56, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 56, 16, 717, { .fields = { 80, 152 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 40, 480, { .fields = { 72, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 8, 0, 1013, { .fields = { 40, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 16, 0, 947, { .fields = { 40, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 24, 0, 893, { .fields = { 96, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 48 } }, 32, 8, 844, { .fields = { 112, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, 32, 72, 267, { .fields = { 112, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 40, 8, 798, { .fields = { 40, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 48, 8, 756, { .fields = { 56, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 64, 16, 684, { .fields = { 80, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 72, 16, 652, { .fields = { 104, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 80, 24, 618, { .fields = { 48, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, 24, 593, { .fields = { 48, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 96, 24, 575, { .fields = { 48, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 80 } }, 104, 24, 552, { .fields = { 120, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 96, 96, 552, { .fields = { 72, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 112, 32, 551, { .fields = { 40, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 120, 32, 516, { .fields = { 32, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 128, 32, 496, { .fields = { 32, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 144, 40, 477, { .fields = { 40, 200 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 152, 40, 450, { .fields = { 40, 240 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, -8, 1029, { .fields = { 72, 248 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 40, 8 } }, 64, 16, 996, { .fields = { 8, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, 96, 8, 1010, { .fields = { 16, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 112, -8, 1019, { .fields = { 56, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, -128, -8, 1000, { .fields = { 96, 216 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -128, 24, 1006, { .fields = { 56, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -120, 48, 980, { .fields = { 80, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, -112, -8, 1006, { .fields = { 48, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -88, -8, 1016, { .fields = { 64, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 24 } }, -96, 16, 1022, { .fields = { 64, 24 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -112, 32, 1014, { .fields = { 32, 216 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -80, 40, 1030, { .fields = { 72, 72 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 16 } }, 16, -8, 1025, { .fields = { 32, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, 8, -8, 1043, { .fields = { 112, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, 8, 32, 1052, { .fields = { 104, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 16 } }, 16, 24, 1034, { .fields = { 32, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, 48, -8, 1010, { .fields = { 96, 152 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 16 } }, 48, 24, 1037, { .fields = { 32, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, 48, 40, 1021, { .fields = { 80, 224 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 64, -8, 1015, { .fields = { 48, 240 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 32 } }, 88, -8, 1037, { .fields = { 80, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 16 } }, 80, 24, 1044, { .fields = { 40, 224 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 24 } }, 88, 40, 1033, { .fields = { 48, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 16 } }, 112, -8, 1069, { .fields = { 32, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 112, 24, 1016, { .fields = { 40, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 136, 24, 1000, { .fields = { 48, 80 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, 136, 40, 1005, { .fields = { 72, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, 144, -8, 992, { .fields = { 88, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing ranges for mapped gorge view 6.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover the complete source array, then `SPRITE_BATCH_END` ends the
/// list. The loaded gorge owns these writable records.
static SpriteBatch _gMineGorgeView6SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 3, 0 } },
    { 4, 0, 0, 0, { 0, 0 } },
    { 4, 29, 0, 0, { 5, 0 } },
    { 33, 22, 0, 0, { 1, 0 } },
    { 55, 4, 0, 0, { 4, 0 } },
    { 59, ARRAY_SIZE(_gMineGorgeView6SpriteSources) - 59, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped gorge view 7.
///
/// The view's batches cover all 100 elements. Positions and dimensions are
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// The loaded gorge owns these writable source records.
static SpriteSource _gMineGorgeView7SpriteSources[100] = {
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -128, -120, 453, { .fields = { 96, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -136, -120, 429, { .fields = { 96, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, -136, -88, 483, { .fields = { 104, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 48 } }, -144, -120, 404, { .fields = { 112, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 48 } }, -144, -72, 466, { .fields = { 120, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, -144, 8, 527, { .fields = { 112, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -144, 48, 513, { .fields = { 40, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -136, 40, 575, { .fields = { 64, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -128, 56, 598, { .fields = { 48, 216 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -112, 80, 657, { .fields = { 40, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -104, 96, 691, { .fields = { 64, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -112, 96, 599, { .fields = { 72, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -128, 72, 550, { .fields = { 72, 72 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, -144, 64, 503, { .fields = { 80, 224 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, 80, 453, { .fields = { 104, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, 40, 447, { .fields = { 96, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -120, 64, 621, { .fields = { 72, 216 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -120, 88, 568, { .fields = { 104, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 48 } }, -160, -120, 364, { .fields = { 112, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, -72, 406, { .fields = { 104, 88 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 72 } }, -152, -32, 460, { .fields = { 120, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 72 } }, -160, -32, 431, { .fields = { 120, 72 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -144, 96, 503, { .fields = { 56, 72 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -128, 96, 535, { .fields = { 64, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -72, 3502, { .fields = { 32, 24 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -64, 2829, { .fields = { 32, 40 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -56, 2486, { .fields = { 32, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -48, 2112, { .fields = { 32, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -40, 2092, { .fields = { 80, 216 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -32, 1560, { .fields = { 104, 40 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -24, 1383, { .fields = { 64, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -16, 1225, { .fields = { 40, 240 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, -8, 1097, { .fields = { 56, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, 0, 980, { .fields = { 32, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, 8, 908, { .fields = { 32, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, 16, 830, { .fields = { 32, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, 24, 777, { .fields = { 32, 200 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, 32, 726, { .fields = { 32, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -160, -120, 338, { .fields = { 40, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -152, -112, 342, { .fields = { 40, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -144, -104, 404, { .fields = { 48, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -136, -96, 428, { .fields = { 48, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -128, -104, 478, { .fields = { 80, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 32 } }, -128, -72, 501, { .fields = { 64, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -120, -40, 580, { .fields = { 88, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -120, -8, 603, { .fields = { 48, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -112, 8, 624, { .fields = { 112, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -112, 32, 647, { .fields = { 40, 80 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -104, 48, 662, { .fields = { 48, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, 56, 697, { .fields = { 48, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -88, 64, 727, { .fields = { 24, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -72, 56, 750, { .fields = { 40, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -64, 48, 763, { .fields = { 40, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -56, 48, 625, { .fields = { 32, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -56, 40, 626, { .fields = { 24, 8 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -56, 64, 646, { .fields = { 72, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, -56, 88, 677, { .fields = { 80, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -16, -40, 1789, { .fields = { 32, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -40, -72, 3370, { .fields = { 64, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -32, -64, 2740, { .fields = { 72, 240 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -24, -56, 2132, { .fields = { 104, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -16, -32, 1677, { .fields = { 48, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 0, -24, 1283, { .fields = { 56, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 8, -16, 1110, { .fields = { 96, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 24, 8, 891, { .fields = { 56, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 16, -8, 992, { .fields = { 72, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 32, 16, 814, { .fields = { 56, 216 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 40, 24, 682, { .fields = { 40, 200 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 64, 32, 799, { .fields = { 48, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 80, 40, 791, { .fields = { 24, 80 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 96, 40, 771, { .fields = { 24, 56 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, 40, 757, { .fields = { 24, 88 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 128, 40, 732, { .fields = { 24, 152 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 144, 32, 723, { .fields = { 48, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 32 } }, 40, 40, 691, { .fields = { 80, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 32 } }, 40, 72, 733, { .fields = { 72, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -128, 24, 686, { .fields = { 48, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -120, 40, 709, { .fields = { 64, 24 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -120, 64, 733, { .fields = { 48, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -112, 24, 697, { .fields = { 48, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -96, 24, 708, { .fields = { 32, 224 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -80, 24, 725, { .fields = { 48, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -64, 24, 745, { .fields = { 96, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -72, 56, 769, { .fields = { 112, 240 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 8 } }, -104, 64, 750, { .fields = { 24, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, -112, 80, 676, { .fields = { 88, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -64, 72, 802, { .fields = { 56, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 64, 24, 691, { .fields = { 24, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 64, 40, 724, { .fields = { 64, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, 64, 64, 749, { .fields = { 24, 216 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, 56, 72, 769, { .fields = { 96, 200 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, 88, 24, 711, { .fields = { 24, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, 112, 24, 738, { .fields = { 88, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 128, 24, 741, { .fields = { 72, 24 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, 16, 736, { .fields = { 24, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, 56, 761, { .fields = { 32, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 32 } }, 104, 56, 764, { .fields = { 72, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, 88, 786, { .fields = { 32, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 8 } }, 128, 56, 786, { .fields = { 40, 248 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 152, 72, 814, { .fields = { 64, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing ranges for mapped gorge view 7.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover the complete source array, then `SPRITE_BATCH_END` ends the
/// list. The loaded gorge owns these writable records.
static SpriteBatch _gMineGorgeView7SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 3, 0 } },
    { 24, 33, 0, 0, { 0, 0 } },
    { 57, 19, 0, 0, { 2, 0 } },
    { 76, ARRAY_SIZE(_gMineGorgeView7SpriteSources) - 76, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped gorge view 8.
///
/// The view's batches cover all 100 elements. Positions and dimensions are
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// The loaded gorge owns these writable source records.
static SpriteSource _gMineGorgeView8SpriteSources[100] = {
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 80, 1839, { .fields = { 104, 248 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 128, 80, 1853, { .fields = { 112, 104 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 120, 72, 1897, { .fields = { 120, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 112, 72, 1868, { .fields = { 112, 136 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, 96, 64, 1692, { .fields = { 112, 224 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 40 } }, 80, 56, 1382, { .fields = { 24, 216 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 48 } }, 64, 48, 1194, { .fields = { 24, 56 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 48 } }, 48, 48, 1062, { .fields = { 96, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 56 } }, 32, 40, 965, { .fields = { 32, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 56 } }, 24, 32, 891, { .fields = { 40, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, 8, 32, 839, { .fields = { 112, 80 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 40 } }, -8, 24, 777, { .fields = { 120, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, 24, 717, { .fields = { 80, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -160, -40, 448, { .fields = { 96, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 104 } }, -160, -24, 464, { .fields = { 80, 104 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 96 } }, -128, -16, 510, { .fields = { 72, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 104 } }, -104, -16, 550, { .fields = { 96, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 96 } }, -80, 0, 601, { .fields = { 56, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 96 } }, -56, 8, 637, { .fields = { 56, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 104 } }, -40, 8, 678, { .fields = { 112, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 32, 1581, { .fields = { 112, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 40, 1902, { .fields = { 104, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -128, 48, 477, { .fields = { 104, 104 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 88, 1857, { .fields = { 96, 248 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, -152, -120, 513, { .fields = { 80, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -128, -120, 536, { .fields = { 104, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -112, -104, 563, { .fields = { 24, 152 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 48 } }, -104, -88, 571, { .fields = { 16, 200 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 64 } }, -112, -56, 534, { .fields = { 64, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 56 } }, -120, 0, 493, { .fields = { 40, 56 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, -96, 586, { .fields = { 96, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -88, -88, 607, { .fields = { 48, 240 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, -80, 630, { .fields = { 104, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -72, -80, 639, { .fields = { 88, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -64, -72, 662, { .fields = { 88, 224 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -56, -64, 689, { .fields = { 88, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -48, -64, 701, { .fields = { 96, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -40, -56, 728, { .fields = { 96, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -32, -48, 758, { .fields = { 104, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 120 } }, -24, -48, 761, { .fields = { 120, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -16, -40, 802, { .fields = { 104, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -8, -32, 834, { .fields = { 96, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 0, -32, 871, { .fields = { 96, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 8, -24, 909, { .fields = { 96, 80 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, 16, -16, 962, { .fields = { 104, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 72 } }, 24, 16, 1011, { .fields = { 48, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 32, -8, 1072, { .fields = { 96, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 40, 0, 1138, { .fields = { 96, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 32 } }, 48, 0, 1202, { .fields = { 120, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 80 } }, 56, 8, 1282, { .fields = { 48, 88 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 64, 16, 1346, { .fields = { 96, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 56 } }, 72, 16, 1404, { .fields = { 56, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 80, 24, 1490, { .fields = { 96, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 80, 64, 1765, { .fields = { 104, 136 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 96, 32, 1727, { .fields = { 96, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 112, 40, 2000, { .fields = { 112, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 40 } }, 120, 40, 2011, { .fields = { 40, 216 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 144, 64, 1914, { .fields = { 96, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 24 } }, 128, 40, 1952, { .fields = { 96, 208 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -104, -24, 886, { .fields = { 88, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -48, 0, 1085, { .fields = { 24, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -32, 8, 1177, { .fields = { 56, 248 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, -8, 16, 1296, { .fields = { 80, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 32, 1614, { .fields = { 16, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 48, 40, 1764, { .fields = { 8, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 72, 48, 2159, { .fields = { 16, 248 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 48, 2284, { .fields = { 0, 40 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 0, 80, 1355, { .fields = { 0, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 24, 88, 1511, { .fields = { 120, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -160, -56, 746, { .fields = { 88, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -152, -48, 770, { .fields = { 88, 80 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -144, -48, 781, { .fields = { 88, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -136, -40, 808, { .fields = { 88, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -120, -32, 835, { .fields = { 88, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -128, -40, 819, { .fields = { 88, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -112, -32, 854, { .fields = { 88, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, -24, 913, { .fields = { 80, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 48 } }, -88, -24, 934, { .fields = { 16, 152 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 64 } }, -96, 16, 934, { .fields = { 72, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, -16, 966, { .fields = { 88, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -72, -16, 985, { .fields = { 88, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, -64, -16, 1018, { .fields = { 112, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -56, -8, 1050, { .fields = { 88, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 88 } }, -40, 0, 1130, { .fields = { 48, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -24, 8, 1223, { .fields = { 88, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, -16, 8, 1242, { .fields = { 80, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 0, 16, 1355, { .fields = { 80, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 8, 16, 1375, { .fields = { 80, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 16, 24, 1429, { .fields = { 0, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 24, 24, 1500, { .fields = { 112, 160 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 40, 32, 1689, { .fields = { 104, 80 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 56, 40, 1862, { .fields = { 8, 232 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 64, 40, 1990, { .fields = { 88, 240 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 40, 48 } }, 120, 72, 1814, { .fields = { 0, 104 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 48 } }, 96, 72, 1926, { .fields = { 0, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 48 } }, 72, 72, 2026, { .fields = { 8, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, 72, 2134, { .fields = { 24, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 40 } }, 24, 80, 2330, { .fields = { 120, 192 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 40 } }, 0, 80, 2486, { .fields = { 104, 40 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X896, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 40 } }, -24, 80, 2826, { .fields = { 120, 152 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing ranges for mapped gorge view 8.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover the complete source array, then `SPRITE_BATCH_END` ends the
/// list. The loaded gorge owns these writable records.
static SpriteBatch _gMineGorgeView8SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 3, 0 } },
    { 20, 39, 0, 0, { 0, 0 } },
    { 59, 34, 0, 0, { 2, 0 } },
    { 93, ARRAY_SIZE(_gMineGorgeView8SpriteSources) - 93, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped gorge view 9.
///
/// The view's batches cover all 40 elements. Positions and dimensions are
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// The loaded gorge owns these writable source records.
static SpriteSource _gMineGorgeView9SpriteSources[40] = {
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 48, -24, 2572, { .fields = { 112, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 32, 8 } }, 56, -8, 3250, { .fields = { 48, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, -56, 112, 1051, { .fields = { 80, 216 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 40, -16, 2235, { .fields = { 80, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 24 } }, 56, -32, 3271, { .fields = { 112, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 64, -32, 3258, { .fields = { 80, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 24, -8, 1671, { .fields = { 80, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 8, 0, 1344, { .fields = { 88, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 48 } }, 0, 8, 1175, { .fields = { 120, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -16, 16, 1030, { .fields = { 96, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 64 } }, -24, 24, 925, { .fields = { 120, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -40, 32, 862, { .fields = { 80, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -56, 40, 744, { .fields = { 96, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -72, 48, 683, { .fields = { 96, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 64 } }, -80, 56, 625, { .fields = { 120, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 24 } }, -96, 64, 576, { .fields = { 104, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, -112, 72, 537, { .fields = { 88, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 24 } }, -136, 80, 488, { .fields = { 96, 200 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 32 } }, -160, 88, 443, { .fields = { 96, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 48, 16 } }, 40, 0, 2750, { .fields = { 48, 32 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 64, 16 } }, 24, 16, 2096, { .fields = { 40, 96 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 72, 24 } }, 8, 32, 1674, { .fields = { 40, 144 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 96, 32 } }, -16, 56, 1353, { .fields = { 24, 64 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 112, 32 } }, -40, 88, 1083, { .fields = { 16, 224 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 8 } }, 24, 112, 451, { .fields = { 96, 112 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 32 } }, 32, 88, 451, { .fields = { 104, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 32, 72, 530, { .fields = { 72, 200 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 16 } }, 48, 56, 648, { .fields = { 72, 184 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 48 } }, 56, 72, 932, { .fields = { 112, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 56, 40, 795, { .fields = { 64, 48 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, 0, 1708, { .fields = { 72, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 16 } }, 96, -32, 3033, { .fields = { 80, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 8, 16 } }, 104, -16, 3050, { .fields = { 72, 16 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 24, 8 } }, 56, 32, 869, { .fields = { 56, 120 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 64, 16, 1097, { .fields = { 64, 128 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 64, 24, 974, { .fields = { 64, 176 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 80, -16, 2628, { .fields = { 64, 168 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 80, -8, 2384, { .fields = { 56, 0 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 80, -24, 2840, { .fields = { 64, 136 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_GORGE_SPRITE_TEXTURE_PAGE_X960, MINE_GORGE_SPRITE_CLUT, { .fields = { 16, 8 } }, 80, -32, 3156, { .fields = { 64, 216 } }, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, MINE_GORGE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing ranges for mapped gorge view 9.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover the complete source array, then `SPRITE_BATCH_END` ends the
/// list. The loaded gorge owns these writable records.
static SpriteBatch _gMineGorgeView9SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 1, 0 } },
    { 24, ARRAY_SIZE(_gMineGorgeView9SpriteSources) - 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped gorge view 10.
///
/// The zero-count first record selects decoded background strips, followed by
/// `SPRITE_BATCH_END`. Its retained source-slot address supplies no sprites.
/// The loaded gorge owns this writable list.
static SpriteBatch _gMineGorgeView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped gorge view 11.
///
/// The zero-count first record selects decoded background strips, followed by
/// `SPRITE_BATCH_END`. Its retained source-slot address supplies no sprites.
/// The loaded gorge owns this writable list.
static SpriteBatch _gMineGorgeView11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gMineGorgeSpriteViews[MINE_GORGE_VIEW_COUNT] = {
    { { .empty = _gMineGorgeView1SpriteBatches }, _gMineGorgeView1SpriteBatches, NULL },
    { { .elements = _gMineGorgeView2SpriteSources }, _gMineGorgeView2SpriteBatches, NULL },
    { { .elements = _gMineGorgeView3SpriteSources }, _gMineGorgeView3SpriteBatches, NULL },
    { { .elements = _gMineGorgeView4SpriteSources }, _gMineGorgeView4SpriteBatches, NULL },
    { { .empty = _gMineGorgeView5SpriteBatches }, _gMineGorgeView5SpriteBatches, NULL },
    { { .elements = _gMineGorgeView6SpriteSources }, _gMineGorgeView6SpriteBatches, NULL },
    { { .elements = _gMineGorgeView7SpriteSources }, _gMineGorgeView7SpriteBatches, NULL },
    { { .elements = _gMineGorgeView8SpriteSources }, _gMineGorgeView8SpriteBatches, NULL },
    { { .elements = _gMineGorgeView9SpriteSources }, _gMineGorgeView9SpriteBatches, NULL },
    { { .empty = _gMineGorgeView10SpriteBatches }, _gMineGorgeView10SpriteBatches, NULL },
    { { .empty = _gMineGorgeView11SpriteBatches }, _gMineGorgeView11SpriteBatches, NULL },
};

/// Six point lights shared by both gorge room states.
///
/// All logical views admit these lights. Positions and falloff radii are in
/// world units; RGB intensities have 12 fractional bits (ONE is full strength).
/// Room setup parents their mutable coordinates to the view, and shading
/// queries overwrite attenuation. Storage remains live while the gorge is loaded.
static WorldCoordPointLight _gMineGorgePointLights[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x42FC, -1993, 3362 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2703 }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x397E, -1993, 597 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2703 }, { 0, 0 } }, 600, 3400 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2A19, -498, 2232 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2703 }, { 0, 0 } }, 600, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8379, -498, 782 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2703 }, { 0, 0 } }, 600, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3974, -1993, 667 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2621 }, { 0, 0 } }, 459, 4663 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1131, -1993, 2674 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2703 }, { 0, 0 } }, 381, 2919 },
};

/// Shared light collection for both gorge room states.
///
/// Borrows all six mutable point lights; there are no directional or cone
/// lights. The record and its array stay live while the gorge is loaded.
static WorldCoordRoomLights _gMineGorgeRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gMineGorgePointLights),
    .pointLights           = _gMineGorgePointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Eight directed view-boundary quads shared by both gorge room states.
///
/// Pairs connect logical views 2<->3, 3<->4, 4<->5 and 5<->6; camera remapping
/// does not change these IDs. The final quad carries LAST and is active, not a
/// sentinel-only record. Room binding fills coordinates and mutable list links;
/// collision latches hits. Keep this array loaded until the runtime list is cleared.
static WorldCollisionTrigger _gMineGorgeViewBoundaryTriggers[8] = {
    { NULL, NULL, NULL, { 0x3AD2, -1392, 2112, 0 }, { { -16, -2368, -2704, 0 }, { 16, -2368, 2704, 0 }, { -16, 2368, -2704, 0 }, { 16, 2368, 2704, 0 } }, { 4097, 0, -25, 0 }, { 0, 0, ONE, 0 }, 3593, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3BC1, -1552, 1984, 0 }, { { 0, -2272, 2720, 0 }, { 0, -2272, -2720, 0 }, { 0, 2272, 2720, 0 }, { 0, 2272, -2720, 0 } }, { -4110, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3537, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3080, -864, 1952, 0 }, { { -16, -2768, -2704, 0 }, { 16, -2768, 2704, 0 }, { -16, 2768, -2704, 0 }, { 16, 2768, 2704, 0 } }, { 4102, 0, -25, 0 }, { 0, 0, ONE, 0 }, 3865, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x31A1, -1280, 1984, 0 }, { { 0, -2272, 2720, 0 }, { 0, -2272, -2720, 0 }, { 0, 2272, 2720, 0 }, { 0, 2272, -2720, 0 } }, { -4110, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3537, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8160, -1216, 1792, 0 }, { { -16, -2768, -2704, 0 }, { 16, -2768, 2704, 0 }, { -16, 2768, -2704, 0 }, { 16, 2768, 2704, 0 } }, { 4102, 0, -25, 0 }, { 0, 0, ONE, 0 }, 3865, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8353, -1344, 1792, 0 }, { { 0, -2272, 2720, 0 }, { 0, -2272, -2720, 0 }, { 0, 2272, 2720, 0 }, { 0, 2272, -2720, 0 } }, { -4110, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3537, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4672, -1312, 1728, 0 }, { { 0, -2272, 2720, 0 }, { 0, -2272, -2720, 0 }, { 0, 2272, 2720, 0 }, { 0, 2272, -2720, 0 } }, { -4110, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3537, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4513, -1408, 1857, 0 }, { { -16, -2768, -2704, 0 }, { 16, -2768, 2704, 0 }, { -16, 2768, -2704, 0 }, { 16, 2768, 2704, 0 } }, { 4102, 0, -25, 0 }, { 0, 0, ONE, 0 }, 3865, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Action quads before the oak board is laid.
///
/// Three exits pack gorge endpoint / destination arrival IDs as 1/2 (tunnel),
/// 2/1 (refuge) and 3/1 (cavern). CAP slots 1, 6 and 7 are interactions;
/// automatic room action 1 starts the encounter, and room event 255 gates board
/// use. The final live quad carries LAST. Coordinates and radii use room game
/// units; normals use 4096 per unit. Setup binds and links this mutable storage,
/// which must stay loaded until the active action list is cleared.
static WorldCollisionTrigger _gMineGorgeRoom1ActionTriggers[8] = {
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 0x45B0, -48, 1504, 0 },
        .vertices     = { { -496, 0, -736, 0 }, { 496, 0, -736, 0 }, { -496, 0, 736, 0 }, { 496, 0, 736, 0 } },
        .normal       = { 0, 4107, 0, 0 },
        .facingNormal = { -ONE, 0, 0, 0 },
        .radius       = 886,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_MINE_TUNNEL,
        .parameter1   = (MINE_GORGE_WARP_TUNNEL << 4) | 2,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 1408, -48, 3744, 0 },
        .vertices     = { { 704, 0, -496, 0 }, { 704, 0, 496, 0 }, { -704, 0, -496, 0 }, { -704, 0, 496, 0 } },
        .normal       = { 0, 4105, 0, 0 },
        .facingNormal = { 0, 0, -ONE, 0 },
        .radius       = 861,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_MINE_REFUGE,
        .parameter1   = (MINE_GORGE_WARP_REFUGE << 4) | 1,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { -96, -48, 1888, 0 },
        .vertices     = { { 496, 0, 1536, 0 }, { -496, 0, 1536, 0 }, { 496, 0, -1536, 0 }, { -496, 0, -1536, 0 } },
        .normal       = { 0, 4097, 0, 0 },
        .facingNormal = { ONE, 0, 0, 0 },
        .radius       = 1614,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_MINE_CAVERN,
        .parameter1   = (MINE_GORGE_WARP_CAVERN << 4) | 1,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 0x2DA0, -64, 1568, 0 },
        .vertices     = { { -688, 0, -1024, 0 }, { 688, 0, -1024, 0 }, { -688, 0, 1024, 0 }, { 688, 0, 1024, 0 } },
        .normal       = { 0, 4105, 0, 0 },
        .facingNormal = { 4095, 0, 0, 0 },
        .radius       = 1227,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = MINE_GORGE_CAP_INTERACTION_1,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 9888, -64, 1536, 0 },
        .vertices     = { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } },
        .normal       = { 0, 4097, 0, 0 },
        .facingNormal = { 4095, 0, 0, 0 },
        .radius       = 1137,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC,
        .parameter0   = MINE_GORGE_ROOM_ACTION_ENCOUNTER,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 2752, -64, 3712, 0 },
        .vertices     = { { 704, 0, -496, 0 }, { 704, 0, 496, 0 }, { -704, 0, -496, 0 }, { -704, 0, 496, 0 } },
        .normal       = { 0, 4105, 0, 0 },
        .facingNormal = { 0, 0, -ONE, 0 },
        .radius       = 861,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = MINE_GORGE_CAP_INTERACTION_6,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 0x42E0, -64, 3648, 0 },
        .vertices     = { { -1840, 0, -1424, 0 }, { 1232, 0, -1424, 0 }, { -1840, 0, 624, 0 }, { 1232, 0, 624, 0 } },
        .normal       = { 0, 4095, 0, 0 },
        .facingNormal = { -ONE, 0, 0, 0 },
        .radius       = 2318,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = MINE_GORGE_CAP_INTERACTION_7,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 0x34D0, -64, 1568, 0 },
        .vertices     = { { -2560, 0, -1024, 0 }, { 2560, 0, -1024, 0 }, { -2560, 0, 1024, 0 }, { 2560, 0, 1024, 0 } },
        .normal       = { 0, 4095, 0, 0 },
        .facingNormal = { 4095, 0, 0, 0 },
        .radius       = 2757,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_ROOM,
        .parameter0   = WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST,
        .hit          = 0,
    },
};

/// Action quads after the oak board is laid.
///
/// Keeps the three exits, automatic encounter action 1 and CAP interaction 6.
/// The tunnel exit expands and the encounter point moves; board event 255 and
/// CAP interactions 1/7 are absent. The final interaction quad carries LAST.
/// Coordinates and radii use room game units; normals use 4096 per unit. Setup
/// mutates the view binding, links, enables and hits, so keep this storage loaded
/// until the active action list is cleared.
static WorldCollisionTrigger _gMineGorgeRoom2ActionTriggers[5] = {
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 0x45B0, -48, 1840, 0 },
        .vertices     = { { -496, 0, -1968, 0 }, { 496, 0, -1968, 0 }, { -496, 0, 1968, 0 }, { 496, 0, 1968, 0 } },
        .normal       = { 0, 4102, 0, 0 },
        .facingNormal = { -ONE, 0, 0, 0 },
        .radius       = 2027,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_MINE_TUNNEL,
        .parameter1   = (MINE_GORGE_WARP_TUNNEL << 4) | 2,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 1408, -48, 3744, 0 },
        .vertices     = { { 704, 0, -496, 0 }, { 704, 0, 496, 0 }, { -704, 0, -496, 0 }, { -704, 0, 496, 0 } },
        .normal       = { 0, 4105, 0, 0 },
        .facingNormal = { 0, 0, -ONE, 0 },
        .radius       = 861,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_MINE_REFUGE,
        .parameter1   = (MINE_GORGE_WARP_REFUGE << 4) | 1,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { -96, -48, 1888, 0 },
        .vertices     = { { 496, 0, 1536, 0 }, { -496, 0, 1536, 0 }, { 496, 0, -1536, 0 }, { -496, 0, -1536, 0 } },
        .normal       = { 0, 4097, 0, 0 },
        .facingNormal = { ONE, 0, 0, 0 },
        .radius       = 1614,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_MINE_CAVERN,
        .parameter1   = (MINE_GORGE_WARP_CAVERN << 4) | 1,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 8927, -64, 1536, 0 },
        .vertices     = { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } },
        .normal       = { 0, 4097, 0, 0 },
        .facingNormal = { 4095, 0, 0, 0 },
        .radius       = 1137,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC,
        .parameter0   = MINE_GORGE_ROOM_ACTION_ENCOUNTER,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 2752, -64, 3712, 0 },
        .vertices     = { { 704, 0, -496, 0 }, { 704, 0, 496, 0 }, { -704, 0, -496, 0 }, { -704, 0, 496, 0 } },
        .normal       = { 0, 4105, 0, 0 },
        .facingNormal = { 0, 0, -ONE, 0 },
        .radius       = 861,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = MINE_GORGE_CAP_INTERACTION_6,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST,
        .hit          = 0,
    },
};

enum {
    MINE_GORGE_BAT_RESOURCE_ID                   = 37,
    MINE_GORGE_MIND_SUCKLER_RESOURCE_ID          = 15,
    MINE_GORGE_MOTH_RESOURCE_ID                  = 8,
    MINE_GORGE_BAT_FILE_NUMBER                   = 37,
    MINE_GORGE_MIND_SUCKLER_FILE_NUMBER          = 15,
    MINE_GORGE_MOTH_FILE_NUMBER                  = 7,
    MINE_GORGE_BAT_MODE_WANDER                   = 0,
    MINE_GORGE_BAT_MODE_WAIT_WAVE_11             = 20,
    MINE_GORGE_MIND_SUCKLER_MODE_WALL_NO_REPERCH = 2,
    MINE_GORGE_AREA_VARIANT_BAT_WAVE             = 1,
    MINE_GORGE_AREA_VARIANT_BATS                 = 2,
    MINE_GORGE_AREA_VARIANT_MIND_SUCKLERS        = 3,
    MINE_GORGE_AREA_VARIANT_MIND_SUCKLERS_MOTHS  = 4,
    MINE_GORGE_AREA_VARIANT_BATS_TURN_ROW_6      = 11
};

/// Variant-1 bat loading and spawn recipe, followed by AREA_PLACEMENT_END.
///
/// Resource ID 37 selects global file 103700 and descriptor zero in the loaded
/// actor_103700 package. This table and its placement pair stay live for loading
/// and actor spawning; its separately stored copies serve other variants.
static AreaResource _gMineGorgeVariant1Resources[2] = {
    { .entryId = MINE_GORGE_BAT_RESOURCE_ID, .fileNumber = MINE_GORGE_BAT_FILE_NUMBER, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor103700BatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant-2 bat loading and spawn recipe, followed by AREA_PLACEMENT_END.
///
/// Resource ID 37 selects global file 103700 and descriptor zero in the loaded
/// actor_103700 package. Borrowed by loading and spawning while the gorge is loaded.
static AreaResource _gMineGorgeVariant2Resources[2] = {
    { .entryId = MINE_GORGE_BAT_RESOURCE_ID, .fileNumber = MINE_GORGE_BAT_FILE_NUMBER, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor103700BatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant-3 Mind Suckler loading and spawn recipe, followed by AREA_PLACEMENT_END.
///
/// Resource ID 15 selects global file 101500 and descriptor zero in the loaded
/// actor_101500 package. Borrowed by loading and spawning while the gorge is loaded.
static AreaResource _gMineGorgeVariant3Resources[2] = {
    { .entryId = MINE_GORGE_MIND_SUCKLER_RESOURCE_ID, .fileNumber = MINE_GORGE_MIND_SUCKLER_FILE_NUMBER, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor01500MindSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant-4 Mind Suckler and moth recipes, followed by AREA_PLACEMENT_END.
///
/// Resource IDs 15 and 8 select files 101500 and 200700, each using descriptor
/// zero in its loaded actor package. Borrowed by loading and spawning while the
/// gorge is loaded; resource IDs need not equal their decimal file numbers.
static AreaResource _gMineGorgeVariant4Resources[3] = {
    { .entryId = MINE_GORGE_MIND_SUCKLER_RESOURCE_ID, .fileNumber = MINE_GORGE_MIND_SUCKLER_FILE_NUMBER, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor01500MindSucklerTaskDesc },
    { .entryId = MINE_GORGE_MOTH_RESOURCE_ID, .fileNumber = MINE_GORGE_MOTH_FILE_NUMBER, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor00700MothTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant-11 bat loading and spawn recipe, followed by AREA_PLACEMENT_END.
///
/// Resource ID 37 selects global file 103700 and descriptor zero in the loaded
/// actor_103700 package. Borrowed by loading and spawning while the gorge is loaded.
static AreaResource _gMineGorgeVariant11Resources[2] = {
    { .entryId = MINE_GORGE_BAT_RESOURCE_ID, .fileNumber = MINE_GORGE_BAT_FILE_NUMBER, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor103700BatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Twelve variant-1 bats held for encounter wave 11, then AREA_PLACEMENT_END.
///
/// Positions use world units and yaw uses 4096 per turn. Mode 20 waits for the
/// encounter counter to reach 11; turn-rate row 5 is within the bat's eight rows.
/// Base files use texture page offset 0 and CLUT row offset 2. Spawned actors
/// borrow these placements, so the gorge must stay loaded for their lifetimes.
static AreaPlacement _gMineGorgeVariant1Placements[13] = {
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WAIT_WAVE_11, 6300, 2500, -600, 600, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WAIT_WAVE_11, 8000, 1200, 2500, 1250, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WAIT_WAVE_11, 8500, 1300, 3000, 1500, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WAIT_WAVE_11, 9500, 1500, 2500, 2048, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WAIT_WAVE_11, 9500, 1400, 3500, 2048, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WAIT_WAVE_11, 0x2904, 1100, 3000, 2700, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WAIT_WAVE_11, 6700, 2400, -800, 400, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WAIT_WAVE_11, 8500, 1200, 0, 500, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WAIT_WAVE_11, 9500, 1300, 500, 0, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WAIT_WAVE_11, 9500, 1400, -500, 0, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WAIT_WAVE_11, 0x2904, 1500, 0, 3300, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WAIT_WAVE_11, 6000, 2700, -400, 900, 0, 0, 2, 5 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Twelve immediately wandering variant-2 bats, then AREA_PLACEMENT_END.
///
/// Positions use world units and yaw uses 4096 per turn; turn-rate row 5 is
/// within the bat's eight rows. Base files use texture page offset 0 and CLUT
/// row offset 2. Spawned actors borrow these placements while the gorge is loaded.
static AreaPlacement _gMineGorgeVariant2Placements[13] = {
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 4000, -1300, 3500, 2600, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 3500, -1500, 2900, 2800, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 5000, -1600, 2500, 2900, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 5500, -1700, 3000, 2800, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 6500, -1300, 2500, 2100, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 6500, -1400, 3500, 2900, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 7500, -1600, 3000, 3100, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 5000, -1800, -500, 3600, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 5500, -1700, 0, 3200, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 6500, -1300, 500, 3900, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 6500, -1500, -500, 3500, 0, 0, 2, 5 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 7500, -1300, 0, 3300, 0, 0, 2, 5 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Six wall-perched variant-3 Mind Sucklers, then AREA_PLACEMENT_END.
///
/// Positions use world units and yaw uses 4096 per turn. Actor variant 0 and
/// mode 2 start on a wall and prevent later wall settling. Base files use texture
/// page offset 0 and CLUT row offset 2; parameter row 0 has no observed reader.
/// Spawned actors borrow these placements while the gorge is loaded.
static AreaPlacement _gMineGorgeVariant3Placements[7] = {
    { MINE_GORGE_MIND_SUCKLER_RESOURCE_ID, 0, MINE_GORGE_MIND_SUCKLER_MODE_WALL_NO_REPERCH, 3000, -2300, 4000, 0, 0, 0, 2, 0 },
    { MINE_GORGE_MIND_SUCKLER_RESOURCE_ID, 0, MINE_GORGE_MIND_SUCKLER_MODE_WALL_NO_REPERCH, 4000, -1800, 4000, 0, 0, 0, 2, 0 },
    { MINE_GORGE_MIND_SUCKLER_RESOURCE_ID, 0, MINE_GORGE_MIND_SUCKLER_MODE_WALL_NO_REPERCH, 2500, -1600, 0, 2048, 0, 0, 2, 0 },
    { MINE_GORGE_MIND_SUCKLER_RESOURCE_ID, 0, MINE_GORGE_MIND_SUCKLER_MODE_WALL_NO_REPERCH, 3000, -2200, 0, 2048, 0, 0, 2, 0 },
    { MINE_GORGE_MIND_SUCKLER_RESOURCE_ID, 0, MINE_GORGE_MIND_SUCKLER_MODE_WALL_NO_REPERCH, 0x3E1C, -2000, 500, 2048, 0, 0, 2, 0 },
    { MINE_GORGE_MIND_SUCKLER_RESOURCE_ID, 0, MINE_GORGE_MIND_SUCKLER_MODE_WALL_NO_REPERCH, 0x3DEA, -2200, 3800, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Four wall-perched Mind Sucklers and eight moths for variant 4, then AREA_PLACEMENT_END.
///
/// Positions use world units and yaw uses 4096 per turn. Mind Sucklers use
/// actor variant 0/mode 2 to prevent later wall settling, and texture offsets
/// (0 pages, 2 CLUT rows). Moths use (2 pages, 4 CLUT rows) before their spawn
/// adjustment and pursuit-speed row 0, within their eight rows. Spawned actors
/// borrow these placements while the gorge is loaded.
static AreaPlacement _gMineGorgeVariant4Placements[13] = {
    { MINE_GORGE_MIND_SUCKLER_RESOURCE_ID, 0, MINE_GORGE_MIND_SUCKLER_MODE_WALL_NO_REPERCH, 3000, -2000, 4000, 0, 0, 0, 2, 0 },
    { MINE_GORGE_MIND_SUCKLER_RESOURCE_ID, 0, MINE_GORGE_MIND_SUCKLER_MODE_WALL_NO_REPERCH, 4000, -2000, 4000, 0, 0, 0, 2, 0 },
    { MINE_GORGE_MIND_SUCKLER_RESOURCE_ID, 0, MINE_GORGE_MIND_SUCKLER_MODE_WALL_NO_REPERCH, 2000, -2000, 0, 2048, 0, 0, 2, 0 },
    { MINE_GORGE_MIND_SUCKLER_RESOURCE_ID, 0, MINE_GORGE_MIND_SUCKLER_MODE_WALL_NO_REPERCH, 3000, -2500, 0, 2048, 0, 0, 2, 0 },
    { MINE_GORGE_MOTH_RESOURCE_ID, 0, 0, 8000, -2000, 2000, 3072, 0, 2, 4, 0 },
    { MINE_GORGE_MOTH_RESOURCE_ID, 0, 0, 9000, -2000, 2000, 3072, 0, 2, 4, 0 },
    { MINE_GORGE_MOTH_RESOURCE_ID, 0, 0, 8000, -1000, 2000, 3072, 0, 2, 4, 0 },
    { MINE_GORGE_MOTH_RESOURCE_ID, 0, 0, 9000, -1000, 2000, 3072, 0, 2, 4, 0 },
    { MINE_GORGE_MOTH_RESOURCE_ID, 0, 0, 8000, -2000, 0, 3072, 0, 2, 4, 0 },
    { MINE_GORGE_MOTH_RESOURCE_ID, 0, 0, 9000, -2000, 0, 3072, 0, 2, 4, 0 },
    { MINE_GORGE_MOTH_RESOURCE_ID, 0, 0, 8000, -1000, 0, 3072, 0, 2, 4, 0 },
    { MINE_GORGE_MOTH_RESOURCE_ID, 0, 0, 9000, -1000, 0, 3072, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Twelve immediately wandering variant-11 bats, then AREA_PLACEMENT_END.
///
/// Positions use world units and yaw uses 4096 per turn. Their XZ/yaw values
/// match variant 1, with Y shifted by -3000; turn-rate row 6 is within the bat's
/// eight rows. Base files use texture page offset 0 and CLUT row offset 2.
/// Spawned actors borrow these placements while the gorge is loaded.
static AreaPlacement _gMineGorgeVariant11Placements[13] = {
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 6300, -500, -600, 600, 0, 0, 2, 6 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 8000, -1800, 2500, 1250, 0, 0, 2, 6 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 8500, -1700, 3000, 1500, 0, 0, 2, 6 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 9500, -1500, 2500, 2048, 0, 0, 2, 6 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 9500, -1600, 3500, 2048, 0, 0, 2, 6 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 0x2904, -1900, 3000, 2700, 0, 0, 2, 6 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 6700, -600, -800, 400, 0, 0, 2, 6 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 8500, -1800, 0, 500, 0, 0, 2, 6 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 9500, -1700, 500, 0, 0, 0, 2, 6 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 9500, -1600, -500, 0, 0, 0, 2, 6 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 0x2904, -1500, 0, 3300, 0, 0, 2, 6 },
    { MINE_GORGE_BAT_RESOURCE_ID, 0, MINE_GORGE_BAT_MODE_WANDER, 6000, -300, -400, 900, 0, 0, 2, 6 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gMineGorgeAreaVariants[MINE_GORGE_AREA_VARIANT_COUNT] = {
    [MINE_GORGE_AREA_VARIANT_BAT_WAVE]            = { .placements = _gMineGorgeVariant1Placements, .resources = _gMineGorgeVariant1Resources },
    [MINE_GORGE_AREA_VARIANT_BATS]                = { .placements = _gMineGorgeVariant2Placements, .resources = _gMineGorgeVariant2Resources },
    [MINE_GORGE_AREA_VARIANT_MIND_SUCKLERS]       = { .placements = _gMineGorgeVariant3Placements, .resources = _gMineGorgeVariant3Resources },
    [MINE_GORGE_AREA_VARIANT_MIND_SUCKLERS_MOTHS] = { .placements = _gMineGorgeVariant4Placements, .resources = _gMineGorgeVariant4Resources },
    [MINE_GORGE_AREA_VARIANT_BATS_TURN_ROW_6]     = { .placements = _gMineGorgeVariant11Placements, .resources = _gMineGorgeVariant11Resources },
};

/// Minimum ambient RGB levels for logical gorge views 1..11.
///
/// Entry zero counts the eleven colours that follow. RGB uses 16 units per
/// 8-bit level; the fourth halfword stores weighted brightness, unused by
/// lighting. Both room states borrow this table while the gorge is loaded.
static WorldCoordRoomAmbientEntry _gMineGorgeViewAmbientTable[12] = {
    { .viewCount = ARRAY_SIZE(_gMineGorgeViewAmbientTable) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 500, 530, 350, 496 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 330, 280, 270, 297 } },
    { .color = { 400, 400, 370, 396 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

// Cue 2 selects each base; cue 1 selects the following loaded type-1 bank entry.
enum {
    MINE_GORGE_SURFACE_CLASS_2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x45,
    MINE_GORGE_SURFACE_CLASS_2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x47,
    MINE_GORGE_SURFACE_CLASS_3_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x49,
    MINE_GORGE_SURFACE_CLASS_3_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x4B
};

/// Paired type-1 footstep requests for gorge surface class 3.
///
/// Walking and scripted jumps share their cue base; running uses another pair.
/// The surface record borrows this table while the gorge is loaded.
static WorldCollisionFootstepSounds _gMineGorgeSurfaceClass3FootstepSounds = {
    .walk         = MINE_GORGE_SURFACE_CLASS_3_WALK_SOUND,
    .run          = MINE_GORGE_SURFACE_CLASS_3_RUN_SOUND,
    .scriptedJump = MINE_GORGE_SURFACE_CLASS_3_WALK_SOUND,
};

/// Paired type-1 footstep requests shared by gorge surface classes 2 and 4.
///
/// Walking and scripted jumps share their cue base; running uses another pair.
/// Both surface records borrow this table while the gorge is loaded.
static WorldCollisionFootstepSounds _gMineGorgeSurfaceClass2FootstepSounds = {
    .walk         = MINE_GORGE_SURFACE_CLASS_2_WALK_SOUND,
    .run          = MINE_GORGE_SURFACE_CLASS_2_RUN_SOUND,
    .scriptedJump = MINE_GORGE_SURFACE_CLASS_2_WALK_SOUND,
};

/// Shared blocking collision policy for gorge classes 0 and 5..7, without footstep cues.
///
/// Permits weapon impacts and applies pushback. The class directory borrows
/// this scalar record while the gorge is loaded; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gMineGorgeBlockingSilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Probe-transparent collision policy for gorge class 1, without footstep cues.
///
/// Ignores weapon impacts but still applies pushback. The class directory borrows
/// this scalar record while the gorge is loaded; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gMineGorgePassThroughSilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking collision policy and footstep cues for gorge class 2.
///
/// Permits weapon impacts and applies pushback. Class 4 borrows the same cue set.
/// The loaded gorge owns this scalar record; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gMineGorgeSurfaceClass2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gMineGorgeSurfaceClass2FootstepSounds,
};

/// Blocking collision policy and footstep cues for gorge class 3.
///
/// Permits weapon impacts and applies pushback, with its own footstep set.
/// The loaded gorge owns this scalar record; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gMineGorgeSurfaceClass3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gMineGorgeSurfaceClass3FootstepSounds,
};

/// Probe-transparent collision policy and class-2 footstep cues for gorge class 4.
///
/// Ignores weapon impacts but still applies pushback. The class directory borrows
/// this scalar record while the gorge is loaded; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gMineGorgeSurfaceClass4Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gMineGorgeSurfaceClass2FootstepSounds,
};

WorldCollisionSurfaceProperties* gMineGorgeSurfacePropertiesByClass[8] = {
    &_gMineGorgeBlockingSilentSurfaceProperties,
    &_gMineGorgePassThroughSilentSurfaceProperties,
    &_gMineGorgeSurfaceClass2Properties,
    &_gMineGorgeSurfaceClass3Properties,
    &_gMineGorgeSurfaceClass4Properties,
    &_gMineGorgeBlockingSilentSurfaceProperties,
    &_gMineGorgeBlockingSilentSurfaceProperties,
    &_gMineGorgeBlockingSilentSurfaceProperties,
};

static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);

/// Draws two consecutive fixed world-point flares with the gorge's texture and size.
///
/// `worldPoints` must contain two word-aligned, readable eight-byte `SVECTOR`s,
/// borrowed only during this call and projected in array order. Each point with
/// nonnegative projection flags queues one flickering semitransparent quad and
/// must have nonzero camera Z / 4. Texture column 1 uses a pixel half-extent of
/// 768 * 39 / (camera Z / 4). Requires composed view matrices, space for one
/// `GlowCentreScratch` on the initialized scratch stack, and a current ordering
/// table and frame packet arena with room for up to two quads.
static inline void _mineGorgeDrawFlarePair(const SVECTOR worldPoints[2])
{
    _glowDrawFlare(&worldPoints[0], MINE_GORGE_FLARE_TEXTURE_COLUMN, MINE_GORGE_FLARE_RADIUS_SCALE);
    _glowDrawFlare(&worldPoints[1], MINE_GORGE_FLARE_TEXTURE_COLUMN, MINE_GORGE_FLARE_RADIUS_SCALE);
}

void mineGorgeDrawViewFlaresTask(Task* unusedTask)
{
    // Gate ambient effects independently of the fixed flares visible in this view.
    gRoomEffectState->roomEffectMode = _gMineGorgeViewEffectModes[(u8)viewGetMappedIndex() - 1];
    switch ((u8)viewGetMappedIndex()) {
        case 2: {
            const SVECTOR* flarePositions = &_gMineGorgeFlarePositions[4];
            _mineGorgeDrawFlarePair(flarePositions);
            break;
        }
        case 3:
        case 7: {
            const SVECTOR* flarePositions = _gMineGorgeFlarePositions;
            _mineGorgeDrawFlarePair(flarePositions);
            _glowDrawFlare(&flarePositions[2], MINE_GORGE_FLARE_TEXTURE_COLUMN, MINE_GORGE_FLARE_RADIUS_SCALE);
            _glowDrawFlare(&flarePositions[3], MINE_GORGE_FLARE_TEXTURE_COLUMN, MINE_GORGE_FLARE_RADIUS_SCALE);
            _glowDrawFlare(&flarePositions[4], MINE_GORGE_FLARE_TEXTURE_COLUMN, MINE_GORGE_FLARE_RADIUS_SCALE);
            break;
        }
        case 4:
        case 5:
        case 9: {
            const SVECTOR* flarePositions = &_gMineGorgeFlarePositions[2];
            _mineGorgeDrawFlarePair(flarePositions);
            _glowDrawFlare(&flarePositions[2], MINE_GORGE_FLARE_TEXTURE_COLUMN, MINE_GORGE_FLARE_RADIUS_SCALE);
            _glowDrawFlare(&flarePositions[3], MINE_GORGE_FLARE_TEXTURE_COLUMN, MINE_GORGE_FLARE_RADIUS_SCALE);
            break;
        }
        case 6: {
            const SVECTOR* flarePositions = _gMineGorgeFlarePositions;
            _mineGorgeDrawFlarePair(flarePositions);
            break;
        }
        case 8: {
            const SVECTOR* flarePositions = &_gMineGorgeFlarePositions[2];
            _glowDrawFlare(&flarePositions[0], MINE_GORGE_FLARE_TEXTURE_COLUMN, MINE_GORGE_FLARE_RADIUS_SCALE);
            _glowDrawFlare(&flarePositions[2], MINE_GORGE_FLARE_TEXTURE_COLUMN, MINE_GORGE_FLARE_RADIUS_SCALE);
            _glowDrawFlare(&flarePositions[3], MINE_GORGE_FLARE_TEXTURE_COLUMN, MINE_GORGE_FLARE_RADIUS_SCALE);
            break;
        }
        case 10:
        case 11: {
            const SVECTOR* flarePositions = &_gMineGorgeFlarePositions[3];
            _glowDrawFlare(&flarePositions[0], MINE_GORGE_FLARE_TEXTURE_COLUMN, MINE_GORGE_FLARE_RADIUS_SCALE);
            break;
        }
    }
}

#include "../../shared/effects/glow_draw_flare.inc.c"
