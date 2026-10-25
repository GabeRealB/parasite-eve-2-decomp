#include "actors/actor_04400.h"
#include "actors/actor_02400.h"
#include "actors/actor_02600.h"
#include "actors/actor_03800.h"
#include "actors/actor_07000.h"
#include "actors/actor_207200.h"
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
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/shelter_b2_breeding_room.h"

#include "shelter_b2_breeding_room_private.h"

#include "common.h"

#include "../../shared/room/room_visual_effects.h"

static RoomFxShade _gRoomVisualEffectsGlowDiscShades[2];

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flying_tasks.inc.c"

#include "../../shared/room/room_visual_effects_disc_data.inc.c"

u8* gShelterB2BreedingRoomRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB2BreedingRoomRoomViewCounts[1] = { 10 };

/// One-based door IDs shared by this room's endpoints and action triggers.
enum {
    SHELTER_B2_BREEDING_ROOM_WARP_MAIN_CORRIDOR             = 1,
    SHELTER_B2_BREEDING_ROOM_WARP_NORTH_MAINTENANCE_WALKWAY = 2,
};

/// Arrival/departure script requests in this room's area sound bank.
enum {
    SHELTER_B2_BREEDING_ROOM_MAIN_CORRIDOR_ARRIVAL_SOUND       = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, 4),
    SHELTER_B2_BREEDING_ROOM_MAIN_CORRIDOR_DEPARTURE_SOUND     = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, 3),
    SHELTER_B2_BREEDING_ROOM_NORTH_MAINTENANCE_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, 2),
    SHELTER_B2_BREEDING_ROOM_NORTH_MAINTENANCE_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, 1),
};

DirectionWarpEntry gShelterB2BreedingRoomWarpEntries[2] = {
    [SHELTER_B2_BREEDING_ROOM_WARP_MAIN_CORRIDOR - 1] = {
        .player         = { { .word = 1024 }, 100, 0, -900 },
        .companion      = { { .word = 1024 }, 100, 0, -900 },
        .arrivalSound   = SHELTER_B2_BREEDING_ROOM_MAIN_CORRIDOR_ARRIVAL_SOUND,
        .departureSound = SHELTER_B2_BREEDING_ROOM_MAIN_CORRIDOR_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    [SHELTER_B2_BREEDING_ROOM_WARP_NORTH_MAINTENANCE_WALKWAY - 1] = {
        .player         = { { .word = 2048 }, 11934, 0, 5960 },
        .companion      = { { .word = 2048 }, 11934, 0, 5960 },
        .arrivalSound   = SHELTER_B2_BREEDING_ROOM_NORTH_MAINTENANCE_ARRIVAL_SOUND,
        .departureSound = SHELTER_B2_BREEDING_ROOM_NORTH_MAINTENANCE_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 6,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Room-space collision normals, with 4096 representing unit length.
static SVECTOR _gShelterB2BreedingRoomCollisionGridNormals[27] = {
#include "assets/shelter_b2_breeding_room_collision_03B34_normals.inc"
};

/// Collision vertices in signed room-coordinate units.
static SVECTOR _gShelterB2BreedingRoomCollisionGridVerts[129] = {
#include "assets/shelter_b2_breeding_room_collision_03B34_verts.inc"
};

/// Indexed collision faces carrying room-local surface classes 0..4.
static WorldCollisionGridFace _gShelterB2BreedingRoomCollisionGridFaces[75] = {
#include "assets/shelter_b2_breeding_room_collision_03B34_faces.inc"
};

/// Packed cell face-index lists, each ending with `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gShelterB2BreedingRoomCollisionGridCells[326] = {
#include "assets/shelter_b2_breeding_room_collision_03B34_cells.inc"
};

/// Resolves a generated halfword offset to a breeding-room cell's face-index list.
///
/// Captures `_gShelterB2BreedingRoomCollisionGridCells`, returning a borrowed
/// `s16*`. `halfwordOffset` is evaluated once; these initializers require a
/// constant list start in 0..325, with `WORLD_COLLISION_GRID_CELL_END` inside
/// the pool. The grid's `cell_face_list_macro` manifest entry selects this
/// binding for its table include. Keep the pool live while the grid is active.
#define SHELTER_B2_BREEDING_ROOM_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB2BreedingRoomCollisionGridCells[(halfwordOffset)])

/// Twelve cell-list pointers in X-major order, with three Z cells per X row.
static s16* _gShelterB2BreedingRoomCollisionGridTable[12] = {
#include "assets/shelter_b2_breeding_room_collision_03B34_table.inc"
};
#undef SHELTER_B2_BREEDING_ROOM_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB2BreedingRoomCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB2BreedingRoomCollisionGridNormals,
    .vertices    = _gShelterB2BreedingRoomCollisionGridVerts,
    .faces       = _gShelterB2BreedingRoomCollisionGridFaces,
    .cellFaceIds = _gShelterB2BreedingRoomCollisionGridTable,
    .xBias       = 890,
    .zBias       = 2602,
    .cellCountX  = 4,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB2BreedingRoomCollisionGridFaces),
};

ViewCamera gShelterB2BreedingRoomViewCameras[10] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -6200, 0x7530, -1480 } }, 564 },
    { { { { -3857, 0, 1377 }, { 1258, 1663, 3524 }, { -559, 3742, -1566 } }, { -1443, 4445, -344 } }, 230 },
    { { { { -4069, 0, -460 }, { 13, 4094, -119 }, { 460, -120, -4068 } }, { -1354, 1031, -4688 } }, 230 },
    { { { { 4052, 0, -595 }, { 112, 4022, 764 }, { 585, -773, 3979 } }, { -1354, 531, -488 } }, 230 },
    { { { { 260, 0, 4087 }, { 199, 4091, -12 }, { -4082, 199, 260 } }, { -0x2884, 1364, -4115 } }, 230 },
    { { { { 367, 0, -4079 }, { -1222, 3907, -110 }, { 3892, 1227, 350 } }, { -5472, 2264, -4115 } }, 230 },
    { { { { 4020, 0, -783 }, { -514, 3087, -2642 }, { 590, 2691, 3030 } }, { -6295, 3146, 279 } }, 230 },
    { { { { -4003, 0, -863 }, { -612, 2887, 2839 }, { 609, 2905, -2822 } }, { -6323, 3121, -2765 } }, 230 },
    { { { { 3946, 0, -1096 }, { -1080, 704, -3887 }, { 188, 4035, 678 } }, { -0x2ACC, 5781, -614 } }, 230 },
    { { { { 3949, 0, 1087 }, { 713, 3090, -2591 }, { -820, 2687, 2979 } }, { -2556, 1291, -4594 } }, 505 },
};

/// Empty sprite batches for mapped view 1, retaining decoded background strips.
///
/// The zero-count header supplies no sources; the following sentinel ends the
/// list. Its address is also retained in the view's unused source slot.
static SpriteBatch _gShelterB2BreedingRoomView1SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 2's room sprites.
///
/// Positions and sizes are pixels, UVs are texels and depths are unscaled OT
/// sorting values. RGB 128 is neutral modulation.
/// Every rectangle uses opaque emission.
/// The paired batch list indexes all 20 elements; storage lives with the room.
static SpriteSource _gShelterB2BreedingRoomView2SpriteSources[20] = {
    { 143, 0x3FC0, { .fields = { 64, 88 } }, -160, -120, 550, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 72 } }, -96, -120, 550, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 24 } }, -104, 96, 400, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 48 } }, -40, 72, 400, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -40, 933, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -48, -48, 949, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, -16, 1138, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -48, 1099, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -32, 1068, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -56, 1117, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, -32, 973, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -16, 934, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 0, 996, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -16, 1113, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 0, 1080, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -128, 0, 1076, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -16, 1097, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, -32, 1067, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, -24, 1122, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, -32, 1048, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
};

