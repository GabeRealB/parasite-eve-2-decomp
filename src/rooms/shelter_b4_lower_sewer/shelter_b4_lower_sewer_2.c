#include "rooms/shelter_b4_lower_sewer.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b4_lower_sewer_private.h"

#include "actors/actor_04400.h"
#include "actors/actor_00400.h"
#include "actors/actor_01100.h"
#include "actors/actor_02000.h"
#include "actors/actor_02300.h"
#include "actors/actor_207200.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
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
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"

/// Empty presence flag so `water_effects.h` declares the shared
/// `_waterDrawSpinU16` and `_waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
#include "../../shared/effects/water_effects.h"

/// Fifteen world-space endpoint pairs for the lower sewer's capsule glows.
///
/// Points 0..27 describe fourteen horizontal spans at Y=-5090; the last pair
/// is at X=-12930, Y=-1800. Consecutive even/odd points form one capsule.
/// Mapped views select overlapping ranges with starts at points 0, 12 and 14;
/// all accesses stay in this single 30-point pool. Storage lasts with the room.
static SVECTOR _gShelterB4LowerSewerGlowEndpoints[30] = {
    { -0x3B42, -5090, 2290, 0 },
    { -0x3692, -5090, 2290, 0 },
    { -0x2B8E, -5090, 2290, 0 },
    { -9950, -5090, 2290, 0 },
    { -7780, -5090, 2290, 0 },
    { -6580, -5090, 2290, 0 },
    { -4250, -5090, 2290, 0 },
    { -3050, -5090, 2290, 0 },
    { -780, -5090, 2290, 0 },
    { -420, -5090, 2290, 0 },
    { 2730, -5090, 2290, 0 },
    { 3930, -5090, 2290, 0 },
    { 6730, -5090, 2290, 0 },
    { 7930, -5090, 2290, 0 },
    { 0x29EA, -5090, 2290, 0 },
    { 0x2E9A, -5090, 2290, 0 },
    { 0x2E9A, -5090, -1290, 0 },
    { 0x29EA, -5090, -1290, 0 },
    { 7930, -5090, -1290, 0 },
    { 6730, -5090, -1290, 0 },
    { 3930, -5090, -1290, 0 },
    { 2730, -5090, -1290, 0 },
    { 420, -5090, -1290, 0 },
    { -780, -5090, -1290, 0 },
    { -3050, -5090, -1290, 0 },
    { -4250, -5090, -1290, 0 },
    { -6580, -5090, -1290, 0 },
    { -7780, -5090, -1290, 0 },
    { -0x3282, -1800, 370, 0 },
    { -0x3282, -1800, 1130, 0 },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

u8* gShelterB4LowerSewerViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB4LowerSewerRoomViewCounts[1] = { 9 };

/// Arrival and departure requests from the lower sewer's area sound bank.
enum {
    SHELTER_B4_LOWER_SEWER_WARP2_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_LOWER_SEWER, 2),
    SHELTER_B4_LOWER_SEWER_WARP2_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_LOWER_SEWER, 1),
    SHELTER_B4_LOWER_SEWER_WARP4_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_LOWER_SEWER, 6),
    SHELTER_B4_LOWER_SEWER_WARP4_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_LOWER_SEWER, 5)
};

