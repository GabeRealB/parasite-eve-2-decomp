#include "rooms/shelter_b6_growth_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "shelter_b6_growth_room_private.h"

#include "gameplay/display.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/areas.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/sound_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "actors/actor_450900.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"

static void _shelterB6GrowthRoomDrawBottomGlow(s16 heightPixels, s16 brightness);
static void _shelterB6GrowthRoomDrawMist(const GfxCoord* coord, u16 frame, s16 size, u16 brightness);
static void _shelterB6GrowthRoomDrawDriftPuff(const GfxCoord* coord, u16 frame, s16 size, s16 angle);

// The two room particles use the same packed size, frame duration and drift speed.
enum {
    SHELTER_B6_GROWTH_ROOM_PARTICLE_SIZE_MASK          = 0xFFF,
    SHELTER_B6_GROWTH_ROOM_PARTICLE_PERIOD_FIELD_MASK  = 0xF000,
    SHELTER_B6_GROWTH_ROOM_PARTICLE_PERIOD_SHIFT       = 12,
    SHELTER_B6_GROWTH_ROOM_PARTICLE_PERIOD_MASK        = 7,
    SHELTER_B6_GROWTH_ROOM_PARTICLE_DEFAULT_PERIOD     = 1,
    SHELTER_B6_GROWTH_ROOM_PARTICLE_SPEED_FIELD_MASK   = 0xFF0000,
    SHELTER_B6_GROWTH_ROOM_PARTICLE_SPEED_SHIFT        = 16,
    SHELTER_B6_GROWTH_ROOM_PARTICLE_SPEED_MASK         = 0xFF,
    SHELTER_B6_GROWTH_ROOM_PARTICLE_DEFAULT_SPEED      = 0x40,
    SHELTER_B6_GROWTH_ROOM_PARTICLE_FRAME_COUNT        = 10,
    SHELTER_B6_GROWTH_ROOM_PARTICLE_TEXTURE_DEPTH_4BIT = 0,
    SHELTER_B6_GROWTH_ROOM_PARTICLE_STATE_INIT         = 0,
    SHELTER_B6_GROWTH_ROOM_PARTICLE_STATE_DRIFT        = 1
};

/// Reserved leading geometry of the movable collision box in the live room mesh.
enum {
    SHELTER_B6_GROWTH_ROOM_BOX_FACE_COUNT   = 4,
    SHELTER_B6_GROWTH_ROOM_BOX_VERTEX_COUNT = 8,
    SHELTER_B6_GROWTH_ROOM_BOX_Y_OFFSET     = 2000,
};

/// Four outward box-wall normals in room space, scaled by 4096.
static SVECTOR _gShelterB6GrowthRoomCollisionBoxTemplateNormals[SHELTER_B6_GROWTH_ROOM_BOX_FACE_COUNT] = {
#include "assets/shelter_b6_growth_room_collision_01C74_normals.inc"
};

/// Eight room-space corners of the box before the optional Y translation.
static SVECTOR _gShelterB6GrowthRoomCollisionBoxTemplateVerts[SHELTER_B6_GROWTH_ROOM_BOX_VERTEX_COUNT] = {
#include "assets/shelter_b6_growth_room_collision_01C74_verts.inc"
};

/// Four side-wall quads using vertices 0..7, normals 0..3 and surface class 0.
static WorldCollisionGridFace _gShelterB6GrowthRoomCollisionBoxTemplateFaces[SHELTER_B6_GROWTH_ROOM_BOX_FACE_COUNT] = {
#include "assets/shelter_b6_growth_room_collision_01C74_faces.inc"
};

/// Face list 0..3 followed by WORLD_COLLISION_GRID_CELL_END and an unread zero word.
static s16 _gShelterB6GrowthRoomCollisionBoxTemplateCells[6] = {
#include "assets/shelter_b6_growth_room_collision_01C74_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterB6GrowthRoomCollisionBoxTemplateCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..5
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_B6_GROWTH_ROOM_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB6GrowthRoomCollisionBoxTemplateCells[(halfwordOffset)])
/// The template's sole cell; retained although box resets do not read cell indices.
static s16* _gShelterB6GrowthRoomCollisionBoxTemplateTable[1] = {
#include "assets/shelter_b6_growth_room_collision_01C74_table.inc"
};
#undef SHELTER_B6_GROWTH_ROOM_CELL_FACE_LIST_AT_OFFSET

/// Authored box geometry used to reset the leading faces of the live room grid.
///
/// Resets copy four normals/faces and eight vertices, optionally adding 2000
/// to Y; this descriptor is never activated. Its retained 1-by-1 cell index
/// has a 4000-unit side and origin (3145,6145). All pools live with this room.
static WorldCollisionGrid _gShelterB6GrowthRoomCollisionBoxTemplate = {
    .viewCoord   = NULL,
    .normals     = _gShelterB6GrowthRoomCollisionBoxTemplateNormals,
    .vertices    = _gShelterB6GrowthRoomCollisionBoxTemplateVerts,
    .faces       = _gShelterB6GrowthRoomCollisionBoxTemplateFaces,
    .cellFaceIds = _gShelterB6GrowthRoomCollisionBoxTemplateTable,
    .xBias       = -3145,
    .zBias       = -6145,
    .cellCountX  = 1,
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB6GrowthRoomCollisionBoxTemplateFaces),
};

/// Index ranges within the room's authored glow and floor-mist anchor array.
enum {
    SHELTER_B6_GROWTH_ROOM_GLOW_ANCHOR_COUNT = 30,
    SHELTER_B6_GROWTH_ROOM_MIST_ANCHOR_START = SHELTER_B6_GROWTH_ROOM_GLOW_ANCHOR_COUNT,
    SHELTER_B6_GROWTH_ROOM_MIST_ANCHOR_COUNT = 6,
};

/// Thirty view-glow centres followed by six floor-mist centres, in room game units.
///
/// Indices 0..29 supply full XYZ centres selected by mapped camera view.
/// Indices 30..35 supply XZ mist centres; their stored Y=0 is not read.
/// Glows borrow a centre during drawing; mist copies it before spawning.
/// The room owns all points for the loaded overlay's lifetime.
static SVECTOR _gShelterB6GrowthRoomEffectAnchors[SHELTER_B6_GROWTH_ROOM_GLOW_ANCHOR_COUNT + SHELTER_B6_GROWTH_ROOM_MIST_ANCHOR_COUNT] = {
    { 665, -1955, 10550, 0 },
    { 1060, -1955, 10550, 0 },
    { 1160, -915, 10550, 0 },
    { 1765, -1955, 10550, 0 },
    { 2165, -1955, 10550, 0 },
    { 2565, -1955, 10550, 0 },
    { 1765, -1430, 10550, 0 },
    { 2665, -920, 10550, 0 },
    { -210, -880, 3925, 0 },
    { -240, -920, 4175, 0 },
    { -210, -885, 4795, 0 },
    { -210, -885, 5300, 0 },
    { -230, -915, 6475, 0 },
    { -210, -885, 7175, 0 },
    { -280, -1360, 585, 0 },
    { -280, -1385, 1240, 0 },
    { -280, -1385, 1500, 0 },
    { -280, -1160, 1240, 0 },
    { -280, -1160, 1500, 0 },
    { -280, -1350, 1890, 0 },
    { -280, -1325, 2185, 0 },
    { 4480, -1120, -3690, 0 },
    { 4690, -1120, -3720, 0 },
    { 4690, -1070, -3660, 0 },
    { 1250, -3450, 9000, 0 },
    { 3750, -3450, 9000, 0 },
    { 1250, -3450, 5500, 0 },
    { 3750, -3450, 5500, 0 },
    { 1250, -3450, 2000, 0 },
    { 3750, -3450, 2000, 0 },
    { 1250, 0, 5500, 0 },
    { 1250, 0, 9000, 0 },
    { 1250, 0, 2000, 0 },
    { 3750, 0, 5500, 0 },
    { 3750, 0, 9000, 0 },
    { 3750, 0, 2000, 0 },
};