/// Source ranges for mapped view 2's 20 depth-sorted sprites.
///
/// The zero-count header retains decoded background strips. The remaining
/// ranges cover the complete source array and end with `SPRITE_BATCH_END`;
/// hidden and excluded flags start clear. The trailing bytes' role is unproven.
static SpriteBatch _gShelterB2BreedingRoomView2SpriteBatches[4] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 4, .field_6 = { 1, 0 } },
    { .firstSprite = 4, .spriteCount = ARRAY_SIZE(_gShelterB2BreedingRoomView2SpriteSources) - 4, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 3's room sprites.
///
/// Positions and sizes are pixels, UVs are texels and depths are unscaled OT
/// sorting values. RGB 128 is neutral modulation.
/// Every rectangle uses opaque emission.
/// The paired batch list indexes all 39 elements; storage lives with the room.
static SpriteSource _gShelterB2BreedingRoomView3SpriteSources[39] = {
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 40, 40, 781, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 48, 40, 763, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, 0, 1019, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 48, -64, 1014, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, 56, -96, 750, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, 56, 0, 750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 72, 0, 625, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 72, -120, 625, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 120 } }, 128, 0, 462, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 120 } }, 96, 0, 462, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 120 } }, 96, -120, 462, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 120 } }, 128, -120, 462, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 128 } }, 120, -120, 297, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 72 } }, 80, -64, 297, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 72 } }, -136, -88, 710, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 72 } }, -112, -88, 752, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -96, -80, 791, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -64, 787, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 56 } }, -104, -72, 906, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 56 } }, -80, -72, 885, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 56 } }, -64, -72, 873, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, -16, 382, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 80, 0, 367, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 8, 358, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 16, 353, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 24, 345, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 24, 333, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 88, 32, 331, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 8, -120, 420, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 16, -120, 415, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 32, -120, 416, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 88 } }, 48, -120, 425, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 88 } }, 72, -120, 456, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 104 } }, -160, -104, 1000, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 112 } }, -160, 0, 1000, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 88 } }, -128, -88, 1250, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 96 } }, -128, 0, 1250, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 72 } }, -96, -72, 1500, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 80 } }, -96, 0, 1500, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
};

/// Source ranges for mapped view 3's 39 depth-sorted sprites.
///
/// The zero-count header retains decoded background strips. The remaining
/// ranges cover the complete source array and end with `SPRITE_BATCH_END`;
/// hidden and excluded flags start clear. The trailing bytes' role is unproven.
static SpriteBatch _gShelterB2BreedingRoomView3SpriteBatches[9] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 12, .field_6 = { 3, 0 } },
    { .firstSprite = 12, .spriteCount = 2, .field_6 = { 4, 0 } },
    { .firstSprite = 14, .spriteCount = 4, .field_6 = { 1, 0 } },
    { .firstSprite = 18, .spriteCount = 3, .field_6 = { 5, 0 } },
    { .firstSprite = 21, .spriteCount = 7, .field_6 = { 0, 0 } },
    { .firstSprite = 28, .spriteCount = 5, .field_6 = { 6, 0 } },
    { .firstSprite = 33, .spriteCount = ARRAY_SIZE(_gShelterB2BreedingRoomView3SpriteSources) - 33, .field_6 = { 2, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 4's room sprites.
///
/// Positions and sizes are pixels, UVs are texels and depths are unscaled OT
/// sorting values. RGB 128 is neutral modulation.
/// Every rectangle uses opaque emission.
/// The paired batch list indexes all 10 elements; storage lives with the room.
static SpriteSource _gShelterB2BreedingRoomView4SpriteSources[10] = {
    { 143, 0x3FC0, { .fields = { 16, 192 } }, 112, -104, 671, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, 128, -120, 608, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 120 } }, 128, -16, 581, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -88, -120, 500, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, -104, 500, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 80 } }, -160, -120, 500, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -32, -96, 670, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -48, -104, 641, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -64, -112, 600, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -88, -112, 560, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
};

