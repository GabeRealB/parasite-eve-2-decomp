#include "actors/actor_04400.h"
#include "actors/actor_02400.h"
#include "actors/actor_207200.h"
#include "actors/actor_300700.h"
#include "actors/actor_401800.h"
#include "actors/actor_402200.h"
#include "actors/actor_403900.h"

#include "gameplay/actor_render.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "gte.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag_ids.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/shelter_b1_sleeping_quarters.h"

#include "shelter_b1_sleeping_quarters_private.h"

#include "common.h"

#include "../../shared/room/room_visual_effects.h"

static RoomFxShade _gRoomVisualEffectsGlowDiscShades[2];

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flying_tasks.inc.c"

#include "../../shared/room/room_visual_effects_disc_data.inc.c"

u8* gShelterB1SleepingQuartersRoomViewMaps[1] = {
    gViewIdentityMap,
};

enum { SHELTER_B1_SLEEPING_QUARTERS_REVERSE_VIEW_SEARCH_COUNT = 11 };

ViewCount gShelterB1SleepingQuartersRoomViewCounts[1] = { SHELTER_B1_SLEEPING_QUARTERS_REVERSE_VIEW_SEARCH_COUNT };

enum {
    SHELTER_B1_SLEEPING_QUARTERS_CORRIDOR_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, 4),
    SHELTER_B1_SLEEPING_QUARTERS_CORRIDOR_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, 3),
    SHELTER_B1_SLEEPING_QUARTERS_WALKWAY_ARRIVAL_SOUND    = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, 2),
    SHELTER_B1_SLEEPING_QUARTERS_WALKWAY_DEPARTURE_SOUND  = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, 1),
};

DirectionWarpEntry gShelterB1SleepingQuartersWarpEntries[2] = {
    { .player         = { .yaw = { .word = 1024 }, .x = 0, .y = 0, .z = -860 },
      .companion      = { .yaw = { .word = 1024 }, .x = 0, .y = 0, .z = -860 },
      .arrivalSound   = SHELTER_B1_SLEEPING_QUARTERS_CORRIDOR_ARRIVAL_SOUND,
      .departureSound = SHELTER_B1_SLEEPING_QUARTERS_CORRIDOR_DEPARTURE_SOUND,
      .blockedSound   = DIRECTION_WARP_SOUND_NONE,
      .initialView    = 2,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE },
    { .player         = { .yaw = { .word = 2048 }, .x = 0x2E2C, .y = 0, .z = 5980 },
      .companion      = { .yaw = { .word = 2048 }, .x = 0x2E2C, .y = 0, .z = 5980 },
      .arrivalSound   = SHELTER_B1_SLEEPING_QUARTERS_WALKWAY_ARRIVAL_SOUND,
      .departureSound = SHELTER_B1_SLEEPING_QUARTERS_WALKWAY_DEPARTURE_SOUND,
      .blockedSound   = DIRECTION_WARP_SOUND_NONE,
      .initialView    = 9,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = GAME_FLAG_MAP_MARK_SHELTER_1B3 },
};

/// Room-space collision normals, with 4096 representing unit length.
static SVECTOR _gShelterB1SleepingQuartersCollisionGridNormals[7] = {
#include "assets/shelter_b1_sleeping_quarters_collision_03B14_normals.inc"
};

/// Collision vertices in signed room-coordinate units.
static SVECTOR _gShelterB1SleepingQuartersCollisionGridVerts[143] = {
#include "assets/shelter_b1_sleeping_quarters_collision_03B14_verts.inc"
};

/// Indexed collision quads; their surface classes select this room's policies.
static WorldCollisionGridFace _gShelterB1SleepingQuartersCollisionGridFaces[64] = {
#include "assets/shelter_b1_sleeping_quarters_collision_03B14_faces.inc"
};

/// Signed face-index lists for the collision cells, each ending at -1.
///
/// The authored storage also retains zeros between lists and after the last list.
static s16 _gShelterB1SleepingQuartersCollisionGridCells[274] = {
#include "assets/shelter_b1_sleeping_quarters_collision_03B14_cells.inc"
};

enum {
    SHELTER_B1_SLEEPING_QUARTERS_COLLISION_CELL_COUNT_X = 4,
    SHELTER_B1_SLEEPING_QUARTERS_COLLISION_CELL_COUNT_Z = 3
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelterB1SleepingQuartersCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..273,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_B1_SLEEPING_QUARTERS_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB1SleepingQuartersCollisionGridCells[(halfwordOffset)])
/// X-major cell pointers, indexed by x * cellCountZ + z.
static s16* _gShelterB1SleepingQuartersCollisionGridTable[SHELTER_B1_SLEEPING_QUARTERS_COLLISION_CELL_COUNT_X * SHELTER_B1_SLEEPING_QUARTERS_COLLISION_CELL_COUNT_Z] = {
#include "assets/shelter_b1_sleeping_quarters_collision_03B14_table.inc"
};
#undef SHELTER_B1_SLEEPING_QUARTERS_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB1SleepingQuartersCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB1SleepingQuartersCollisionGridNormals,
    .vertices    = _gShelterB1SleepingQuartersCollisionGridVerts,
    .faces       = _gShelterB1SleepingQuartersCollisionGridFaces,
    .cellFaceIds = _gShelterB1SleepingQuartersCollisionGridTable,
    .xBias       = 1400,
    .zBias       = 4400,
    .cellCountX  = SHELTER_B1_SLEEPING_QUARTERS_COLLISION_CELL_COUNT_X,
    .cellCountZ  = SHELTER_B1_SLEEPING_QUARTERS_COLLISION_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB1SleepingQuartersCollisionGridFaces),
};