DirectionWarpEntry gShelterB4LowerSewerWarpEntries[4] = {
    { { { .word = 1024 }, -7821, 0, 2453 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -7800, 0, 1620 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_RESERVOIR_1BF },
    { { { .word = 3072 }, 0x3075, 0, 1820 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x3075, 0, 1180 }, { 0, 0, 0, 0 }, SHELTER_B4_LOWER_SEWER_WARP2_ARRIVAL_SOUND, SHELTER_B4_LOWER_SEWER_WARP2_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -6000, -2000, -1180 }, { 0, 0, 0, 0 }, { { .word = 768 }, -5400, -2000, -400 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 9, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, 0x37DC, -2000, -410 }, { 0, 0, 0, 0 }, { { .word = 3328 }, 0x3908, -2000, -1400 }, { 0, 0, 0, 0 }, SHELTER_B4_LOWER_SEWER_WARP4_ARRIVAL_SOUND, SHELTER_B4_LOWER_SEWER_WARP4_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Eight-point Bog Diver patrol ring at Z=1000, borrowed by selector slots 0 and 2.
///
/// XYZ use room-coordinate units; swimming goal Y adds the sampled water height
/// to the unsigned waypoint Y. The ring wraps after entry 7 and has no terminator.
static SVECTOR _gShelterB4LowerSewerDiverPatrolWaypoints1[8] = {
    { 8256, 1000, 1000, 0 },
    { 3584, 1000, 1000, 0 },
    { -2592, 1000, 1000, 0 },
    { -6528, 1000, 1000, 0 },
    { -1700, 1000, 1000, 0 },
    { 2000, 1000, 1000, 0 },
    { 7300, 1000, 1000, 0 },
    { 0x2CEC, 1000, 1000, 0 },
};

/// Eight-point Bog Diver patrol ring at Z=2000, borrowed by selector slots 1 and 3.
///
/// XYZ use room-coordinate units; swimming goal Y adds the sampled water height
/// to the unsigned waypoint Y. The ring wraps after entry 7 and has no terminator.
static SVECTOR _gShelterB4LowerSewerDiverPatrolWaypoints2[8] = {
    { 8256, 1000, 2000, 0 },
    { 3584, 1000, 2000, 0 },
    { -2592, 1000, 2000, 0 },
    { -6528, 1000, 2000, 0 },
    { -1700, 1000, 2000, 0 },
    { 2000, 1000, 2000, 0 },
    { 7300, 1000, 2000, 0 },
    { 0x2CEC, 1000, 2000, 0 },
};

SVECTOR* gShelterB4LowerSewerDiverPatrolRings[4] = {
    _gShelterB4LowerSewerDiverPatrolWaypoints1,
    _gShelterB4LowerSewerDiverPatrolWaypoints2,
    _gShelterB4LowerSewerDiverPatrolWaypoints1,
    _gShelterB4LowerSewerDiverPatrolWaypoints2,
};

/// Initial and terminal claim marks in the room's writable emergence-spot list.
enum {
    SHELTER_B4_LOWER_SEWER_DIVER_SPOT_FREE = 0,
    SHELTER_B4_LOWER_SEWER_DIVER_SPOT_END  = -1
};

SVECTOR gShelterB4LowerSewerDiverSurfaceSpots[10] = {
    { 0, 0, 0, SHELTER_B4_LOWER_SEWER_DIVER_SPOT_FREE },
    { -4900, 1000, 1000, SHELTER_B4_LOWER_SEWER_DIVER_SPOT_FREE },
    { -2600, 1000, 1000, SHELTER_B4_LOWER_SEWER_DIVER_SPOT_FREE },
    { 200, 1000, 1000, SHELTER_B4_LOWER_SEWER_DIVER_SPOT_FREE },
    { 2000, 1000, 1000, SHELTER_B4_LOWER_SEWER_DIVER_SPOT_FREE },
    { 3800, 1000, 1000, SHELTER_B4_LOWER_SEWER_DIVER_SPOT_FREE },
    { 5400, 1000, 1000, SHELTER_B4_LOWER_SEWER_DIVER_SPOT_FREE },
    { 8400, 1000, 1000, SHELTER_B4_LOWER_SEWER_DIVER_SPOT_FREE },
    { 0x2CEC, 1000, 1000, SHELTER_B4_LOWER_SEWER_DIVER_SPOT_FREE },
    { 0, 0, 0, SHELTER_B4_LOWER_SEWER_DIVER_SPOT_END },
};

/// Room-space collision normals, with 4096 representing unit length.
static SVECTOR _gShelterB4LowerSewerCollisionGridNormals[] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_normals.inc"
};

/// Collision vertices in signed room-coordinate units.
static SVECTOR _gShelterB4LowerSewerCollisionGridVerts[] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_verts.inc"
};

/// Indexed collision triangles and quads carrying room-local surface classes.
static WorldCollisionGridFace _gShelterB4LowerSewerCollisionGridFaces[] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_faces.inc"
};

/// Packed cell face-index lists, each ending at `WORLD_COLLISION_GRID_CELL_END`.
///
/// The cell table selects list starts; retained halfwords between lists are outside the scans.
static s16 _gShelterB4LowerSewerCollisionGridCells[] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterB4LowerSewerCollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..261
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_B4_LOWER_SEWER_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB4LowerSewerCollisionGridCells[(halfwordOffset)])
/// X-major face-list starts for eight X cells and two Z cells.
static s16* _gShelterB4LowerSewerCollisionGridTable[] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_table.inc"
};
#undef SHELTER_B4_LOWER_SEWER_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB4LowerSewerCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB4LowerSewerCollisionGridNormals,
    .vertices    = _gShelterB4LowerSewerCollisionGridVerts,
    .faces       = _gShelterB4LowerSewerCollisionGridFaces,
    .cellFaceIds = _gShelterB4LowerSewerCollisionGridTable,
    .xBias       = 14760,
    .zBias       = 3210,
    .cellCountX  = 8,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB4LowerSewerCollisionGridFaces),
};

ViewCamera gShelterB4LowerSewerViewCameras[9] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x7530, 0 } }, 207 },
    { { { { -855, 0, -4005 }, { -579, 4052, 123 }, { 3963, 592, -846 } }, { -4200, 2980, -1880 } }, 257 },
    { { { { -601, 0, -4051 }, { -787, 4017, 116 }, { 3974, 795, -589 } }, { 3545, 3080, -2150 } }, 257 },
    { { { { -804, 0, 4016 }, { 783, 4017, 156 }, { -3939, 799, -788 } }, { -5335, 3080, -2150 } }, 257 },
    { { { { -384, 0, 4077 }, { 623, 4047, 58 }, { -4029, 626, -379 } }, { 2470, 2980, -1245 } }, 257 },
    { { { { 842, 0, -4008 }, { -693, 4034, -145 }, { 3947, 709, 829 } }, { -7260, 4330, 1250 } }, 257 },
    { { { { -1021, 0, -3966 }, { 547, 4056, -140 }, { 3928, -565, -1011 } }, { 0, 2490, -130 } }, 257 },
    { { { { -780, 0, 4020 }, { -569, 4054, -110 }, { -3980, -580, -772 } }, { -4910, 2670, -90 } }, 257 },
    { { { { -974, 0, 3978 }, { 865, 3997, 211 }, { -3883, 891, -950 } }, { 410, 4410, -280 } }, 257 },
};