/// Source ranges for mapped view 4's 10 depth-sorted sprites.
///
/// The zero-count header retains decoded background strips. The remaining
/// ranges cover the complete source array and end with `SPRITE_BATCH_END`;
/// hidden and excluded flags start clear. The trailing bytes' role is unproven.
static SpriteBatch _gShelterB2BreedingRoomView4SpriteBatches[5] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 3, .field_6 = { 1, 0 } },
    { .firstSprite = 3, .spriteCount = 3, .field_6 = { 2, 0 } },
    { .firstSprite = 6, .spriteCount = ARRAY_SIZE(_gShelterB2BreedingRoomView4SpriteSources) - 6, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 5's room sprites.
///
/// Positions and sizes are pixels, UVs are texels and depths are unscaled OT
/// sorting values. RGB 128 is neutral modulation.
/// 18 rectangles use semitransparent emission; the remainder are opaque.
/// The paired batch list indexes all 69 elements; storage lives with the room.
static SpriteSource _gShelterB2BreedingRoomView5SpriteSources[69] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 48, 1080, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 56, 48, 1130, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, -120, 484, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, -120, 499, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -104, 1004, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -96, 1003, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -72, 1020, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, -160, -24, 476, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -152, 112, 580, { .fields = { 40, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -152, 104, 607, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -152, 96, 633, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -144, 88, 671, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -136, 80, 694, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, -112, 548, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -104, 529, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -160, -96, 593, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -152, -88, 647, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, -120, 592, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -136, -120, 628, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -128, -120, 655, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -120, 688, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -136, -80, 705, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -136, 8, 703, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 192 } }, -112, -112, 708, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, -104, -104, 799, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -96, 8, 822, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -96, -120, 1051, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -96, -112, 1051, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -96, -96, 873, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -88, 982, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, -80, 1196, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -80, -104, 1208, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -72, -64, 1278, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, -64, 1137, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, -56, 1207, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, 16, 1278, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -72, -16, 1280, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -72, -48, 1272, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -96, 40, 1177, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -96, -64, 1120, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -88, -40, 1199, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -88, -24, 1218, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -88, -8, 1260, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -88, 16, 1233, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -96, 56, 1175, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -64, -112, 1526, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -56, -48, 1727, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 96 } }, -112, -112, 1475, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 64 } }, -112, -16, 1475, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 96 } }, -160, -112, 1500, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 64 } }, -160, -16, 1500, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -128, -120, 612, { .fields = { 40, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, -88, -64, 1133, { .fields = { 72, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -88, 8, 1148, { .fields = { 56, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 104 } }, -80, -56, 1276, { .fields = { 80, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 16 } }, -120, -120, 676, { .fields = { 80, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 24 } }, -112, -120, 734, { .fields = { 88, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 32 } }, -104, -120, 803, { .fields = { 120, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, -96, -120, 886, { .fields = { 32, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -88, -120, 989, { .fields = { 64, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -80, -112, 1118, { .fields = { 48, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 120 } }, -160, -96, 577, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 88 } }, -160, 24, 576, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 112 } }, -144, -88, 643, { .fields = { 88, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 80 } }, -144, 24, 642, { .fields = { 120, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 104 } }, -128, -80, 705, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 64 } }, -128, 24, 703, { .fields = { 112, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 80 } }, -120, -56, 747, { .fields = { 0, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, -120, 24, 753, { .fields = { 120, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Source ranges for mapped view 5's 69 depth-sorted sprites.
///
/// The zero-count header retains decoded background strips. The remaining
/// ranges cover the complete source array and end with `SPRITE_BATCH_END`;
/// hidden and excluded flags start clear. The trailing bytes' role is unproven.
static SpriteBatch _gShelterB2BreedingRoomView5SpriteBatches[7] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 2, .field_6 = { 0, 0 } },
    { .firstSprite = 2, .spriteCount = 33, .field_6 = { 3, 0 } },
    { .firstSprite = 35, .spriteCount = 10, .field_6 = { 2, 0 } },
    { .firstSprite = 45, .spriteCount = 6, .field_6 = { 4, 0 } },
    { .firstSprite = 51, .spriteCount = ARRAY_SIZE(_gShelterB2BreedingRoomView5SpriteSources) - 51, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 6's room sprites.
///
/// Positions and sizes are pixels, UVs are texels and depths are unscaled OT
/// sorting values. RGB 128 is neutral modulation.
/// 11 rectangles use semitransparent emission; the remainder are opaque.
/// The paired batch list indexes all 77 elements; storage lives with the room.
static SpriteSource _gShelterB2BreedingRoomView6SpriteSources[77] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 16, 1338, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -56, 16, 1238, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -80, 16, 1075, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -80, 24, 1029, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -80, 40, 1138, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -80, -8, 1154, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -56, 1193, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, -88, 1715, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -24, 1835, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 56, 16, 1490, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 64, -32, 1497, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 64, -88, 1534, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -56, 508, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 64, 964, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 128, 88, 554, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 48, 525, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 8, 503, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -24, 487, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 112, 735, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 96, 104, 765, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 96, 96, 785, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 96, 88, 802, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 88, 80, 823, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 64, 918, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, 56, 760, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 48, 956, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 32, 955, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 16, 1179, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 96, 32, 732, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -88, 1631, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, -88, 1030, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 0, 888, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 104, 0, 708, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, 0, 1030, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, -16, 983, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, -16, 851, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 80, -72, 988, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 104, -16, 684, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 96, -40, 815, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 96, -64, 822, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -96, 810, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -96, 724, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, -104, 643, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 136, -120, 563, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, -104, 524, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 112, -72, 598, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 112, -64, 597, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 112, -56, 639, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 112, -32, 660, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 88, -120, 192, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, 32, -104, 198, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -160, -104, 211, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 16 } }, -72, -104, 204, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 40, -96, 764, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -48, -88, 780, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 16 } }, -120, -96, 812, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -32, -80, 1816, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -120, 942, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 120, -72, 958, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -24, 958, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, 32, 958, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 64, 888, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -80, 56, 879, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 80, 836, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 80, 830, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, 64, 872, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 88, -72, 1154, { .fields = { 56, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, 80, 16, 1179, { .fields = { 16, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 72 } }, 152, -56, 584, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 112, 48, 809, { .fields = { 96, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 40 } }, 120, 0, 629, { .fields = { 32, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 40 } }, 120, 40, 623, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 32 } }, 120, 80, 617, { .fields = { 24, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 128, -48, 661, { .fields = { 72, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 56 } }, 136, -64, 523, { .fields = { 88, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 56 } }, 136, -8, 548, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, 136, 48, 565, { .fields = { 48, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Source ranges for mapped view 6's 77 depth-sorted sprites.
///
/// The zero-count header retains decoded background strips. The remaining
/// ranges cover the complete source array and end with `SPRITE_BATCH_END`;
/// hidden and excluded flags start clear. The trailing bytes' role is unproven.
static SpriteBatch _gShelterB2BreedingRoomView6SpriteBatches[11] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 7, .field_6 = { 0, 0 } },
    { .firstSprite = 7, .spriteCount = 5, .field_6 = { 5, 0 } },
    { .firstSprite = 12, .spriteCount = 37, .field_6 = { 4, 0 } },
    { .firstSprite = 49, .spriteCount = 4, .field_6 = { 6, 0 } },
    { .firstSprite = 53, .spriteCount = 3, .field_6 = { 1, 0 } },
    { .firstSprite = 56, .spriteCount = 1, .field_6 = { 7, 0 } },
    { .firstSprite = 57, .spriteCount = 4, .field_6 = { 3, 0 } },
    { .firstSprite = 61, .spriteCount = 5, .field_6 = { 8, 0 } },
    { .firstSprite = 66, .spriteCount = ARRAY_SIZE(_gShelterB2BreedingRoomView6SpriteSources) - 66, .field_6 = { 2, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 7's room sprites.
///
/// Positions and sizes are pixels, UVs are texels and depths are unscaled OT
/// sorting values. RGB 128 is neutral modulation.
/// 13 rectangles use semitransparent emission; the remainder are opaque.
/// The paired batch list indexes all 59 elements; storage lives with the room.
static SpriteSource _gShelterB2BreedingRoomView7SpriteSources[59] = {
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -160, -120, 657, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -120, -120, 639, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -80, -120, 654, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -48, -120, 668, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 48, -120, 770, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 96, -120, 802, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 0, -120, 738, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 0, -96, 801, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -104, -88, 733, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -160, -88, 698, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, 80, -96, 865, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 0, -64, 921, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 48 } }, 72, -64, 982, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 48 } }, -152, -64, 830, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 0, -16, 1030, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 8, 1089, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 8, 8, 1075, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -24, 0, 1141, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 24 } }, -128, -16, 950, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -120, 8, 1044, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 40, -16, 1082, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 72, -16, 1100, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -56, 8, 1117, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -48, 8, 1125, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -112, 670, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, -72, 750, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -136, -16, 895, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -160, -16, 648, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -160, 48, 692, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 120, -120, 797, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 112, -96, 776, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 104, -72, 869, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 80, -24, 1142, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 96, -48, 973, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 136, -48, 811, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, 112, -24, 885, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 112, 16, 911, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 120, 56, 892, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -128, 104, 657, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -128, 88, 690, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -112, 72, 766, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -112, 56, 800, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -112, 40, 840, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -104, 32, 866, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -104, 24, 896, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -104, 16, 932, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 24 } }, -144, -96, 786, { .fields = { 8, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 40, 32 } }, -136, -72, 842, { .fields = { 104, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 24 } }, -120, -40, 913, { .fields = { 24, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 24 } }, -112, -16, 994, { .fields = { 16, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 40, 16 } }, -104, 8, 1066, { .fields = { 104, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 48 } }, 16, -104, 858, { .fields = { 72, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 16 } }, 8, -8, 1074, { .fields = { 56, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 40 } }, 40, -104, 879, { .fields = { 0, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, 72, -104, 899, { .fields = { 0, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 32 } }, 32, -64, 971, { .fields = { 96, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 32 } }, 64, -64, 997, { .fields = { 120, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 32 } }, 32, -32, 1076, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 32 } }, 56, -32, 1103, { .fields = { 72, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Source ranges for mapped view 7's 59 depth-sorted sprites.
///
/// The zero-count header retains decoded background strips. The remaining
/// ranges cover the complete source array and end with `SPRITE_BATCH_END`;
/// hidden and excluded flags start clear. The trailing bytes' role is unproven.
static SpriteBatch _gShelterB2BreedingRoomView7SpriteBatches[7] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 24, .field_6 = { 0, 0 } },
    { .firstSprite = 24, .spriteCount = 5, .field_6 = { 3, 0 } },
    { .firstSprite = 29, .spriteCount = 9, .field_6 = { 2, 0 } },
    { .firstSprite = 38, .spriteCount = 8, .field_6 = { 4, 0 } },
    { .firstSprite = 46, .spriteCount = ARRAY_SIZE(_gShelterB2BreedingRoomView7SpriteSources) - 46, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Nineteen opaque source rectangles and sorting depths for mapped view 8.
///
/// XY and sizes use pixels, UVs use texels and depths are unscaled OT values.
/// RGB 128 is neutral modulation; the paired batch list spans every source.
/// The mutable array lives with the room overlay.
static SpriteSource _gShelterB2BreedingRoomView8SpriteSources[19] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -16, 973, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, -8, 959, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 0, 924, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 8, 893, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 80, 72, 696, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 80, 56, 737, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, 40, 775, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 32, 815, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 24, 825, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 16, 865, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 80, 104, 639, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 80, 88, 654, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -128, -120, 953, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, -120, 784, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -112, -72, 1126, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -112, -8, 1094, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, -160, -72, 917, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -136, -8, 975, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, -160, -8, 866, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

/// View 8's source ranges, with an image-strip header and an empty trailing range.
///
/// Ranges 0..11 and 12..18 cover the source array. The zero-count range at
/// its end is retained before the sentinel. Visibility and packet exclusion
/// start clear; the role of the serialized `field_6` bytes is unproven.
static SpriteBatch _gShelterB2BreedingRoomView8SpriteBatches[5] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 12, .field_6 = { 1, 0 } },
    { .firstSprite = 12, .spriteCount = ARRAY_SIZE(_gShelterB2BreedingRoomView8SpriteSources) - 12, .field_6 = { 2, 0 } },
    { .firstSprite = ARRAY_SIZE(_gShelterB2BreedingRoomView8SpriteSources), .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 9's 110 source rectangles, including a final range of 22 translucent sprites.
///
/// XY and sizes use pixels, UVs use texels and depths are unscaled OT values.
/// RGB 128 is neutral modulation; sources 0..87 are opaque and 88..109 carry
/// `SPRITE_SOURCE_SEMI_TRANSPARENT`. The mutable array lives with the room.
static SpriteSource _gShelterB2BreedingRoomView9SpriteSources[110] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -72, 1301, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -136, -120, 640, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -104, -120, 697, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, -120, 913, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 8, -120, 909, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -32, -120, 886, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -112, 881, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -80, -104, 879, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -112, -96, 889, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -40, -48, 1513, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -64, -48, 1507, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, -40, 1511, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 24, -88, 1159, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, -72, 1399, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -48, 1384, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -96, -48, 1228, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -120, -32, 1348, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -104, -24, 1404, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -88, -48, 1307, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -80, -48, 1383, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -120, 636, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, -64, 1273, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, -56, 1321, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -32, -80, 1076, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -32, -72, 1137, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 0, -64, 1365, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 0, -56, 1449, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 16, -56, 1498, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 16, -64, 1387, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -24, -48, 1461, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, -48, 1517, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, -40, 1154, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, -40, 1232, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -144, -48, 942, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -120, -48, 1133, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -128, -56, 1059, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -160, -56, 885, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -160, -72, 886, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -136, -72, 890, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -160, -88, 868, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -136, -88, 893, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -128, -48, 1088, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, -24, 1068, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, -72, 859, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, -48, 842, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -160, -24, 826, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -160, 0, 810, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -160, 24, 779, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -160, 48, 769, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -160, 80, 735, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, 80, 993, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, 80, 1215, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, 48, 981, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -112, 32, 1026, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 0, 1056, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 0, 1302, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 32, 1265, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 56, 1204, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -16, 1392, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -104, -32, 1414, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, 96, 725, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 96, 0, 843, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 96, 24, 902, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 80, 0, 968, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 80, 24, 1041, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, 32, 1268, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, 8, 1268, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, 104, 827, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, 88, 993, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 24, 80, 1002, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 48, 72, 1001, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 88, 80, 773, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 88, 808, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, 88, 771, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, 88, 753, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 88, 750, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 88, 762, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 48, 80, 903, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 64, 80, 840, { .fields = { 56, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 72, 872, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 32, 104, 741, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 64, 104, 711, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -96, -32, 1345, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -88, 16, 1138, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -96, -8, 1318, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -80, 40, 1268, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -80, 64, 1222, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -72, 88, 1240, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, 32, -120, 969, { .fields = { 64, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 0, -120, 904, { .fields = { 64, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 24 } }, -48, -120, 892, { .fields = { 96, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 32 } }, -80, -120, 889, { .fields = { 40, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 48 } }, -104, -120, 889, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 40 } }, -136, -112, 852, { .fields = { 80, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 32 } }, -160, -104, 834, { .fields = { 72, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 8 } }, -144, -72, 934, { .fields = { 64, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, -136, -64, 998, { .fields = { 72, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 8 } }, -104, -32, 1354, { .fields = { 96, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 8 } }, 8, -80, 1176, { .fields = { 56, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 8 } }, 8, -72, 1269, { .fields = { 56, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 8 } }, 8, -64, 1377, { .fields = { 56, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 8 } }, -24, -72, 1159, { .fields = { 48, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 8 } }, -16, -64, 1288, { .fields = { 120, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 8 } }, -16, -56, 1374, { .fields = { 40, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 40, 8 } }, -120, -48, 1176, { .fields = { 48, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 8 } }, -112, -40, 1284, { .fields = { 40, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 8 } }, -24, -80, 1105, { .fields = { 56, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 8 } }, -16, -88, 1039, { .fields = { 16, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 8 } }, 8, -88, 1107, { .fields = { 56, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 8 } }, 8, -96, 1035, { .fields = { 56, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// View 9's five contiguous source ranges, preceded by an image-strip header.
///
/// Ranges cover all 110 sources and end with `SPRITE_BATCH_END`. Visibility
/// and packet exclusion start clear; the role of the serialized `field_6`
/// bytes is unproven. The list and its sources live with the room overlay.
static SpriteBatch _gShelterB2BreedingRoomView9SpriteBatches[7] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 41, .field_6 = { 0, 0 } },
    { .firstSprite = 41, .spriteCount = 19, .field_6 = { 3, 0 } },
    { .firstSprite = 60, .spriteCount = 22, .field_6 = { 2, 0 } },
    { .firstSprite = 82, .spriteCount = 6, .field_6 = { 4, 0 } },
    { .firstSprite = 88, .spriteCount = ARRAY_SIZE(_gShelterB2BreedingRoomView9SpriteSources) - 88, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty view-10 batch list retaining decoded image strips for the close-up.
///
/// A zero-count header precedes the sentinel; no source elements are read.
/// Its address is also retained in the view's empty source slot.
static SpriteBatch _gShelterB2BreedingRoomView10SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gShelterB2BreedingRoomSpriteViews[10] = {
    { { .empty = _gShelterB2BreedingRoomView1SpriteBatches }, _gShelterB2BreedingRoomView1SpriteBatches, NULL },
    { { .elements = _gShelterB2BreedingRoomView2SpriteSources }, _gShelterB2BreedingRoomView2SpriteBatches, NULL },
    { { .elements = _gShelterB2BreedingRoomView3SpriteSources }, _gShelterB2BreedingRoomView3SpriteBatches, NULL },
    { { .elements = _gShelterB2BreedingRoomView4SpriteSources }, _gShelterB2BreedingRoomView4SpriteBatches, NULL },
    { { .elements = _gShelterB2BreedingRoomView5SpriteSources }, _gShelterB2BreedingRoomView5SpriteBatches, NULL },
    { { .elements = _gShelterB2BreedingRoomView6SpriteSources }, _gShelterB2BreedingRoomView6SpriteBatches, NULL },
    { { .elements = _gShelterB2BreedingRoomView7SpriteSources }, _gShelterB2BreedingRoomView7SpriteBatches, NULL },
    { { .elements = _gShelterB2BreedingRoomView8SpriteSources }, _gShelterB2BreedingRoomView8SpriteBatches, NULL },
    { { .elements = _gShelterB2BreedingRoomView9SpriteSources }, _gShelterB2BreedingRoomView9SpriteBatches, NULL },
    { { .empty = _gShelterB2BreedingRoomView10SpriteBatches }, _gShelterB2BreedingRoomView10SpriteBatches, NULL },
};

/// Nine room-owned point lights with world-unit positions and radii and Q12 RGB.
///
/// All views admit them. Coordinate composition and lighting queries mutate
/// their transforms and attenuation while the room overlay remains loaded.
static WorldCoordPointLight _gShelterB2BreedingRoomPointLights[9] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1950, -1548, -1250 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 2341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2D96, -1808, 6220 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 3415, 2703 }, { 0, 0 } }, 0, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2AF8, -1528, 1150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3537, 2405 }, { 0, 0 } }, 700, 3002 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6970, -1588, 1150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3537, 2405 }, { 0, 0 } }, 700, 3041 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9660, -1970, 5310 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1800, 4222 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2890, -2010, 3610 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3276 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2060, -1970, 1110 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3276 }, { 0, 0 } }, 0, 2900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5760, -1769, 4690 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 0, 3041 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -100, -2090, -1141 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 3090, 2703 }, { 0, 0 } }, 0, 1600 },
};

