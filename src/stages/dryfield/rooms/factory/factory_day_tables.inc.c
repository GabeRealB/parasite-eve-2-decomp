// Dryfield factory by day: the room's tables, included by factory_panel.c.

WorldCollisionRoomResources gDryfieldFactoryRoomCollisionResources[2] = {
    { .grid = &gDryfieldFactoryCollisionGrid, .viewBoundaryTriggers = gDryfieldFactoryViewBoundaryTriggers, .actionTriggers = gDryfieldFactoryActionTriggers, .occluders = NULL },
    { .grid = &gDryfieldFactoryCollisionGrid, .viewBoundaryTriggers = gDryfieldFactoryViewBoundaryTriggers, .actionTriggers = gDryfieldFactoryActionTriggers, .occluders = NULL },
};

/// Room-2 logical-view map used after the barrier is cleared.
///
/// Logical views 1..19 map to camera/image/sprite indices 1..19 in a
/// different order. The twentieth stored byte is zero, outside the valid
/// logical-view domain; lookups use the separate count rather than a sentinel.
static u8 _gDryfieldFactoryClearedBarrierViewMap[20] = {
    1,
    13,
    14,
    15,
    5,
    16,
    17,
    8,
    9,
    10,
    11,
    12,
    2,
    3,
    4,
    6,
    7,
    18,
    19,
    0,
};

u8* gDryfieldFactoryRoomViewMaps[2] = {
    gViewIdentityMap,
    _gDryfieldFactoryClearedBarrierViewMap,
};

WorldCoordRoomLighting gDryfieldFactoryRoomLighting[2] = {
    { .lights = &gDryfieldFactoryRoomLights, .ambientTable = gDryfieldFactoryViewAmbientMinima },
    { .lights = &gDryfieldFactoryRoomLights, .ambientTable = gDryfieldFactoryViewAmbientMinima },
};

enum { DRYFIELD_FACTORY_LOGICAL_VIEW_COUNT = 19 };

ViewCount gDryfieldFactoryRoomViewCounts[2] = { DRYFIELD_FACTORY_LOGICAL_VIEW_COUNT, DRYFIELD_FACTORY_LOGICAL_VIEW_COUNT };

/// Map-marker nibbles shared with the opposite endpoint of each factory door.
enum {
    DRYFIELD_FACTORY_DRIVEWAY_MAP_FLAG  = 474,
    DRYFIELD_FACTORY_BREEZEWAY_MAP_FLAG = 475,
    DRYFIELD_FACTORY_GARAGE_MAP_FLAG    = 473
};

DirectionWarpEntry gDryfieldFactoryWarpEntries[3] = {
    {
        .player         = { { .word = 3072 }, 5178, 0, 1454 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 1024 }, 4530, 0, 1200 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_FACTORY_DRIVEWAY_MAP_FLAG,
    },
    {
        .player         = { { .word = 1024 }, 642, 0, 7493 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 1024 }, 4530, 0, 1200 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 4),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 3),
        .blockedSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 5),
        .initialView    = 7,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_FACTORY_BREEZEWAY_MAP_FLAG,
    },
    {
        .player         = { { .word = 3072 }, 5445, 0, 6866 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 5420, 0, 6000 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x14),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 6),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 6,
        .flags          = DIRECTION_WARP_FLAG_FADE_DEPARTURE,
        .mapFlagId      = DRYFIELD_FACTORY_GARAGE_MAP_FLAG,
    },
};

enum {
    DRYFIELD_FACTORY_COLLISION_CELL_COUNT_X = 2,
    DRYFIELD_FACTORY_COLLISION_CELL_COUNT_Z = 4,
};

/// Room-space normals; the barrier owns entries 0..1 and the lift owns 2..5.
static SVECTOR _gDryfieldFactoryCollisionGridNormals[28] = {
#include "assets/dryfield_factory_collision_0A638_normals.inc"
};

/// Room-space vertices; the barrier owns entries 0..7 and the lift owns 8..15.
static SVECTOR _gDryfieldFactoryCollisionGridVerts[170] = {
#include "assets/dryfield_factory_collision_0A638_verts.inc"
};

/// Live room faces, with entries 0..5 reserved for the barrier and lift.
static WorldCollisionGridFace _gDryfieldFactoryCollisionGridFaces[72] = {
#include "assets/dryfield_factory_collision_0A638_faces.inc"
};

/// Face-index lists for the eight cells, each ended by WORLD_COLLISION_GRID_CELL_END.
static s16 _gDryfieldFactoryCollisionGridCells[288] = {
#include "assets/dryfield_factory_collision_0A638_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldFactoryCollisionGridCells` (288 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_FACTORY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldFactoryCollisionGridCells[(halfwordOffset)])

/// X-major cell pointers, indexed by x * cellCountZ + z.
static s16* _gDryfieldFactoryCollisionGridTable[DRYFIELD_FACTORY_COLLISION_CELL_COUNT_X * DRYFIELD_FACTORY_COLLISION_CELL_COUNT_Z] = {
#include "assets/dryfield_factory_collision_0A638_table.inc"
};
#undef DRYFIELD_FACTORY_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gDryfieldFactoryCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldFactoryCollisionGridNormals,
    .vertices    = _gDryfieldFactoryCollisionGridVerts,
    .faces       = _gDryfieldFactoryCollisionGridFaces,
    .cellFaceIds = _gDryfieldFactoryCollisionGridTable,
    .xBias       = 444,
    .zBias       = 222,
    .cellCountX  = DRYFIELD_FACTORY_COLLISION_CELL_COUNT_X,
    .cellCountZ  = DRYFIELD_FACTORY_COLLISION_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldFactoryCollisionGridFaces),
};