/// Texture-page, palette and neutral modulation shared by this room's view sprites.
enum {
    SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE         = getTPage(1, 0, 960, 0),
    SHELTER_B4_LOWER_SEWER_SPRITE_CLUT          = getClut(0, 255),
    SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR = 128,
};

/// Empty sprite list for mapped view 1, retaining the decoded background.
///
/// The zero-count first batch precedes the end marker; the source slot aliases this list.
static SpriteBatch _gShelterB4LowerSewerView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 2, retaining the decoded background.
static SpriteBatch _gShelterB4LowerSewerView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 3, retaining the decoded background.
static SpriteBatch _gShelterB4LowerSewerView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Three opaque depth-sorted texture rectangles for mapped view 4.
///
/// Positions are pixels relative to the view centre; depths are unscaled OT sorting values.
static SpriteSource _gShelterB4LowerSewerView4SpriteSources[] = {
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 32, 24 } }, 40, -72, 3800, { .fields = { 96, 64 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 40, 32 } }, 32, -48, 3775, { .fields = { 88, 0 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 40, 32 } }, 32, -16, 3750, { .fields = { 88, 32 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Decoded background followed by every source sprite for mapped view 4.
static SpriteBatch _gShelterB4LowerSewerView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4LowerSewerView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sixteen opaque depth-sorted texture rectangles for mapped view 5.
///
/// Positions are pixels relative to the view centre; depths are unscaled OT sorting values.
static SpriteSource _gShelterB4LowerSewerView5SpriteSources[] = {
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 16, 56 } }, 24, -72, 1800, { .fields = { 112, 0 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 40, 32 } }, 40, -48, 1925, { .fields = { 88, 56 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 64, 24 } }, 40, 64, 1625, { .fields = { 64, 112 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 16, 24 } }, 24, 64, 1420, { .fields = { 112, 88 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 56, 16 } }, 40, 48, 1675, { .fields = { 56, 0 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 16, 16 } }, 24, 48, 1438, { .fields = { 112, 232 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 56, 16 } }, 40, 32, 1725, { .fields = { 56, 16 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 16, 16 } }, 24, 32, 1488, { .fields = { 96, 88 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 56, 16 } }, 40, 16, 1775, { .fields = { 56, 32 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 16, 16 } }, 24, 16, 1506, { .fields = { 112, 216 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 48, 16 } }, 40, 0, 1825, { .fields = { 80, 152 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 16, 16 } }, 24, 0, 1700, { .fields = { 112, 136 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 48, 16 } }, 40, -16, 1875, { .fields = { 80, 168 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 16, 16 } }, 24, -16, 1750, { .fields = { 112, 200 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 24, 16 } }, 80, 88, 1377, { .fields = { 104, 184 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 24, 8 } }, 24, 88, 1450, { .fields = { 104, 248 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Decoded background followed by every source sprite for mapped view 5.
static SpriteBatch _gShelterB4LowerSewerView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4LowerSewerView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Nine opaque eight-pixel-high texture strips for mapped view 6, each with its own depth.
///
/// Positions are pixels relative to the view centre; depths are unscaled OT sorting values.
static SpriteSource _gShelterB4LowerSewerView6SpriteSources[] = {
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 64, 8 } }, -8, 48, 1725, { .fields = { 64, 0 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 64, 8 } }, -8, 56, 1618, { .fields = { 64, 8 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 72, 8 } }, -16, 64, 1575, { .fields = { 56, 16 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 80, 8 } }, -24, 72, 1452, { .fields = { 48, 24 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 80, 8 } }, -24, 80, 1425, { .fields = { 48, 32 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 88, 8 } }, -32, 88, 1350, { .fields = { 40, 40 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 88, 8 } }, -32, 96, 1275, { .fields = { 40, 48 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 96, 8 } }, -40, 104, 1200, { .fields = { 32, 56 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 96, 8 } }, -40, 112, 1125, { .fields = { 32, 64 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Decoded background followed by every source sprite for mapped view 6.
static SpriteBatch _gShelterB4LowerSewerView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4LowerSewerView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 7, retaining the decoded background.
static SpriteBatch _gShelterB4LowerSewerView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 8, retaining the decoded background.
static SpriteBatch _gShelterB4LowerSewerView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Seventeen opaque eight-pixel-high texture strips for mapped view 9, each with its own depth.
///
/// Positions are pixels relative to the view centre; depths are unscaled OT sorting values.
static SpriteSource _gShelterB4LowerSewerView9SpriteSources[] = {
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 64, 8 } }, -8, 16, 1690, { .fields = { 64, 0 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 64, 8 } }, -8, 24, 1686, { .fields = { 64, 8 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 72, 8 } }, -16, 32, 1626, { .fields = { 56, 16 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 72, 8 } }, -16, 40, 1625, { .fields = { 56, 24 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 80, 8 } }, -32, 48, 1500, { .fields = { 48, 32 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 80, 8 } }, -32, 56, 1425, { .fields = { 48, 40 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 80, 8 } }, -32, 64, 1400, { .fields = { 48, 48 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 80, 8 } }, -32, 72, 1250, { .fields = { 48, 56 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 80, 8 } }, -32, 80, 1175, { .fields = { 48, 64 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 80, 8 } }, -32, 88, 1125, { .fields = { 48, 72 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 80, 8 } }, -40, 96, 1075, { .fields = { 48, 80 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 80, 8 } }, -40, 104, 1025, { .fields = { 48, 88 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 80, 8 } }, -40, 112, 975, { .fields = { 48, 96 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 64, 8 } }, -8, 8, 1742, { .fields = { 64, 104 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 64, 8 } }, -8, 0, 1808, { .fields = { 64, 112 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 64, 8 } }, -8, -8, 1873, { .fields = { 64, 120 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B4_LOWER_SEWER_SPRITE_TPAGE, SHELTER_B4_LOWER_SEWER_SPRITE_CLUT, { .fields = { 64, 8 } }, -8, -16, 2005, { .fields = { 64, 128 } }, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, SHELTER_B4_LOWER_SEWER_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Decoded background followed by every source sprite for mapped view 9.
static SpriteBatch _gShelterB4LowerSewerView9SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4LowerSewerView9SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB4LowerSewerSpriteViews[9] = {
    { { .empty = _gShelterB4LowerSewerView1SpriteBatches }, _gShelterB4LowerSewerView1SpriteBatches, NULL },
    { { .empty = _gShelterB4LowerSewerView2SpriteBatches }, _gShelterB4LowerSewerView2SpriteBatches, NULL },
    { { .empty = _gShelterB4LowerSewerView3SpriteBatches }, _gShelterB4LowerSewerView3SpriteBatches, NULL },
    { { .elements = _gShelterB4LowerSewerView4SpriteSources }, _gShelterB4LowerSewerView4SpriteBatches, NULL },
    { { .elements = _gShelterB4LowerSewerView5SpriteSources }, _gShelterB4LowerSewerView5SpriteBatches, NULL },
    { { .elements = _gShelterB4LowerSewerView6SpriteSources }, _gShelterB4LowerSewerView6SpriteBatches, NULL },
    { { .empty = _gShelterB4LowerSewerView7SpriteBatches }, _gShelterB4LowerSewerView7SpriteBatches, NULL },
    { { .empty = _gShelterB4LowerSewerView8SpriteBatches }, _gShelterB4LowerSewerView8SpriteBatches, NULL },
    { { .elements = _gShelterB4LowerSewerView9SpriteSources }, _gShelterB4LowerSewerView9SpriteBatches, NULL },
};

/// Fourteen authored point lights admitted in every view, with world-unit falloff radii.
///
/// Room-light setup binds the coordinate parents; composition and shading mutate their caches.
static WorldCoordPointLight _gShelterB4LowerSewerPointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7100, -1900, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 3662, 5242 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x3908, -4600, 2199 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 2500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3700, -4000, -430 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -99, -4000, -430 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 4341, 4920 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3400, -4000, -430 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7400, -4000, -430 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 4162, 5341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2C88, -4000, -430 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 4501, 4898 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7100, -4000, -430 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2EE5, -1800, 749 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 3859, 3860 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2C88, -1900, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7400, -1900, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 3121, 4161 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3400, -1900, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 2481, 4983 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -99, -1900, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 2300, 3921 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3700, -1900, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 2300, 5121 },
};

WorldCoordRoomLights gShelterB4LowerSewerRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB4LowerSewerPointLights),
    .pointLights           = _gShelterB4LowerSewerPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB4LowerSewerViewBoundaryTriggers[12] = {
    { NULL, NULL, NULL, { 0x2A5F, -3585, -1922, 0 }, { { -6, -3824, 2091, 0 }, { -7, -3824, -2101, 0 }, { -6, 3824, 2091, 0 }, { -7, 3824, -2101, 0 } }, { -4102, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x299F, -3569, -2081, 0 }, { { -4, -3904, -2130, 0 }, { -6, -3904, 2121, 0 }, { -4, 3904, -2130, 0 }, { -6, 3904, 2121, 0 } }, { 4098, 0, 1, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2495, -3553, -1985, 0 }, { { -2, -3760, 2094, 0 }, { -2, -3760, -2098, 0 }, { -2, 3761, 2094, 0 }, { -2, 3761, -2098, 0 } }, { -4096, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4283, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2367, -3665, -1985, 0 }, { { 92, -3744, -2128, 0 }, { -95, -3744, 2125, 0 }, { 92, 3744, -2128, 0 }, { -95, 3744, 2125, 0 } }, { 4090, 0, 179, 0 }, { 0, 0, 4096, 0 }, 4283, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3266, -3617, -2017, 0 }, { { 0, -3696, 2095, 0 }, { -1, -3696, -2096, 0 }, { 0, 3696, 2095, 0 }, { -1, 3696, -2096, 0 } }, { -4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3489, -3633, -1985, 0 }, { { 0, -3776, -2128, 0 }, { -1, -3776, 2127, 0 }, { 0, 3777, -2128, 0 }, { -1, 3777, 2127, 0 } }, { 4103, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4314, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9023, -1633, 1951, 0 }, { { -390, -2864, -2096, 0 }, { 383, -2864, 2088, 0 }, { -390, 2864, -2096, 0 }, { 383, 2864, 2088, 0 } }, { 4028, 0, -745, 0 }, { 0, 0, 4096, 0 }, 3565, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9343, -1600, 1887, 0 }, { { 300, -2928, 2067, 0 }, { -314, -2928, -2080, 0 }, { 300, 2928, 2067, 0 }, { -314, 2928, -2080, 0 } }, { -4052, 0, 599, 0 }, { 0, 0, 4096, 0 }, 3593, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1151, -1617, 2111, 0 }, { { -1, -2880, 2095, 0 }, { -1, -2880, -2097, 0 }, { -1, 2880, 2095, 0 }, { -1, 2880, -2097, 0 } }, { -4105, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3556, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 895, -1617, 2080, 0 }, { { -1, -2880, -2129, 0 }, { -2, -2880, 2126, 0 }, { -1, 2880, -2129, 0 }, { -2, 2880, 2126, 0 } }, { 4103, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3574, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6914, -1649, 1951, 0 }, { { -210, -2880, -2117, 0 }, { 209, -2880, 2116, 0 }, { -210, 2880, -2117, 0 }, { 209, 2880, 2116, 0 } }, { 4081, 0, -405, 0 }, { 0, 0, 4096, 0 }, 3574, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6818, -1600, 1887, 0 }, { { 101, -2896, 2092, 0 }, { -103, -2896, -2094, 0 }, { 101, 2896, 2092, 0 }, { -103, 2896, -2094, 0 } }, { -4091, 0, 198, 0 }, { 0, 0, 4096, 0 }, 3574, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger gShelterB4LowerSewerActionTriggers[12] = {
    { NULL, NULL, NULL, { -8288, -48, 2976, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_FACING, 53, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -8736, -1888, 2384, 0 }, { { 0, 1600, -784, 0 }, { 0, -1600, -784, 0 }, { 0, 1600, 784, 0 }, { 0, -1600, 784, 0 } }, { 4109, 0, 0, 0 }, { 4096, 0, 0, 0 }, 1778, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, 0x12, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3200, -48, 1472, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B4_UPPER_SEWER, 0x21, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3760, -2080, -224, 0 }, { { 1024, 0, -544, 0 }, { 1024, 0, 544, 0 }, { -1024, 0, -544, 0 }, { -1024, 0, 544, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B4_UPPER_SEWER, 0x44, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6528, -2048, -1024, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_FACING, 85, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7072, -2737, -992, 0 }, { { 0, 1296, -1024, 0 }, { 0, -1295, -1024, 0 }, { 0, 1296, 1024, 0 }, { 0, -1295, 1024, 0 } }, { 4102, 0, 0, 0 }, { 4096, 0, 0, 0 }, 1649, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, GAME_AREA_SHELTER_B3_ELEVATOR_HALL, 0x32, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x30E0, -64, 736, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -176, -64, 304, 0 }, { { -1072, 0, -720, 0 }, { 1072, 0, -720, 0 }, { -1072, 0, 720, 0 }, { 1072, 0, 720, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1286, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 704, -64, 2496, 0 }, { { -1360, 0, -720, 0 }, { 1360, 0, -720, 0 }, { -1360, 0, 720, 0 }, { 1360, 0, 720, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1536, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2528, -2048, -2304, 0 }, { { -1360, 0, -720, 0 }, { 1360, 0, -720, 0 }, { -1360, 0, 720, 0 }, { 1360, 0, 720, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1536, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6976, -64, 2528, 0 }, { { -1360, 0, -720, 0 }, { 1360, 0, -720, 0 }, { -1360, 0, 720, 0 }, { 1360, 0, 720, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1536, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2784, -64, 2560, 0 }, { { -1360, 0, -720, 0 }, { 1360, 0, -720, 0 }, { -1360, 0, 720, 0 }, { 1360, 0, 720, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1536, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room-local resource keys shared by the enemy recipes and placements.
enum {
    SHELTER_B4_LOWER_SEWER_RESOURCE_MAD_CHASER        = 44,
    SHELTER_B4_LOWER_SEWER_RESOURCE_BOG_DIVER         = 4,
    SHELTER_B4_LOWER_SEWER_RESOURCE_CREEPING_STRANGER = 72,
    SHELTER_B4_LOWER_SEWER_RESOURCE_SKULL_STALKER     = 73,
    SHELTER_B4_LOWER_SEWER_RESOURCE_PAWN_GOLEM        = 20,
    SHELTER_B4_LOWER_SEWER_RESOURCE_ROOK_GOLEM        = 23,
    SHELTER_B4_LOWER_SEWER_RESOURCE_MOSSBACK          = 49,
};

/// Enemy resource recipes for lower-sewer variant 1, terminated by `AREA_PLACEMENT_END`.
///
/// Two Mad Chasers; task tables are borrowed from the selected actor-load slots.
static AreaResource _gShelterB4LowerSewerVariant1Resources[2] = {
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_MAD_CHASER, .fileNumber = 44, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = ACTOR_04400_MAD_CHASER_TASK_ORDINARY, .unknown_6 = { 0, 0 }, .taskTable = gActor04400MadChaserTaskDescs },
    { .entryId = AREA_PLACEMENT_END, .fileNumber = 0, .fileGroupIndex = 0, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = NULL },
};

/// Enemy resource recipes for lower-sewer variant 2, terminated by `AREA_PLACEMENT_END`.
///
/// One Bog Diver; task tables are borrowed from the selected actor-load slots.
static AreaResource _gShelterB4LowerSewerVariant2Resources[2] = {
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_BOG_DIVER, .fileNumber = 4, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = ACTOR_00400_BODY_TASK_INDEX, .unknown_6 = { 0, 0 }, .taskTable = gActor100400Tasks },
    { .entryId = AREA_PLACEMENT_END, .fileNumber = 0, .fileGroupIndex = 0, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = NULL },
};

/// Enemy resource recipes for lower-sewer variant 3, terminated by `AREA_PLACEMENT_END`.
///
/// Two Bog Divers, a Creeping Stranger and two Skull Stalkers; task tables are borrowed from the selected actor-load slots.
static AreaResource _gShelterB4LowerSewerVariant3Resources[4] = {
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_BOG_DIVER, .fileNumber = 4, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = ACTOR_00400_BODY_TASK_INDEX, .unknown_6 = { 0, 0 }, .taskTable = gActor100400Tasks },
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_CREEPING_STRANGER, .fileNumber = 72, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor207200CreepingStrangerTaskDesc },
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_SKULL_STALKER, .fileNumber = 73, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor207200SkullStalkerTaskDesc },
    { .entryId = AREA_PLACEMENT_END, .fileNumber = 0, .fileGroupIndex = 0, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = NULL },
};

/// Enemy resource recipes for lower-sewer variant 7, terminated by `AREA_PLACEMENT_END`.
///
/// One Pawn GOLEM; task tables are borrowed from the selected actor-load slots.
static AreaResource _gShelterB4LowerSewerVariant7Resources[2] = {
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_PAWN_GOLEM, .fileNumber = 20, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor102000PawnGolemTasks },
    { .entryId = AREA_PLACEMENT_END, .fileNumber = 0, .fileGroupIndex = 0, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = NULL },
};

/// Enemy resource recipes for lower-sewer variant 8, terminated by `AREA_PLACEMENT_END`.
///
/// One Rook GOLEM; task tables are borrowed from the selected actor-load slots.
static AreaResource _gShelterB4LowerSewerVariant8Resources[2] = {
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_ROOK_GOLEM, .fileNumber = 23, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor102300RookGolemTasks },
    { .entryId = AREA_PLACEMENT_END, .fileNumber = 0, .fileGroupIndex = 0, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = NULL },
};