WorldCoordRoomLights gShelterB2BreedingRoomRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB2BreedingRoomPointLights),
    .pointLights           = _gShelterB2BreedingRoomPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB2BreedingRoomViewBoundaryTriggers[14] = {
    { NULL, NULL, NULL, { 2176, -1664, -671, 0 }, { { -1552, -3136, 1808, 0 }, { 1552, -3136, -1808, 0 }, { -1552, 3136, 1808, 0 }, { 1552, 3136, -1808, 0 } }, { -3108, 0, -2668, 0 }, { 0, 0, ONE, 0 }, 3932, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1968, -1697, -751, 0 }, { { 1600, -3136, -1824, 0 }, { -1600, -3136, 1824, 0 }, { 1600, 3136, -1824, 0 }, { -1600, 3136, 1824, 0 } }, { 3083, 0, 2704, 0 }, { 0, 0, ONE, 0 }, 3957, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1712, -1632, 2593, 0 }, { { 2032, -3136, 64, 0 }, { -2032, -3136, -64, 0 }, { 2032, 3136, 64, 0 }, { -2032, 3136, -64, 0 } }, { -130, 0, 4105, 0 }, { 0, 0, ONE, 0 }, 3736, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1792, -1600, 2816, 0 }, { { -1904, -3136, -64, 0 }, { 1904, -3136, 64, 0 }, { -1904, 3136, -64, 0 }, { 1904, 3136, 64, 0 } }, { 138, 0, -4108, 0 }, { 0, 0, ONE, 0 }, 3665, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3968, -1729, 4432, 0 }, { { -960, -3136, -1776, 0 }, { 960, -3136, 1776, 0 }, { -960, 3136, -1776, 0 }, { 960, 3136, 1776, 0 } }, { 3613, 0, -1954, 0 }, { 0, 0, ONE, 0 }, 3727, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4272, -1761, 4544, 0 }, { { 912, -3136, 1616, 0 }, { -912, -3136, -1616, 0 }, { 912, 3136, 1616, 0 }, { -912, 3136, -1616, 0 } }, { -3570, 0, 2014, 0 }, { 0, 0, ONE, 0 }, 3638, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8048, -1761, 4384, 0 }, { { 16, -3136, -1776, 0 }, { -16, -3136, 1776, 0 }, { 16, 3136, -1776, 0 }, { -16, 3136, 1776, 0 } }, { 4111, 0, 37, 0 }, { 0, 0, ONE, 0 }, 3602, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8193, -1761, 4353, 0 }, { { 16, -3136, 1680, 0 }, { -16, -3136, -1680, 0 }, { 16, 3136, 1680, 0 }, { -16, 3136, -1680, 0 } }, { -4115, 0, 39, 0 }, { 0, 0, ONE, 0 }, 3556, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6849, -1729, 1056, 0 }, { { 2032, -3136, 64, 0 }, { -2032, -3136, -64, 0 }, { 2032, 3136, 64, 0 }, { -2032, 3136, -64, 0 } }, { -130, 0, 4105, 0 }, { 0, 0, ONE, 0 }, 3736, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6848, -1760, 1184, 0 }, { { -2096, -3136, -64, 0 }, { 2096, -3136, 64, 0 }, { -2096, 3136, -64, 0 }, { 2096, 3136, 64, 0 } }, { 125, 0, -4099, 0 }, { 0, 0, ONE, 0 }, 3771, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6160, -1889, 2832, 0 }, { { -1056, -3136, -16, 0 }, { 1056, -3136, 16, 0 }, { -1056, 3136, -16, 0 }, { 1056, 3136, 16, 0 } }, { 62, 0, -4103, 0 }, { 0, 0, ONE, 0 }, 3308, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6112, -1792, 2672, 0 }, { { 1088, -3136, 0, 0 }, { -1088, -3136, 0, 0 }, { 1088, 3136, 0, 0 }, { -1088, 3136, 0, 0 } }, { 0, 0, 4099, 0 }, { 0, 0, ONE, 0 }, 3318, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2920, -1824, 2592, 0 }, { { 1088, -3136, 0, 0 }, { -1088, -3136, 0, 0 }, { 1088, 3136, 0, 0 }, { -1088, 3136, 0, 0 } }, { 0, 0, 4099, 0 }, { 0, 0, ONE, 0 }, 3318, 0, 6, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2920, -1696, 2752, 0 }, { { -1056, -3136, -16, 0 }, { 1056, -3136, 16, 0 }, { -1056, 3136, -16, 0 }, { 1056, 3136, 16, 0 } }, { 62, 0, -4103, 0 }, { 0, 0, ONE, 0 }, 3308, 0, 9, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room-local resource keys matched by the placement and resource tables below.
