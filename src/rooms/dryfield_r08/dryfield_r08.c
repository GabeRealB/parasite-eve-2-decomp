#include "rooms/dryfield_r08.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_121300.h"

#include "gameplay/display.h"
#include "gameplay/area.h"
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
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
/// This room's `glowDrawDisc` stores the on-screen half-extent ahead of the
/// GTE flag word. Defined before `glow_draw.h`, which otherwise selects
/// `GlowCentreScratch`.
#define GLOW_DRAW_DISC_SCRATCH GlowCentreRadiusFirstScratch
#include "../../shared/effects/glow_draw.h"
#include "../../shared/effects/effect_sprite.h"

static WorldCoordRoomLights _gDryfieldR08DefaultRoomLights;
static WorldCoordRoomLights _gDryfieldR08AlternateRoomLights;

static void _dryfieldR08DrawBankedDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);
static void _dryfieldR08DrawAlternateDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);
static void _dryfieldR08DrawLampGlow(const SVECTOR* worldPoint, s32 radiusScale, s32 packedTint);

static WorldCollisionGrid _gDryfieldR08CollisionGrid;

/// World-space glow centres of the twelve lamps, in their shattering order.
///
/// Coordinates are signed integer world units. The cutoff skips a shattered
/// prefix; views 3 and 4 draw the remaining centres. This overlay owns the
/// immutable positions, whose pointers must not survive unloading it.
static const SVECTOR _gDryfieldR08LampGlowPoints[12] SECTION(".data") = {
    { 4950, -1750, 2570, 0 },
    { 5170, -1750, 2570, 0 },
    { 5340, -1750, 2700, 0 },
    { 5410, -1750, 2900, 0 },
    { 5340, -1750, 3100, 0 },
    { 5170, -1750, 3230, 0 },
    { 4950, -1750, 3230, 0 },
    { 4770, -1750, 3100, 0 },
    { 4690, -1750, 2900, 0 },
    { 4770, -1750, 2700, 0 },
    { 4900, -1750, 2900, 0 },
    { 5200, -1750, 2900, 0 },
};

/// World-space centres for the fixed room glows in views 2, 5 and 6.
///
/// Coordinates are signed integer world units. Each view selects its own subset
/// of entries 0..29; all 67 authored records are retained. This overlay owns the
/// immutable table and the drawer borrows each selected point synchronously.
static const SVECTOR _gDryfieldR08RoomGlowPoints[67] SECTION(".data") = {
    { 3360, 200, 3525, 0 },
    { 3880, 200, 3525, 0 },
    { 4600, 200, 3525, 0 },
    { 5235, 200, 3525, 0 },
    { 5855, 200, 3525, 0 },
    { 6475, 200, 3525, 0 },
    { 7100, 200, 3525, 0 },
    { 3360, 200, 2925, 0 },
    { 3880, 200, 2925, 0 },
    { 4600, 200, 2925, 0 },
    { 5235, 200, 2925, 0 },
    { 5855, 200, 2925, 0 },
    { 6475, 200, 2925, 0 },
    { 7100, 200, 2925, 0 },
    { 3360, 200, 2310, 0 },
    { 3880, 200, 2310, 0 },
    { 4600, 200, 2310, 0 },
    { 5235, 200, 2310, 0 },
    { 5855, 200, 2310, 0 },
    { 6475, 200, 2310, 0 },
    { 7100, 200, 2310, 0 },
    { 3885, -2245, 4640, 0 },
    { 4865, -2245, 4640, 0 },
    { 5845, -2245, 4640, 0 },
    { 6825, -2245, 4640, 0 },
    { 3885, -2245, 960, 0 },
    { 4865, -2245, 960, 0 },
    { 5845, -2245, 960, 0 },
    { 6825, -2245, 960, 0 },
    { 2560, -2245, 3670, 0 },
    { 2560, -2245, 2800, 0 },
    { 2560, -2245, 1930, 0 },
    { 8140, -2245, 3670, 0 },
    { 8140, -2245, 2800, 0 },
    { 8140, -2245, 1930, 0 },
    { -150, -1820, 5300, 0 },
    { 460, -1820, 5300, 0 },
    { 360, -2245, 3825, 0 },
    { 720, -2245, 3825, 0 },
    { 360, -2245, 1345, 0 },
    { 720, -2245, 1345, 0 },
    { 390, -2025, 295, 0 },
    { 690, -2025, 295, 0 },
    { 1690, -2130, 1810, 0 },
    { 1690, -2130, 1425, 0 },
    { 2310, -1980, 1625, 0 },
    { 2310, -1980, 1390, 0 },
    { 1875, -1825, 1090, 0 },
    { 2115, -1825, 1090, 0 },
    { 2980, -1765, 335, 0 },
    { 4365, -1765, 335, 0 },
    { 2980, -995, 335, 0 },
    { 4365, -995, 335, 0 },
    { 8560, -1980, 2310, 0 },
    { 8560, -1980, 1925, 0 },
    { 9990, -2300, -5940, 0 },
    { 0x2887, -2300, -5940, 0 },
    { 0x27A1, -2300, -45, 0 },
    { 0x291D, -2300, -45, 0 },
    { 8915, -2720, -4090, 0 },
    { 8915, -2720, -3605, 0 },
    { 8915, -2720, -2125, 0 },
    { 8915, -2720, -1640, 0 },
    { 0x2D50, -2720, -4090, 0 },
    { 0x2D50, -2720, -3605, 0 },
    { 0x2D50, -2720, -2125, 0 },
    { 0x2D50, -2720, -1640, 0 },
};

WorldCollisionRoomResources gDryfieldR08RoomCollisionResources[2] = {
    { .grid = &_gDryfieldR08CollisionGrid, .viewBoundaryTriggers = NULL, .actionTriggers = NULL, .occluders = NULL },
    { .grid = &_gDryfieldR08CollisionGrid, .viewBoundaryTriggers = NULL, .actionTriggers = NULL, .occluders = NULL },
};

u8* gDryfieldR08RoomViewMaps[2] = {
    gViewIdentityMap,
    gViewIdentityMap,
};

ViewCount gDryfieldR08RoomViewCounts[2] = { ARRAY_SIZE(gDryfieldR08ViewCameras), ARRAY_SIZE(gDryfieldR08ViewCameras) };

WorldCoordRoomLighting gDryfieldR08RoomLighting[2] = {
    { .lights = &_gDryfieldR08DefaultRoomLights, .ambientTable = NULL },
    { .lights = &_gDryfieldR08AlternateRoomLights, .ambientTable = NULL },
};