/// Enemy resource recipes for lower-sewer variant 11, terminated by `AREA_PLACEMENT_END`.
///
/// Two Bog Divers and a Mossback; task tables are borrowed from the selected actor-load slots.
static AreaResource _gShelterB4LowerSewerVariant11Resources[3] = {
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_BOG_DIVER, .fileNumber = 4, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = ACTOR_00400_BODY_TASK_INDEX, .unknown_6 = { 0, 0 }, .taskTable = gActor100400Tasks },
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_MOSSBACK, .fileNumber = 49, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor01100Tasks },
    { .entryId = AREA_PLACEMENT_END, .fileNumber = 0, .fileGroupIndex = 0, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = NULL },
};

/// Enemy placements for lower-sewer variant 1, terminated by `AREA_PLACEMENT_END`.
///
/// Two Mad Chasers; XYZ use world units and yaw uses 4096 units per turn.
/// Actors borrow these records for their lifetimes.
static AreaPlacement _gShelterB4LowerSewerVariant1Placements[3] = {
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_MAD_CHASER, .variant = 0, .mode = 0, .x = -500, .y = 0, .z = 700, .yaw = 2700, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_MAD_CHASER, .variant = 0, .mode = 0, .x = 4500, .y = 0, .z = 2500, .yaw = 3400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 0, .rowIndex = 0 },
};