enum {
    SHELTER_B2_BREEDING_ROOM_ENTRY_KNIGHT_GOLEM      = 22,
    SHELTER_B2_BREEDING_ROOM_ENTRY_AMOEBA            = 24,
    SHELTER_B2_BREEDING_ROOM_ENTRY_MAGGOT            = 26,
    SHELTER_B2_BREEDING_ROOM_ENTRY_BLACK_BEETLE      = 38,
    SHELTER_B2_BREEDING_ROOM_ENTRY_BISHOP_GOLEM      = 39,
    SHELTER_B2_BREEDING_ROOM_ENTRY_MAD_CHASER        = 44,
    SHELTER_B2_BREEDING_ROOM_ENTRY_SLOUCH            = 70,
    SHELTER_B2_BREEDING_ROOM_ENTRY_SUCKLERCEPH       = 71,
    SHELTER_B2_BREEDING_ROOM_ENTRY_CREEPING_STRANGER = 72,
    SHELTER_B2_BREEDING_ROOM_ENTRY_SKULL_STALKER     = 73,
};

/// Maggot and Mad Chaser starting behaviors used by these room placements.
enum {
    SHELTER_B2_BREEDING_ROOM_MAGGOT_DORMANT    = 0,
    SHELTER_B2_BREEDING_ROOM_MAGGOT_AIMED      = 1,
    SHELTER_B2_BREEDING_ROOM_MAD_CHASER_LURK   = 0,
    SHELTER_B2_BREEDING_ROOM_MAD_CHASER_DANGLE = 1,
};