DirectionWarpEntry gDryfieldR08WarpEntries[1] = {
    {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 3360, 0, 2976 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 3360, 0, 2976 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

enum {
    DRYFIELD_R08_COLLISION_CELL_COUNT_X = 4,
    DRYFIELD_R08_COLLISION_CELL_COUNT_Z = 2,
};

/// Room-space collision normals, with 4096 representing unit length.
static SVECTOR _gDryfieldR08CollisionGridNormals[6] = {
#include "assets/dryfield_r08_collision_025D8_normals.inc"
};

/// Collision mesh vertices in signed integer room-coordinate units.
static SVECTOR _gDryfieldR08CollisionGridVerts[59] = {
#include "assets/dryfield_r08_collision_025D8_verts.inc"
};

/// Indexed room collision quads, all using surface class 0.
static WorldCollisionGridFace _gDryfieldR08CollisionGridFaces[31] = {
#include "assets/dryfield_r08_collision_025D8_faces.inc"
};

/// Packed signed face-index lists; each active cell list ends with -1.
static s16 _gDryfieldR08CollisionGridCells[86] = {
#include "assets/dryfield_r08_collision_025D8_cells.inc"
};

// Bind the generated table's halfword offsets to this grid's face-index pool.
// Each constant offset is evaluated once; the binding ends after the include.
/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldR08CollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..85 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_R08_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldR08CollisionGridCells[(halfwordOffset)])
/// X-major cell pointers, indexed by x * cellCountZ + z in the 4 by 2 grid.
static s16* _gDryfieldR08CollisionGridTable[DRYFIELD_R08_COLLISION_CELL_COUNT_X * DRYFIELD_R08_COLLISION_CELL_COUNT_Z] = {
#include "assets/dryfield_r08_collision_025D8_table.inc"
};
#undef DRYFIELD_R08_CELL_FACE_LIST_AT_OFFSET

/// Shared collision mesh for both room variants and its 4 by 2 XZ cell index.
///
/// Cells are 4000 world units wide, with XZ origin (-500, 500). Activation
/// binds this mutable descriptor to the current view coordinate. The overlay
/// owns the descriptor and every mesh/index pool throughout active use.
static WorldCollisionGrid _gDryfieldR08CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldR08CollisionGridNormals,
    .vertices    = _gDryfieldR08CollisionGridVerts,
    .faces       = _gDryfieldR08CollisionGridFaces,
    .cellFaceIds = _gDryfieldR08CollisionGridTable,
    .xBias       = 500,
    .zBias       = -500,
    .cellCountX  = DRYFIELD_R08_COLLISION_CELL_COUNT_X,
    .cellCountZ  = DRYFIELD_R08_COLLISION_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldR08CollisionGridFaces),
};

ViewCamera gDryfieldR08ViewCameras[6] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -5500, 0x5208, -3500 } }, 380 },
    { { { { 108, 0, 4094 }, { 3896, 1258, -103 }, { -1257, 3897, 33 } }, { -5020, 1757, -3014 } }, 230 },
    { { { { 4095, 0, 0 }, { 0, 4017, -796 }, { 0, 796, 4017 } }, { -4888, 1408, -1622 } }, 261 },
    { { { { 15, 0, -4095 }, { 3773, 1592, 14 }, { 1592, -3773, 5 } }, { -4755, 952, -2617 } }, 257 },
    { { { { 3327, 0, 2388 }, { 304, 4062, -423 }, { -2368, 521, 3300 } }, { -7528, 1726, -67 } }, 415 },
    { { { { 108, 0, 4094 }, { 3896, 1258, -103 }, { -1257, 3897, 33 } }, { -5020, 1757, -3014 } }, 230 },
};

// Room sprites use two 8-bit texture pages and a palette at VRAM (0, 255).
// Source RGB uses 128 as neutral modulation for colour-modulated emission.
enum {
    DRYFIELD_R08_SPRITE_TPAGE_X896    = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    DRYFIELD_R08_SPRITE_TPAGE_X960    = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    DRYFIELD_R08_SPRITE_CLUT          = getClut(0, 255),
    DRYFIELD_R08_SPRITE_NEUTRAL_COLOR = 128,
};