/// Enemy placements for lower-sewer variant 2, terminated by `AREA_PLACEMENT_END`.
///
/// One Bog Diver; XYZ use world units and yaw uses 4096 units per turn.
/// Actors borrow these records for their lifetimes.
static AreaPlacement _gShelterB4LowerSewerVariant2Placements[2] = {
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_BOG_DIVER, .variant = 0, .mode = 1, .x = 0x2CEC, .y = 1000, .z = 1000, .yaw = 1280, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 4 },
    { .entryId = AREA_PLACEMENT_END, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 0, .rowIndex = 0 },
};

/// Enemy placements for lower-sewer variant 3, terminated by `AREA_PLACEMENT_END`.
///
/// Two Bog Divers, a Creeping Stranger and two Skull Stalkers; XYZ use world units and yaw uses 4096 units per turn.
/// Actors borrow these records for their lifetimes.
static AreaPlacement _gShelterB4LowerSewerVariant3Placements[6] = {
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_BOG_DIVER, .variant = 0, .mode = 1, .x = 0x2CEC, .y = 1000, .z = 1000, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_BOG_DIVER, .variant = 0, .mode = 17, .x = -5000, .y = 1000, .z = 2000, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_CREEPING_STRANGER, .variant = 0, .mode = 0, .x = 0, .y = -2000, .z = -930, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_SKULL_STALKER, .variant = 0, .mode = 1, .x = -3600, .y = -2000, .z = -1400, .yaw = 1200, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_SKULL_STALKER, .variant = 0, .mode = 1, .x = 2250, .y = -2000, .z = -400, .yaw = 700, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 0, .rowIndex = 0 },
};