/// Variant 1's Maggot resource recipe, followed by `AREA_PLACEMENT_END`.
///
/// Loads actor-slot-1 file 102600 and borrows its body descriptor at index 0.
/// Keep the room table and actor package live during loading and spawning.
static AreaResource _gShelterB2BreedingRoomVariant1Resources[2] = {
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAGGOT, .fileNumber = 26, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskTable = &gActor02600MaggotCaterpillarBodyTask },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's Maggot and Amoeba resource recipes, ending with `AREA_PLACEMENT_END`.
///
/// Loads actor-slot-1 files 102600 and 202400. Both spawn descriptor 0;
/// the table and borrowed actor descriptors must remain loaded while used.
static AreaResource _gShelterB2BreedingRoomVariant2Resources[3] = {
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAGGOT, .fileNumber = 26, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskTable = &gActor02600MaggotCaterpillarBodyTask },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_AMOEBA, .fileNumber = 24, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskTable = gActor202400Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 3's ordinary Mad Chaser, standing Slouch and standing Sucklerceph recipes.
///
/// File 104400 supplies the Chaser; 207000/207100 share the Slouch/Sucklerceph
/// package. Each selects descriptor 0, and `AREA_PLACEMENT_END` ends the list.
/// Keep the room table and borrowed actor descriptors loaded while used.
static AreaResource _gShelterB2BreedingRoomVariant3Resources[4] = {
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAD_CHASER, .fileNumber = 44, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = ACTOR_04400_MAD_CHASER_TASK_ORDINARY, .taskTable = gActor04400MadChaserTaskDescs },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_SLOUCH, .fileNumber = 70, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskTable = gActor207000SlouchTaskDescs },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_SUCKLERCEPH, .fileNumber = 71, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskTable = gActor207000SucklercephTaskDescs },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 4's ordinary Mad Chaser, Creeping Stranger and Skull Stalker recipes.
///
/// Files 104400, 207200 and 207300 select descriptor 0 of their borrowed task
/// tables. `AREA_PLACEMENT_END` ends the list; keep room and actor storage live.
static AreaResource _gShelterB2BreedingRoomVariant4Resources[4] = {
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAD_CHASER, .fileNumber = 44, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = ACTOR_04400_MAD_CHASER_TASK_ORDINARY, .taskTable = gActor04400MadChaserTaskDescs },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_CREEPING_STRANGER, .fileNumber = 72, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskTable = &gActor207200CreepingStrangerTaskDesc },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_SKULL_STALKER, .fileNumber = 73, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskTable = &gActor207200SkullStalkerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 7's Knight GOLEM recipe for file 402200, with an end record.
///
/// Spawns descriptor 0. The room table and borrowed actor descriptor stay live
/// during loading and spawning; `AREA_PLACEMENT_END` ends the list.
static AreaResource _gShelterB2BreedingRoomVariant7Resources[2] = {
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_KNIGHT_GOLEM, .fileNumber = 22, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskTable = &gActor402200GolemTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 11's Maggot and Black Beetle recipes for files 102600 and 203800.
///
/// Both select descriptor 0. `AREA_PLACEMENT_END` ends the table; keep the
/// room table and borrowed actor descriptors loaded during loading and spawning.
static AreaResource _gShelterB2BreedingRoomVariant11Resources[3] = {
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAGGOT, .fileNumber = 26, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskTable = &gActor02600MaggotCaterpillarBodyTask },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_BLACK_BEETLE, .fileNumber = 38, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskTable = &gActor203800BlackBeetleTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 21's Bishop GOLEM recipe for file 403900, with an end record.
///
/// Spawns descriptor 0. The room table and borrowed actor descriptor stay live
/// during loading and spawning; `AREA_PLACEMENT_END` ends the list.
static AreaResource _gShelterB2BreedingRoomVariant21Resources[2] = {
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_BISHOP_GOLEM, .fileNumber = 39, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskTable = &gActor403900GolemTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 1's four Maggot placements, alternating aimed and dormant starts.
///
/// Positions use world units and yaw uses 4096 units per turn. Every placement
/// selects base resource 26 with a two-row CLUT relocation and timing row 0.
/// The end record terminates the table; live enemies borrow these placements.
static AreaPlacement _gShelterB2BreedingRoomVariant1Placements[5] = {
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAGGOT, .mode = SHELTER_B2_BREEDING_ROOM_MAGGOT_AIMED, .x = 6500, .z = 2300, .yaw = 3900, .clutRowOffset = 2 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAGGOT, .mode = SHELTER_B2_BREEDING_ROOM_MAGGOT_DORMANT, .x = 6200, .z = 50, .yaw = 1500, .clutRowOffset = 2 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAGGOT, .mode = SHELTER_B2_BREEDING_ROOM_MAGGOT_AIMED, .x = 0x2904, .z = 2000, .clutRowOffset = 2 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAGGOT, .mode = SHELTER_B2_BREEDING_ROOM_MAGGOT_DORMANT, .x = 0x2E7C, .z = 300, .yaw = 300, .clutRowOffset = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's three dormant Maggots and two default-variant Amoebas.
///
/// Positions use world units and yaw uses 4096 units per turn. All use base
/// files; Maggots add two CLUT rows, while Amoebas add two texture pages and
/// four CLUT rows. The end record terminates the table; enemies borrow it.
static AreaPlacement _gShelterB2BreedingRoomVariant2Placements[6] = {
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAGGOT, .mode = SHELTER_B2_BREEDING_ROOM_MAGGOT_DORMANT, .x = 8400, .z = 3400, .yaw = 1000, .clutRowOffset = 2 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAGGOT, .mode = SHELTER_B2_BREEDING_ROOM_MAGGOT_DORMANT, .x = 6400, .z = 5200, .yaw = 700, .clutRowOffset = 2 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAGGOT, .mode = SHELTER_B2_BREEDING_ROOM_MAGGOT_DORMANT, .x = 2300, .z = 2400, .yaw = 2900, .clutRowOffset = 2 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_AMOEBA, .x = 7500, .z = 300, .yaw = 3800, .texturePageOffset = 2, .clutRowOffset = 4 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_AMOEBA, .x = 0x2AF8, .z = 900, .yaw = 4000, .texturePageOffset = 2, .clutRowOffset = 4 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 3's two dangling Mad Chasers, one Slouch and two Sucklercephs.
///
/// Positions use world units and yaw uses 4096 units per turn. All use base
/// files; Chasers add two CLUT rows, and the other actors add two texture pages
/// and four CLUT rows. The end record terminates the borrowed placement table.
static AreaPlacement _gShelterB2BreedingRoomVariant3Placements[6] = {
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAD_CHASER, .mode = SHELTER_B2_BREEDING_ROOM_MAD_CHASER_DANGLE, .x = 6800, .y = -2350, .z = 4800, .yaw = 3072, .clutRowOffset = 2 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAD_CHASER, .mode = SHELTER_B2_BREEDING_ROOM_MAD_CHASER_DANGLE, .x = 2400, .y = -2900, .z = 4700, .clutRowOffset = 2 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_SLOUCH, .x = 6500, .z = 1600, .texturePageOffset = 2, .clutRowOffset = 4 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_SUCKLERCEPH, .mode = 1, .x = 0x2968, .z = 1250, .texturePageOffset = 2, .clutRowOffset = 4 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_SUCKLERCEPH, .mode = 1, .x = 0x2E7C, .z = 200, .yaw = 3650, .texturePageOffset = 2, .clutRowOffset = 4 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 4's two lurking Mad Chasers, one Creeping Stranger and one Skull Stalker.
///
/// Positions use world units and yaw uses 4096 units per turn. All use base
/// files; Chasers add two CLUT rows, and the other actors add two texture pages
/// and four CLUT rows. The end record terminates the borrowed placement table.
static AreaPlacement _gShelterB2BreedingRoomVariant4Placements[5] = {
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAD_CHASER, .mode = SHELTER_B2_BREEDING_ROOM_MAD_CHASER_LURK, .x = 1800, .z = 2600, .yaw = 1024, .clutRowOffset = 2 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAD_CHASER, .mode = SHELTER_B2_BREEDING_ROOM_MAD_CHASER_LURK, .x = 8750, .z = 4000, .yaw = 2048, .clutRowOffset = 2 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_CREEPING_STRANGER, .x = 6250, .yaw = 1024, .texturePageOffset = 2, .clutRowOffset = 4 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_SKULL_STALKER, .mode = 1, .x = 900, .z = -350, .yaw = 2750, .texturePageOffset = 2, .clutRowOffset = 4 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 7's Knight GOLEM placement at world origin.
///
/// Uses base resource 22, zero yaw and a two-row CLUT relocation. The all-zero
/// pose is a live placement; only the following end record terminates the list.
/// The room table remains live while the enemy borrows its placement.
static AreaPlacement _gShelterB2BreedingRoomVariant7Placements[2] = {
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_KNIGHT_GOLEM, .clutRowOffset = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 11's two dormant Maggots and four initially idle floor Black Beetles.
///
/// Positions use world units and yaw uses 4096 units per turn. Maggots use
/// timing row 1 and add two CLUT rows; Beetles add two texture pages and four
/// CLUT rows. All use base files. The end record terminates the borrowed table.
static AreaPlacement _gShelterB2BreedingRoomVariant11Placements[7] = {
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAGGOT, .mode = SHELTER_B2_BREEDING_ROOM_MAGGOT_DORMANT, .x = 0x2E7C, .z = 150, .yaw = 500, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_MAGGOT, .mode = SHELTER_B2_BREEDING_ROOM_MAGGOT_DORMANT, .x = 0x2F44, .z = 1950, .yaw = 450, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_BLACK_BEETLE, .x = 6300, .z = 50, .yaw = 3000, .texturePageOffset = 2, .clutRowOffset = 4 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_BLACK_BEETLE, .x = 6400, .z = 1100, .yaw = 1024, .texturePageOffset = 2, .clutRowOffset = 4 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_BLACK_BEETLE, .x = 7300, .z = 1100, .texturePageOffset = 2, .clutRowOffset = 4 },
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_BLACK_BEETLE, .x = 8000, .z = 2000, .yaw = 2050, .texturePageOffset = 2, .clutRowOffset = 4 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 21's Bishop GOLEM placement at (3000, 0, 4400), facing yaw 3072.
///
/// Position uses world units and yaw uses 4096 units per turn. Uses base
/// resource 39 with a two-row CLUT relocation. The following end record
/// terminates the table, which stays live while the enemy borrows its placement.
static AreaPlacement _gShelterB2BreedingRoomVariant21Placements[2] = {
    { .entryId = SHELTER_B2_BREEDING_ROOM_ENTRY_BISHOP_GOLEM, .x = 3000, .z = 4400, .yaw = 3072, .clutRowOffset = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gShelterB2BreedingRoomAreaVariants[22] = {
    [AREA_DEFAULT_VARIANT] = { .placements = _gShelterB2BreedingRoomVariant1Placements, .resources = _gShelterB2BreedingRoomVariant1Resources },
    [2]                    = { .placements = _gShelterB2BreedingRoomVariant2Placements, .resources = _gShelterB2BreedingRoomVariant2Resources },
    [3]                    = { .placements = _gShelterB2BreedingRoomVariant3Placements, .resources = _gShelterB2BreedingRoomVariant3Resources },
    [4]                    = { .placements = _gShelterB2BreedingRoomVariant4Placements, .resources = _gShelterB2BreedingRoomVariant4Resources },
    [7]                    = { .placements = _gShelterB2BreedingRoomVariant7Placements, .resources = _gShelterB2BreedingRoomVariant7Resources },
    [11]                   = { .placements = _gShelterB2BreedingRoomVariant11Placements, .resources = _gShelterB2BreedingRoomVariant11Resources },
    [21]                   = { .placements = _gShelterB2BreedingRoomVariant21Placements, .resources = _gShelterB2BreedingRoomVariant21Resources },
};

/// Packed exit warp selectors and the room-dispatched first-scene command.
enum {
    SHELTER_B2_BREEDING_ROOM_MAIN_CORRIDOR_WARP_SELECTOR     = (SHELTER_B2_BREEDING_ROOM_WARP_MAIN_CORRIDOR << 4) | 5,
    SHELTER_B2_BREEDING_ROOM_NORTH_MAINTENANCE_WARP_SELECTOR = (SHELTER_B2_BREEDING_ROOM_WARP_NORTH_MAINTENANCE_WALKWAY << 4) | 2,
    SHELTER_B2_BREEDING_ROOM_COMMAND_FIRST_SCENE             = 22,
};

WorldCollisionTrigger gShelterB2BreedingRoomActionTriggers[22] = {
    { NULL, NULL, NULL, { -62, -48, -1024, 0 }, { { -416, 0, -736, 0 }, { 416, 0, -736, 0 }, { -416, 0, 736, 0 }, { 416, 0, 736, 0 } }, { 0, 4103, 0, 0 }, { ONE, 0, 0, 0 }, 844, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, SHELTER_B2_BREEDING_ROOM_MAIN_CORRIDOR_WARP_SELECTOR, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2E60, -48, 6144, 0 }, { { -896, 0, 560, 0 }, { -896, 0, -560, 0 }, { 896, 0, 560, 0 }, { 896, 0, -560, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -ONE, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY, SHELTER_B2_BREEDING_ROOM_NORTH_MAINTENANCE_WARP_SELECTOR, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1712, -64, -1728, 0 }, { { -1120, 0, -736, 0 }, { 1120, 0, -736, 0 }, { -1120, 0, 736, 0 }, { 1120, 0, 736, 0 } }, { 0, 4097, 0, 0 }, { -201, 0, 4090, 0 }, 1336, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B2_BREEDING_ROOM_COMMAND_FIRST_SCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8976, -64, 5200, 0 }, { { -1104, 0, -432, 0 }, { 1104, 0, -432, 0 }, { -1104, 0, 432, 0 }, { 1104, 0, 432, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4097, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6656, -64, 5184, 0 }, { { -1104, 0, -432, 0 }, { 1104, 0, -432, 0 }, { -1104, 0, 432, 0 }, { 1104, 0, 432, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4097, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2400, -64, 6080, 0 }, { { -848, 0, -1665, 0 }, { 656, 0, -1215, 0 }, { -848, 0, -289, 0 }, { 656, 0, 161, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 1868, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 399, -64, -2127, 0 }, { { -560, 0, -560, 0 }, { 560, 0, -560, 0 }, { -560, 0, 560, 0 }, { 560, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, ONE, 0 }, 791, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2605, -64, -608, 0 }, { { -640, 0, -560, 0 }, { 480, 0, -560, 0 }, { -320, 0, 560, 0 }, { 480, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { -ONE, 0, 0, 0 }, 849, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2880, -64, 640, 0 }, { { -560, 0, -560, 0 }, { 560, 0, -560, 0 }, { -560, 0, 560, 0 }, { 560, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { -ONE, 0, 0, 0 }, 791, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2880, -64, 2112, 0 }, { { -560, 0, -560, 0 }, { 560, 0, -560, 0 }, { -560, 0, 560, 0 }, { 560, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { -ONE, 0, 0, 0 }, 791, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1120, -64, 992, 0 }, { { -560, 0, -560, 0 }, { 560, 0, -560, 0 }, { -560, 0, 560, 0 }, { 560, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { ONE, 0, 0, 0 }, 791, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1120, -64, 2208, 0 }, { { -560, 0, -560, 0 }, { 560, 0, -560, 0 }, { -560, 0, 560, 0 }, { 560, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { ONE, 0, 0, 0 }, 791, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1536, -64, 3392, 0 }, { { -560, 0, -560, 0 }, { 560, 0, -560, 0 }, { -560, 0, 560, 0 }, { 560, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { ONE, 0, 0, 0 }, 791, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1088, -64, 4672, 0 }, { { -560, 0, -560, 0 }, { 560, 0, -560, 0 }, { -560, 0, 560, 0 }, { 560, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { ONE, 0, 0, 0 }, 791, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3888, -64, 5856, 0 }, { { -928, 0, -560, 0 }, { 928, 0, -560, 0 }, { -928, 0, 560, 0 }, { 928, 0, 560, 0 } }, { 0, 4095, 0, 0 }, { 401, 0, -4076, 0 }, 1078, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5920, -64, -256, 0 }, { { -928, 0, -560, 0 }, { 928, 0, -560, 0 }, { -928, 0, 560, 0 }, { 928, 0, 560, 0 } }, { 0, 4095, 0, 0 }, { 401, 0, -4076, 0 }, 1078, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 8112, -64, 688, 0 }, { { -464, 0, -672, 0 }, { 464, 0, -672, 0 }, { -464, 0, 672, 0 }, { 464, 0, 672, 0 } }, { 0, 4101, 0, 0 }, { -4076, 0, 401, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7936, -64, 2112, 0 }, { { 48, 0, -672, 0 }, { 976, 0, -672, 0 }, { -976, 0, 672, 0 }, { -48, 0, 672, 0 } }, { 0, 4101, 0, 0 }, { -3166, 0, -2599, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2FC0, -64, -96, 0 }, { { -464, 0, -480, 0 }, { 464, 0, -480, 0 }, { -464, 0, 480, 0 }, { 464, 0, 480, 0 } }, { 0, 4106, 0, 0 }, { -2599, 0, 3165, 0 }, 665, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2EE0, -64, 480, 0 }, { { -1440, 0, -792, 0 }, { 960, 0, -792, 0 }, { -480, 0, 520, 0 }, { 960, 0, 1064, 0 } }, { 0, 4095, 0, 0 }, { 401, 0, -4076, 0 }, 1639, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 2320, -64, -1728, 0 }, { { -720, 0, -736, 0 }, { 720, 0, -736, 0 }, { -720, 0, 736, 0 }, { 720, 0, 736, 0 } }, { 0, 4103, 0, 0 }, { -4092, 0, 201, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9888, -64, 1952, 0 }, { { -464, 0, -672, 0 }, { 464, 0, -672, 0 }, { -464, 0, 672, 0 }, { 464, 0, 672, 0 } }, { 0, 4101, 0, 0 }, { 4095, 0, 0, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry gShelterB2BreedingRoomAmbientByView[11] = {
    { .viewCount = ARRAY_SIZE(gShelterB2BreedingRoomAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 450, 437, 475, 446 } },
    { .color = { 300, 300, 300, 300 } },
    { .color = { 290, 290, 290, 290 } },
    { .color = { 250, 250, 250, 250 } },
    { .color = { 700, 700, 700, 700 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionOccluder gShelterB2BreedingRoomOccluders[5] = {
    { NULL, NULL, { 4448, -1664, -176, 0 }, { { -1376, 2688, 3056, 0 }, { 1376, 2688, -3056, 0 }, { -1376, -2688, 3056, 0 }, { 1376, -2688, -3056, 0 } }, { 3755, 0, 1690, 0 }, 4283, 1, 0 },
    { NULL, NULL, { 3935, -1504, -289, 0 }, { { 1033, 2688, 3189, 0 }, { -1032, 2688, -3188, 0 }, { 1033, -2688, 3189, 0 }, { -1032, -2688, -3188, 0 } }, { 3917, 0, -1270, 0 }, 4283, 1, 0 },
    { NULL, NULL, { 8894, -1504, -161, 0 }, { { 63, 2688, 2967, 0 }, { -62, 2688, -2967, 0 }, { 63, -2688, 2967, 0 }, { -62, -2688, -2967, 0 } }, { 4097, 0, -87, 0 }, 3998, 1, 0 },
    { NULL, NULL, { 8415, -1472, 2831, 0 }, { { 1111, 2688, -29, 0 }, { -1110, 2688, 29, 0 }, { 1111, -2688, -29, 0 }, { -1110, -2688, 29, 0 } }, { -109, 0, -4108, 0 }, 2907, 1, 0 },
    { NULL, NULL, { 0x30A0, -1152, 2880, 0 }, { { 1111, 2688, -29, 0 }, { -1110, 2688, 29, 0 }, { 1111, -2688, -29, 0 }, { -1110, -2688, 29, 0 } }, { -109, 0, -4108, 0 }, 2907, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Cue-2 entry bases in the loaded type-1 bank; cue 1 adds one, companions add 100.
enum {
    SHELTER_B2_BREEDING_ROOM_SURFACE_1_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x2D,
    SHELTER_B2_BREEDING_ROOM_SURFACE_1_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x2F,
    SHELTER_B2_BREEDING_ROOM_SURFACE_3_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x59,
    SHELTER_B2_BREEDING_ROOM_SURFACE_3_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5B,
    SHELTER_B2_BREEDING_ROOM_SURFACE_4_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5D,
    SHELTER_B2_BREEDING_ROOM_SURFACE_4_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5F,
};

/// Walking, running and scripted-jump footstep cues for surface class 1.
///
/// Scripted jumps reuse walking cues. The surface record borrows these cue-2
/// bases from the loaded room; cue 1 adds one and companion playback adds 100.
static WorldCollisionFootstepSounds _gShelterB2BreedingRoomSurface1FootstepSounds = {
    .walk         = SHELTER_B2_BREEDING_ROOM_SURFACE_1_WALK_CUE_BASE,
    .run          = SHELTER_B2_BREEDING_ROOM_SURFACE_1_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B2_BREEDING_ROOM_SURFACE_1_WALK_CUE_BASE,
};

/// Walking, running and scripted-jump footstep cues for surface class 3.
///
/// Scripted jumps reuse walking cues. The surface record borrows these cue-2
/// bases from the loaded room; cue 1 adds one and companion playback adds 100.
static WorldCollisionFootstepSounds _gShelterB2BreedingRoomSurface3FootstepSounds = {
    .walk         = SHELTER_B2_BREEDING_ROOM_SURFACE_3_WALK_CUE_BASE,
    .run          = SHELTER_B2_BREEDING_ROOM_SURFACE_3_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B2_BREEDING_ROOM_SURFACE_3_WALK_CUE_BASE,
};

/// Walking, running and scripted-jump footstep cues for surface class 4.
///
/// Scripted jumps reuse walking cues. The surface record borrows these cue-2
/// bases from the loaded room; cue 1 adds one and companion playback adds 100.
static WorldCollisionFootstepSounds _gShelterB2BreedingRoomSurface4FootstepSounds = {
    .walk         = SHELTER_B2_BREEDING_ROOM_SURFACE_4_WALK_CUE_BASE,
    .run          = SHELTER_B2_BREEDING_ROOM_SURFACE_4_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B2_BREEDING_ROOM_SURFACE_4_WALK_CUE_BASE,
};

/// Silent, probe-blocking surface policy shared by classes 0 and 5..7.
///
/// Weapon impacts and pushback are enabled. The leading byte's role is
/// unproven; retain its stored zero. The class directory borrows this record.
static WorldCollisionSurfaceProperties _gShelterB2BreedingRoomSilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class-1 probe-blocking surface policy with weapon impacts, pushback and footsteps.
///
/// The leading byte's role is unproven; retain its stored zero. The directory
/// and borrowed footstep cues require the room overlay to remain loaded.
static WorldCollisionSurfaceProperties _gShelterB2BreedingRoomSurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB2BreedingRoomSurface1FootstepSounds,
};

/// Class-2 probe-passing surface policy with pushback, no weapon impacts or footsteps.
///
/// The leading byte's role is unproven; retain its stored zero. The class
/// directory borrows this room-owned record while the overlay remains loaded.
static WorldCollisionSurfaceProperties _gShelterB2BreedingRoomSurface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class-3 probe-blocking surface policy with weapon impacts, pushback and footsteps.
///
/// The leading byte's role is unproven; retain its stored zero. The directory
/// and borrowed footstep cues require the room overlay to remain loaded.
static WorldCollisionSurfaceProperties _gShelterB2BreedingRoomSurface3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB2BreedingRoomSurface3FootstepSounds,
};

/// Class-4 probe-blocking surface policy with weapon impacts, pushback and footsteps.
///
/// The leading byte's role is unproven; retain its stored zero. The directory
/// and borrowed footstep cues require the room overlay to remain loaded.
static WorldCollisionSurfaceProperties _gShelterB2BreedingRoomSurface4Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB2BreedingRoomSurface4FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB2BreedingRoomSurfacePropertiesByClass[8] = {
    &_gShelterB2BreedingRoomSilentSurfaceProperties,
    &_gShelterB2BreedingRoomSurface1Properties,
    &_gShelterB2BreedingRoomSurface2Properties,
    &_gShelterB2BreedingRoomSurface3Properties,
    &_gShelterB2BreedingRoomSurface4Properties,
    &_gShelterB2BreedingRoomSilentSurfaceProperties,
    &_gShelterB2BreedingRoomSilentSurfaceProperties,
    &_gShelterB2BreedingRoomSilentSurfaceProperties,
};

void shelterB2BreedingRoomRoomVisualEffectsGlowDiscTask(Task* task)
{
    _roomVisualEffectsGlowDiscTask(task);
}

void shelterB2BreedingRoomRoomVisualEffectsFlyingSparkTask(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room/room_visual_effects_burst.inc.c"

void shelterB2BreedingRoomRoomVisualEffectsFlyingOrangeBurstTask(Task* task)
{
    _roomVisualEffectsFlyingOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_burst_draw.inc.c"