/// View 1's empty sprite list, retaining decoded background strips.
///
/// The zero-count first record selects the background; the next record ends
/// the list. Its address also occupies the unused source slot in `gDryfieldR08SpriteViews`.
static SpriteBatch _gDryfieldR08View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// The 85 source rectangles for mapped view 2.
///
/// Positions and sizes are pixels relative to the view centre; UVs are texels.
/// Batches select ranges 0..18 and 19..84. Both frame buffers snapshot geometry
/// and texture state; cached linking reads the source depths each frame.
static SpriteSource _gDryfieldR08View2SpriteSources[85] = {
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, -72, 48, 249, { .fields = { 56, 136 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, 64, 48, 250, { .fields = { 32, 80 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, 72, 64, 243, { .fields = { 32, 88 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, 72, 96, 227, { .fields = { 40, 40 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 24 } }, -80, 24, 238, { .fields = { 0, 200 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 32 } }, 40, 16, 215, { .fields = { 32, 96 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 24 } }, 56, 24, 238, { .fields = { 40, 232 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 24 } }, 16, 16, 204, { .fields = { 16, 232 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -80, 72, 222, { .fields = { 80, 152 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -56, 72, 202, { .fields = { 48, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -32, 72, 190, { .fields = { 48, 32 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -8, 72, 189, { .fields = { 48, 104 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, 16, 72, 190, { .fields = { 56, 136 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, 40, 72, 201, { .fields = { 72, 104 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 24 } }, 64, 72, 227, { .fields = { 0, 24 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 32 } }, -48, 16, 207, { .fields = { 72, 32 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 32 } }, -32, 16, 207, { .fields = { 72, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 24 } }, -16, 16, 201, { .fields = { 8, 72 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 24 } }, 0, 16, 201, { .fields = { 8, 160 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, -80, -112, 314, { .fields = { 88, 192 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, -80, -56, 302, { .fields = { 64, 248 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, -32, 454, { .fields = { 64, 240 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, -72, -16, 471, { .fields = { 88, 200 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 48 } }, -96, -104, 519, { .fields = { 80, 96 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -8, -120, 320, { .fields = { 88, 40 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 24 } }, 16, -120, 322, { .fields = { 16, 48 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 48 } }, 16, -40, 255, { .fields = { 96, 48 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 32 } }, 88, 72, 400, { .fields = { 88, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 16 } }, 32, 16, 212, { .fields = { 112, 240 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 8 } }, -8, -88, 522, { .fields = { 104, 152 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 16 } }, -104, -88, 313, { .fields = { 96, 192 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 16 } }, -32, 16, 205, { .fields = { 72, 240 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 48 } }, 64, -32, 466, { .fields = { 64, 48 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -136, -48, 274, { .fields = { 32, 136 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -112, -48, 274, { .fields = { 24, 32 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -88, -48, 276, { .fields = { 24, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 24 } }, -64, -24, 323, { .fields = { 32, 232 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 40 } }, -56, -32, 254, { .fields = { 32, 136 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 40 } }, -32, -32, 254, { .fields = { 8, 120 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 40, 32 } }, -72, 88, 224, { .fields = { 24, 64 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 32 } }, 112, -72, 275, { .fields = { 56, 200 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 16 } }, -64, -48, 256, { .fields = { 0, 240 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 16 } }, -32, -48, 256, { .fields = { 32, 176 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 40 } }, -160, 40, 252, { .fields = { 0, 80 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 40 } }, -160, 80, 240, { .fields = { 0, 160 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 40 } }, -128, 40, 252, { .fields = { 32, 192 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 40 } }, -128, 80, 240, { .fields = { 32, 96 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -96, 88, 240, { .fields = { 24, 168 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 8 } }, -96, 48, 255, { .fields = { 96, 72 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -96, 56, 246, { .fields = { 8, 96 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 40, 40 } }, -72, 48, 250, { .fields = { 8, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 40, 32 } }, -72, 16, 250, { .fields = { 64, 216 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 40 } }, -40, -88, 310, { .fields = { 96, 80 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 32 } }, -40, -120, 310, { .fields = { 64, 72 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 32 } }, -72, -80, 310, { .fields = { 72, 184 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 40 } }, -72, -120, 310, { .fields = { 104, 200 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 40 } }, 24, -72, 260, { .fields = { 96, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 40 } }, 48, -72, 260, { .fields = { 104, 160 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 56 } }, 48, 16, 251, { .fields = { 104, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 56 } }, 72, 16, 251, { .fields = { 104, 56 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 48 } }, 24, -120, 295, { .fields = { 80, 144 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, -120, 295, { .fields = { 80, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 48 } }, 72, -120, 293, { .fields = { 104, 160 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 48 } }, 96, -120, 293, { .fields = { 104, 112 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 48 } }, 96, -24, 230, { .fields = { 88, 208 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 48 } }, 120, -24, 230, { .fields = { 112, 208 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 40 } }, 96, 24, 219, { .fields = { 112, 40 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 40 } }, 120, 24, 219, { .fields = { 120, 120 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 48 } }, 96, 64, 214, { .fields = { 40, 48 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 48 } }, 120, 64, 214, { .fields = { 48, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 40 } }, 72, -72, 273, { .fields = { 8, 200 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 40 } }, 96, -72, 273, { .fields = { 120, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 48 } }, 24, -32, 254, { .fields = { 64, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 48 } }, 40, -32, 254, { .fields = { 64, 192 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -96, 16, 267, { .fields = { 96, 120 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -96, -16, 267, { .fields = { 48, 168 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, 72, 227, { .fields = { 56, 144 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 48 } }, 72, 72, 226, { .fields = { 64, 96 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 40 } }, -8, -80, 258, { .fields = { 8, 40 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -120, -16, 273, { .fields = { 8, 128 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 24 } }, -120, 16, 262, { .fields = { 40, 232 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 24 } }, -144, 16, 263, { .fields = { 0, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -144, -16, 265, { .fields = { 32, 200 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 8 } }, -40, 8, 252, { .fields = { 64, 64 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 8 } }, -64, 8, 252, { .fields = { 88, 248 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 2's two sprite ranges, independently hiding the first nineteen sources.
///
/// The leading empty record keeps background strips. Ranges cover all 85
/// sources, followed by `SPRITE_BATCH_END`; visibility edits preserve cached
/// packet positions. The two serialized trailing bytes retain unproven roles.
static SpriteBatch _gDryfieldR08View2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, ARRAY_SIZE(_gDryfieldR08View2SpriteSources) - 19, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// The twelve source rectangles for mapped view 3.
///
/// Positions and sizes are view-centred pixels, UVs are texels and depths are
/// unscaled ordering values. One hideable batch covers the whole array.
static SpriteSource _gDryfieldR08View3SpriteSources[12] = {
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 16 } }, -24, 96, 204, { .fields = { 96, 192 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 16 } }, 16, 24, 234, { .fields = { 104, 176 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 16 } }, 24, 96, 204, { .fields = { 96, 160 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 16 } }, -16, 24, 240, { .fields = { 104, 240 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 16 } }, 80, 24, 279, { .fields = { 96, 208 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 24 } }, 88, 40, 239, { .fields = { 96, 136 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 24 } }, 104, 64, 233, { .fields = { 104, 64 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 16 } }, 104, 88, 228, { .fields = { 96, 224 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, 24, 64, 223, { .fields = { 104, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 24 } }, 24, 40, 246, { .fields = { 104, 112 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 24 } }, -16, 40, 246, { .fields = { 104, 88 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -16, 64, 224, { .fields = { 104, 32 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 3's hideable batch of all twelve source rectangles.
///
/// The leading empty record keeps background strips; the final record ends
/// the list. Visibility changes retain both buffers' cached packet positions.
static SpriteBatch _gDryfieldR08View3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldR08View3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// The fourteen lamp-image rectangles for mapped view 4.
///
/// Eleven batches cover the whole array in shattering order, omitting the
/// seventh lamp. Positions and sizes are view-centred pixels, UVs are texels
/// and depths are unscaled ordering values. Geometry is cached in both buffers.
static SpriteSource _gDryfieldR08View4LampSpriteSources[14] = {
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 48 } }, 16, -72, 199, { .fields = { 96, 160 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 40 } }, -16, -64, 199, { .fields = { 56, 80 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 56, 40 } }, -16, 0, 222, { .fields = { 32, 40 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 48, 40 } }, -40, 40, 286, { .fields = { 40, 120 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 40, 48 } }, -88, 48, 244, { .fields = { 88, 112 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 48, 40 } }, -152, 40, 238, { .fields = { 56, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 32 } }, -160, 0, 286, { .fields = { 40, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 56 } }, -160, -120, 188, { .fields = { 104, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 40, 32 } }, -120, -120, 184, { .fields = { 48, 200 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 40, 56 } }, -56, -120, 187, { .fields = { 88, 56 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 40 } }, -16, -120, 187, { .fields = { 72, 160 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 48, 32 } }, -112, -80, 194, { .fields = { 24, 160 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 40, 24 } }, -104, -48, 194, { .fields = { 48, 232 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 40, 48 } }, -97, 0, 242, { .fields = { 88, 208 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 4's eleven hideable lamp batches in shattering order.
///
/// The leading empty record keeps background strips; records 1..11 cover all
/// fourteen sources and exclude the seventh lamp, which has no sprites. The
/// final record ends the list. Hidden batches keep their cached packet positions.
/// Authored trailing bytes are retained; their meaning is unproven.
static SpriteBatch _gDryfieldR08View4LampSpriteBatches[13] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 1, 0 } },
    { 2, 1, 0, 0, { 6, 0 } },
    { 3, 1, 0, 0, { 2, 0 } },
    { 4, 1, 0, 0, { 7, 0 } },
    { 5, 1, 0, 0, { 5, 0 } },
    { 6, 1, 0, 0, { 8, 0 } },
    { 7, 1, 0, 0, { 4, 0 } },
    { 8, 1, 0, 0, { 9, 0 } },
    { 9, 2, 0, 0, { 0, 0 } },
    { 11, 2, 0, 0, { 10, 0 } },
    { 13, 1, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// The eight source rectangles for mapped view 5.
///
/// Positions and sizes are view-centred pixels, UVs are texels and depths are
/// unscaled ordering values. The sole drawing batch covers the whole array.
static SpriteSource _gDryfieldR08View5SpriteSources[8] = {
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 112, 977, { .fields = { 120, 224 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 56, 96 } }, 24, 24, 1250, { .fields = { 8, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 104 } }, 80, 16, 1250, { .fields = { 112, 120 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 56 } }, 96, 16, 1250, { .fields = { 40, 96 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 40, 48 } }, -136, 24, 1250, { .fields = { 16, 200 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 48 } }, -136, 72, 1250, { .fields = { 32, 152 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 56, 104 } }, -96, 16, 1250, { .fields = { 56, 120 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 64, 120 } }, -40, 0, 1250, { .fields = { 64, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 5's batch of all eight source rectangles.
///
/// The leading empty record keeps background strips; the final record ends
/// the list. Both frame buffers cache packets for the complete source range.
static SpriteBatch _gDryfieldR08View5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldR08View5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// The thirty source rectangles for mapped view 6.
///
/// Positions and sizes are view-centred pixels, UVs are texels and depths are
/// unscaled ordering values. The sole drawing batch covers the whole array.
static SpriteSource _gDryfieldR08View6SpriteSources[30] = {
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, -80, -112, 314, { .fields = { 104, 104 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, 8, -96, 530, { .fields = { 104, 112 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, -96, -72, 510, { .fields = { 104, 120 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, -80, -56, 302, { .fields = { 48, 240 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, -64, -8, 465, { .fields = { 48, 248 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 8 } }, -64, 8, 262, { .fields = { 104, 96 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 16 } }, -104, -88, 313, { .fields = { 104, 240 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 48, 72 } }, 88, -24, 228, { .fields = { 56, 96 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 72, 72 } }, -144, -48, 268, { .fields = { 32, 168 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 40 } }, -152, -16, 272, { .fields = { 16, 160 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, -96, -104, 328, { .fields = { 8, 224 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 16 } }, -88, -72, 303, { .fields = { 112, 240 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 144, 128 } }, -56, -8, 237, { .fields = { 112, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 80, 112 } }, -72, -120, 258, { .fields = { 48, 128 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 96, 96 } }, 24, -120, 282, { .fields = { 16, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 32 } }, -72, 16, 245, { .fields = { 0, 0 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 32 } }, -72, 88, 223, { .fields = { 0, 32 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 40 } }, -72, 48, 234, { .fields = { 16, 216 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 16 } }, 8, -24, 254, { .fields = { 88, 240 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 16 } }, 24, -24, 255, { .fields = { 72, 240 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 32, 16 } }, 56, -24, 337, { .fields = { 56, 240 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 32 } }, 8, -80, 258, { .fields = { 120, 224 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 24 } }, 8, -48, 256, { .fields = { 0, 184 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 24 } }, 8, -120, 315, { .fields = { 0, 160 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 48, 64 } }, 96, 48, 210, { .fields = { 8, 96 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 8, 56 } }, 88, 48, 240, { .fields = { 24, 160 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 72, 96 } }, -144, 24, 248, { .fields = { 104, 128 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 32 } }, -160, 24, 259, { .fields = { 0, 64 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X960, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 32 } }, -160, 56, 248, { .fields = { 32, 224 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_R08_SPRITE_TPAGE_X896, DRYFIELD_R08_SPRITE_CLUT, { .fields = { 16, 32 } }, -160, 88, 239, { .fields = { 104, 224 } }, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, DRYFIELD_R08_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 6's batch of all thirty source rectangles.
///
/// The leading empty record keeps background strips; the final record ends
/// the list. Both frame buffers cache packets for the complete source range.
static SpriteBatch _gDryfieldR08View6SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldR08View6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldR08SpriteViews[6] = {
    { .sources = { .empty = _gDryfieldR08View1SpriteBatches }, .batches = _gDryfieldR08View1SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldR08View2SpriteSources }, .batches = _gDryfieldR08View2SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldR08View3SpriteSources }, .batches = _gDryfieldR08View3SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldR08View4LampSpriteSources }, .batches = _gDryfieldR08View4LampSpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldR08View5SpriteSources }, .batches = _gDryfieldR08View5SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldR08View6SpriteSources }, .batches = _gDryfieldR08View6SpriteBatches, .drawAreas = NULL },
};

/// The default lighting bank's white point light, contributing in every room view.
///
/// Position and falloff radii use integer world units; RGB intensities use
/// 12 fractional bits, with each channel initially at three times `ONE`.
/// The loaded room overlay owns this mutable array: coordinate updates parent
/// and compose its transform, and lighting queries overwrite attenuation.
static WorldCoordPointLight _gDryfieldR08DefaultPointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5215, -1041, 3017 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3 * ONE, 3 * ONE, 3 * ONE },
        },
        .inner = 381,
        .outer = 1500,
    },
};

/// The default room-light collection: one white point light.
///
/// Borrows its mutable point-light array, with no directional or cone lights.
/// The room lighting directory publishes this collection while the overlay is
/// loaded; neither the record nor the array may outlive that overlay.
static WorldCoordRoomLights _gDryfieldR08DefaultRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldR08DefaultPointLights),
    .pointLights           = _gDryfieldR08DefaultPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// The alternate lighting bank's two cyan and two red point lights.
///
/// All four accept every room view. Positions and falloff radii use integer
/// world units; RGB intensities have 12 fractional bits. The loaded room
/// overlay owns this mutable array: coordinate updates parent and compose its
/// transforms, and lighting queries overwrite attenuation. Pointers must not
/// survive unloading the overlay.
static WorldCoordPointLight _gDryfieldR08AlternatePointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5781, 225, 2364 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3440, ONE, ONE },
        },
        .inner = 400,
        .outer = 1150,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4644, 204, 2683 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3440, ONE, ONE },
        },
        .inner = 400,
        .outer = 1000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6730, -2042, 2920 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, 409, 163 },
        },
        .inner = 0,
        .outer = 2200,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3860, -2042, 2980 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, 409, 163 },
        },
        .inner = 0,
        .outer = 2700,
    },
};

/// The alternate room-light collection: two cyan and two red point lights.
///
/// Borrows its mutable point-light array, with no directional or cone lights.
/// The room lighting directory publishes this collection while the overlay is
/// loaded; neither the record nor the array may outlive that overlay.
static WorldCoordRoomLights _gDryfieldR08AlternateRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldR08AlternatePointLights),
    .pointLights           = _gDryfieldR08AlternatePointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Placement-entry keys and decimal file numbers for this room's resource recipes.
enum {
    DRYFIELD_R08_RESOURCE_AYA_DOUBLE_SCENE = 132,
    DRYFIELD_R08_FILE_AYA_DOUBLE_SCENE     = 213,
};

/// Aya Double Scene resource recipes for placement variant 1.
///
/// Keys match `gMapDryfieldR08Variant1Placements`; AREA_PLACEMENT_END ends the list.
/// Spawning borrows the room recipes, paired map placements and loaded actor
/// descriptors; retain those overlays through spawning and model-flag updates.
static AreaResource _gDryfieldR08Variant1Resources[2] = {
    { .entryId = DRYFIELD_R08_RESOURCE_AYA_DOUBLE_SCENE, .fileNumber = DRYFIELD_R08_FILE_AYA_DOUBLE_SCENE, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor121300SceneTasks },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldR08AreaVariants[DRYFIELD_R08_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapDryfieldR08Variant1Placements, .resources = _gDryfieldR08Variant1Resources },
};

/// Three retained words with the values of type-1 sound-script requests.
///
/// No C or assembly consumer is established. Their layout also fits
/// `WorldCollisionFootstepSounds`, but their role and subdivision are unproven;
/// the surface policy does not reference them.
static s32 D_dryfield_r08_80180BF0[3] = {
    0x10000011,
    0x10000013,
    0x10000011,
};

/// Blocking policy shared by all eight room-local surface classes.
///
/// Allows weapon impacts and collision pushback, with no footstep sound table.
/// The first byte retains its authored zero; its role is unproven. The class
/// directory borrows this record while the room overlay is loaded.
static WorldCollisionSurfaceProperties _gDryfieldR08DefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gDryfieldR08SurfacePropertiesByClass[8] = {
    &_gDryfieldR08DefaultSurfaceProperties,
    &_gDryfieldR08DefaultSurfaceProperties,
    &_gDryfieldR08DefaultSurfaceProperties,
    &_gDryfieldR08DefaultSurfaceProperties,
    &_gDryfieldR08DefaultSurfaceProperties,
    &_gDryfieldR08DefaultSurfaceProperties,
    &_gDryfieldR08DefaultSurfaceProperties,
    &_gDryfieldR08DefaultSurfaceProperties,
};

/// Number of lamps already shattered, also the first surviving glow-point index.
///
/// Reset to zero on the glow task's first update. Stores the setter's full s32
/// without clamping: callers must supply nonnegative counts, and 12 or greater
/// draws no lamp glows. This state belongs to the loaded room overlay.
static s32 _gDryfieldR08ShatteredLampCount = 0;

void dryfieldR08LampGlowTask(Task* task)
{
    enum {
        DRYFIELD_R08_GLOW_TASK_INITIALIZE = 0,
        DRYFIELD_R08_ROOM_GLOW_SIZE       = 0x200,
        DRYFIELD_R08_LAMP_GLOW_SIZE       = 0xA0,
        DRYFIELD_R08_ROOM_GLOW_WHITE      = 0x444,
        DRYFIELD_R08_ROOM_GLOW_RED        = 0x400,
        DRYFIELD_R08_ROOM_GLOW_WARM       = 0x433,
        DRYFIELD_R08_LAMP_GLOW_TINT       = 0x3888
    };
    s32 lampIndex;
    u8  mappedView;

    // Reset the shared shattering cutoff before this task's first draw.
    if (task->state == DRYFIELD_R08_GLOW_TASK_INITIALIZE) {
        _gDryfieldR08ShatteredLampCount = 0;
        task->state                     = task->state + 1;
    }

    mappedView = viewGetMappedIndex();
    switch (mappedView) {
        case 2: {
            const SVECTOR* roomGlowPoints;

            roomGlowPoints = _gDryfieldR08RoomGlowPoints;
            glowDrawDisc(&roomGlowPoints[0], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WHITE);
            glowDrawDisc(&roomGlowPoints[2], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WHITE);
            glowDrawDisc(&roomGlowPoints[3], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WHITE);
            glowDrawDisc(&roomGlowPoints[6], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WHITE);
            glowDrawDisc(&roomGlowPoints[14], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WHITE);
            glowDrawDisc(&roomGlowPoints[15], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WHITE);
            glowDrawDisc(&roomGlowPoints[16], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WHITE);
            glowDrawDisc(&roomGlowPoints[17], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WHITE);
            break;
        }
        case 3:
            for (lampIndex = _gDryfieldR08ShatteredLampCount; lampIndex < (s32)ARRAY_SIZE(_gDryfieldR08LampGlowPoints); lampIndex++) {
                _dryfieldR08DrawLampGlow(&_gDryfieldR08LampGlowPoints[lampIndex], DRYFIELD_R08_LAMP_GLOW_SIZE, DRYFIELD_R08_LAMP_GLOW_TINT);
            }
            break;
        case 4:
            for (lampIndex = _gDryfieldR08ShatteredLampCount; lampIndex < (s32)ARRAY_SIZE(_gDryfieldR08LampGlowPoints); lampIndex++) {
                _dryfieldR08DrawLampGlow(&_gDryfieldR08LampGlowPoints[lampIndex], DRYFIELD_R08_LAMP_GLOW_SIZE, DRYFIELD_R08_LAMP_GLOW_TINT);
            }
            break;
        case 5: {
            const SVECTOR* roomGlowPoints;

            roomGlowPoints = _gDryfieldR08RoomGlowPoints;
            glowDrawDisc(&roomGlowPoints[0], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WHITE);
            glowDrawDisc(&roomGlowPoints[1], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WHITE);
            glowDrawDisc(&roomGlowPoints[2], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WHITE);
            glowDrawDisc(&roomGlowPoints[7], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WHITE);
            glowDrawDisc(&roomGlowPoints[8], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WHITE);
            glowDrawDisc(&roomGlowPoints[21], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_RED);
            glowDrawDisc(&roomGlowPoints[22], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_RED);
            glowDrawDisc(&roomGlowPoints[23], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_RED);
            glowDrawDisc(&roomGlowPoints[29], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_RED);
            break;
        }
        case 6: {
            const SVECTOR* roomGlowPoints;

            roomGlowPoints = _gDryfieldR08RoomGlowPoints;
            glowDrawDisc(&roomGlowPoints[0], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WARM);
            glowDrawDisc(&roomGlowPoints[2], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WARM);
            glowDrawDisc(&roomGlowPoints[3], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WARM);
            glowDrawDisc(&roomGlowPoints[6], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WARM);
            glowDrawDisc(&roomGlowPoints[14], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WARM);
            glowDrawDisc(&roomGlowPoints[15], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WARM);
            glowDrawDisc(&roomGlowPoints[16], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WARM);
            glowDrawDisc(&roomGlowPoints[17], DRYFIELD_R08_ROOM_GLOW_SIZE, DRYFIELD_R08_ROOM_GLOW_WARM);
            break;
        }
    }
}

/// Writes the first drift-sprite corner offset and returns its perspective numerator.
///
/// `projection` borrows a live workspace with positive `depth`; only its corner
/// offsets change. The return is `sizeFactor * 47`, shared by both corner pairs;
/// dividing it by depth gives the signed pixel half-diagonal, truncated toward
/// zero. Rotated products must fit s32; their arithmetic shift rounds down.
///
/// `cornerAngle` uses 4096 units per turn. X points right and Y points up, so a
/// positive half-diagonal at zero angle points up and at a quarter turn points
/// right. The drawer subtracts Y from the screen centre. No pointer is retained.
static inline s32 _dryfieldR08BeginDriftCorners(EffectBillboardScratch* projection, s16 sizeFactor, s32 cornerAngle)
{
    enum {
        DRYFIELD_R08_DRIFT_PERSPECTIVE_SCALE  = 47, // Multiplier in the depth-divided half-diagonal numerator
        DRYFIELD_R08_DRIFT_TRIG_FRACTION_BITS = 12  // Fractional bits in rsin/rcos samples; 4096 represents 1.0
    };
    s32 trigSample;
    s32 perspectiveNumerator;
    s32 halfDiagonalPixels;

    trigSample                = rsin(cornerAngle);
    perspectiveNumerator      = sizeFactor * DRYFIELD_R08_DRIFT_PERSPECTIVE_SCALE;
    halfDiagonalPixels        = perspectiveNumerator / projection->depth;
    projection->cornerOffsetX = (halfDiagonalPixels * trigSample) >> DRYFIELD_R08_DRIFT_TRIG_FRACTION_BITS;
    trigSample                = rcos(cornerAngle);
    halfDiagonalPixels        = perspectiveNumerator / projection->depth;
    projection->cornerOffsetY = (halfDiagonalPixels * trigSample) >> DRYFIELD_R08_DRIFT_TRIG_FRACTION_BITS;
    return perspectiveNumerator;
}

/// Writes a drift-sprite corner offset at an absolute screen-space angle.
///
/// `perspectiveNumerator` is the signed `sizeFactor * 47` returned by
/// `_dryfieldR08BeginDriftCorners`. The borrowed workspace requires positive
/// depth; division gives the pixel half-diagonal, truncated toward zero, before
/// Q12 rotation. Products must fit s32; their arithmetic shift rounds down.
///
/// Only the corner offsets change. `cornerAngle` uses 4096 units per turn with
/// X right and Y up; the drawer supplies the first angle plus a quarter turn
/// for the second pair of opposite corners. No pointer is retained.
static inline void _dryfieldR08RotateDriftCorner(EffectBillboardScratch* projection, s32 perspectiveNumerator, s32 cornerAngle)
{
    enum { DRYFIELD_R08_DRIFT_TRIG_FRACTION_BITS = 12 }; // rsin/rcos fractional bits
    s32 halfDiagonalPixels;
    s32 trigSample;

    trigSample                = rsin(cornerAngle);
    halfDiagonalPixels        = perspectiveNumerator / projection->depth;
    projection->cornerOffsetX = (halfDiagonalPixels * trigSample) >> DRYFIELD_R08_DRIFT_TRIG_FRACTION_BITS;
    trigSample                = rcos(cornerAngle);
    halfDiagonalPixels        = perspectiveNumerator / projection->depth;
    projection->cornerOffsetY = (halfDiagonalPixels * trigSample) >> DRYFIELD_R08_DRIFT_TRIG_FRACTION_BITS;
}

/// Binds Dryfield R08's exported `void (Task*)` callback for this drift instance.
///
/// Function-identifier alias supplied before the fragment and cleared after it.
/// Used only as the definition name; no arguments, captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_TASK dryfieldR08SpriteDriftTask
/// Binds the twelve-frame drift drawer: `(const GfxCoord*, u16 frameAndPalette, s16 size, s16 angle)`.
///
/// Function-identifier alias supplied before the fragment and cleared after it.
/// Arguments are evaluated once; the coordinate is borrowed, size is a perspective
/// numerator, and angle uses 4096 units per turn. No captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_DRAW_BANKED _dryfieldR08DrawBankedDriftSprite
/// Binds the ten-frame drift drawer with the same coordinate, packed-frame, size and angle contract.
///
/// Function-identifier alias supplied before the fragment and cleared after it.
/// Arguments are evaluated once; the coordinate is borrowed, size is a perspective
/// numerator, and angle uses 4096 units per turn. No captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_DRAW_ALTERNATE _dryfieldR08DrawAlternateDriftSprite
/// Uses the banked drawer without the saved palette selector while sprite updates are suspended.
///
/// Define as 1 before including `effect_sprite_drift.inc.c`; 0 or undefined
/// retains the usual drawer and palette selection. Dryfield R08 enables this
/// override for every non-running effect-control value, including the final
/// redraw before cancellation, regardless of the task's drawer state. The cell
/// index, scale and rotation are retained; the banked drawer receives palette selector 0.
/// This takes precedence over `EFFECT_SPRITE_DRIFT_SIGN_BANK` for suspended draws
/// and is undefined at the end of the fragment.
#define EFFECT_SPRITE_DRIFT_PAUSED_DRAW_A_UNBANKED 1
#include "../../shared/effects/effect_sprite_drift.inc.c"

/// Draws a rotating twelve-cell sprite with a per-cell or alternate palette.
///
/// `coord->workm` must be composed for `GsWSMATRIX`; only its translation,
/// narrowed to s16, is projected. `frameAndPalette` packs cell 0..11 in bits
/// 0..11 and a palette selector in bits 12..15: 0/1 use per-cell CLUT rows
/// 270/271, 2..15 use CLUT 0x428F. The five-column 48-texel sheet starts at
/// V=104 on texture page 0x2B; GPU byte UVs wrap across V=255.
///
/// `size * 47 / depth` is the signed pixel half-diagonal before Q12 rotation;
/// `angle` uses 4096 units per turn. Accepted projections require nonzero
/// SZ3/4 depth. A nonnegative GTE FLAG queues one additive raw-texture FT4,
/// requiring packet space at the primitive cursor. One initialized scratch
/// stack block is borrowed and released on every path; no pointer is retained.
/// A packet is reserved even when the projection is rejected.
static void _dryfieldR08DrawBankedDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle)
{
    enum {
        // Signed row offsets wrap to GPU UV bytes; last - first is 47 modulo 256.
        DRYFIELD_R08_DRIFT_FIRST_TEXEL_ROW     = 104,
        DRYFIELD_R08_DRIFT_LAST_TEXEL_ROW      = -105,
        DRYFIELD_R08_DRIFT_CLUT_ROW_SHIFT      = 6,
        DRYFIELD_R08_DRIFT_PALETTE_ROW_COUNT   = 2,
        DRYFIELD_R08_DRIFT_FRAME_MASK          = 0xFFF,
        DRYFIELD_R08_DRIFT_PALETTE_SHIFT       = 12,
        DRYFIELD_R08_DRIFT_CELLS_PER_ROW       = 5,
        DRYFIELD_R08_DRIFT_CELL_PITCH_TEXELS   = 48,
        DRYFIELD_R08_DRIFT_UV_SPAN_TEXELS      = 47,
        DRYFIELD_R08_DRIFT_QUARTER_TURN        = 0x400,
        DRYFIELD_R08_DRIFT_PACKET_WORDS        = sizeof(POLY_FT4) / sizeof(u32) - 1,
        DRYFIELD_R08_DRIFT_TEXTURED_QUAD_CODE  = 0x2C,
        DRYFIELD_R08_DRIFT_RAW_SEMITRANSPARENT = 3,
        DRYFIELD_R08_DRIFT_TEXTURE_PAGE        = 0x2B,
        DRYFIELD_R08_DRIFT_ALTERNATE_CLUT      = 0x428F,
        DRYFIELD_R08_DRIFT_FIRST_PALETTE_ROW   = 0x10E,
        DRYFIELD_R08_DRIFT_CLUT_COLUMN_MASK    = 0x3F
    };
    EffectBillboardScratch* projection;
    POLY_FT4*               quad;
    s32                     cornerAngle;
    s32                     perpendicularAngle;
    s32                     perspectiveNumerator;
    s32                     cellU;
    s32                     cellV;
    s32                     lastU;
    s32                     lastV;
    s32                     frameIndex;
    u16                     paletteSelector;

    projection                = SCRATCH_STACK_RESERVE_BLOCK(EffectBillboardScratch);
    projection->worldPoint.vx = coord->workm.t[0];
    projection->worldPoint.vy = coord->workm.t[1];
    projection->worldPoint.vz = coord->workm.t[2];
    frameIndex                = frameAndPalette & DRYFIELD_R08_DRIFT_FRAME_MASK;
    paletteSelector           = frameAndPalette >> DRYFIELD_R08_DRIFT_PALETTE_SHIFT;
    // Project the composed centre; the quad rotates only in screen space.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setlen(quad, DRYFIELD_R08_DRIFT_PACKET_WORDS);
    setcode(quad, DRYFIELD_R08_DRIFT_TEXTURED_QUAD_CODE);
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        quad->tpage = DRYFIELD_R08_DRIFT_TEXTURE_PAGE;
        quad->code |= DRYFIELD_R08_DRIFT_RAW_SEMITRANSPARENT;
        if (paletteSelector >= DRYFIELD_R08_DRIFT_PALETTE_ROW_COUNT) {
            quad->clut = DRYFIELD_R08_DRIFT_ALTERNATE_CLUT;
        } else {
            quad->clut = ((paletteSelector + DRYFIELD_R08_DRIFT_FIRST_PALETTE_ROW) << DRYFIELD_R08_DRIFT_CLUT_ROW_SHIFT) | (frameIndex & DRYFIELD_R08_DRIFT_CLUT_COLUMN_MASK);
        }
        cornerAngle = angle;
        cellU       = ((u16)frameIndex % DRYFIELD_R08_DRIFT_CELLS_PER_ROW) * DRYFIELD_R08_DRIFT_CELL_PITCH_TEXELS;
        cellV       = ((u16)frameIndex / DRYFIELD_R08_DRIFT_CELLS_PER_ROW) * DRYFIELD_R08_DRIFT_CELL_PITCH_TEXELS;
        lastU       = cellU + DRYFIELD_R08_DRIFT_UV_SPAN_TEXELS;
        lastV       = cellV + DRYFIELD_R08_DRIFT_LAST_TEXEL_ROW;
        cellV       = cellV + DRYFIELD_R08_DRIFT_FIRST_TEXEL_ROW;
        setUV4(quad, cellU, cellV, lastU, cellV, cellU, lastV, lastU, lastV);
        perspectiveNumerator = _dryfieldR08BeginDriftCorners(projection, size, cornerAngle);
        quad->x0             = projection->screenX + (u16)projection->cornerOffsetX;
        quad->x3             = projection->screenX - (u16)projection->cornerOffsetX;
        quad->y0             = projection->screenY - (u16)projection->cornerOffsetY;
        quad->y3             = projection->screenY + (u16)projection->cornerOffsetY;
        perpendicularAngle   = cornerAngle + DRYFIELD_R08_DRIFT_QUARTER_TURN;
        _dryfieldR08RotateDriftCorner(projection, perspectiveNumerator, perpendicularAngle);
        quad->x1 = projection->screenX + (u16)projection->cornerOffsetX;
        quad->x2 = projection->screenX - (u16)projection->cornerOffsetX;
        quad->y1 = projection->screenY - (u16)projection->cornerOffsetY;
        quad->y2 = projection->screenY + (u16)projection->cornerOffsetY;
        // Sort by projection depth; the GPU consumes a complete textured-quad packet.
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBillboardScratch);
}

/// Draws a rotating ten-cell sprite with a base or alternate palette.
///
/// `coord->workm` must be composed for `GsWSMATRIX`; its translation is
/// narrowed to s16. Bits 0..11 of `frameAndPalette` hold cell 0..9; any bit
/// in 12..15 selects CLUT 0x428F instead of 0x43D0. The five-column sheet
/// has 48-texel cells starting at V=128 on texture page 0x2C.
///
/// `size * 47 / depth` is the signed pixel half-diagonal before Q12 rotation;
/// `angle` uses 4096 units per turn. Accepted projections require nonzero
/// SZ3/4 depth. A nonnegative GTE FLAG queues an additive raw-texture FT4.
/// The initialized scratch stack and primitive cursor must provide one block
/// and one packet; scratch is released on every path and no pointer is retained.
/// A packet is reserved even when the projection is rejected.
static void _dryfieldR08DrawAlternateDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle)
{
    enum {
        // Signed row offsets wrap to GPU UV bytes; last - first is 47 modulo 256.
        DRYFIELD_R08_DRIFT_FIRST_TEXEL_ROW     = -128,
        DRYFIELD_R08_DRIFT_LAST_TEXEL_ROW      = -81,
        DRYFIELD_R08_DRIFT_FRAME_MASK          = 0xFFF,
        DRYFIELD_R08_DRIFT_PALETTE_SHIFT       = 12,
        DRYFIELD_R08_DRIFT_CELLS_PER_ROW       = 5,
        DRYFIELD_R08_DRIFT_CELL_PITCH_TEXELS   = 48,
        DRYFIELD_R08_DRIFT_UV_SPAN_TEXELS      = 47,
        DRYFIELD_R08_DRIFT_QUARTER_TURN        = 0x400,
        DRYFIELD_R08_DRIFT_PACKET_WORDS        = sizeof(POLY_FT4) / sizeof(u32) - 1,
        DRYFIELD_R08_DRIFT_TEXTURED_QUAD_CODE  = 0x2C,
        DRYFIELD_R08_DRIFT_RAW_SEMITRANSPARENT = 3,
        DRYFIELD_R08_DRIFT_TEXTURE_PAGE        = 0x2C,
        DRYFIELD_R08_DRIFT_ALTERNATE_CLUT      = 0x428F,
        DRYFIELD_R08_DRIFT_BASE_CLUT           = 0x43D0
    };
    EffectBillboardScratch* projection;
    POLY_FT4*               quad;
    s32                     cornerAngle;
    s32                     perpendicularAngle;
    s32                     perspectiveNumerator;
    s32                     cellU;
    s32                     cellV;
    s32                     lastU;
    s32                     lastV;
    u16                     frameIndex;
    u16                     paletteSelector;

    projection                = SCRATCH_STACK_RESERVE_BLOCK(EffectBillboardScratch);
    projection->worldPoint.vx = coord->workm.t[0];
    paletteSelector           = frameAndPalette >> DRYFIELD_R08_DRIFT_PALETTE_SHIFT;
    projection->worldPoint.vy = coord->workm.t[1];
    projection->worldPoint.vz = coord->workm.t[2];
    frameIndex                = frameAndPalette & DRYFIELD_R08_DRIFT_FRAME_MASK;
    // Project the composed centre; the quad rotates only in screen space.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setlen(quad, DRYFIELD_R08_DRIFT_PACKET_WORDS);
    setcode(quad, DRYFIELD_R08_DRIFT_TEXTURED_QUAD_CODE);
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        quad->tpage = DRYFIELD_R08_DRIFT_TEXTURE_PAGE;
        quad->code |= DRYFIELD_R08_DRIFT_RAW_SEMITRANSPARENT;
        if (paletteSelector) {
            quad->clut = DRYFIELD_R08_DRIFT_ALTERNATE_CLUT;
        } else {
            quad->clut = DRYFIELD_R08_DRIFT_BASE_CLUT;
        }
        cornerAngle = angle;
        cellU       = (frameIndex % DRYFIELD_R08_DRIFT_CELLS_PER_ROW) * DRYFIELD_R08_DRIFT_CELL_PITCH_TEXELS;
        cellV       = (frameIndex / DRYFIELD_R08_DRIFT_CELLS_PER_ROW) * DRYFIELD_R08_DRIFT_CELL_PITCH_TEXELS;
        lastU       = cellU + DRYFIELD_R08_DRIFT_UV_SPAN_TEXELS;
        lastV       = cellV + DRYFIELD_R08_DRIFT_LAST_TEXEL_ROW;
        cellV       = cellV + DRYFIELD_R08_DRIFT_FIRST_TEXEL_ROW;
        setUV4(quad, cellU, cellV, lastU, cellV, cellU, lastV, lastU, lastV);
        perspectiveNumerator = _dryfieldR08BeginDriftCorners(projection, size, cornerAngle);
        quad->x0             = projection->screenX + projection->cornerOffsetX;
        quad->x3             = projection->screenX - projection->cornerOffsetX;
        quad->y0             = projection->screenY - projection->cornerOffsetY;
        quad->y3             = projection->screenY + projection->cornerOffsetY;
        perpendicularAngle   = cornerAngle + DRYFIELD_R08_DRIFT_QUARTER_TURN;
        _dryfieldR08RotateDriftCorner(projection, perspectiveNumerator, perpendicularAngle);
        quad->x1 = projection->screenX + projection->cornerOffsetX;
        quad->x2 = projection->screenX - projection->cornerOffsetX;
        quad->y1 = projection->screenY - projection->cornerOffsetY;
        quad->y2 = projection->screenY + projection->cornerOffsetY;
        // Sort by projection depth; the GPU consumes a complete textured-quad packet.
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBillboardScratch);
}

#include "../../shared/effects/glow_draw_disc.inc.c"

/// Initializes the colour gradient of a lamp-glow disc wedge or ray.
///
/// Borrows one word-aligned writable `POLY_G4` without retaining it. Vertex 2
/// receives the RGB bytes; vertices 0, 1 and 3 are black. Wider colour arguments
/// wrap modulo 256 at the call boundary. Sets the eight-word payload length and
/// opaque Gouraud-quad command; the caller supplies screen coordinates,
/// ordering-table linkage and additive blend setup.
static inline void _dryfieldR08InitLampGlowWedge(POLY_G4* wedge, u8 red, u8 green, u8 blue)
{
    setPolyG4(wedge);
    setRGB0(wedge, 0, 0, 0);
    setRGB1(wedge, 0, 0, 0);
    setRGB2(wedge, red, green, blue);
    setRGB3(wedge, 0, 0, 0);
}

/// Draws a flickering additive lamp disc with four overlaid cross rays.
///
/// Borrows a world point for one view-matrix projection. The signed low
/// halfword of `radiusScale` gives outer and inner pixel radii of size * 64 /
/// depth and size * 8 / depth, with depth equal to camera Z / 4. Rejects
/// negative GTE flags; an accepted projection must have nonzero depth.
/// `packedTint` bits 8..11, 4..7 and 0..3 are RGB nibbles scaled by 16;
/// bits 12..15 select an odd-frame addition of 1 << shift, with shift 0..7.
/// Colour bytes wrap before halving for the rays. Alternating ray tips extend
/// to the outer radius and twice it; all shoulders use the inner radius.
/// Requires initialized scratch and frame packet/ordering-table space for
/// twelve quads plus additive blend commands; queued packets live through GPU use.
static void _dryfieldR08DrawLampGlow(const SVECTOR* worldPoint, s32 radiusScale, s32 packedTint)
{
    RoomDiscScratch* projection;
    POLY_G4*         wedge;
    s32              signedRadiusScale;
    s32              angle;
    s32              halfStepAngle;
    s32              nextAngle;
    s32              previousShoulderAngle;
    s32              nextShoulderAngle;
    s32              oppositeAngle;
    s32              animationFrame;
    s32              shiftedTint;
    s32              flickerBoost;
    s32              red;
    s32              green;
    s32              blue;
    s32              outerRadiusPixels;
    s32              innerRadiusPixels;

    projection = SCRATCH_STACK_RESERVE_BLOCK(RoomDiscScratch);

    // Project once; every wedge shares the centre and unbiased sorting depth.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&projection->sx);
    gte_stflg(&projection->flag);
    if (projection->flag >= 0) {
        gte_stszotz(&projection->otz);
        signedRadiusScale       = (s16)radiusScale;
        outerRadiusPixels       = (signedRadiusScale * GLOW_RADIUS_SCALE) / projection->otz;
        animationFrame          = gDisplayState.animFrame;
        projection->outerRadius = outerRadiusPixels;
        innerRadiusPixels       = (signedRadiusScale * GLOW_INNER_RADIUS_SCALE) / projection->otz;
        angle                   = 0;
        shiftedTint             = packedTint << 16;
        flickerBoost            = (animationFrame & 1) << (shiftedTint >> 28);
        red                     = flickerBoost + ((shiftedTint >> 20) & 0xF0);
        green                   = flickerBoost + ((shiftedTint >> 16) & 0xF0);
        blue                    = flickerBoost + ((packedTint & 0xF) << 4);
        projection->innerRadius = innerRadiusPixels;
        // Build an eight-wedge disc with a full-bright centre and black rim.
        do {
            wedge          = gGpuPrimCursor;
            gGpuPrimCursor = wedge + 1;
            _dryfieldR08InitLampGlowWedge(wedge, red, green, blue);
            wedge->x0     = projection->sx + ((projection->outerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            halfStepAngle = angle + GLOW_SIXTEENTH_TURN;
            wedge->y0     = projection->sy + ((projection->outerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            wedge->x1     = projection->sx + ((projection->outerRadius * rsin(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            wedge->y1     = projection->sy + ((projection->outerRadius * rcos(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            nextAngle     = angle + GLOW_EIGHTH_TURN;
            wedge->x2     = projection->sx;
            wedge->y2     = projection->sy;
            wedge->x3     = projection->sx + ((projection->outerRadius * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
            wedge->y3     = projection->sy + ((projection->outerRadius * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
            angle         = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->otz),
                    wedge);
            gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, projection->otz);
        } while (angle < GLOW_FULL_TURN);

        // Overlay four half-bright rays with alternating normal and doubled tips.
        red   = (u8)red >> 1;
        green = (u8)green >> 1;
        blue  = (u8)blue >> 1;
        angle = GLOW_EIGHTH_TURN;
        do {
            previousShoulderAngle = angle - GLOW_QUARTER_TURN;
            wedge                 = gGpuPrimCursor;
            gGpuPrimCursor        = wedge + 1;
            _dryfieldR08InitLampGlowWedge(wedge, red, green, blue);
            wedge->x0         = projection->sx + ((projection->innerRadius * rsin(previousShoulderAngle)) >> GLOW_TRIG_SHIFT);
            wedge->y0         = projection->sy + ((projection->innerRadius * rcos(previousShoulderAngle)) >> GLOW_TRIG_SHIFT);
            wedge->x1         = projection->sx + ((projection->outerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            wedge->y1         = projection->sy + ((projection->outerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            nextShoulderAngle = angle + GLOW_QUARTER_TURN;
            wedge->x2         = projection->sx;
            wedge->y2         = projection->sy;
            wedge->x3         = projection->sx + ((projection->innerRadius * rsin(nextShoulderAngle)) >> GLOW_TRIG_SHIFT);
            wedge->y3         = projection->sy + ((projection->innerRadius * rcos(nextShoulderAngle)) >> GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->otz),
                    wedge);
            gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, projection->otz);

            wedge          = gGpuPrimCursor;
            gGpuPrimCursor = wedge + 1;
            _dryfieldR08InitLampGlowWedge(wedge, red, green, blue);
            wedge->x0     = projection->sx + ((projection->innerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            wedge->y0     = projection->sy + ((projection->innerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            wedge->x1     = projection->sx + ((projection->outerRadius * rsin(nextShoulderAngle)) >> (GLOW_TRIG_SHIFT - 1));
            wedge->y1     = projection->sy + ((projection->outerRadius * rcos(nextShoulderAngle)) >> (GLOW_TRIG_SHIFT - 1));
            oppositeAngle = angle + GLOW_HALF_TURN;
            wedge->x2     = projection->sx;
            wedge->y2     = projection->sy;
            wedge->x3     = projection->sx + ((projection->innerRadius * rsin(oppositeAngle)) >> GLOW_TRIG_SHIFT);
            wedge->y3     = projection->sy + ((projection->innerRadius * rcos(oppositeAngle)) >> GLOW_TRIG_SHIFT);
            angle         = oppositeAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->otz),
                    wedge);
            gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, projection->otz);
        } while (angle < GLOW_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomDiscScratch);
}

void dryfieldR08SetShatteredLampCount(s32 shatteredLampCount)
{
    _gDryfieldR08ShatteredLampCount = shatteredLampCount;
}

void dryfieldR08SetLampSpritesHidden(u8 lampSpriteIndex, u8 hidden)
{
    // Exclude the leading empty batch and the terminal batch from lamp indices.
    enum { DRYFIELD_R08_LAMP_SPRITE_COUNT = ARRAY_SIZE(_gDryfieldR08View4LampSpriteBatches) - 2 };
    GameLocationKey* location;
    SpriteBatch*     batches;

    location = &gGameSession->location;
    if (lampSpriteIndex < DRYFIELD_R08_LAMP_SPRITE_COUNT) {
        batches = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1][3].batches;
        if (hidden) {
            batches[lampSpriteIndex + 1].hidden = true;
            return;
        }
        batches[lampSpriteIndex + 1].hidden = false;
    }
}

/// Shows or hides batch 1 in one of the room's second and third sprite views.
///
/// Selector 0 uses view array entry 1; selectors 1 and 2 use entry 2; values
/// 3..255 do nothing. Zero `hidden` shows the batch; every nonzero byte hides
/// it. Requires the current stage/area directory to select loaded Dryfield R08
/// view records. Mutates their borrowed batch list; no pointers are retained.
static void _dryfieldR08SetViewSpriteBatchHidden(u8 viewSelector, u8 hidden)
{
    enum { DRYFIELD_R08_SPRITE_VIEW_SELECTOR_COUNT = 3 };
    GameLocationKey* location;
    SpriteView*      views;
    SpriteBatch*     batches;

    location = &gGameSession->location;
    if (viewSelector < DRYFIELD_R08_SPRITE_VIEW_SELECTOR_COUNT) {
        views = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1];
        if (viewSelector == 0) {
            batches = views[1].batches;
        } else {
            batches = views[2].batches;
        }
        if (hidden) {
            batches[1].hidden = true;
            return;
        }
        batches[1].hidden = false;
    }
}

void dryfieldR08SelectLightingBank(s16 useAlternate)
{
    if (useAlternate == 0) {
        gDryfieldR08RoomLighting[0].lights = &_gDryfieldR08DefaultRoomLights;
        return;
    }
    gDryfieldR08RoomLighting[0].lights = &_gDryfieldR08AlternateRoomLights;
}