ViewCamera gDryfieldFactoryViewCameras[DRYFIELD_FACTORY_MAPPED_VIEW_COUNT] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3057, 0x44F4, -6028 } }, 240 },
    { { { { 1147, 0, -3931 }, { -130, 4093, -37 }, { 3929, 135, 1147 } }, { -214, 1283, -1020 } }, 246 },
    { { { { 981, 0, 3976 }, { 62, 4095, -15 }, { -3976, 64, 981 } }, { -5724, 1309, -1020 } }, 246 },
    { { { { -3926, 0, -1165 }, { -458, 3765, 1545 }, { 1070, 1612, -3609 } }, { -847, 2531, -7624 } }, 257 },
    { { { { 0, 0, 4096 }, { 23, 4095, 0 }, { -4095, 23, 0 } }, { -2274, 1463, -666 } }, 680 },
    { { { { -4095, 0, 24 }, { 12, 3554, 2035 }, { -21, 2035, -3554 } }, { -4773, 2743, -9126 } }, 230 },
    { { { { -3993, 0, -911 }, { -270, 3911, 1183 }, { 870, 1214, -3813 } }, { -1048, 2819, -0x2BC8 } }, 246 },
    { { { { -1035, 0, 3962 }, { 929, 3981, 242 }, { -3852, 960, -1006 } }, { -5890, 2089, -0x284C } }, 240 },
    { { { { -489, 0, -4066 }, { -641, 4044, 77 }, { 4015, 646, -483 } }, { -337, 1837, -9758 } }, 240 },
    { { { { 619, 0, -4048 }, { -727, 4029, -111 }, { 3982, 736, 608 } }, { -4642, 1552, -5446 } }, 257 },
    { { { { 1741, 0, -3707 }, { 404, 4071, 190 }, { 3685, -446, 1731 } }, { -4747, 1512, -9880 } }, 282 },
    { { { { 0, 0, 4096 }, { 23, 4095, 0 }, { -4095, 23, 0 } }, { -2274, 1463, -666 } }, 680 },
    { { { { 1147, 0, -3931 }, { -130, 4093, -37 }, { 3929, 135, 1147 } }, { -214, 1283, -1020 } }, 246 },
    { { { { 981, 0, 3976 }, { 62, 4095, -15 }, { -3976, 64, 981 } }, { -5724, 1309, -1020 } }, 246 },
    { { { { -3926, 0, -1165 }, { -458, 3765, 1545 }, { 1070, 1612, -3609 } }, { -847, 2531, -7624 } }, 257 },
    { { { { -4095, 0, 24 }, { 12, 3554, 2035 }, { -21, 2035, -3554 } }, { -4773, 2743, -9126 } }, 230 },
    { { { { -3993, 0, -911 }, { -270, 3911, 1183 }, { 870, 1214, -3813 } }, { -1048, 2819, -0x2BC8 } }, 246 },
    { { { { -3738, 0, -1672 }, { -680, 3741, 1520 }, { 1528, 1665, -3415 } }, { -2135, 3083, -0x2DB6 } }, 263 },
    { { { { -3738, 0, -1672 }, { -680, 3741, 1520 }, { 1528, 1665, -3415 } }, { -2135, 3083, -0x2DB6 } }, 263 },
};

enum { DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION = 128 };