u8* gShelterB6GrowthRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB6GrowthRoomViewCounts[1] = { ARRAY_SIZE(gShelterB6GrowthRoomViewCameras) };

/// Optional map-marker nibble keys published by these arrival endpoints.
enum {
    SHELTER_B6_GROWTH_ROOM_WARP_1_MAP_FLAG = 437,
};

DirectionWarpEntry gShelterB6GrowthRoomWarpEntries[1] = {
    {
        .player         = { { .word = 0 }, 4030, 0, -2950 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, 4030, 0, -2950 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 4,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = SHELTER_B6_GROWTH_ROOM_WARP_1_MAP_FLAG,
    },
};

/// Room-space Q12 normal pool, including the four reserved box normals.
static SVECTOR _gShelterB6GrowthRoomCollisionGridNormals[14] = {
#include "assets/shelter_b6_growth_room_collision_02530_normals.inc"
};

/// Room-space vertex pool, including the eight reserved box vertices.
static SVECTOR _gShelterB6GrowthRoomCollisionGridVerts[80] = {
#include "assets/shelter_b6_growth_room_collision_02530_verts.inc"
};

/// Indexed room faces; the leading four receive the movable box at entry.
static WorldCollisionGridFace _gShelterB6GrowthRoomCollisionGridFaces[54] = {
#include "assets/shelter_b6_growth_room_collision_02530_faces.inc"
};

/// Packed signed face-index lists, each ending at `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gShelterB6GrowthRoomCollisionGridCells[208] = {
#include "assets/shelter_b6_growth_room_collision_02530_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterB6GrowthRoomCollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..207
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_B6_GROWTH_ROOM_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB6GrowthRoomCollisionGridCells[(halfwordOffset)])
/// Eight X-major cell-list pointers, four Z cells per X column.
static s16* _gShelterB6GrowthRoomCollisionGridTable[8] = {
#include "assets/shelter_b6_growth_room_collision_02530_table.inc"
};
#undef SHELTER_B6_GROWTH_ROOM_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB6GrowthRoomCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB6GrowthRoomCollisionGridNormals,
    .vertices    = _gShelterB6GrowthRoomCollisionGridVerts,
    .faces       = _gShelterB6GrowthRoomCollisionGridFaces,
    .cellFaceIds = _gShelterB6GrowthRoomCollisionGridTable,
    .xBias       = 500,
    .zBias       = 4000,
    .cellCountX  = 2,
    .cellCountZ  = 4,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB6GrowthRoomCollisionGridFaces),
};

ViewCamera gShelterB6GrowthRoomViewCameras[SHELTER_B6_GROWTH_ROOM_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -2500, 25000, -3500 } }, 289 },
    { { { { -4032, 0, 718 }, { 2, 4095, 13 }, { -718, 13, -4032 } }, { -3570, 1470, -8390 } }, 230 },
    { { { { 4076, 0, 402 }, { 2, 4095, -23 }, { -402, 23, 4076 } }, { -3490, 1430, -2020 } }, 230 },
    { { { { -4066, 0, 489 }, { 331, 3011, 2756 }, { -359, 2776, -2990 } }, { -4350, 3350, 360 } }, 257 },
    { { { { 4036, 0, 697 }, { 316, 3649, -1831 }, { -621, 1858, 3596 } }, { -3112, 3278, -6073 } }, 230 },
    { { { { -4021, 0, 776 }, { 248, 3880, 1286 }, { -735, 1310, -3810 } }, { -3440, 2690, -4930 } }, 257 },
    { { { { -1147, 0, 3932 }, { 1405, 3825, 410 }, { -3672, 1463, -1071 } }, { -2780, 1860, -4500 } }, 289 },
    { { { { 798, 0, -4017 }, { -97, 4094, -19 }, { 4016, 99, 798 } }, { -4250, 1470, -5010 } }, 329 },
};

/// View 1's empty sprite list; a zero first count retains decoded background strips.
static SpriteBatch _gShelterB6GrowthRoomView1SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture encoding and neutral opaque modulation of the two populated sprite views.
enum {
    SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE            = 0x8F,   // 8-bit texture page at VRAM (960, 0), blend mode 0
    SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT             = 0x3FC0, // Palette at VRAM (0, 255)
    SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR    = 128,
    SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED = 0,
};