ViewCamera gShelterB1SleepingQuartersViewCameras[10] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -6200, 0x7530, -1480 } }, 564 },
    { { { { -1996, 0, 3576 }, { -1019, 3925, -569 }, { -3427, -1167, -1913 } }, { -2791, 356, -744 } }, 230 },
    { { { { -4054, 0, 582 }, { 17, 4094, 123 }, { -582, 124, -4052 } }, { -2265, 1531, -3824 } }, 230 },
    { { { { 4060, 0, 540 }, { 37, 4086, -282 }, { -538, 285, 4050 } }, { -2734, 1706, 1550 } }, 230 },
    { { { { -826, 0, -4011 }, { 352, 4080, -72 }, { 3996, -359, -823 } }, { -801, 1095, -4190 } }, 243 },
    { { { { -3643, 0, -1870 }, { -1870, 40, 3643 }, { 18, 4095, -36 } }, { -5726, 4115, -296 } }, 230 },
    { { { { -337, 0, 4082 }, { -581, 4054, -48 }, { -4040, -583, -334 } }, { -0x3038, 829, -4528 } }, 243 },
    { { { { 284, 0, -4086 }, { 443, 4071, 30 }, { 4061, -444, 282 } }, { -7170, 849, -4356 } }, 243 },
    { { { { 3619, 0, -1917 }, { -1102, 3351, -2080 }, { 1569, 2355, 2961 } }, { -0x28E9, 3289, -4191 } }, 243 },
    { { { { -4061, 0, 531 }, { 531, 240, 4054 }, { -31, 4088, -238 } }, { -9925, 5691, -1450 } }, 230 },
};

/// Empty sprite batch list retaining decoded image strips for mapped view 1.
///
/// The first zero count selects the image; the next record terminates the list.
static SpriteBatch _gShelterB1SleepingQuartersView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list retaining decoded image strips for mapped view 2.
///
/// The first zero count selects the image; the next record terminates the list.
static SpriteBatch _gShelterB1SleepingQuartersView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 3's room sprites.
///
/// Positions and dimensions are pixels; depth is the unscaled ordering value.
/// View 3's batches select all 5 elements while the overlay stays loaded.
static SpriteSource _gShelterB1SleepingQuartersView3SpriteSources[5] = {
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 88, -32, 750, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, 80, 40, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -32, 700, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 112, 16, 700, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 80 } }, 112, 40, 700, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
};

/// Sprite ranges layered over the decoded background of mapped view 3.
///
/// One five-sprite range covers the source array.
/// The first zero-count record retains image strips; the final record ends the list.
static SpriteBatch _gShelterB1SleepingQuartersView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB1SleepingQuartersView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 4's room sprites.
///
/// Positions and dimensions are pixels; depth is the unscaled ordering value.
/// View 4's batches select all 6 elements while the overlay stays loaded.
static SpriteSource _gShelterB1SleepingQuartersView4SpriteSources[6] = {
    { 143, 0x3FC0, { .fields = { 8, 120 } }, 112, 0, 675, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 120, 24, 650, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 144, 24, 625, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 120, -64, 675, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 136, -64, 625, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, -96, 575, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
};

/// Sprite ranges layered over the decoded background of mapped view 4.
///
/// One six-sprite range covers the source array.
/// The first zero-count record retains image strips; the final record ends the list.
static SpriteBatch _gShelterB1SleepingQuartersView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB1SleepingQuartersView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 5's room sprites.
///
/// Positions and dimensions are pixels; depth is the unscaled ordering value.
/// View 5's batches select all 85 elements while the overlay stays loaded.
static SpriteSource _gShelterB1SleepingQuartersView5SpriteSources[85] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -24, 1720, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -8, 1710, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, 8, 1700, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 24, 1695, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 40, 1661, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 56, 1440, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 8, 1625, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 24, 1625, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 40, 1625, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 56, 1625, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -32, 1820, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -16, 1810, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 0, 1800, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 16, 1790, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 32, 1780, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 48, 1770, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -32, 1750, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -16, 1750, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 0, 1750, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 16, 1750, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 32, 1750, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, 48, 1750, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -16, 1525, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -16, 1487, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -16, 1462, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, -16, 1450, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, -16, 1437, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 0, 1437, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 0, 1450, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 0, 1462, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 0, 1487, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 0, 1525, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 16, 1525, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 16, 1487, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 16, 1462, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 16, 1450, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 16, 1437, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 32, 1450, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 32, 1462, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 32, 1487, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 32, 1525, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 48, 1525, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 48, 1487, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 48, 1462, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 48, 1450, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 48, 1437, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 32, 1437, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 64, 1525, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 64, 1487, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 64, 1462, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 64, 1450, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 64, 1437, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 88, 1080, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -120, 1110, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 96, -120, 1087, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, -112, 1015, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -112, 1020, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 96, -72, 1070, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -72, 1095, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, -72, 1150, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -48, 1352, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -72, 1162, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -32, 1430, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -32, 1100, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 8, 1055, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 48, 1057, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, -32, 1090, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -32, 1082, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 96, -32, 1055, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -32, 990, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -32, 890, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 8, 1067, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 8, 1085, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 96, 8, 1041, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 8, 1050, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 8, 887, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 48, 1067, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 48, 1050, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 96, 48, 1020, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 48, 1025, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 48, 885, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 88, 897, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 88, 1037, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 88, 1032, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 88, 1062, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
};