/// Enemy placements for lower-sewer variant 7, terminated by `AREA_PLACEMENT_END`.
///
/// One Pawn GOLEM; XYZ use world units and yaw uses 4096 units per turn.
/// Actors borrow these records for their lifetimes.
static AreaPlacement _gShelterB4LowerSewerVariant7Placements[2] = {
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_PAWN_GOLEM, .variant = 4, .mode = 1, .x = -2000, .y = 0, .z = 1600, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 0, .rowIndex = 0 },
};

/// Enemy placements for lower-sewer variant 8, terminated by `AREA_PLACEMENT_END`.
///
/// One Rook GOLEM; XYZ use world units and yaw uses 4096 units per turn.
/// Actors borrow these records for their lifetimes.
static AreaPlacement _gShelterB4LowerSewerVariant8Placements[2] = {
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_ROOK_GOLEM, .variant = 5, .mode = 1, .x = 7000, .y = -2000, .z = -1000, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 0, .rowIndex = 0 },
};

/// Enemy placements for lower-sewer variant 11, terminated by `AREA_PLACEMENT_END`.
///
/// Two Bog Divers and a Mossback; XYZ use world units and yaw uses 4096 units per turn.
/// Actors borrow these records for their lifetimes.
static AreaPlacement _gShelterB4LowerSewerVariant11Placements[4] = {
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_BOG_DIVER, .variant = 0, .mode = 1, .x = 0x2CEC, .y = 1000, .z = 1000, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_BOG_DIVER, .variant = 0, .mode = 17, .x = -5000, .y = 1000, .z = 2000, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = SHELTER_B4_LOWER_SEWER_RESOURCE_MOSSBACK, .variant = 1, .mode = 0, .x = 1950, .y = -2000, .z = -1300, .yaw = 200, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 0, .rowIndex = 0 },
};