/// Six depth-sorted rectangles for view 2, in screen pixels and texture texels.
static SpriteSource _gShelterB6GrowthRoomView2SpriteSources[6] = {
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, 40, 1110, { .fields = { 112, 0 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, 40, 1124, { .fields = { 112, 16 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, 24, 1129, { .fields = { 120, 32 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, 64, 1156, { .fields = { 112, 48 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, 64, 1150, { .fields = { 112, 64 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 16, 8 } }, 96, 56, 1139, { .fields = { 112, 80 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
};

/// View 2's background selector, complete source range and terminal record.
static SpriteBatch _gShelterB6GrowthRoomView2SpriteBatches[3] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterB6GrowthRoomView2SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Nineteen depth-sorted rectangles for view 3, in screen pixels and texture texels.
static SpriteSource _gShelterB6GrowthRoomView3SpriteSources[19] = {
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, -160, 16, 1203, { .fields = { 112, 32 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 16, 32 } }, -160, 40, 1154, { .fields = { 112, 0 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, -144, 16, 1164, { .fields = { 112, 56 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, -144, 40, 1175, { .fields = { 112, 144 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, -144, 56, 1178, { .fields = { 112, 160 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 16, 8 } }, -144, 8, 1210, { .fields = { 104, 240 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -128, 8, 1210, { .fields = { 120, 224 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, -128, 24, 1221, { .fields = { 112, 128 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, -128, 40, 1206, { .fields = { 112, 176 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, -128, 56, 1205, { .fields = { 112, 208 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, 16, 1243, { .fields = { 120, 192 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, 32, 1243, { .fields = { 112, 80 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, 56, 1236, { .fields = { 120, 240 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -104, 16, 1237, { .fields = { 112, 96 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -104, 32, 1245, { .fields = { 112, 112 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 8, 24 } }, -104, 48, 1300, { .fields = { 120, 104 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -112, 16, 1178, { .fields = { 112, 192 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -112, 32, 1244, { .fields = { 112, 224 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B6_GROWTH_ROOM_SPRITE_TPAGE, SHELTER_B6_GROWTH_ROOM_SPRITE_CLUT, { .fields = { 8, 24 } }, -112, 48, 1290, { .fields = { 120, 80 } }, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_NEUTRAL_COLOR, SHELTER_B6_GROWTH_ROOM_SPRITE_OPAQUE_MODULATED },
};

/// View 3's background selector, complete source range and terminal record.
static SpriteBatch _gShelterB6GrowthRoomView3SpriteBatches[3] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterB6GrowthRoomView3SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 4's empty sprite list, retaining decoded background strips.
static SpriteBatch _gShelterB6GrowthRoomView4SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 5's empty sprite list, retaining decoded background strips.
static SpriteBatch _gShelterB6GrowthRoomView5SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 6's empty sprite list, retaining decoded background strips.
static SpriteBatch _gShelterB6GrowthRoomView6SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 7's empty sprite list, retaining decoded background strips.
static SpriteBatch _gShelterB6GrowthRoomView7SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 8's empty sprite list, retaining decoded background strips.
static SpriteBatch _gShelterB6GrowthRoomView8SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gShelterB6GrowthRoomSpriteViews[SHELTER_B6_GROWTH_ROOM_VIEW_COUNT] = {
    { { .empty = _gShelterB6GrowthRoomView1SpriteBatches }, _gShelterB6GrowthRoomView1SpriteBatches, NULL },
    { { .elements = _gShelterB6GrowthRoomView2SpriteSources }, _gShelterB6GrowthRoomView2SpriteBatches, NULL },
    { { .elements = _gShelterB6GrowthRoomView3SpriteSources }, _gShelterB6GrowthRoomView3SpriteBatches, NULL },
    { { .empty = _gShelterB6GrowthRoomView4SpriteBatches }, _gShelterB6GrowthRoomView4SpriteBatches, NULL },
    { { .empty = _gShelterB6GrowthRoomView5SpriteBatches }, _gShelterB6GrowthRoomView5SpriteBatches, NULL },
    { { .empty = _gShelterB6GrowthRoomView6SpriteBatches }, _gShelterB6GrowthRoomView6SpriteBatches, NULL },
    { { .empty = _gShelterB6GrowthRoomView7SpriteBatches }, _gShelterB6GrowthRoomView7SpriteBatches, NULL },
    { { .empty = _gShelterB6GrowthRoomView8SpriteBatches }, _gShelterB6GrowthRoomView8SpriteBatches, NULL },
};

/// White room point light at (2400, -2850, 4050), active in every logical view.
///
/// Full strength within 5000 world units, fading to zero at 10000. Room setup
/// and shading update the coordinate cache, parent and attenuation in place.
static WorldCoordPointLight _gShelterB6GrowthRoomPointLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2400, -2850, 4050 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 5000, 10000 },
};

WorldCoordRoomLights gShelterB6GrowthRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB6GrowthRoomPointLights),
    .pointLights           = _gShelterB6GrowthRoomPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB6GrowthRoomViewBoundaryTriggers[12] = {
    { NULL, NULL, NULL, { 2769, -2144, 5184, 0 }, { { 3833, -2544, -381, 0 }, { -3841, -2544, 375, 0 }, { 3833, 2544, -381, 0 }, { -3841, 2544, 375, 0 } }, { 402, 0, 4087, 0 }, { 0, 0, 4096, 0 }, 4608, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2608, -2145, 5312, 0 }, { { -3808, -2544, 372, 0 }, { 3803, -2544, -378, 0 }, { -3808, 2544, 372, 0 }, { 3803, 2544, -378, 0 } }, { -402, 0, -4078, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4064, -1985, -1967, 0 }, { { 1952, -2448, 16, 0 }, { -1952, -2448, -16, 0 }, { 1952, 2448, 16, 0 }, { -1952, 2448, -16, 0 } }, { -35, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 3124, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4032, -2081, -1808, 0 }, { { -1856, -2384, -32, 0 }, { 1856, -2384, 32, 0 }, { -1856, 2384, -32, 0 }, { 1856, 2384, 32, 0 } }, { 70, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 3018, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4576, -2080, 9118, 0 }, { { 2230, -2544, 851, 0 }, { -2237, -2544, -860, 0 }, { 2230, 2544, 851, 0 }, { -2237, 2544, -860, 0 } }, { -1469, 0, 3833, 0 }, { 0, 0, 4096, 0 }, 3491, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4605, -2096, 9372, 0 }, { { -2272, -2496, -875, 0 }, { 2262, -2496, 865, 0 }, { -2272, 2496, -875, 0 }, { 2262, 2496, 865, 0 } }, { 1470, 0, -3833, 0 }, { 0, 0, 4096, 0 }, 3481, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 192, -2112, 8961, 0 }, { { -2178, -2576, 465, 0 }, { 2178, -2576, -464, 0 }, { -2178, 2576, 465, 0 }, { 2178, 2576, -464, 0 } }, { -858, 0, -4018, 0 }, { 0, 0, 4096, 0 }, 3396, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 240, -2064, 8784, 0 }, { { 2224, -2560, -457, 0 }, { -2224, -2560, 458, 0 }, { 2224, 2560, -457, 0 }, { -2224, 2560, 458, 0 } }, { 827, 0, 4026, 0 }, { 0, 0, 4096, 0 }, 3415, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -194, -1760, 1199, 0 }, { { -2333, -2544, -889, 0 }, { 2334, -2544, 890, 0 }, { -2333, 2544, -889, 0 }, { 2334, 2544, 890, 0 } }, { 1462, 0, -3838, 0 }, { 0, 0, 4096, 0 }, 3556, 0, 6, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4383, -1728, 1407, 0 }, { { -2404, -2544, 676, 0 }, { 2404, -2544, -675, 0 }, { -2404, 2544, 676, 0 }, { 2404, 2544, -675, 0 } }, { -1112, 0, -3954, 0 }, { 0, 0, 4096, 0 }, 3556, 0, 6, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -321, -1792, 1022, 0 }, { { 2334, -2544, 890, 0 }, { -2333, -2544, -889, 0 }, { 2334, 2544, 890, 0 }, { -2333, 2544, -889, 0 } }, { -1463, 0, 3836, 0 }, { 0, 0, 4096, 0 }, 3556, 0, 2, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4351, -1760, 1278, 0 }, { { 2404, -2544, -675, 0 }, { -2404, -2544, 676, 0 }, { 2404, 2544, -675, 0 }, { -2404, 2544, 676, 0 } }, { 1110, 0, 3952, 0 }, { 0, 0, 4096, 0 }, 3556, 0, 2, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Loads actor package 450900 for the companion scene in area variant 1.
///
/// The paired placement list is empty: the room starts these tasks and scripts
/// explicitly. Entry 101 remains the resource key; the second record terminates.
static AreaResource _gShelterB6GrowthRoomSceneResources[2] = {
    { 101, 509, AREA_RESOURCE_FILE_GROUP_BASE_40, ACTOR_450900_TASK_EXIT, { 0, 0 }, gActor450900Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gShelterB6GrowthRoomAreaVariants[SHELTER_B6_GROWTH_ROOM_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapNeoArkGrowthRoomDefaultPlacements, .resources = _gShelterB6GrowthRoomSceneResources },
};

/// Room action and dialogue identifiers carried by the interaction records.
///
/// CAP command 1 and command 16 are sent as room commands; the remaining CAP
/// identifiers select the loaded dialogue directly. These values are authored
/// request IDs, independent of the trigger indices.
enum {
    SHELTER_B6_GROWTH_ROOM_ACTION_CONVERSATION      = 1,
    SHELTER_B6_GROWTH_ROOM_ACTION_DEPARTURE         = 2,
    SHELTER_B6_GROWTH_ROOM_COMMAND_COMPANION_CHOICE = 1,
    SHELTER_B6_GROWTH_ROOM_COMMAND_IDLE_EVENT       = 16,
    SHELTER_B6_GROWTH_ROOM_CAP_EXAMINE_13           = 13,
    SHELTER_B6_GROWTH_ROOM_CAP_EXAMINE_14           = 14,
    SHELTER_B6_GROWTH_ROOM_CAP_EXAMINE_15           = 15,
    SHELTER_B6_GROWTH_ROOM_CAP_EXAMINE_18           = 18,
    SHELTER_B6_GROWTH_ROOM_CAP_EXAMINE_19           = 19,
};

WorldCollisionTrigger gShelterB6GrowthRoomActionTriggers[12] = {
    { NULL, NULL, NULL, { 4263, -64, 6519, 0 }, { { -462, 0, -603, 0 }, { 451, 0, -620, 0 }, { -442, 0, 604, 0 }, { 455, 0, 621, 0 } }, { 0, 4114, 0, 0 }, { 4075, 0, 402, 0 }, 768, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B6_GROWTH_ROOM_ACTION_CONVERSATION, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5152, -64, 6768, 0 }, { { -1024, 0, -1360, 0 }, { 1024, 0, -1360, 0 }, { -1024, 0, 1360, 0 }, { 1024, 0, 1360, 0 } }, { 0, 4111, 0, 0 }, { -4097, 0, 0, 0 }, 1698, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B6_GROWTH_ROOM_COMMAND_COMPANION_CHOICE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4032, -64, -3520, 0 }, { { -1024, 0, -608, 0 }, { 1024, 0, -608, 0 }, { -1024, 0, 608, 0 }, { 1024, 0, 608, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1187, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B6_GROWTH_ROOM_ACTION_DEPARTURE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3496, -64, 6403, 0 }, { { 87, 0, -378, 0 }, { 1021, 0, -621, 0 }, { 77, 0, 599, 0 }, { 961, 0, 880, 0 } }, { 0, 4100, 0, 0 }, { 4090, 0, -201, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B6_GROWTH_ROOM_ACTION_CONVERSATION, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 736, -64, 5584, 0 }, { { -1262, 0, -2763, 0 }, { 1251, 0, -2780, 0 }, { -1242, 0, 2764, 0 }, { 1255, 0, 2781, 0 } }, { 0, 4101, 0, 0 }, { 4075, 0, 402, 0 }, 3050, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B6_GROWTH_ROOM_CAP_EXAMINE_15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 368, -64, 1072, 0 }, { { -574, 0, -1499, 0 }, { 563, 0, -1516, 0 }, { -554, 0, 1500, 0 }, { 567, 0, 1517, 0 } }, { 0, 4095, 0, 0 }, { 4075, 0, 402, 0 }, 1619, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B6_GROWTH_ROOM_CAP_EXAMINE_13, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, 9600, 0 }, { { -574, 0, -1147, 0 }, { 563, 0, -1164, 0 }, { -554, 0, 1148, 0 }, { 567, 0, 1165, 0 } }, { 0, 4096, 0, 0 }, { 4094, 0, 1, 0 }, 1292, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B6_GROWTH_ROOM_CAP_EXAMINE_18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1312, -64, 0x28D0, 0 }, { { -1886, 0, -587, 0 }, { 1875, 0, -604, 0 }, { -1866, 0, 588, 0 }, { 1879, 0, 605, 0 } }, { 0, 4122, 0, 0 }, { 201, 0, -4091, 0 }, 1974, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B6_GROWTH_ROOM_CAP_EXAMINE_14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5152, -64, 8816, 0 }, { { -1024, 0, -656, 0 }, { 1024, 0, -656, 0 }, { -1024, 0, 656, 0 }, { 1024, 0, 656, 0 } }, { 0, 4096, 0, 0 }, { -4097, 0, 0, 0 }, 1214, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B6_GROWTH_ROOM_COMMAND_IDLE_EVENT, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5152, -64, 2720, 0 }, { { -1024, 0, -2704, 0 }, { 1024, 0, -2704, 0 }, { -1024, 0, 2704, 0 }, { 1024, 0, 2704, 0 } }, { 0, 4106, 0, 0 }, { -4097, 0, 0, 0 }, 2884, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B6_GROWTH_ROOM_COMMAND_IDLE_EVENT, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4064, -64, 0x2F20, 0 }, { { -1086, 0, -475, 0 }, { 1075, 0, -492, 0 }, { -1066, 0, 476, 0 }, { 1079, 0, 493, 0 } }, { 0, 4096, 0, 0 }, { 402, 0, -4075, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B6_GROWTH_ROOM_CAP_EXAMINE_19, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1312, -64, -1120, 0 }, { { -1086, 0, -475, 0 }, { 1075, 0, -492, 0 }, { -1066, 0, 476, 0 }, { 1079, 0, 493, 0 } }, { 0, 4096, 0, 0 }, { -1, 0, 4094, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B6_GROWTH_ROOM_CAP_EXAMINE_19, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry gShelterB6GrowthRoomAmbientByView[SHELTER_B6_GROWTH_ROOM_VIEW_COUNT + 1] = {
    { .viewCount = ARRAY_SIZE(gShelterB6GrowthRoomAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 539, 477, 477, 500 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Type-1 footstep cue bases for growth-room surface class 1.
///
/// Walk and scripted jump use entries 89/90, run uses 91/92; the second entry
/// is animation cue 1. Companion playback adds 100 to these entry numbers.
static WorldCollisionFootstepSounds _gShelterB6GrowthRoomSurface1FootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | 89,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | 91,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | 89,
};

/// Type-1 footstep cue bases for growth-room surface class 2.
///
/// Walk and scripted jump use entries 93/94, run uses 95/96; the second entry
/// is animation cue 1. Companion playback adds 100 to these entry numbers.
static WorldCollisionFootstepSounds _gShelterB6GrowthRoomSurface2FootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | 93,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | 95,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | 93,
};

/// Blocking, impact-enabled surface with pushback and no footstep cues (classes 0, 3..7).
static WorldCollisionSurfaceProperties _gShelterB6GrowthRoomSilentSurface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface class 1's blocking, weapon-impact, pushback and footstep policies.
static WorldCollisionSurfaceProperties _gShelterB6GrowthRoomSurface1 = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB6GrowthRoomSurface1FootstepSounds,
};

/// Surface class 2's blocking, weapon-impact, pushback and footstep policies.
static WorldCollisionSurfaceProperties _gShelterB6GrowthRoomSurface2 = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB6GrowthRoomSurface2FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB6GrowthRoomSurfaceProperties[8] = {
    &_gShelterB6GrowthRoomSilentSurface,
    &_gShelterB6GrowthRoomSurface1,
    &_gShelterB6GrowthRoomSurface2,
    &_gShelterB6GrowthRoomSilentSurface,
    &_gShelterB6GrowthRoomSilentSurface,
    &_gShelterB6GrowthRoomSilentSurface,
    &_gShelterB6GrowthRoomSilentSurface,
    &_gShelterB6GrowthRoomSilentSurface,
};

/// Low-nibble requests in the authored entry-area policies.
enum {
    SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_CLEAR = 0,
    SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET   = 1,
};

AreaApplyRec gShelterB6GrowthRoomEntryAreaUpdates[SHELTER_B6_GROWTH_ROOM_ENTRY_AREA_UPDATE_COUNT] = {
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_AIRLOCK, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_CLEAR },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_NORTH_PROMENADE, 3, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_FOREST_ZONE, 5, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL, 3, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_CLEAR },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PAVILION, 5, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ISLAND, 3, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_GARDEN, 2, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_CLEAR },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_1, 4, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SAVANNA_ZONE, 5, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SOUTH_PROMENADE, 5, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 5, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_BRIDGE, 5, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 4, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_GALLERY, 3, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PYRAMID, 4, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_GORGE, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_SECRET_PASSAGE, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STOREROOM, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, 11, AREA_APPLY_MODE_REPLAY_SCAVENGER | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, 17, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_CONTROL_ROOM, 13, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, 11, AREA_APPLY_MODE_REPLAY_SCAVENGER | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, 17, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_OPERATING_ROOM, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_ELEVATOR_HALL, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_LOWER_SEWER, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_RESERVOIR, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_WATER_SUPPLY, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 11, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_CLEAR },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE, 21, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 21, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE, 21, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1, 21, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2, 21, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT, 21, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER, 21, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY, 21, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY, 21, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, 21, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, 21, AREA_APPLY_MODE_ALWAYS | SHELTER_B6_GROWTH_ROOM_ENTRY_MAP_MARK_SET },
    { AREA_APPLY_END, 0, 0, 0 },
};

void shelterB6GrowthRoomResetCollisionBox(s32 useYOffset)
{
    WorldCollisionGrid*       liveGrid;
    const WorldCollisionGrid* boxTemplate;
    SVECTOR                   offset;
    s32                       geometryIndex;

    liveGrid    = &gShelterB6GrowthRoomCollisionGrid;
    boxTemplate = &_gShelterB6GrowthRoomCollisionBoxTemplate;

    // Restore complete faces and vector XYZs while retaining fourth halfwords and cell lists.
    for (geometryIndex = 0; geometryIndex < SHELTER_B6_GROWTH_ROOM_BOX_FACE_COUNT; geometryIndex++) {
        liveGrid->normals[geometryIndex].vx = boxTemplate->normals[geometryIndex].vx;
        liveGrid->normals[geometryIndex].vy = boxTemplate->normals[geometryIndex].vy;
        liveGrid->normals[geometryIndex].vz = boxTemplate->normals[geometryIndex].vz;
        liveGrid->faces[geometryIndex]      = boxTemplate->faces[geometryIndex];
    }
    for (geometryIndex = 0; geometryIndex < SHELTER_B6_GROWTH_ROOM_BOX_VERTEX_COUNT; geometryIndex++) {
        liveGrid->vertices[geometryIndex].vx = boxTemplate->vertices[geometryIndex].vx;
        liveGrid->vertices[geometryIndex].vy = boxTemplate->vertices[geometryIndex].vy;
        liveGrid->vertices[geometryIndex].vz = boxTemplate->vertices[geometryIndex].vz;
    }

    if (useYOffset == 0) {
        setVector(&offset, 0, 0, 0);
    } else {
        setVector(&offset, 0, SHELTER_B6_GROWTH_ROOM_BOX_Y_OFFSET, 0);
    }

    // Always restore the template first; repeated calls must not accumulate the shift.
    for (geometryIndex = 0; geometryIndex < SHELTER_B6_GROWTH_ROOM_BOX_VERTEX_COUNT; geometryIndex++) {
        liveGrid->vertices[geometryIndex].vx += offset.vx;
        liveGrid->vertices[geometryIndex].vy += offset.vy;
        liveGrid->vertices[geometryIndex].vz += offset.vz;
    }
}

void shelterB6GrowthRoomAmbientEffectsTask(Task* task)
{
    enum {
        SHELTER_B6_GROWTH_ROOM_RAMP_LIMIT           = 304,
        SHELTER_B6_GROWTH_ROOM_RAMP_FRAME_MASK      = 7,
        SHELTER_B6_GROWTH_ROOM_MIST_SOURCE_LIMIT    = SHELTER_B6_GROWTH_ROOM_MIST_ANCHOR_COUNT,
        SHELTER_B6_GROWTH_ROOM_MIST_DENSITY_SHIFT   = 4,
        SHELTER_B6_GROWTH_ROOM_MIST_ANGLE_MASK      = 0x7FF,
        SHELTER_B6_GROWTH_ROOM_MIST_HALF_ARC        = 0x400,
        SHELTER_B6_GROWTH_ROOM_MIST_RADIUS          = 1000,
        SHELTER_B6_GROWTH_ROOM_TRIG_SHIFT           = 12,
        SHELTER_B6_GROWTH_ROOM_MIST_SPAWN_ARG       = (16 << SHELTER_B6_GROWTH_ROOM_PARTICLE_SPEED_SHIFT) | (6 << SHELTER_B6_GROWTH_ROOM_PARTICLE_PERIOD_SHIFT) | 1280,
        SHELTER_B6_GROWTH_ROOM_PUFF_SPAWN_ARG       = (24 << SHELTER_B6_GROWTH_ROOM_PARTICLE_SPEED_SHIFT) | (3 << SHELTER_B6_GROWTH_ROOM_PARTICLE_PERIOD_SHIFT) | 640,
        SHELTER_B6_GROWTH_ROOM_GLOW_BASE_BRIGHTNESS = 80,
        SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_SMALL    = 0x100,
        SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM   = 0x180,
        SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE    = 0x200,
        SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN      = 0x044,
        SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_RED       = 0x400,
        SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_YELLOW    = 0x440,
        SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_GREY      = 0x444
    };
    SVECTOR spawnPosition;
    s32     mistAngle;
    s32     mistSourceIndex;
    s32     puffBaseZ;

/// Emits a mist puff on a radius-1000 semicircle around floor anchor 0..5.
///
/// Requires a live Task* with spawnArg1.value in 0..304 and a valid anchorIndex.
/// position is a writable SVECTOR lvalue, angle a writable s32 lvalue; all arguments
/// must be stable and side-effect-free. Draws bearing then height from the shared
/// unsigned LCG. Bearing is -1024..1023 in 4096 units/turn. XYZ uses world input
/// coordinates, with negative Y sampled below ((spawnArg1.value + 1) * 8).
/// Uses room-owned anchors 30..35 and this controller's particle constants.
/// Q12 trig results narrow to s16 XYZ; pad remains untouched. Spawning copies XYZ
/// immediately; the mist task never reads the retained stack offset pointer.
/// Expands to a compound statement and leaves position and angle as outputs.
#define SHELTER_B6_GROWTH_ROOM_SPAWN_MIST(effectTask, anchorIndex, position, angle)                                                                                                                                    \
    {                                                                                                                                                                                                                  \
        RANDOM_LCG_ADVANCE();                                                                                                                                                                                          \
        (angle)       = (RANDOM_LCG_DRAW() & SHELTER_B6_GROWTH_ROOM_MIST_ANGLE_MASK) - SHELTER_B6_GROWTH_ROOM_MIST_HALF_ARC;                                                                                           \
        (position).vx = _gShelterB6GrowthRoomEffectAnchors[(anchorIndex) + SHELTER_B6_GROWTH_ROOM_MIST_ANCHOR_START].vx + ((rcos((angle)) * SHELTER_B6_GROWTH_ROOM_MIST_RADIUS) >> SHELTER_B6_GROWTH_ROOM_TRIG_SHIFT); \
        RANDOM_LCG_ADVANCE();                                                                                                                                                                                          \
        (position).vy = -((s32)RANDOM_LCG_DRAW() % (((effectTask)->spawnArg1.value + 1) * 8));                                                                                                                         \
        (position).vz = _gShelterB6GrowthRoomEffectAnchors[(anchorIndex) + SHELTER_B6_GROWTH_ROOM_MIST_ANCHOR_START].vz + ((rsin((angle)) * SHELTER_B6_GROWTH_ROOM_MIST_RADIUS) >> SHELTER_B6_GROWTH_ROOM_TRIG_SHIFT); \
        effectSpawn(EFFECT_GROWTH_ROOM_MIST, NULL, SHELTER_B6_GROWTH_ROOM_MIST_SPAWN_ARG, &(position));                                                                                                                \
    }

    // Ramp the glow and mist spread; source count latches at six floor anchors.
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        if (task->spawnArg1.value < SHELTER_B6_GROWTH_ROOM_RAMP_LIMIT && !(gDisplayState.animFrame & SHELTER_B6_GROWTH_ROOM_RAMP_FRAME_MASK)) {
            task->spawnArg1.value++;
        }
    }
    if (task->state < SHELTER_B6_GROWTH_ROOM_MIST_SOURCE_LIMIT) {
        task->state = (task->spawnArg1.value >> SHELTER_B6_GROWTH_ROOM_MIST_DENSITY_SHIFT) + 1;
    }
    if (gDisplayState.animFrame % (task->state * 2 + 4) == 0) {
        for (mistSourceIndex = 0; mistSourceIndex < task->state; mistSourceIndex++) {
            SHELTER_B6_GROWTH_ROOM_SPAWN_MIST(task, mistSourceIndex, spawnPosition, mistAngle);
        }
    }
#undef SHELTER_B6_GROWTH_ROOM_SPAWN_MIST
    // Puffs continue on alternating frames even while the ramp is paused.
    if (!(gDisplayState.animFrame & 1)) {
        RANDOM_LCG_ADVANCE();
        spawnPosition.vx = -1000;
        RANDOM_LCG_ADVANCE();
        spawnPosition.vy = -1200 - RANDOM_LCG_DRAW() % 400;
        RANDOM_LCG_ADVANCE();
        puffBaseZ = RANDOM_LCG_DRAW() % 400 + 3500;
        RANDOM_LCG_ADVANCE();
        spawnPosition.vz = puffBaseZ + (RANDOM_LCG_DRAW() & 1) * 1000;
        effectSpawn(EFFECT_SHELTER_B6_GROWTH_ROOM_DRIFT_PUFF, NULL, SHELTER_B6_GROWTH_ROOM_PUFF_SPAWN_ARG, &spawnPosition);
    }
    _shelterB6GrowthRoomDrawBottomGlow(task->spawnArg1.value, (task->spawnArg1.value >> 1) + SHELTER_B6_GROWTH_ROOM_GLOW_BASE_BRIGHTNESS);
    // Mapped views select the visible subset of the first thirty glow anchors.
    switch (viewGetMappedIndex() & 0xFF) {
        case 2:
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[8], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[9], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_RED);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[14], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_RED);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[15], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[16], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[17], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[18], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[19], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_YELLOW);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[20], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[21], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[22], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_SMALL, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[23], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_SMALL, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[28], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_GREY);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[29], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_GREY);
            break;
        case 3:
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[0], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[1], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[2], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[3], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[4], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[5], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[6], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_RED);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[7], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[12], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[13], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[24], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_GREY);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[25], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_GREY);
            break;
        case 4:
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[21], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[22], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_SMALL, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[23], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_SMALL, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            break;
        case 5:
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[0], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[1], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[2], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[3], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[4], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[5], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[6], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_RED);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[7], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            break;
        case 6:
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[14], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_RED);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[15], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[16], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[21], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[22], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_SMALL, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[23], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_SMALL, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            break;
        case 7:
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[8], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[9], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_RED);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[10], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[11], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[17], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[18], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[19], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_MEDIUM, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_YELLOW);
            glowDrawDisc(&_gShelterB6GrowthRoomEffectAnchors[20], SHELTER_B6_GROWTH_ROOM_GLOW_RADIUS_LARGE, SHELTER_B6_GROWTH_ROOM_GLOW_COLOR_CYAN);
            break;
    }
}

#include "../../shared/effects/glow_draw_disc.inc.c"

/// Draws an additive black-to-grey gradient rising from the viewport's bottom edge.
///
/// `heightPixels` is the signed distance above centred Y=120; heights above 240
/// extend beyond the viewport. `brightness` supplies the low byte
/// of the bottom corners' RGB. Requires a live primitive arena and ordering table.
static void _shelterB6GrowthRoomDrawBottomGlow(s16 heightPixels, s16 brightness)
{
    enum {
        SHELTER_B6_GROWTH_ROOM_BOTTOM_GLOW_SORT_DEPTH        = 0x40,
        SHELTER_B6_GROWTH_ROOM_BOTTOM_GLOW_HALF_WIDTH_PIXELS = 160,
        SHELTER_B6_GROWTH_ROOM_BOTTOM_GLOW_BOTTOM_Y_PIXELS   = 120
    };
    POLY_G4* gradient;

    gradient       = gGpuPrimCursor;
    gGpuPrimCursor = gradient + 1;
    setPolyG4(gradient);
    setRGB0(gradient, 0, 0, 0);
    setRGB1(gradient, 0, 0, 0);
    setRGB2(gradient, brightness, brightness, brightness);
    setRGB3(gradient, brightness, brightness, brightness);
    setXY4(gradient,
           -SHELTER_B6_GROWTH_ROOM_BOTTOM_GLOW_HALF_WIDTH_PIXELS, SHELTER_B6_GROWTH_ROOM_BOTTOM_GLOW_BOTTOM_Y_PIXELS - heightPixels,
           SHELTER_B6_GROWTH_ROOM_BOTTOM_GLOW_HALF_WIDTH_PIXELS, SHELTER_B6_GROWTH_ROOM_BOTTOM_GLOW_BOTTOM_Y_PIXELS - heightPixels,
           -SHELTER_B6_GROWTH_ROOM_BOTTOM_GLOW_HALF_WIDTH_PIXELS, SHELTER_B6_GROWTH_ROOM_BOTTOM_GLOW_BOTTOM_Y_PIXELS,
           SHELTER_B6_GROWTH_ROOM_BOTTOM_GLOW_HALF_WIDTH_PIXELS, SHELTER_B6_GROWTH_ROOM_BOTTOM_GLOW_BOTTOM_Y_PIXELS);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(SHELTER_B6_GROWTH_ROOM_BOTTOM_GLOW_SORT_DEPTH), gradient);
    gpuSetPrimitiveBlendMode(gradient, GPU_BLEND_ADD, SHELTER_B6_GROWTH_ROOM_BOTTOM_GLOW_SORT_DEPTH);
}

/// Sets a room particle's local drift displacement from its direction and speed.
///
/// `work` borrows live, writable effect storage. Its nonzero horizontal `move`
/// is normalized to an approximate Q12 unit vector, then scaled by `step`
/// (1..255 local coordinate units per task tick). Each component rounds down,
/// so the displacement's length need not equal the requested speed.
///
/// Writes only `move.vx`, `move.vy` and `move.vz`; the vector's `pad`, `step`
/// and the rest of the work stay intact. Reads `step` after normalization.
/// Overwrites GTE state and retains no pointer.
static inline void _shelterB6GrowthRoomScaleDriftVelocity(EffectWork* work)
{
    SVECTOR* driftVelocity = &work->move;

    VectorNormalSS(driftVelocity, driftVelocity);
    gte_lddp(work->step);
    gte_ldsv(driftVelocity);
    gte_gpf12();
    gte_stsv(driftVelocity);
}

void shelterB6GrowthRoomMistTask(Task* task)
{
    enum {
        SHELTER_B6_GROWTH_ROOM_MIST_MAX_BRIGHTNESS = 64,
        SHELTER_B6_GROWTH_ROOM_MIST_FADE_STEP      = 4,
        SHELTER_B6_GROWTH_ROOM_MIST_FADE_TICKS     = 16
    };
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    s32         velocityZ;
    s32         fadeAge;
    s16         driftSpeed;
    u32         randomX;
    u32         randomZ;

    work->age++;
    if (task->state == SHELTER_B6_GROWTH_ROOM_PARTICLE_STATE_INIT) {
        work->scale = task->spawnArg1.value & SHELTER_B6_GROWTH_ROOM_PARTICLE_SIZE_MASK;

        if (task->spawnArg1.value & SHELTER_B6_GROWTH_ROOM_PARTICLE_PERIOD_FIELD_MASK) {
            work->period = (task->spawnArg1.value >> SHELTER_B6_GROWTH_ROOM_PARTICLE_PERIOD_SHIFT) & SHELTER_B6_GROWTH_ROOM_PARTICLE_PERIOD_MASK;
        } else {
            work->period = SHELTER_B6_GROWTH_ROOM_PARTICLE_DEFAULT_PERIOD;
        }

        work->age   = 0;
        task->state = SHELTER_B6_GROWTH_ROOM_PARTICLE_STATE_DRIFT;

        if (task->spawnArg1.value & SHELTER_B6_GROWTH_ROOM_PARTICLE_SPEED_FIELD_MASK) {
            driftSpeed = (task->spawnArg1.value >> SHELTER_B6_GROWTH_ROOM_PARTICLE_SPEED_SHIFT) & SHELTER_B6_GROWTH_ROOM_PARTICLE_SPEED_MASK;
        } else {
            driftSpeed = SHELTER_B6_GROWTH_ROOM_PARTICLE_DEFAULT_SPEED;
        }

        work->step      = driftSpeed;
        work->move.vy   = 0;
        randomX         = RANDOM_LCG_NEXT(gRandomLcgState);
        gRandomLcgState = randomX;
        work->move.vx   = 0x80 - ((randomX >> 16) & 0xFF);
        randomZ         = RANDOM_LCG_NEXT(gRandomLcgState);
        gRandomLcgState = randomZ;
        velocityZ       = 0x80 - ((randomZ >> 16) & 0xFF);
        work->move.vz   = velocityZ;
        _shelterB6GrowthRoomScaleDriftVelocity(work);
    }

    // The shared angle slot carries grey modulation here, rather than a rotation.
    if (work->age < work->period * SHELTER_B6_GROWTH_ROOM_PARTICLE_FRAME_COUNT - SHELTER_B6_GROWTH_ROOM_MIST_FADE_TICKS) {
        if (work->angle < SHELTER_B6_GROWTH_ROOM_MIST_MAX_BRIGHTNESS) {
            work->angle += SHELTER_B6_GROWTH_ROOM_MIST_FADE_STEP;
        }
    } else {
        fadeAge     = work->age + SHELTER_B6_GROWTH_ROOM_MIST_FADE_TICKS;
        work->angle = SHELTER_B6_GROWTH_ROOM_MIST_MAX_BRIGHTNESS - (fadeAge - work->period * SHELTER_B6_GROWTH_ROOM_PARTICLE_FRAME_COUNT) * SHELTER_B6_GROWTH_ROOM_MIST_FADE_STEP;
    }

    _shelterB6GrowthRoomDrawMist(coord, work->index, work->scale, work->angle);

    // Draw the current placement before advancing it for the next update.
    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    // Age zero already advances the texture cursor; keep that first-tick ordering.
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= SHELTER_B6_GROWTH_ROOM_PARTICLE_FRAME_COUNT) {
            effectKillTask(work, task);
        }
    }
}

/// Draws one brightness-modulated, additive mist cell at a composed coordinate.
///
/// `coord` is borrowed read-only; its cached translation must be current in
/// `GsWSMATRIX`'s input space. `frame` is 0..9 in a two-column 128x32-texel sheet.
/// `size` is a signed perspective numerator: half-extents are size * 127 or 31
/// divided by SZ3/4. RGB stores keep brightness's low byte. Projections require
/// nonnegative FLAG and SZ3/4 >= 65. Requires initialized scratch and GPU arenas;
/// releases its scratch block on every path and changes GTE state.
static void _shelterB6GrowthRoomDrawMist(const GfxCoord* coord, u16 frame, s16 size, u16 brightness)
{
    enum {
        SHELTER_B6_GROWTH_ROOM_MIST_MIN_DEPTH          = 65,
        SHELTER_B6_GROWTH_ROOM_MIST_COLUMN_SHIFT       = 7,
        SHELTER_B6_GROWTH_ROOM_MIST_ROW_SHIFT          = 5,
        SHELTER_B6_GROWTH_ROOM_MIST_WIDTH_SPAN_TEXELS  = 127,
        SHELTER_B6_GROWTH_ROOM_MIST_HEIGHT_SPAN_TEXELS = 31
    };
    EffectShapeScratch* block;
    POLY_FT4*           quad;
    s32                 uLeft;
    s32                 vTop;
    s32                 uRight;
    s32                 vBottom;
    s16                 edgePixels;

    // Project the centre once; the texture remains aligned with the screen axes.
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        if (block->depth >= SHELTER_B6_GROWTH_ROOM_MIST_MIN_DEPTH) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            quad->tpage = getTPage(SHELTER_B6_GROWTH_ROOM_PARTICLE_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 768, 0);
            quad->clut  = getClut(96, 270);
            uLeft       = (frame & 1) << SHELTER_B6_GROWTH_ROOM_MIST_COLUMN_SHIFT;
            vTop        = (frame >> 1) << SHELTER_B6_GROWTH_ROOM_MIST_ROW_SHIFT;
            uRight      = uLeft + SHELTER_B6_GROWTH_ROOM_MIST_WIDTH_SPAN_TEXELS;
            vBottom     = vTop + SHELTER_B6_GROWTH_ROOM_MIST_HEIGHT_SPAN_TEXELS;
            setRGB0(quad, brightness, brightness, brightness);
            setUV4(quad, uLeft, vTop, uRight, vTop, uLeft, vBottom, uRight, vBottom);
            setSemiTrans(quad, 1);
            block->extent.corner.x = (size * SHELTER_B6_GROWTH_ROOM_MIST_WIDTH_SPAN_TEXELS) / block->depth;
            block->extent.corner.y = (size * SHELTER_B6_GROWTH_ROOM_MIST_HEIGHT_SPAN_TEXELS) / block->depth;
            edgePixels             = block->screenX - (u16)block->extent.corner.x;
            quad->x2               = edgePixels;
            quad->x0               = edgePixels;
            edgePixels             = block->screenX + (u16)block->extent.corner.x;
            quad->x3               = edgePixels;
            quad->x1               = edgePixels;
            edgePixels             = block->screenY - (u16)block->extent.corner.y;
            quad->y1               = edgePixels;
            quad->y0               = edgePixels;
            edgePixels             = block->screenY + (u16)block->extent.corner.y;
            quad->y3               = edgePixels;
            quad->y2               = edgePixels;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void shelterB6GrowthRoomDriftPuffTask(Task* task)
{
    enum {
        SHELTER_B6_GROWTH_ROOM_PUFF_ANGLE_MASK     = ONE - 1,
        SHELTER_B6_GROWTH_ROOM_PUFF_Y_ACCELERATION = 2
    };
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    s32         velocityZ;
    s16         driftSpeed;
    u32         randomX;
    u32         randomZ;

    work->age++;
    if (task->state == SHELTER_B6_GROWTH_ROOM_PARTICLE_STATE_INIT) {
        work->scale = task->spawnArg1.value & SHELTER_B6_GROWTH_ROOM_PARTICLE_SIZE_MASK;
        RANDOM_LCG_ADVANCE();
        work->angle = RANDOM_LCG_DRAW() & SHELTER_B6_GROWTH_ROOM_PUFF_ANGLE_MASK;

        if (task->spawnArg1.value & SHELTER_B6_GROWTH_ROOM_PARTICLE_PERIOD_FIELD_MASK) {
            work->period = (task->spawnArg1.value >> SHELTER_B6_GROWTH_ROOM_PARTICLE_PERIOD_SHIFT) & SHELTER_B6_GROWTH_ROOM_PARTICLE_PERIOD_MASK;
        } else {
            work->period = SHELTER_B6_GROWTH_ROOM_PARTICLE_DEFAULT_PERIOD;
        }

        work->age   = 0;
        task->state = SHELTER_B6_GROWTH_ROOM_PARTICLE_STATE_DRIFT;

        if (task->spawnArg1.value & SHELTER_B6_GROWTH_ROOM_PARTICLE_SPEED_FIELD_MASK) {
            driftSpeed = (task->spawnArg1.value >> SHELTER_B6_GROWTH_ROOM_PARTICLE_SPEED_SHIFT) & SHELTER_B6_GROWTH_ROOM_PARTICLE_SPEED_MASK;
        } else {
            driftSpeed = SHELTER_B6_GROWTH_ROOM_PARTICLE_DEFAULT_SPEED;
        }

        work->step      = driftSpeed;
        work->move.vy   = 0;
        randomX         = RANDOM_LCG_NEXT(gRandomLcgState);
        gRandomLcgState = randomX;
        work->move.vx   = ((randomX >> 16) & 0x7F) + 0x40;
        randomZ         = RANDOM_LCG_NEXT(gRandomLcgState);
        gRandomLcgState = randomZ;
        velocityZ       = 0x40 - ((randomZ >> 16) & 0x7F);
        work->move.vz   = velocityZ;
        _shelterB6GrowthRoomScaleDriftVelocity(work);
    }

    _shelterB6GrowthRoomDrawDriftPuff(coord, work->index, work->scale, work->angle);

    // Draw the current placement before advancing it for the next update.
    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->move.vy      += SHELTER_B6_GROWTH_ROOM_PUFF_Y_ACCELERATION;

    // Age zero already advances the texture cursor; keep that first-tick ordering.
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= SHELTER_B6_GROWTH_ROOM_PARTICLE_FRAME_COUNT) {
            effectKillTask(work, task);
        }
    }
}

/// Draws one raw-texture, additive puff cell as a rotated screen-space square.
///
/// `coord` is borrowed read-only with a current cache in `GsWSMATRIX`'s input
/// space. `frame` is 0..9: five columns with a 48-texel pitch, sampling rows
/// 40..87 or 88..135. `size` * 47 / (SZ3/4) is the signed pixel half-diagonal;
/// `angle` uses 4096 units per turn. Q12 products round down when negative.
/// Requires nonnegative FLAG, SZ3/4 >= 65 and initialized scratch/GPU arenas;
/// releases its scratch block on every path and changes GTE state.
static void _shelterB6GrowthRoomDrawDriftPuff(const GfxCoord* coord, u16 frame, s16 size, s16 angle)
{
    enum {
        SHELTER_B6_GROWTH_ROOM_PUFF_MIN_DEPTH                = 65,
        SHELTER_B6_GROWTH_ROOM_PUFF_COLUMNS                  = 5,
        SHELTER_B6_GROWTH_ROOM_PUFF_CELL_PITCH_TEXELS        = 48,
        SHELTER_B6_GROWTH_ROOM_PUFF_UV_SPAN_TEXELS           = 47,
        SHELTER_B6_GROWTH_ROOM_PUFF_FIRST_TEXEL_ROW          = 40,
        SHELTER_B6_GROWTH_ROOM_PUFF_LAST_CELL_TEXEL_ROW      = 87,
        SHELTER_B6_GROWTH_ROOM_PUFF_QUARTER_TURN             = ONE / 4,
        SHELTER_B6_GROWTH_ROOM_PUFF_TRIG_FRACTION_BITS       = 12,
        SHELTER_B6_GROWTH_ROOM_PUFF_RAW_SEMITRANSPARENT_CODE = 0x2F
    };
    EffectShapeScratch* block;
    POLY_FT4*           quad;
    s32                 uLeft;
    s32                 vTop;
    s32                 uRight;
    s32                 vBottom;
    s32                 cornerAngle;
    s32                 perpendicularAngle;

    // Project one centre, then build the two pairs of opposite corners around it.
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        if (block->depth >= SHELTER_B6_GROWTH_ROOM_PUFF_MIN_DEPTH) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
            setcode(quad, SHELTER_B6_GROWTH_ROOM_PUFF_RAW_SEMITRANSPARENT_CODE);
            quad->tpage = getTPage(SHELTER_B6_GROWTH_ROOM_PARTICLE_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 704, 0);
            quad->clut  = getClut(64, 270);
            uLeft       = (frame % SHELTER_B6_GROWTH_ROOM_PUFF_COLUMNS) * SHELTER_B6_GROWTH_ROOM_PUFF_CELL_PITCH_TEXELS;
            vTop        = (frame / SHELTER_B6_GROWTH_ROOM_PUFF_COLUMNS) * SHELTER_B6_GROWTH_ROOM_PUFF_CELL_PITCH_TEXELS;
            uRight      = uLeft + SHELTER_B6_GROWTH_ROOM_PUFF_UV_SPAN_TEXELS;
            vBottom     = vTop + SHELTER_B6_GROWTH_ROOM_PUFF_LAST_CELL_TEXEL_ROW;
            vTop        = vTop + SHELTER_B6_GROWTH_ROOM_PUFF_FIRST_TEXEL_ROW;
            setUV4(quad, uLeft, vTop, uRight, vTop, uLeft, vBottom, uRight, vBottom);
            cornerAngle            = angle;
            block->extent.corner.x = (((size * SHELTER_B6_GROWTH_ROOM_PUFF_UV_SPAN_TEXELS) / block->depth) * rsin(cornerAngle)) >> SHELTER_B6_GROWTH_ROOM_PUFF_TRIG_FRACTION_BITS;
            block->extent.corner.y = (((size * SHELTER_B6_GROWTH_ROOM_PUFF_UV_SPAN_TEXELS) / block->depth) * rcos(cornerAngle)) >> SHELTER_B6_GROWTH_ROOM_PUFF_TRIG_FRACTION_BITS;
            quad->x0               = block->screenX + (u16)block->extent.corner.x;
            quad->x3               = block->screenX - (u16)block->extent.corner.x;
            quad->y0               = block->screenY - (u16)block->extent.corner.y;
            perpendicularAngle     = cornerAngle + SHELTER_B6_GROWTH_ROOM_PUFF_QUARTER_TURN;
            quad->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((size * SHELTER_B6_GROWTH_ROOM_PUFF_UV_SPAN_TEXELS) / block->depth) * rsin(perpendicularAngle)) >> SHELTER_B6_GROWTH_ROOM_PUFF_TRIG_FRACTION_BITS;
            block->extent.corner.y = (((size * SHELTER_B6_GROWTH_ROOM_PUFF_UV_SPAN_TEXELS) / block->depth) * rcos(perpendicularAngle)) >> SHELTER_B6_GROWTH_ROOM_PUFF_TRIG_FRACTION_BITS;
            quad->x1               = block->screenX + (u16)block->extent.corner.x;
            quad->x2               = block->screenX - (u16)block->extent.corner.x;
            quad->y1               = block->screenY - (u16)block->extent.corner.y;
            quad->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