/// Sprite ranges layered over the decoded background of mapped view 5.
///
/// Ranges [0, 52) and [52, 85) cover the source array.
/// The first zero-count record retains image strips; the final record ends the list.
static SpriteBatch _gShelterB1SleepingQuartersView5SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 52, 0, 0, { 1, 0 } },
    { 52, ARRAY_SIZE(_gShelterB1SleepingQuartersView5SpriteSources) - 52, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Clip and full-screen restore commands for mapped view 5.
///
/// Clips to (79, 99, 62, 140) draw-buffer pixels until sorting depth 1662.
/// The second record ends the list; the renderer adds the active buffer's Y origin.
static SpriteDrawArea _gShelterB1SleepingQuartersView5SpriteDrawAreas[2] = {
    { { 79, 99, 62, 140 }, 1662 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Texture rectangles and sorting depths for mapped view 6's room sprites.
///
/// Positions and dimensions are pixels; depth is the unscaled ordering value.
/// View 6's batches select all 112 elements while the overlay stays loaded.
static SpriteSource _gShelterB1SleepingQuartersView6SpriteSources[112] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, 56, 751, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, 56, 784, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -64, 56, 863, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, 56, 844, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, 64, 697, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, 64, 760, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -64, 64, 831, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, 64, 828, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 72, 628, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, 72, 679, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 711, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -64, 72, 700, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 80, 614, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 80, 640, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, 80, 668, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 80, 709, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 88, 561, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 88, 582, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, 88, 584, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, 88, 588, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 96, 549, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, 96, 577, { .fields = { 56, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 104, 516, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, 104, 535, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, 104, 533, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -160, 96, 415, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -112, 96, 489, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -80, 40, 1016, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 48, 731, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -112, 56, 682, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 88, 481, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -152, 80, 501, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -120, 64, 654, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 48, 1014, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -80, 48, 860, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 72, 583, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -136, 72, 524, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 72, 1015, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 88, 834, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 88, 1014, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 104, 1024, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -8, 64, 975, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -16, 104, 724, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -16, 112, 674, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 72, 792, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 88, 718, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 96, 783, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 88, 852, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 80, 934, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 72, 1020, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 88, 510, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 80, 547, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -88, 112, 509, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 56 } }, -160, 48, 487, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -160, 0, 410, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, -160, -24, 338, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -160, -72, 277, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -160, -120, 291, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 64 } }, -136, -120, 338, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 56 } }, -136, -56, 462, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -136, 0, 517, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -136, 40, 550, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -120, 40, 628, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -112, 0, 638, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -104, -40, 562, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -104, -88, 591, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 32 } }, -104, -120, 604, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -72, -120, 671, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -80, -88, 632, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -88, -40, 667, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -96, 0, 710, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -104, 40, 713, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, -88, 40, 895, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -80, 0, 840, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -72, -40, 703, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -64, -88, 610, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -56, -120, 664, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -48, -88, 726, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -56, -40, 700, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -64, 0, 1087, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, -40, -40, 847, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -32, -80, 924, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 32, -8, 1105, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, 24, 16, 972, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 48, -8, 717, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, 40, 16, 704, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 16, 48, 783, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 32 } }, 0, 88, 774, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 0, 48, 972, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 16, 32, 1008, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 32, 48, 705, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, 24, 88, 645, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 64, -8, 620, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, 56, 16, 631, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 48, 48, 612, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, 40, 88, 652, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, 56, 88, 599, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 64, 48, 602, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 72, 16, 628, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 80, -8, 645, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, 96, -8, 588, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 32 } }, 88, 16, 576, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, 80, 48, 570, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 32 } }, 72, 88, 398, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 32 } }, 96, 88, 332, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, 104, 48, 352, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 32 } }, 112, 16, 370, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, 120, -8, 391, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 144, -8, 298, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 32 } }, 136, 16, 311, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 40 } }, 128, 48, 287, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 32 } }, 120, 88, 272, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
};