AreaVariant gShelterB4LowerSewerAreaVariants[12] = {
    [1]  = { .placements = _gShelterB4LowerSewerVariant1Placements, .resources = _gShelterB4LowerSewerVariant1Resources },
    [2]  = { .placements = _gShelterB4LowerSewerVariant2Placements, .resources = _gShelterB4LowerSewerVariant2Resources },
    [3]  = { .placements = _gShelterB4LowerSewerVariant3Placements, .resources = _gShelterB4LowerSewerVariant3Resources },
    [7]  = { .placements = _gShelterB4LowerSewerVariant7Placements, .resources = _gShelterB4LowerSewerVariant7Resources },
    [8]  = { .placements = _gShelterB4LowerSewerVariant8Placements, .resources = _gShelterB4LowerSewerVariant8Resources },
    [11] = { .placements = _gShelterB4LowerSewerVariant11Placements, .resources = _gShelterB4LowerSewerVariant11Resources },
};

/// Type-1 cue-2 bases; cue 1 adds one and companion playback adds 100.
enum {
    SHELTER_B4_LOWER_SEWER_SURFACE5_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x41,
    SHELTER_B4_LOWER_SEWER_SURFACE5_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x43,
    SHELTER_B4_LOWER_SEWER_SURFACE5_JUMP_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x55,
    SHELTER_B4_LOWER_SEWER_SURFACE2_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3D,
    SHELTER_B4_LOWER_SEWER_SURFACE2_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3F,
    SHELTER_B4_LOWER_SEWER_SURFACE3_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    SHELTER_B4_LOWER_SEWER_SURFACE3_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
    SHELTER_B4_LOWER_SEWER_SURFACE3_JUMP_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x19,
};

/// Distinct walking, running and scripted-jump cue pairs for surface class 5.
static WorldCollisionFootstepSounds _gShelterB4LowerSewerSurface5FootstepSounds = {
    .walk         = SHELTER_B4_LOWER_SEWER_SURFACE5_WALK_CUE_BASE,
    .run          = SHELTER_B4_LOWER_SEWER_SURFACE5_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B4_LOWER_SEWER_SURFACE5_JUMP_CUE_BASE,
};

/// Surface-class-2 cue pairs, with scripted jumps using the walking pair.
static WorldCollisionFootstepSounds _gShelterB4LowerSewerSurface2FootstepSounds = {
    .walk         = SHELTER_B4_LOWER_SEWER_SURFACE2_WALK_CUE_BASE,
    .run          = SHELTER_B4_LOWER_SEWER_SURFACE2_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B4_LOWER_SEWER_SURFACE2_WALK_CUE_BASE,
};

/// Distinct walking, running and scripted-jump cue pairs for surface class 3.
static WorldCollisionFootstepSounds _gShelterB4LowerSewerSurface3FootstepSounds = {
    .walk         = SHELTER_B4_LOWER_SEWER_SURFACE3_WALK_CUE_BASE,
    .run          = SHELTER_B4_LOWER_SEWER_SURFACE3_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B4_LOWER_SEWER_SURFACE3_JUMP_CUE_BASE,
};

/// Silent blocking policy shared by surface classes 0, 4, 6 and 7.
static WorldCollisionSurfaceProperties _gShelterB4LowerSewerDefaultSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class-1 policy: pass probes, ignore weapon impacts and apply pushback, with silent footsteps.
static WorldCollisionSurfaceProperties _gShelterB4LowerSewerSurface1 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class-2 blocking policy with weapon impacts, pushback and its own footstep cues.
static WorldCollisionSurfaceProperties _gShelterB4LowerSewerSurface2 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB4LowerSewerSurface2FootstepSounds,
};

/// Class-3 blocking policy with weapon impacts, pushback and its own footstep cues.
static WorldCollisionSurfaceProperties _gShelterB4LowerSewerSurface3 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB4LowerSewerSurface3FootstepSounds,
};