/// Empty sprite list for mapped view 1; keeps the decoded background image.
static SpriteBatch _gDryfieldFactoryView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 2, in batch-index order.
static SpriteSource _gDryfieldFactoryView2SpriteSources[13] = {
    { 142, 0x3FC0, { .fields = { 48, 80 } }, 104, 40, 250, { .fields = { 104, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 88, 40, 700, { .fields = { 8, 80 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 80, 48 } }, -48, -32, 0x3847, { .fields = { 80, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, 16, 1058, { .fields = { 72, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -32, 16, 1058, { .fields = { 112, 80 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, 32, 0x3318, { .fields = { 0, 160 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -8, 16, 0x2749, { .fields = { 32, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -24, 8, 1150, { .fields = { 104, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -40, 8, 1140, { .fields = { 88, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, -160, -120, 1037, { .fields = { 56, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, -136, -120, 1125, { .fields = { 80, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, -112, -120, 1200, { .fields = { 104, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 200 } }, -88, -120, 0x37CF, { .fields = { 24, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Sprite ranges for mapped view 2, after an image-preserving header and before the end marker.
static SpriteBatch _gDryfieldFactoryView2SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 5, 0, 0, { 0, 0 } },
    { 7, 2, 0, 0, { 2, 0 } },
    { 9, ARRAY_SIZE(_gDryfieldFactoryView2SpriteSources) - 9, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 3, in batch-index order.
static SpriteSource _gDryfieldFactoryView3SpriteSources[10] = {
    { 143, 0x3FC0, { .fields = { 72, 240 } }, -160, -120, 0, { .fields = { 56, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 88, 88 } }, 72, 32, 0, { .fields = { 72, 128 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, 64, -120, 2000, { .fields = { 32, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 72, 72 } }, 88, -120, 2000, { .fields = { 48, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 40, 128 } }, 120, -48, 974, { .fields = { 120, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 88, -48, 1153, { .fields = { 88, 72 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 88, -24, 1161, { .fields = { 120, 216 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 88, 0, 1162, { .fields = { 88, 96 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 88, 24, 1162, { .fields = { 88, 216 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 88, 48, 1150, { .fields = { 24, 216 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Sprite ranges for mapped view 3, after an image-preserving header and before the end marker.
static SpriteBatch _gDryfieldFactoryView3SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 1, 0, 0, { 2, 0 } },
    { 2, ARRAY_SIZE(_gDryfieldFactoryView3SpriteSources) - 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 4; keeps the decoded background image.
static SpriteBatch _gDryfieldFactoryView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 5; keeps the decoded background image.
static SpriteBatch _gDryfieldFactoryView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 6, in batch-index order.
static SpriteSource _gDryfieldFactoryView6SpriteSources[4] = {
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -64, 96, 425, { .fields = { 72, 112 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 88, 56 } }, -160, 64, 425, { .fields = { 40, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 96, 0, { .fields = { 120, 88 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, -72, 64, 0, { .fields = { 64, 56 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Sprite ranges for mapped view 6, after an image-preserving header and before the end marker.
static SpriteBatch _gDryfieldFactoryView6SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldFactoryView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 7, in batch-index order.
static SpriteSource _gDryfieldFactoryView7SpriteSources[2] = {
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 96, 500, { .fields = { 96, 72 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x4000, { .fields = { 40, 72 } }, -160, 40, 1200, { .fields = { 88, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Sprite ranges for mapped view 7, after an image-preserving header and before the end marker.
static SpriteBatch _gDryfieldFactoryView7SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, ARRAY_SIZE(_gDryfieldFactoryView7SpriteSources) - 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 8, in batch-index order.
static SpriteSource _gDryfieldFactoryView8SpriteSources[12] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -128, -120, 125, { .fields = { 96, 56 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, -72, 125, { .fields = { 104, 248 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -120, -64, 125, { .fields = { 96, 104 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, -24, 125, { .fields = { 96, 32 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, -8, 125, { .fields = { 104, 48 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, 0, 125, { .fields = { 104, 224 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, 24, 125, { .fields = { 112, 144 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -96, 56, 125, { .fields = { 120, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 88, 125, { .fields = { 120, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -144, 64, 125, { .fields = { 88, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -160, 80, 125, { .fields = { 64, 16 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -136, 96, 125, { .fields = { 96, 176 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Sprite ranges for mapped view 8, after an image-preserving header and before the end marker.
static SpriteBatch _gDryfieldFactoryView8SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldFactoryView8SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 9, in batch-index order.
static SpriteSource _gDryfieldFactoryView9SpriteSources[5] = {
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -80, -48, 1312, { .fields = { 80, 128 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, 16, 1062, { .fields = { 112, 192 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 56, 16, 1062, { .fields = { 112, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 40, 16, 1062, { .fields = { 112, 64 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 32, 16, 1062, { .fields = { 120, 128 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Sprite ranges for mapped view 9, after an image-preserving header and before the end marker.
static SpriteBatch _gDryfieldFactoryView9SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, ARRAY_SIZE(_gDryfieldFactoryView9SpriteSources) - 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 10; keeps the decoded background image.
static SpriteBatch _gDryfieldFactoryView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangle and sorting depth for the power-scene sprite in mapped view 11.
static SpriteSource _gDryfieldFactoryView11SpriteSources[1] = {
    { 143, 0x3FC0, { .fields = { 80, 136 } }, -16, -72, 279, { .fields = { 48, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Image-preserving list for mapped view 11; the power scene toggles batch 1's hidden flag.
static SpriteBatch _gDryfieldFactoryView11SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldFactoryView11SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 12; keeps the decoded background image.
static SpriteBatch _gDryfieldFactoryView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 13, in batch-index order.
static SpriteSource _gDryfieldFactoryView13SpriteSources[13] = {
    { 143, 0x3FC0, { .fields = { 48, 80 } }, 104, 40, 300, { .fields = { 8, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 88, 40, 700, { .fields = { 40, 80 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 80, 48 } }, -48, -32, 0x3847, { .fields = { 88, 80 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, 16, 1058, { .fields = { 72, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 16, 1021, { .fields = { 16, 128 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, 32, 0x3318, { .fields = { 32, 160 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -8, 16, 0x2749, { .fields = { 32, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -24, 8, 1150, { .fields = { 104, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -40, 8, 1140, { .fields = { 88, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, -160, -120, 1037, { .fields = { 64, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, -136, -120, 1125, { .fields = { 88, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 200 } }, -112, -120, 1162, { .fields = { 112, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, -96, -120, 1212, { .fields = { 56, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Sprite ranges for mapped view 13, after an image-preserving header and before the end marker.
static SpriteBatch _gDryfieldFactoryView13SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 5, 0, 0, { 0, 0 } },
    { 7, 2, 0, 0, { 2, 0 } },
    { 9, ARRAY_SIZE(_gDryfieldFactoryView13SpriteSources) - 9, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 14, in batch-index order.
static SpriteSource _gDryfieldFactoryView14SpriteSources[12] = {
    { 143, 0x3FC0, { .fields = { 72, 240 } }, -160, -120, 0, { .fields = { 56, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 88, 88 } }, 72, 32, 0, { .fields = { 24, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 40, 200 } }, 120, -120, 2000, { .fields = { 112, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 200 } }, 64, -120, 2000, { .fields = { 24, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 24, 80 } }, 96, -120, 2000, { .fields = { 88, 88 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, 56, 1150, { .fields = { 32, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 40, 1157, { .fields = { 32, 240 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 24, 1155, { .fields = { 8, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 8, 1153, { .fields = { 80, 240 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, -8, 1147, { .fields = { 104, 240 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, -24, 1151, { .fields = { 56, 240 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, -40, 1150, { .fields = { 32, 224 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Sprite ranges for mapped view 14, after an image-preserving header and before the end marker.
static SpriteBatch _gDryfieldFactoryView14SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 1, 0, 0, { 2, 0 } },
    { 2, ARRAY_SIZE(_gDryfieldFactoryView14SpriteSources) - 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 15, in batch-index order.
static SpriteSource _gDryfieldFactoryView15SpriteSources[82] = {
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -152, 8, 1041, { .fields = { 40, 24 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -152, 32, 1103, { .fields = { 40, 80 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -152, 56, 0, { .fields = { 32, 160 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -16, 8, 0, { .fields = { 8, 160 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 32, 978, { .fields = { 8, 184 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -16, 48, 965, { .fields = { 40, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -128, 8, 1130, { .fields = { 88, 216 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -88, 8, 1106, { .fields = { 72, 104 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -56, 8, 0, { .fields = { 88, 144 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -48, 24, 1031, { .fields = { 120, 96 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -48, 40, 937, { .fields = { 96, 192 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 136, 8 } }, -152, 64, 0, { .fields = { 32, 248 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -128, 24, 1075, { .fields = { 120, 64 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -128, 40, 1086, { .fields = { 64, 80 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -88, 24, 1063, { .fields = { 120, 208 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -88, 40, 1004, { .fields = { 72, 120 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, -72, 1487, { .fields = { 112, 120 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, -48, 1475, { .fields = { 104, 144 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -24, 1488, { .fields = { 40, 96 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -128, -24, 1455, { .fields = { 32, 168 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, -48, 1450, { .fields = { 96, 56 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, -72, 1475, { .fields = { 104, 168 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -96, -72, 0, { .fields = { 16, 128 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -96, -40, 1455, { .fields = { 80, 96 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, -24, 0, { .fields = { 48, 32 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -24, 1500, { .fields = { 64, 64 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -24, 1450, { .fields = { 56, 32 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -160, -80, 1025, { .fields = { 64, 80 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -104, -80, 954, { .fields = { 80, 24 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -48, -80, 908, { .fields = { 88, 168 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, 16, -80, 839, { .fields = { 72, 128 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, 80, -80, 795, { .fields = { 104, 192 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 80, -64, 820, { .fields = { 0, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 128, -32, 0, { .fields = { 120, 136 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 120, 0, 0, { .fields = { 104, 136 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 80, -32, 883, { .fields = { 112, 48 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 72, 0, 979, { .fields = { 120, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 72, -16, 873, { .fields = { 32, 216 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, -64, 0, { .fields = { 104, 208 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 72, 16, 986, { .fields = { 120, 80 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 32, 0, { .fields = { 72, 168 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -64, 0, { .fields = { 112, 136 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 64, 32, 985, { .fields = { 0, 152 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, 48, 986, { .fields = { 88, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 48, 136 } }, 16, -64, 0, { .fields = { 80, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, -64, 1014, { .fields = { 32, 192 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -104, -64, 999, { .fields = { 48, 144 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -152, -64, 1053, { .fields = { 24, 104 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -64, 0, { .fields = { 72, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -152, -40, 1092, { .fields = { 56, 168 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -104, -40, 1040, { .fields = { 8, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, -48, -40, 996, { .fields = { 0, 64 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, -8, 1049, { .fields = { 32, 56 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, -104, -8, 1107, { .fields = { 112, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -144, -16, 1150, { .fields = { 8, 128 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -160, -16, 0, { .fields = { 72, 24 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, 8, 0, { .fields = { 64, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -136, 8, 1040, { .fields = { 64, 24 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, 8, 1077, { .fields = { 24, 32 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -160, 32, 0, { .fields = { 56, 48 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -160, 40, 0, { .fields = { 72, 224 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -104, 16, 1044, { .fields = { 104, 96 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, 16, 1064, { .fields = { 88, 32 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -40, 40, 1039, { .fields = { 96, 112 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 40, 1078, { .fields = { 104, 24 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 120, 24 } }, -160, 48, 0, { .fields = { 40, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -40, 56, 0, { .fields = { 64, 64 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -40, 844, { .fields = { 32, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -16, 0, { .fields = { 112, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 8, 908, { .fields = { 40, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 24, 1284, { .fields = { 56, 248 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 0, 1350, { .fields = { 32, 96 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -40, 0, { .fields = { 72, 48 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -40, 1347, { .fields = { 64, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, -8, 1255, { .fields = { 72, 88 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 72, 8, 1250, { .fields = { 80, 240 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 72, -16, 1237, { .fields = { 24, 168 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 72, -40, 1230, { .fields = { 8, 24 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -16, 16, 1083, { .fields = { 80, 192 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -24, 987, { .fields = { 56, 208 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -64, 884, { .fields = { 56, 128 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -16, -120, 792, { .fields = { 80, 136 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Sprite ranges for mapped view 15, after an image-preserving header and before the end marker.
static SpriteBatch _gDryfieldFactoryView15SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { 16, 11, 0, 0, { 3, 0 } },
    { 27, 40, 0, 0, { 2, 0 } },
    { 67, 11, 0, 0, { 4, 0 } },
    { 78, ARRAY_SIZE(_gDryfieldFactoryView15SpriteSources) - 78, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 16, in batch-index order.
static SpriteSource _gDryfieldFactoryView16SpriteSources[32] = {
    { 142, 0x3FC0, { .fields = { 96, 32 } }, -160, 88, 425, { .fields = { 80, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -64, 96, 425, { .fields = { 40, 56 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 128, 40 } }, 8, -24, 1416, { .fields = { 0, 168 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 112, -88, 1750, { .fields = { 88, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -48, -56, 1550, { .fields = { 96, 128 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -48, -88, 1432, { .fields = { 24, 208 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -16, -48, 1684, { .fields = { 16, 32 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -48, 0, { .fields = { 40, 32 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, -48, 1750, { .fields = { 40, 128 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -16, -64, 1704, { .fields = { 80, 240 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -120, 1143, { .fields = { 48, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -96, 1197, { .fields = { 64, 112 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -72, 1244, { .fields = { 80, 176 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -48, 1327, { .fields = { 64, 88 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -24, 1301, { .fields = { 32, 176 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -24, 1386, { .fields = { 24, 136 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -48, 1294, { .fields = { 48, 224 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -72, 1229, { .fields = { 40, 32 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -96, 1178, { .fields = { 32, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -120, 1123, { .fields = { 112, 88 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -120, 1123, { .fields = { 104, 64 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -96, 1243, { .fields = { 112, 128 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -72, 1192, { .fields = { 96, 208 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -48, 1302, { .fields = { 72, 152 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -24, 1368, { .fields = { 88, 32 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, -56, 1351, { .fields = { 48, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, -88, 1216, { .fields = { 48, 32 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, -120, 1083, { .fields = { 56, 128 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, -24, 1383, { .fields = { 112, 208 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x4000, { .fields = { 56, 32 } }, -64, 88, 625, { .fields = { 56, 208 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x4000, { .fields = { 96, 56 } }, -160, 64, 500, { .fields = { 32, 72 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 142, 0x4000, { .fields = { 56, 24 } }, -64, 64, 0, { .fields = { 96, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Sprite ranges for mapped view 16, after an image-preserving header and before the end marker.
static SpriteBatch _gDryfieldFactoryView16SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 4, 0 } },
    { 3, 7, 0, 0, { 1, 0 } },
    { 10, 15, 0, 0, { 3, 0 } },
    { 25, 4, 0, 0, { 2, 0 } },
    { 29, ARRAY_SIZE(_gDryfieldFactoryView16SpriteSources) - 29, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 17, in batch-index order.
static SpriteSource _gDryfieldFactoryView17SpriteSources[40] = {
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 96, 500, { .fields = { 0, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 56, -32, 2250, { .fields = { 56, 144 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, -48, 2377, { .fields = { 0, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, -32, 2425, { .fields = { 0, 144 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -80, -16, 2375, { .fields = { 32, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -56, -16, 2375, { .fields = { 56, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -16, 2475, { .fields = { 16, 24 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -48, -32, 2450, { .fields = { 0, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, -8, 1935, { .fields = { 24, 96 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -40, -8, 1935, { .fields = { 32, 200 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -16, -8, 1935, { .fields = { 32, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 8, -8, 1935, { .fields = { 32, 136 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -64, -88, 1833, { .fields = { 96, 128 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -64, -24, 1982, { .fields = { 72, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 8, -24, 1848, { .fields = { 80, 192 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 8, -88, 1740, { .fields = { 96, 64 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, -88, 1710, { .fields = { 80, 144 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -32, -88, 1791, { .fields = { 104, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -8, -88, 1791, { .fields = { 112, 192 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -32, -24, 1911, { .fields = { 72, 96 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -24, 1911, { .fields = { 64, 48 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 56, -88, 1675, { .fields = { 16, 168 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, -56, 1675, { .fields = { 56, 192 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -56, 1675, { .fields = { 56, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -16, 1779, { .fields = { 48, 40 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, -16, 1779, { .fields = { 48, 96 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, -8, 1792, { .fields = { 32, 64 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, -40, 1719, { .fields = { 32, 32 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -88, 1638, { .fields = { 80, 48 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -128, 80, 1200, { .fields = { 64, 40 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 40, 1200, { .fields = { 96, 240 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 56, 1200, { .fields = { 16, 48 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 72, 1200, { .fields = { 16, 64 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 88, 1200, { .fields = { 16, 80 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -160, 104, 1200, { .fields = { 56, 136 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 48, 1200, { .fields = { 16, 128 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 64, 1200, { .fields = { 48, 80 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 80, 1200, { .fields = { 80, 240 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 96, 1200, { .fields = { 16, 216 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -128, 64, 1200, { .fields = { 96, 48 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Sprite ranges for mapped view 17, after an image-preserving header and before the end marker.
static SpriteBatch _gDryfieldFactoryView17SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 7, 0, 0, { 4, 0 } },
    { 8, 4, 0, 0, { 3, 0 } },
    { 12, 14, 0, 0, { 5, 0 } },
    { 26, 3, 0, 0, { 2, 0 } },
    { 29, ARRAY_SIZE(_gDryfieldFactoryView17SpriteSources) - 29, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 18, in batch-index order.
static SpriteSource _gDryfieldFactoryView18SpriteSources[17] = {
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 40, 1300, { .fields = { 8, 224 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -56, 48, 1750, { .fields = { 56, 216 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -56, 64, 1750, { .fields = { 80, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 8, 1325, { .fields = { 40, 216 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 8, 1325, { .fields = { 48, 248 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 16, 1312, { .fields = { 40, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 16, 1320, { .fields = { 32, 8 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 24, 1322, { .fields = { 40, 240 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 24, 1315, { .fields = { 32, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 32, 1300, { .fields = { 96, 248 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 32, 1362, { .fields = { 80, 248 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 40, 1375, { .fields = { 112, 248 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 40, 1312, { .fields = { 64, 248 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, 40, 1275, { .fields = { 104, 216 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 32, 1282, { .fields = { 56, 240 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 24, 1287, { .fields = { 56, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 80, 216 } }, -112, -120, 500, { .fields = { 48, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Sprite ranges for mapped view 18, after an image-preserving header and before the end marker.
static SpriteBatch _gDryfieldFactoryView18SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 10, 0, 0, { 0, 0 } },
    { 13, 3, 0, 0, { 2, 0 } },
    { 16, ARRAY_SIZE(_gDryfieldFactoryView18SpriteSources) - 16, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 19, in batch-index order.
static SpriteSource _gDryfieldFactoryView19SpriteSources[17] = {
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 40, 1300, { .fields = { 8, 224 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -56, 48, 1750, { .fields = { 56, 216 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -56, 64, 1750, { .fields = { 80, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 8, 1325, { .fields = { 40, 216 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 8, 1325, { .fields = { 48, 248 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 16, 1312, { .fields = { 40, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 16, 1320, { .fields = { 32, 8 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 24, 1322, { .fields = { 40, 240 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 24, 1315, { .fields = { 32, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 32, 1300, { .fields = { 96, 248 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 32, 1362, { .fields = { 80, 248 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 40, 1375, { .fields = { 112, 248 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 40, 1312, { .fields = { 64, 248 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, 40, 1275, { .fields = { 104, 216 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 32, 1282, { .fields = { 56, 240 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 24, 1287, { .fields = { 56, 232 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
    { 143, 0x3FC0, { .fields = { 80, 216 } }, -112, -120, 500, { .fields = { 48, 0 } }, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_FACTORY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Sprite ranges for mapped view 19, after an image-preserving header and before the end marker.
static SpriteBatch _gDryfieldFactoryView19SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 10, 0, 0, { 0, 0 } },
    { 13, 3, 0, 0, { 2, 0 } },
    { 16, ARRAY_SIZE(_gDryfieldFactoryView19SpriteSources) - 16, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldFactorySpriteViews[DRYFIELD_FACTORY_MAPPED_VIEW_COUNT] = {
    { { .empty = _gDryfieldFactoryView1SpriteBatches }, _gDryfieldFactoryView1SpriteBatches, NULL },
    { { .elements = _gDryfieldFactoryView2SpriteSources }, _gDryfieldFactoryView2SpriteBatches, NULL },
    { { .elements = _gDryfieldFactoryView3SpriteSources }, _gDryfieldFactoryView3SpriteBatches, NULL },
    { { .empty = _gDryfieldFactoryView4SpriteBatches }, _gDryfieldFactoryView4SpriteBatches, NULL },
    { { .empty = _gDryfieldFactoryView5SpriteBatches }, _gDryfieldFactoryView5SpriteBatches, NULL },
    { { .elements = _gDryfieldFactoryView6SpriteSources }, _gDryfieldFactoryView6SpriteBatches, NULL },
    { { .elements = _gDryfieldFactoryView7SpriteSources }, _gDryfieldFactoryView7SpriteBatches, NULL },
    { { .elements = _gDryfieldFactoryView8SpriteSources }, _gDryfieldFactoryView8SpriteBatches, NULL },
    { { .elements = _gDryfieldFactoryView9SpriteSources }, _gDryfieldFactoryView9SpriteBatches, NULL },
    { { .empty = _gDryfieldFactoryView10SpriteBatches }, _gDryfieldFactoryView10SpriteBatches, NULL },
    { { .elements = _gDryfieldFactoryView11SpriteSources }, _gDryfieldFactoryView11SpriteBatches, NULL },
    { { .empty = _gDryfieldFactoryView12SpriteBatches }, _gDryfieldFactoryView12SpriteBatches, NULL },
    { { .elements = _gDryfieldFactoryView13SpriteSources }, _gDryfieldFactoryView13SpriteBatches, NULL },
    { { .elements = _gDryfieldFactoryView14SpriteSources }, _gDryfieldFactoryView14SpriteBatches, NULL },
    { { .elements = _gDryfieldFactoryView15SpriteSources }, _gDryfieldFactoryView15SpriteBatches, NULL },
    { { .elements = _gDryfieldFactoryView16SpriteSources }, _gDryfieldFactoryView16SpriteBatches, NULL },
    { { .elements = _gDryfieldFactoryView17SpriteSources }, _gDryfieldFactoryView17SpriteBatches, NULL },
    { { .elements = _gDryfieldFactoryView18SpriteSources }, _gDryfieldFactoryView18SpriteBatches, NULL },
    { { .elements = _gDryfieldFactoryView19SpriteSources }, _gDryfieldFactoryView19SpriteBatches, NULL },
};

WorldCollisionTrigger gDryfieldFactoryViewBoundaryTriggers[14] = {
    { NULL, NULL, NULL, { 3021, -2368, 1935, 0 }, { { -32, -2976, -2293, 0 }, { -32, 2976, -2293, 0 }, { 31, -2976, 2292, 0 }, { 31, 2976, 2292, 0 } }, { -4100, 0, 55, 0 }, { 0, 0, 4096, 0 }, 3753, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2704, -2368, 1920, 0 }, { { 26, -2976, 2404, 0 }, { 26, 2976, 2404, 0 }, { -27, -2976, -2405, 0 }, { -27, 2976, -2405, 0 } }, { 4097, 0, -46, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1312, -2304, 5840, 0 }, { { 1616, -2976, 160, 0 }, { 1616, 2976, 160, 0 }, { -1616, -2976, -160, 0 }, { -1616, 2976, -160, 0 } }, { 405, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 3386, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1280, -2368, 5712, 0 }, { { -1632, -2976, -144, 0 }, { -1632, 2976, -144, 0 }, { 1632, -2976, 144, 0 }, { 1632, 2976, 144, 0 } }, { -362, 0, 4091, 0 }, { 0, 0, 4096, 0 }, 3396, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1712, -2336, 8096, 0 }, { { -1520, -2976, 1280, 0 }, { -1520, 2976, 1280, 0 }, { 1520, -2976, -1280, 0 }, { 1520, 2976, -1280, 0 } }, { 2640, 0, 3134, 0 }, { 0, 0, 4096, 0 }, 3574, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1760, -2336, 8289, 0 }, { { 1520, -2976, -1280, 0 }, { 1520, 2976, -1280, 0 }, { -1520, -2976, 1280, 0 }, { -1520, 2976, 1280, 0 } }, { -2641, 0, -3136, 0 }, { 0, 0, 4096, 0 }, 3574, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3232, -2336, 5040, 0 }, { { 64, -2976, -752, 0 }, { 64, 2976, -752, 0 }, { -64, -2976, 752, 0 }, { -64, 2976, 752, 0 } }, { -4094, 0, -349, 0 }, { 0, 0, 4096, 0 }, 3061, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3088, -2368, 5087, 0 }, { { -64, -2976, 752, 0 }, { -64, 2976, 752, 0 }, { 64, -2976, -752, 0 }, { 64, 2976, -752, 0 } }, { 4091, 0, 348, 0 }, { 0, 0, 4096, 0 }, 3061, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3360, -2336, 9648, 0 }, { { 112, -2976, -2032, 0 }, { 112, 2976, -2032, 0 }, { -112, -2976, 2032, 0 }, { -112, 2976, 2032, 0 } }, { -4097, 0, -227, 0 }, { 0, 0, 4096, 0 }, 3602, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3184, -2272, 9648, 0 }, { { -128, -2976, 2112, 0 }, { -128, 2976, 2112, 0 }, { 128, -2976, -2112, 0 }, { 128, 2976, -2112, 0 } }, { 4091, 0, 247, 0 }, { 0, 0, 4096, 0 }, 3647, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1104, -2144, 4080, 0 }, { { -1152, -2976, 16, 0 }, { -1152, 2976, 16, 0 }, { 1152, -2976, -16, 0 }, { 1152, 2976, -16, 0 } }, { 56, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 3187, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1040, -2144, 4192, 0 }, { { 1360, -2976, -48, 0 }, { 1360, 2976, -48, 0 }, { -1360, -2976, 48, 0 }, { -1360, 2976, 48, 0 } }, { -146, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 3268, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4992, -2080, 4275, 0 }, { { 1360, -2976, 0, 0 }, { 1360, 2976, 0, 0 }, { -1360, -2976, 0, 0 }, { -1360, 2976, 0, 0 } }, { 0, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 3268, 0, 2, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5008, -2112, 4160, 0 }, { { -1600, -2976, 0, 0 }, { -1600, 2976, 0, 0 }, { 1600, -2976, 0, 0 }, { 1600, 2976, 0, 0 } }, { 0, 0, 4107, 0 }, { 0, 0, 4096, 0 }, 3376, 0, 6, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room message commands and the automatic action selected by these triggers.
enum {
    DRYFIELD_FACTORY_TRIGGER_CAP_SCENE          = 1,
    DRYFIELD_FACTORY_TRIGGER_BARRIER_TRANSITION = 2,
    DRYFIELD_FACTORY_TRIGGER_LAMP_SCENE         = 3,
    DRYFIELD_FACTORY_TRIGGER_POWER_SCENE        = 5,
    DRYFIELD_FACTORY_TRIGGER_PANEL_SESSION      = 6,
    DRYFIELD_FACTORY_TRIGGER_HATCH_SCENE        = 12,
    DRYFIELD_FACTORY_TRIGGER_AUTOMATIC_SCENE    = 1,
};

/// Packed 1-based departure/arrival slots for the three factory exits.
enum {
    DRYFIELD_FACTORY_DRIVEWAY_WARP_SLOTS  = (1 << 4) | 1,
    DRYFIELD_FACTORY_BREEZEWAY_WARP_SLOTS = (2 << 4) | 2,
    DRYFIELD_FACTORY_GARAGE_WARP_SLOTS    = (3 << 4) | 1,
};

WorldCollisionTrigger gDryfieldFactoryActionTriggers[20] = {
    { NULL, NULL, NULL, { 5344, -48, 1232, 0 }, { { -416, 0, -720, 0 }, { 416, 0, -720, 0 }, { -416, 0, 720, 0 }, { 416, 0, 720, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 829, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_DRIVEWAY, DRYFIELD_FACTORY_DRIVEWAY_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5744, -64, 0x2B50, 0 }, { { -672, 0, -928, 0 }, { 672, 0, -928, 0 }, { -672, 0, 928, 0 }, { 672, 0, 928, 0 } }, { 0, 4108, 0, 0 }, { -3857, 0, -1380, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_FACTORY_TRIGGER_POWER_SCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 464, -64, 512, 0 }, { { -688, 0, -672, 0 }, { 688, 0, -672, 0 }, { -688, 0, 672, 0 }, { 688, 0, 672, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 960, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_FACTORY_TRIGGER_PANEL_SESSION, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5856, -64, 5488, 0 }, { { -496, 0, -464, 0 }, { 496, 0, -464, 0 }, { -496, 0, 464, 0 }, { 496, 0, 464, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 677, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_FACTORY_TRIGGER_LAMP_SCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5904, -64, 3968, 0 }, { { -1264, 0, -416, 0 }, { 1264, 0, -416, 0 }, { -1264, 0, 416, 0 }, { 1264, 0, 416, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 1330, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_FACTORY_TRIGGER_CAP_SCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5920, 0, 4816, 0 }, { { -1264, 0, -448, 0 }, { 1264, 0, -448, 0 }, { -1264, 0, 448, 0 }, { 1264, 0, 448, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, 4096, 0 }, 1336, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_FACTORY_TRIGGER_BARRIER_TRANSITION, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 464, -48, 7424, 0 }, { { -448, 0, -848, 0 }, { 448, 0, -848, 0 }, { -448, 0, 848, 0 }, { 448, 0, 848, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 957, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_BREEZEWAY, DRYFIELD_FACTORY_BREEZEWAY_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5456, -48, 7056, 0 }, { { -512, 0, -928, 0 }, { 512, 0, -928, 0 }, { -512, 0, 928, 0 }, { 512, 0, 928, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_GARAGE, DRYFIELD_FACTORY_GARAGE_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1568, -64, 7104, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_FACTORY_TRIGGER_HATCH_SCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3984, -64, 0x2850, 0 }, { { -320, 0, -1360, 0 }, { 320, 0, -1360, 0 }, { -320, 0, 1360, 0 }, { 320, 0, 1360, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1396, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_FACTORY_TRIGGER_AUTOMATIC_SCENE, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 3728, -64, 2528, 0 }, { { -448, 0, -656, 0 }, { 448, 0, -656, 0 }, { -448, 0, 656, 0 }, { 448, 0, 656, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4928, -64, 992, 0 }, { { -784, 0, -320, 0 }, { 784, 0, -320, 0 }, { -784, 0, 320, 0 }, { 784, 0, 320, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 846, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3272, -64, 128, 0 }, { { -1592, 0, -32, 0 }, { 1064, 0, -32, 0 }, { -792, 0, 1280, 0 }, { 1064, 0, 1280, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1664, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 144, -64, 2344, 0 }, { { -288, 0, -1352, 0 }, { 1536, 0, -1000, 0 }, { -288, 0, 792, 0 }, { 1536, 0, 792, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1832, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2039, -64, 0x27B7, 0 }, { { -1018, 0, -36, 0 }, { -446, 0, -871, 0 }, { 518, 0, 886, 0 }, { 948, 0, 23, 0 } }, { 0, 4105, 0, 0 }, { 2276, 0, -3406, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3584, -64, 7008, 0 }, { { -2224, 0, -2496, 0 }, { 1712, 0, -2496, 0 }, { -2224, 0, 2496, 0 }, { 1712, 0, 2496, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, 4096, 0 }, 3337, WORLD_COLLISION_TRIGGER_ACTION_CAP, 24, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1248, -64, 9024, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { 4091, 0, 201, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_CAP, 22, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4832, -64, 1712, 0 }, { { -960, 0, -352, 0 }, { 960, 0, -352, 0 }, { -960, 0, 352, 0 }, { 960, 0, 352, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1021, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4831, -64, 3407, 0 }, { { -1128, 0, -580, 0 }, { 1267, 0, -109, 0 }, { -1266, 0, 110, 0 }, { 1129, 0, 581, 0 } }, { 0, 4099, 0, 0 }, { -1567, 0, 3784, 0 }, 1267, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5728, -64, 9824, 0 }, { { -672, 0, -384, 0 }, { 672, 0, -384, 0 }, { -672, 0, 384, 0 }, { 672, 0, 384, 0 } }, { 0, 4095, 0, 0 }, { -3857, 0, 1380, 0 }, 773, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Five authored point lights shared by both daytime room variants.
///
/// Positions and falloff radii use room coordinates; intensity has 12 fractional
/// bits. All views admit every light. Composition and attenuation stay mutable.
static WorldCoordPointLight _gDryfieldFactoryPointLights[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2620, -1827, 2739 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3935, 3894, 3850 }, { 0, 0 } }, 936, 2214 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1447, -1740, 9089 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3903, 3866, 3901 }, { 0, 0 } }, 979, 1281 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6134, -1422, 5354 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1764, 1727, 1701 }, { 0, 0 } }, 0, 6724 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3603, -1838, 8945 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3464, 3400, 3377 }, { 0, 0 } }, 1021, 1477 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3061, -1123, 6690 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2387, 2318, 2252 }, { 0, 0 } }, 1439, 4621 },
};

WorldCoordRoomLights gDryfieldFactoryRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldFactoryPointLights),
    .pointLights           = _gDryfieldFactoryPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCoordRoomAmbientEntry gDryfieldFactoryViewAmbientMinima[20] = {
    { .viewCount = ARRAY_SIZE(gDryfieldFactoryViewAmbientMinima) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 919, 888, 865, 896 } },
    { .color = { 1062, 1050, 1035, 1052 } },
    { .color = { 601, 598, 595, 598 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 521, 515, 510, 516 } },
    { .color = { 739, 735, 728, 735 } },
    { .color = { 777, 773, 765, 773 } },
    { .color = { 1158, 1079, 1026, 1102 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 739, 735, 728, 735 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Type-1 bank requests for cue 2; cue 1 uses the following entry.
enum {
    DRYFIELD_FACTORY_CLASS2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x51,
    DRYFIELD_FACTORY_CLASS2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x53,
    DRYFIELD_FACTORY_CLASS3_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    DRYFIELD_FACTORY_CLASS3_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
    DRYFIELD_FACTORY_CLASS3_JUMP_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x19,
};

/// Footstep cue bases for surface class 2; scripted jumping retains the walk pair.
static WorldCollisionFootstepSounds _gDryfieldFactoryClass2FootstepSounds = {
    .walk         = DRYFIELD_FACTORY_CLASS2_WALK_SOUND,
    .run          = DRYFIELD_FACTORY_CLASS2_RUN_SOUND,
    .scriptedJump = DRYFIELD_FACTORY_CLASS2_WALK_SOUND,
};

/// Footstep cue bases for surface class 3, with distinct walk, run and jump requests.
static WorldCollisionFootstepSounds _gDryfieldFactoryClass3FootstepSounds = {
    .walk         = DRYFIELD_FACTORY_CLASS3_WALK_SOUND,
    .run          = DRYFIELD_FACTORY_CLASS3_RUN_SOUND,
    .scriptedJump = DRYFIELD_FACTORY_CLASS3_JUMP_SOUND,
};

/// Blocks probes and permits weapon impacts and pushback; has no footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldFactorySolidSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Passes probes and suppresses weapon impacts while retaining physical pushback.
static WorldCollisionSurfaceProperties _gDryfieldFactoryProbePassSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Solid surface class 2, with its walk/run footstep cue bases.
static WorldCollisionSurfaceProperties _gDryfieldFactoryClass2Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldFactoryClass2FootstepSounds,
};

/// Solid surface class 3, with its walk/run/jump footstep cue bases.
static WorldCollisionSurfaceProperties _gDryfieldFactoryClass3Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldFactoryClass3FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldFactorySurfaceClasses[8] = {
    &_gDryfieldFactorySolidSurface,
    &_gDryfieldFactoryProbePassSurface,
    &_gDryfieldFactoryClass2Surface,
    &_gDryfieldFactoryClass3Surface,
    &_gDryfieldFactorySolidSurface,
    &_gDryfieldFactorySolidSurface,
    &_gDryfieldFactorySolidSurface,
    &_gDryfieldFactorySolidSurface,
};

enum {
    DRYFIELD_FACTORY_JOLT_VARIABLE_PEAK_SEGMENT  = 0,
    DRYFIELD_FACTORY_JOLT_VARIABLE_DECAY_SEGMENT = 1,
    DRYFIELD_FACTORY_JOLT_BINARY_SEGMENT         = 2,
};

PadScriptCmd gDryfieldFactoryLiftJoltCommands[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, DRYFIELD_FACTORY_JOLT_BINARY_SEGMENT), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, DRYFIELD_FACTORY_JOLT_VARIABLE_PEAK_SEGMENT) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, DRYFIELD_FACTORY_JOLT_VARIABLE_DECAY_SEGMENT) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
};

PadScriptVibrationSegment gDryfieldFactoryLiftJoltSegments[3] = {
    [DRYFIELD_FACTORY_JOLT_VARIABLE_PEAK_SEGMENT]  = { 255, 255, 8, 1 },
    [DRYFIELD_FACTORY_JOLT_VARIABLE_DECAY_SEGMENT] = { 150, 80, 20, 1 },
    [DRYFIELD_FACTORY_JOLT_BINARY_SEGMENT]         = { 0, 0, 5, 0 },
};