/// Sprite ranges layered over the decoded background of mapped view 6.
///
/// Ranges [0, 53), [53, 82) and [82, 112) cover the source array.
/// The first zero-count record retains image strips; the final record ends the list.
static SpriteBatch _gShelterB1SleepingQuartersView6SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 53, 0, 0, { 2, 0 } },
    { 53, 29, 0, 0, { 1, 0 } },
    { 82, ARRAY_SIZE(_gShelterB1SleepingQuartersView6SpriteSources) - 82, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Clip and full-screen restore commands for mapped view 6.
///
/// Retains a zero-size clip at (109, 215) until sorting depth 0x57E4.
/// The second record ends the list; the renderer adds the active buffer's Y origin.
static SpriteDrawArea _gShelterB1SleepingQuartersView6SpriteDrawAreas[2] = {
    { { 109, 215, 0, 0 }, 0x57E4 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Texture rectangles and sorting depths for mapped view 7's room sprites.
///
/// Positions and dimensions are pixels; depth is the unscaled ordering value.
/// View 7's batches select all 37 elements while the overlay stays loaded.
static SpriteSource _gShelterB1SleepingQuartersView7SpriteSources[37] = {
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, -64, 757, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, -16, 743, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, -64, 725, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -64, 685, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, -64, 650, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, -64, 560, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, -16, 605, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, -24, 650, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -24, 663, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -16, 717, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 32, 605, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 80, 591, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 16, 647, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 16, 682, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -120, 16, 715, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 48, 635, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 48, 667, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 56, 713, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 88, 678, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, 80, 650, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, 80, 636, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -8, 1064, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 0, 1060, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 8, 1054, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 16, 1050, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 24, 1044, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 32, 1040, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 40, 1035, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 48, 1023, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 56, 1019, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 64, 1014, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 72, 1010, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 80, 986, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -72, -40, 1500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -72, 24, 1500, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 40, -32, 1500, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 40, 24, 1500, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
};

/// Sprite ranges layered over the decoded background of mapped view 7.
///
/// Ranges [0, 21) and [21, 37) cover the source array.
/// The first zero-count record retains image strips; the final record ends the list.
static SpriteBatch _gShelterB1SleepingQuartersView7SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 1, 0 } },
    { 21, ARRAY_SIZE(_gShelterB1SleepingQuartersView7SpriteSources) - 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Clip and full-screen restore commands for mapped view 7.
///
/// Clips to (121, 87, 83, 152) draw-buffer pixels until sorting depth 1375.
/// The second record ends the list; the renderer adds the active buffer's Y origin.
static SpriteDrawArea _gShelterB1SleepingQuartersView7SpriteDrawAreas[2] = {
    { { 121, 87, 83, 152 }, 1375 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Texture rectangles and sorting depths for mapped view 8's room sprites.
///
/// Positions and dimensions are pixels; depth is the unscaled ordering value.
/// View 8's batches select all 39 elements while the overlay stays loaded.
static SpriteSource _gShelterB1SleepingQuartersView8SpriteSources[39] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 16, 818, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, -80, 750, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -64, 800, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -48, 786, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, -48, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 8, 784, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -128, 8, 750, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, 56, 782, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -128, 56, 750, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 8, 725, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 8, 700, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, 64, 725, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, -48, 725, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -48, 700, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, 64, 700, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, -80, 700, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, -80, 725, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 80, 796, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 48, 819, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 80, 925, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, 40, 907, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 72, 1028, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 40, 1045, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 40, 1189, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, 32, 1467, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -120, 770, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -72, 890, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, -24, 892, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 8, 829, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 40, 796, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 8, 1179, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 72, -120, 1271, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 72, -72, 1373, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 72, -24, 1507, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 8, 1457, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 96, -120, 983, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 96, -72, 1061, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, -24, 996, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -24, 1137, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
};

/// Sprite ranges layered over the decoded background of mapped view 8.
///
/// Ranges [0, 17) and [17, 39) cover the source array.
/// The first zero-count record retains image strips; the final record ends the list.
static SpriteBatch _gShelterB1SleepingQuartersView8SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 1, 0 } },
    { 17, ARRAY_SIZE(_gShelterB1SleepingQuartersView8SpriteSources) - 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches retaining decoded background strips for mapped view 9.
///
/// The zero-count first record selects the image; the second ends the list.
static SpriteBatch _gShelterB1SleepingQuartersView9SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 10's 57 room sprites.
///
/// Positions/sizes are pixels, UVs are texels and depth is an unscaled OT value.
/// The view's five nonempty batches cover every element. Cached packet creation
/// snapshots geometry and texture data; linking reads depths from this live array.
static SpriteSource _gShelterB1SleepingQuartersView10SpriteSources[57] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 64, 504, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 72, 503, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 80, 502, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 88, 500, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 80, 500, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -104, 72, 502, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -104, 96, 499, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 32 } }, -56, 64, 875, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -56, 96, 499, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 8, 96, 750, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 40, 88, 750, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 32 } }, 88, 88, 750, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 72, 48, 1311, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 72, 64, 1125, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, 72, 1000, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 112, 48, 850, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 48, 858, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 112, 64, 840, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 112, 80, 662, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 8, 56, 1033, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 16, 64, 1067, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 16, 72, 891, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 16, 80, 875, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 16, 88, 875, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 80, 843, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 64, 1026, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -64, 500, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -32, 500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -160, 0, 500, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -160, 32, 500, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -152, 56, 500, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -152, 72, 500, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 40, -72, 1125, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 48, -88, 1100, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 64, -88, 1050, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -96, -64, 1175, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -80, -64, 1200, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, -32, 1200, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 112, 48, 1130, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 104, -64, 1202, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 112, -40, 1189, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, 8, 1139, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, -16, 1139, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 112, -16, 1164, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, -40, 1189, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 120, -72, 1214, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 128, -80, 1189, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 128, -40, 1189, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 128, 0, 1139, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 128, 32, 1117, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 144, 24, 1105, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 144, -16, 1008, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 144, -56, 1214, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 144, -88, 1214, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, 32, 1130, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 80 } }, -24, -88, 519, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 48 } }, -8, -56, 517, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
};

/// Five contiguous sprite ranges layered over mapped view 10's background.
///
/// Ranges [0,26), [26,32), [32,38), [38,55) and [55,57) cover the sources.
/// The first zero-count record retains image strips and the final record ends
/// the list. The serialized `field_6` bytes are retained; their role is unproven.
static SpriteBatch _gShelterB1SleepingQuartersView10SpriteBatches[7] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 26, .field_6 = { 3, 0 } },
    { .firstSprite = 26, .spriteCount = 6, .field_6 = { 4, 0 } },
    { .firstSprite = 32, .spriteCount = 6, .field_6 = { 2, 0 } },
    { .firstSprite = 38, .spriteCount = 17, .field_6 = { 1, 0 } },
    { .firstSprite = 55, .spriteCount = ARRAY_SIZE(_gShelterB1SleepingQuartersView10SpriteSources) - 55 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty sprite batches retaining decoded background strips for mapped view 11.
///
/// The zero-count first record selects the image; the second ends the list.
static SpriteBatch _gShelterB1SleepingQuartersView11SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gShelterB1SleepingQuartersSpriteViews[11] = {
    { .sources = { .empty = _gShelterB1SleepingQuartersView1SpriteBatches }, .batches = _gShelterB1SleepingQuartersView1SpriteBatches, .drawAreas = NULL },
    { .sources = { .empty = _gShelterB1SleepingQuartersView2SpriteBatches }, .batches = _gShelterB1SleepingQuartersView2SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gShelterB1SleepingQuartersView3SpriteSources }, .batches = _gShelterB1SleepingQuartersView3SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gShelterB1SleepingQuartersView4SpriteSources }, .batches = _gShelterB1SleepingQuartersView4SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gShelterB1SleepingQuartersView5SpriteSources }, .batches = _gShelterB1SleepingQuartersView5SpriteBatches, .drawAreas = _gShelterB1SleepingQuartersView5SpriteDrawAreas },
    { .sources = { .elements = _gShelterB1SleepingQuartersView6SpriteSources }, .batches = _gShelterB1SleepingQuartersView6SpriteBatches, .drawAreas = _gShelterB1SleepingQuartersView6SpriteDrawAreas },
    { .sources = { .elements = _gShelterB1SleepingQuartersView7SpriteSources }, .batches = _gShelterB1SleepingQuartersView7SpriteBatches, .drawAreas = _gShelterB1SleepingQuartersView7SpriteDrawAreas },
    { .sources = { .elements = _gShelterB1SleepingQuartersView8SpriteSources }, .batches = _gShelterB1SleepingQuartersView8SpriteBatches, .drawAreas = NULL },
    { .sources = { .empty = _gShelterB1SleepingQuartersView9SpriteBatches }, .batches = _gShelterB1SleepingQuartersView9SpriteBatches, .drawAreas = NULL },
    { .sources = { .elements = _gShelterB1SleepingQuartersView10SpriteSources }, .batches = _gShelterB1SleepingQuartersView10SpriteBatches, .drawAreas = NULL },
    { .sources = { .empty = _gShelterB1SleepingQuartersView11SpriteBatches }, .batches = _gShelterB1SleepingQuartersView11SpriteBatches, .drawAreas = NULL },
};