/// Class-5 blocking policy with weapon impacts, pushback and its own footstep cues.
static WorldCollisionSurfaceProperties _gShelterB4LowerSewerSurface5 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB4LowerSewerSurface5FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB4LowerSewerSurfacePropertiesByClass[8] = {
    &_gShelterB4LowerSewerDefaultSurface,
    &_gShelterB4LowerSewerSurface1,
    &_gShelterB4LowerSewerSurface2,
    &_gShelterB4LowerSewerSurface3,
    &_gShelterB4LowerSewerDefaultSurface,
    &_gShelterB4LowerSewerSurface5,
    &_gShelterB4LowerSewerDefaultSurface,
    &_gShelterB4LowerSewerDefaultSurface,
};

u8* gShelterB4LowerSewerWaterPacketCursor = NULL;

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Selects the lower sewer's effect task IDs for actor spawns.
///
/// Called once when the room's glow task initializes. Flash, twin trail and
/// spark burst are always selected. Water ripple and spray are selected only
/// when `GAME_FLAG_B4_RESERVOIR_EVENT_DONE` equals 1; otherwise their IDs are unchanged.
static inline void _shelterB4LowerSewerInstallActorEffects(void)
{
    enum { SHELTER_B4_LOWER_SEWER_RESERVOIR_COMPLETE = 1 };

    gRoomEffectFlashId      = EFFECT_SHELTER_B4_LOWER_SEWER_FLASH;
    gRoomEffectTwinTrailId  = EFFECT_SHELTER_B4_LOWER_SEWER_TWIN_TRAIL;
    gRoomEffectSparkBurstId = EFFECT_SHELTER_B4_LOWER_SEWER_SPARK_BURST;
    if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) == SHELTER_B4_LOWER_SEWER_RESERVOIR_COMPLETE) {
        gRoomEffectWaterRippleId = EFFECT_SHELTER_B4_LOWER_SEWER_WATER_RIPPLE;
        gRoomEffectWaterSprayId  = EFFECT_SHELTER_B4_LOWER_SEWER_WATER_SPRAY;
    }
}

void shelterB4LowerSewerDrawGlowsTask(Task* task)
{
    enum {
        SHELTER_B4_LOWER_SEWER_GLOW_INITIALIZE   = 0,
        SHELTER_B4_LOWER_SEWER_GLOW_ACTIVE       = 1,
        SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE = 0x200, // Radius numerator before 64 / depth projection
        SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES  = 0x222  // RGB 32/32/32 before frame flicker
    };

    if (task->state == SHELTER_B4_LOWER_SEWER_GLOW_INITIALIZE) {
        _shelterB4LowerSewerInstallActorEffects();
        task->state = SHELTER_B4_LOWER_SEWER_GLOW_ACTIVE;
    }

    // Select world-space endpoint pairs visible from the mapped camera.
    switch ((u8)viewGetMappedIndex()) {
        case 2:
        case 6: {
            const SVECTOR* capsulePoints = &_gShelterB4LowerSewerGlowEndpoints[14];
            _glowDrawCapsule(&capsulePoints[0], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[2], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            break;
        }
        case 3: {
            const SVECTOR* capsulePoints = &_gShelterB4LowerSewerGlowEndpoints[12];
            _glowDrawCapsule(&capsulePoints[0], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[2], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[4], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[6], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            break;
        }
        case 4: {
            const SVECTOR* capsulePoints = _gShelterB4LowerSewerGlowEndpoints;
            _glowDrawCapsule(&capsulePoints[0], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[2], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[4], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[6], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[24], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[26], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[28], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            break;
        }
        case 5: {
            const SVECTOR* capsulePoints = _gShelterB4LowerSewerGlowEndpoints;
            _glowDrawCapsule(&capsulePoints[0], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[2], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[28], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            break;
        }
        case 7: {
            const SVECTOR* capsulePoints = &_gShelterB4LowerSewerGlowEndpoints[12];
            _glowDrawCapsule(&capsulePoints[0], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[2], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[4], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[6], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[8], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            break;
        }
        case 8: {
            const SVECTOR* capsulePoints = _gShelterB4LowerSewerGlowEndpoints;
            _glowDrawCapsule(&capsulePoints[0], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[2], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[4], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[6], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[8], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[22], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[24], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[26], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[28], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            break;
        }
        case 9: {
            const SVECTOR* capsulePoints = _gShelterB4LowerSewerGlowEndpoints;
            _glowDrawCapsule(&capsulePoints[0], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[2], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[4], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[24], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[26], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            _glowDrawCapsule(&capsulePoints[28], SHELTER_B4_LOWER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_LOWER_SEWER_GLOW_RGB_NIBBLES);
            break;
        }
    }
}

#include "../../shared/effects/glow_draw_capsule.inc.c"

#include "../../shared/effects/water_ripple_task.inc.c"

void shelterB4LowerSewerWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/effects/water_splash.inc.c"

#include "../../shared/effects/water_drift_task_u16.inc.c"

void shelterB4LowerSewerWaterDriftTaskU16(Task* task)
{
    _waterDriftTaskU16(task);
}

#include "../../shared/effects/water_spin_u16.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelterB4LowerSewerRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelterB4LowerSewerRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelterB4LowerSewerRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