/// Four directional model-shading sources active in every room view.
///
/// Translation supplies the direction; RGB intensity has 12 fractional bits.
/// The lighting task binds and composes transforms, and queries update attenuation.
static WorldCoordLight _gShelterB1SleepingQuartersDirectionalLights[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, 1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, -1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 409, 409, 409 }, { 0, 0 } },
};

/// Five point sources with room-space positions and distance falloff.
///
/// All views admit them; RGB intensity has 12 fractional bits and radii use game units.
/// The lighting task binds and composes transforms, and queries update attenuation.
static WorldCoordPointLight _gShelterB1SleepingQuartersPointLights[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4500, -2620, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 1339, 2360 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3200, -2159, -922 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 2220, 3442 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -766, -2000, -4618 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1319, 2059 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1240, -5082, 2801 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 7000, 7001 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3966, -2581, 2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1800, 3001 },
};

WorldCoordRoomLights gShelterB1SleepingQuartersRoomLights = {
    .directionalLightCount = ARRAY_SIZE(_gShelterB1SleepingQuartersDirectionalLights),
    .directionalLights     = _gShelterB1SleepingQuartersDirectionalLights,
    .pointLightCount       = ARRAY_SIZE(_gShelterB1SleepingQuartersPointLights),
    .pointLights           = _gShelterB1SleepingQuartersPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB1SleepingQuartersViewBoundaryTriggers[18] = {
    { NULL, NULL, NULL, { 1936, -2000, 1200, 0 }, { { 2224, -3360, -256, 0 }, { -2224, -3360, 256, 0 }, { 2224, 3360, -256, 0 }, { -2224, 3360, 256, 0 } }, { 469, 0, 4076, 0 }, { 0, 0, ONE, 0 }, 4031, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1983, -1728, 1280, 0 }, { { -2480, -3360, 288, 0 }, { 2480, -3360, -288, 0 }, { -2480, 3360, 288, 0 }, { 2480, 3360, -288, 0 } }, { -473, 0, -4069, 0 }, { 0, 0, ONE, 0 }, 4159, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3921, -1792, 3296, 0 }, { { 0, -3360, -2080, 0 }, { 0, -3360, 2080, 0 }, { 0, 3360, -2080, 0 }, { 0, 3360, 2080, 0 } }, { 4102, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3949, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4000, -1760, 3344, 0 }, { { 0, -3360, 2032, 0 }, { 0, -3360, -2032, 0 }, { 0, 3360, 2032, 0 }, { 0, 3360, -2032, 0 } }, { -4103, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3924, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7424, -1793, 4176, 0 }, { { 0, -3360, -1232, 0 }, { 0, -3360, 1232, 0 }, { 0, 3360, -1232, 0 }, { 0, 3360, 1232, 0 } }, { 4097, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3574, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7552, -1760, 4192, 0 }, { { 0, -3360, 1232, 0 }, { 0, -3360, -1232, 0 }, { 0, 3360, 1232, 0 }, { 0, 3360, -1232, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3574, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9856, -1792, 4624, 0 }, { { 0, -3360, 1760, 0 }, { 0, -3360, -1760, 0 }, { 0, 3360, 1760, 0 }, { 0, 3360, -1760, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3788, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9760, -1793, 4560, 0 }, { { 0, -3360, -1760, 0 }, { 0, -3360, 1760, 0 }, { 0, 3360, -1760, 0 }, { 0, 3360, 1760, 0 } }, { 4098, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3788, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2CA0, -1792, 5792, 0 }, { { -1088, -3360, 288, 0 }, { 1088, -3360, -288, 0 }, { -1088, 3360, 288, 0 }, { 1088, 3360, -288, 0 } }, { -1049, 0, -3961, 0 }, { 0, 0, ONE, 0 }, 3537, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2C71, -1761, 5744, 0 }, { { 1104, -3360, -304, 0 }, { -1104, -3360, 304, 0 }, { 1104, 3360, -304, 0 }, { -1104, 3360, 304, 0 } }, { 1087, 0, 3950, 0 }, { 0, 0, ONE, 0 }, 3547, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 545, -1792, -784, 0 }, { { 0, -3360, -1792, 0 }, { 0, -3360, 1792, 0 }, { 0, 3360, -1792, 0 }, { 0, 3360, 1792, 0 } }, { 4108, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3805, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -64, -1888, -2641, 0 }, { { -704, -3360, -128, 0 }, { 704, -3360, 128, 0 }, { -704, 3360, -128, 0 }, { 704, 3360, 128, 0 } }, { 733, 0, -4033, 0 }, { 0, 0, ONE, 0 }, 3434, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 48, -1952, -2768, 0 }, { { 816, -3360, 176, 0 }, { -816, -3360, -176, 0 }, { 816, 3360, 176, 0 }, { -816, 3360, -176, 0 } }, { -867, 0, 4011, 0 }, { 0, 0, ONE, 0 }, 3453, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 720, -1856, -832, 0 }, { { 0, -3360, 1840, 0 }, { 0, -3360, -1840, 0 }, { 0, 3360, 1840, 0 }, { 0, 3360, -1840, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3822, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5760, -1568, 1920, 0 }, { { -704, -3360, 0, 0 }, { 704, -3360, 0, 0 }, { -704, 3360, 0, 0 }, { 704, 3360, 0, 0 } }, { 0, 0, -4106, 0 }, { 0, 0, ONE, 0 }, 3425, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5776, -1536, 1792, 0 }, { { 816, -3360, 0, 0 }, { -816, -3360, 0, 0 }, { 816, 3360, 0, 0 }, { -816, 3360, 0, 0 } }, { 0, 0, 4101, 0 }, { 0, 0, ONE, 0 }, 3453, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8864, -1600, 2848, 0 }, { { -864, -3360, 0, 0 }, { 864, -3360, 0, 0 }, { -864, 3360, 0, 0 }, { 864, 3360, 0, 0 } }, { 0, 0, -4106, 0 }, { 0, 0, ONE, 0 }, 3463, 0, 10, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8880, -1632, 2752, 0 }, { { 896, -3360, 0, 0 }, { -896, -3360, 0, 0 }, { 896, 3360, 0, 0 }, { -896, 3360, 0, 0 } }, { 0, 0, 4108, 0 }, { 0, 0, ONE, 0 }, 3472, 0, 7, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder gShelterB1SleepingQuartersOccluders[5] = {
    { NULL, NULL, { 4944, -2240, -1152, 0 }, { { -16, 3264, 3008, 0 }, { 16, 3264, -3008, 0 }, { -16, -3264, 3008, 0 }, { 16, -3264, -3008, 0 } }, { 4111, 0, 21, 0 }, 4434, 1, 0 },
    { NULL, NULL, { 6848, -2176, -128, 0 }, { { -16, 3200, 3008, 0 }, { 16, 3200, -3008, 0 }, { -16, -3200, 3008, 0 }, { 16, -3200, -3008, 0 } }, { 4102, 0, 21, 0 }, 4374, 1, 0 },
    { NULL, NULL, { 0x2D00, -2112, 2528, 0 }, { { -1968, 3136, 0, 0 }, { 1968, 3136, 0, 0 }, { -1968, -3136, 0, 0 }, { 1968, -3136, 0, 0 } }, { 0, 0, 4103, 0 }, 3700, 1, 0 },
    { NULL, NULL, { 7536, -2176, 6304, 0 }, { { -32, 3200, 960, 0 }, { 32, 3200, -960, 0 }, { -32, -3200, 960, 0 }, { 32, -3200, -960, 0 } }, { 4100, 0, 136, 0 }, 3337, 1, 0 },
    { NULL, NULL, { 7262, -2048, 2949, 0 }, { { -1008, 3264, 0, 0 }, { 1008, 3264, 0, 0 }, { -1008, -3264, 0, 0 }, { 1008, -3264, 0, 0 } }, { 0, 0, 4102, 0 }, 3415, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Packed warp slots: current-area departure in bits 4..7, destination arrival in 0..3.
enum {
    SHELTER_B1_SLEEPING_QUARTERS_CORRIDOR_WARP_SLOTS = (1 << 4) | 5,
    SHELTER_B1_SLEEPING_QUARTERS_WALKWAY_WARP_SLOTS  = (2 << 4) | 1,
};

/// Room-message selectors for flag-dependent playback and the alternate CAP file.
enum {
    SHELTER_B1_SLEEPING_QUARTERS_CAP_FLAG_EVENT     = 3,
    SHELTER_B1_SLEEPING_QUARTERS_CAP_ALTERNATE_FILE = 8,
};

WorldCollisionTrigger gShelterB1SleepingQuartersActionTriggers[17] = {
    { NULL, NULL, NULL, { -96, -48, -992, 0 }, { { -512, 0, -960, 0 }, { 512, 0, -960, 0 }, { -512, 0, 960, 0 }, { 512, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, SHELTER_B1_SLEEPING_QUARTERS_CORRIDOR_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2DD0, -48, 6288, 0 }, { { 720, 0, -352, 0 }, { 720, 0, 352, 0 }, { -720, 0, -352, 0 }, { -720, 0, 352, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -ONE, 0 }, 799, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, SHELTER_B1_SLEEPING_QUARTERS_WALKWAY_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1008, -64, 2848, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_SLEEPING_QUARTERS_CAP_FLAG_EVENT, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 960, -64, 5024, 0 }, { { -400, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 400, 0, 1024, 0 } }, { 0, 4098, 0, 0 }, { ONE, 0, 0, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2496, -64, 4704, 0 }, { { -1024, 0, 400, 0 }, { -1024, 0, -400, 0 }, { 1024, 0, 400, 0 }, { 1024, 0, -400, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -ONE, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5152, -64, 4720, 0 }, { { -1024, 0, 416, 0 }, { -1024, 0, -416, 0 }, { 1024, 0, 416, 0 }, { 1024, 0, -416, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -ONE, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2432, -64, -1664, 0 }, { { -384, 0, 1392, 0 }, { -384, 0, -1392, 0 }, { 384, 0, 1392, 0 }, { 384, 0, -1392, 0 } }, { 0, 4111, 0, 0 }, { -ONE, 0, 0, 0 }, 1442, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1072, -64, -2752, 0 }, { { -1552, 0, 368, 0 }, { -1552, 0, -368, 0 }, { 1552, 0, 368, 0 }, { 1552, 0, -368, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, ONE, 0 }, 1593, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9216, -64, 5632, 0 }, { { -1264, 0, 336, 0 }, { -1264, 0, -336, 0 }, { 1264, 0, 336, 0 }, { 1264, 0, -336, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -ONE, 0 }, 1305, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2F90, -64, 3792, 0 }, { { -512, 0, 832, 0 }, { -512, 0, -832, 0 }, { 512, 0, 832, 0 }, { 512, 0, -832, 0 } }, { 0, 4095, 0, 0 }, { -ONE, 0, 0, 0 }, 976, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2F80, -64, 5168, 0 }, { { -496, 0, 480, 0 }, { -496, 0, -480, 0 }, { 496, 0, 480, 0 }, { 496, 0, -480, 0 } }, { 0, 4098, 0, 0 }, { -ONE, 0, 0, 0 }, 689, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_SLEEPING_QUARTERS_CAP_ALTERNATE_FILE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2C60, -64, 1776, 0 }, { { -1264, 0, 320, 0 }, { -1264, 0, -320, 0 }, { 1264, 0, 320, 0 }, { 1264, 0, -320, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -ONE, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2F60, -64, 6080, 0 }, { { -512, 0, 416, 0 }, { -512, 0, -416, 0 }, { 512, 0, 416, 0 }, { 512, 0, -416, 0 } }, { 0, 4095, 0, 0 }, { -ONE, 0, 0, 0 }, 658, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3440, -64, 544, 0 }, { { -400, 0, -800, 0 }, { 400, 0, -800, 0 }, { -400, 0, 800, 0 }, { 400, 0, 800, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 893, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5776, -64, 848, 0 }, { { -576, 0, -368, 0 }, { 576, 0, -368, 0 }, { -576, 0, 368, 0 }, { 576, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, ONE, 0 }, 683, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2780, -64, 224, 0 }, { { -2656, 0, -368, 0 }, { 2656, 0, -368, 0 }, { -2656, 0, 368, 0 }, { 2656, 0, 368, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, ONE, 0 }, 2672, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9984, -64, 3296, 0 }, { { -480, 0, -368, 0 }, { 480, 0, -368, 0 }, { -480, 0, 368, 0 }, { 480, 0, 368, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, ONE, 0 }, 603, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Placement-to-resource keys shared by this room's layout tables.
enum {
    SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_RAT               = 7,
    SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_ODD_STRANGER      = 18,
    SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_GOLEM_KNIGHT      = 22,
    SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_AMOEBA            = 24,
    SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_GOLEM_BISHOP      = 39,
    SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_MAD_CHASER        = 44,
    SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_CREEPING_STRANGER = 72,
};

/// Variant 1's Odd Stranger and rat file/task recipes, ending with AREA_PLACEMENT_END.
///
/// Resource keys match the placements; task descriptors are borrowed from loaded
/// actor packages. File-group bases 40 and 30 select their base files.
static AreaResource _gShelterB1SleepingQuartersVariant1Resources[3] = {
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_ODD_STRANGER, .fileNumber = 18, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor401800OddStrangerTaskDesc },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_RAT, .fileNumber = 7, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .taskTable = &gActor300700RatTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's amoeba file and body-task recipe, followed by AREA_PLACEMENT_END.
///
/// Base 10/file 24 loads actor_102400, an instance of the amoeba implementation;
/// descriptor 0 selects its body. The loaded actor package owns the task table.
static AreaResource _gShelterB1SleepingQuartersVariant2Resources[2] = {
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_AMOEBA, .fileNumber = 24, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor102400Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 3's Mad Chaser and Creeping Stranger file/task recipes and terminator.
///
/// Placement keys select base-10/file-44 and base-20/file-72 recipes respectively.
/// Task descriptors are borrowed from the loaded actor packages.
static AreaResource _gShelterB1SleepingQuartersVariant3Resources[3] = {
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_MAD_CHASER, .fileNumber = 44, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = ACTOR_04400_MAD_CHASER_TASK_ORDINARY, .taskTable = gActor04400MadChaserTaskDescs },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_CREEPING_STRANGER, .fileNumber = 72, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = &gActor207200CreepingStrangerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 11's GOLEM Knight file/task recipe, followed by AREA_PLACEMENT_END.
///
/// Base 40/file 22 selects actor_402200; the live actor package owns descriptor 0.
static AreaResource _gShelterB1SleepingQuartersVariant11Resources[2] = {
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_GOLEM_KNIGHT, .fileNumber = 22, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor402200GolemTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 17's GOLEM Bishop file/task recipe, followed by AREA_PLACEMENT_END.
///
/// Base 40/file 39 selects actor_403900; the live actor package owns descriptor 0.
static AreaResource _gShelterB1SleepingQuartersVariant17Resources[2] = {
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_GOLEM_BISHOP, .fileNumber = 39, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor403900GolemTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Odd Stranger spawn selectors: stalking behaviour and the third tuning record.
enum {
    SHELTER_B1_SLEEPING_QUARTERS_ODD_STRANGER_STALK        = 0x10,
    SHELTER_B1_SLEEPING_QUARTERS_ODD_STRANGER_TUNING_THIRD = 1,
};

/// Variant 1's Odd Stranger and four rats in world units, yaw 4096 per turn.
///
/// Entries use base files only; rats select tuning row 4 and texture offsets
/// (page +2, CLUT row +4). AREA_PLACEMENT_END stops the walk. Live actors borrow
/// their records until teardown, requiring the room to remain loaded.
static AreaPlacement _gShelterB1SleepingQuartersVariant1Placements[6] = {
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_ODD_STRANGER, .variant = SHELTER_B1_SLEEPING_QUARTERS_ODD_STRANGER_STALK, .mode = SHELTER_B1_SLEEPING_QUARTERS_ODD_STRANGER_TUNING_THIRD, .x = 1950, .y = 0, .z = -100, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 1100, .y = 0, .z = 4700, .yaw = 2750, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 4 },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 1200, .y = 0, .z = 4300, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 4 },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 1100, .y = 0, .z = 3900, .yaw = 2500, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 4 },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 1350, .y = 0, .z = 3650, .yaw = 3400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 4 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's seven amoebae in world units, yaw 4096 per turn, and terminator.
///
/// Uses the base file with texture page unchanged and CLUT row +2. The first
/// five placements select parameter set 0; the last two select set 1, which
/// also adds one CLUT row during actor initialization.
/// Live actors borrow their records until teardown; keep the room loaded.
static AreaPlacement _gShelterB1SleepingQuartersVariant2Placements[8] = {
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_AMOEBA, .variant = 0, .mode = 0, .x = 3450, .y = 0, .z = 50, .yaw = 3400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_AMOEBA, .variant = 0, .mode = 0, .x = -250, .y = 0, .z = 500, .yaw = 1600, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_AMOEBA, .variant = 0, .mode = 0, .x = 7950, .y = 0, .z = 550, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_AMOEBA, .variant = 0, .mode = 0, .x = 9350, .y = 0, .z = 440, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_AMOEBA, .variant = 0, .mode = 0, .x = 0x29FE, .y = 0, .z = 550, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_AMOEBA, .variant = 0, .mode = 1, .x = 0x300C, .y = 0, .z = 400, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_AMOEBA, .variant = 0, .mode = 1, .x = 5750, .y = 0, .z = 1100, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 3's two Mad Chasers and two Creeping Strangers, followed by a terminator.
///
/// Positions use world units and yaw uses 4096 per turn. Base-file placements
/// relocate Chaser CLUTs by two rows; Strangers use page +2 and CLUT row +4.
/// Live actors borrow their records until teardown; keep the room loaded.
static AreaPlacement _gShelterB1SleepingQuartersVariant3Placements[5] = {
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_MAD_CHASER, .variant = 0, .mode = 1, .x = 0x2904, .y = -3400, .z = 4000, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_MAD_CHASER, .variant = 0, .mode = 1, .x = 1800, .y = -3000, .z = 2200, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_CREEPING_STRANGER, .variant = 0, .mode = 0, .x = 9200, .y = 0, .z = 500, .yaw = -100, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_CREEPING_STRANGER, .variant = 0, .mode = 0, .x = 7650, .y = 0, .z = 4150, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 11's single GOLEM Knight at world (3000, 0, 3600), yaw half a turn.
///
/// Base-file placement with CLUT row +2; AREA_PLACEMENT_END ends the list.
/// The actor borrows its placement until teardown, so keep the room loaded.
static AreaPlacement _gShelterB1SleepingQuartersVariant11Placements[2] = {
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_GOLEM_KNIGHT, .variant = 0, .mode = 0, .x = 3000, .y = 0, .z = 3600, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 17's single GOLEM Bishop at world (3000, 0, 3600), yaw half a turn.
///
/// Base-file placement with CLUT row +2; AREA_PLACEMENT_END ends the list.
/// The actor borrows its placement until teardown, so keep the room loaded.
static AreaPlacement _gShelterB1SleepingQuartersVariant17Placements[2] = {
    { .entryId = SHELTER_B1_SLEEPING_QUARTERS_RESOURCE_GOLEM_BISHOP, .variant = 0, .mode = 0, .x = 3000, .y = 0, .z = 3600, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gShelterB1SleepingQuartersAreaVariants[22] = {
    [1]  = { .placements = _gShelterB1SleepingQuartersVariant1Placements, .resources = _gShelterB1SleepingQuartersVariant1Resources },
    [2]  = { .placements = _gShelterB1SleepingQuartersVariant2Placements, .resources = _gShelterB1SleepingQuartersVariant2Resources },
    [3]  = { .placements = _gShelterB1SleepingQuartersVariant3Placements, .resources = _gShelterB1SleepingQuartersVariant3Resources },
    [11] = { .placements = _gShelterB1SleepingQuartersVariant11Placements, .resources = _gShelterB1SleepingQuartersVariant11Resources },
    [17] = { .placements = _gShelterB1SleepingQuartersVariant17Placements, .resources = _gShelterB1SleepingQuartersVariant17Resources },
};

enum {
    SHELTER_B1_SLEEPING_QUARTERS_WALK_FOOTSTEP_SCRIPT = 0x15,
    SHELTER_B1_SLEEPING_QUARTERS_RUN_FOOTSTEP_SCRIPT  = 0x17
};

/// Walk, run and scripted-jump cue bases for room surface class 2.
///
/// Type-1 requests select the loaded bank; animation cue 1 adds one to each base.
/// Scripted jumps reuse the walk pair. The surface borrows this record while loaded.
static WorldCollisionFootstepSounds _gShelterB1SleepingQuartersFootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B1_SLEEPING_QUARTERS_WALK_FOOTSTEP_SCRIPT,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B1_SLEEPING_QUARTERS_RUN_FOOTSTEP_SCRIPT,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B1_SLEEPING_QUARTERS_WALK_FOOTSTEP_SCRIPT,
};

/// Silent blocking policy shared by room surface classes 0 and 3..7.
static WorldCollisionSurfaceProperties _gShelterB1SleepingQuartersDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1's silent policy: passes probes, ignores impacts and applies pushback.
static WorldCollisionSurfaceProperties _gShelterB1SleepingQuartersProbePassSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 2's blocking, impact-enabled policy with the room's footstep cue pairs.
static WorldCollisionSurfaceProperties _gShelterB1SleepingQuartersFootstepSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB1SleepingQuartersFootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB1SleepingQuartersSurfacePropertiesByClass[8] = {
    &_gShelterB1SleepingQuartersDefaultSurfaceProperties,
    &_gShelterB1SleepingQuartersProbePassSurfaceProperties,
    &_gShelterB1SleepingQuartersFootstepSurfaceProperties,
    &_gShelterB1SleepingQuartersDefaultSurfaceProperties,
    &_gShelterB1SleepingQuartersDefaultSurfaceProperties,
    &_gShelterB1SleepingQuartersDefaultSurfaceProperties,
    &_gShelterB1SleepingQuartersDefaultSurfaceProperties,
    &_gShelterB1SleepingQuartersDefaultSurfaceProperties,
};

void shelterB1SleepingQuartersRoomVisualEffectsGlowDiscTask(Task* task)
{
    _roomVisualEffectsGlowDiscTask(task);
}

void shelterB1SleepingQuartersRoomVisualEffectsFlyingSparkTask(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room/room_visual_effects_burst.inc.c"

void shelterB1SleepingQuartersRoomVisualEffectsFlyingOrangeBurstTask(Task* task)
{
    _roomVisualEffectsFlyingOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_burst_draw.inc.c"
