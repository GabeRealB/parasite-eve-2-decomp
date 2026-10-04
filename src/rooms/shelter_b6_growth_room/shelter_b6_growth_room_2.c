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
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

#define D_shelter_b6_growth_room_8017F298 (D_shelter_b6_growth_room_8017F258 + 8)
#define D_shelter_b6_growth_room_8017F2C8 (D_shelter_b6_growth_room_8017F258 + 14)
#define D_shelter_b6_growth_room_8017F300 (D_shelter_b6_growth_room_8017F258 + 21)

static void func_shelter_b6_growth_room_8017E448(s16 arg0, s16 arg1);
static void func_shelter_b6_growth_room_8017E7F0(GfxCoord* coord, u16 arg1, s16 arg2, u16 arg3);
static void func_shelter_b6_growth_room_8017ED28(GfxCoord* coord, u16 arg1, s16 arg2, s16 arg3);

extern TaskDesc D_actor_450900_80135E78;

/// The layout template and the live copy the reset below restores from it.
extern WorldCollisionGrid D_shelter_b6_growth_room_8017F234;

static SVECTOR _gShelterB6GrowthRoomCollision01C74Normals[4] = {
#include "assets/shelter_b6_growth_room_collision_01C74_normals.inc"
};

static SVECTOR _gShelterB6GrowthRoomCollision01C74Verts[8] = {
#include "assets/shelter_b6_growth_room_collision_01C74_verts.inc"
};

static WorldCollisionGridFace _gShelterB6GrowthRoomCollision01C74Faces[4] = {
#include "assets/shelter_b6_growth_room_collision_01C74_faces.inc"
};

static s16 _gShelterB6GrowthRoomCollision01C74Cells[6] = {
#include "assets/shelter_b6_growth_room_collision_01C74_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB6GrowthRoomCollision01C74Cells[i])
static s16* _gShelterB6GrowthRoomCollision01C74Table[1] = {
#include "assets/shelter_b6_growth_room_collision_01C74_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b6_growth_room_8017F234 = { NULL, _gShelterB6GrowthRoomCollision01C74Normals, _gShelterB6GrowthRoomCollision01C74Verts, _gShelterB6GrowthRoomCollision01C74Faces, _gShelterB6GrowthRoomCollision01C74Table, -3145, -6145, 1, 1, 4000, 4 };

/// World positions shared by the view-specific starts below.
SVECTOR D_shelter_b6_growth_room_8017F258[36] = {
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

u8* D_shelter_b6_growth_room_8017F378[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b6_growth_room_8017F37C[1] = { 8 };

DirectionWarpEntry D_shelter_b6_growth_room_8017F380[1] = {
    { { { .word = 0 }, 4030, 0, -2950 }, { 0, 0, 0, 0 }, { { .word = 0 }, 4030, 0, -2950 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, 437 },
};

static SVECTOR _gShelterB6GrowthRoomCollision02530Normals[14] = {
#include "assets/shelter_b6_growth_room_collision_02530_normals.inc"
};

static SVECTOR _gShelterB6GrowthRoomCollision02530Verts[80] = {
#include "assets/shelter_b6_growth_room_collision_02530_verts.inc"
};

static WorldCollisionGridFace _gShelterB6GrowthRoomCollision02530Faces[54] = {
#include "assets/shelter_b6_growth_room_collision_02530_faces.inc"
};

static s16 _gShelterB6GrowthRoomCollision02530Cells[208] = {
#include "assets/shelter_b6_growth_room_collision_02530_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB6GrowthRoomCollision02530Cells[i])
static s16* _gShelterB6GrowthRoomCollision02530Table[8] = {
#include "assets/shelter_b6_growth_room_collision_02530_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b6_growth_room_8017FAF0 = { NULL, _gShelterB6GrowthRoomCollision02530Normals, _gShelterB6GrowthRoomCollision02530Verts, _gShelterB6GrowthRoomCollision02530Faces, _gShelterB6GrowthRoomCollision02530Table, 500, 4000, 2, 4, 4000, 54 };

ViewCamera D_shelter_b6_growth_room_8017FB14[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -2500, 0x61A8, -3500 } }, 289 },
    { { { { -4032, 0, 718 }, { 2, 4095, 13 }, { -718, 13, -4032 } }, { -3570, 1470, -8390 } }, 230 },
    { { { { 4076, 0, 402 }, { 2, 4095, -23 }, { -402, 23, 4076 } }, { -3490, 1430, -2020 } }, 230 },
    { { { { -4066, 0, 489 }, { 331, 3011, 2756 }, { -359, 2776, -2990 } }, { -4350, 3350, 360 } }, 257 },
    { { { { 4036, 0, 697 }, { 316, 3649, -1831 }, { -621, 1858, 3596 } }, { -3112, 3278, -6073 } }, 230 },
    { { { { -4021, 0, 776 }, { 248, 3880, 1286 }, { -735, 1310, -3810 } }, { -3440, 2690, -4930 } }, 257 },
    { { { { -1147, 0, 3932 }, { 1405, 3825, 410 }, { -3672, 1463, -1071 } }, { -2780, 1860, -4500 } }, 289 },
    { { { { 798, 0, -4017 }, { -97, 4094, -19 }, { 4016, 99, 798 } }, { -4250, 1470, -5010 } }, 329 },
};

SpriteBatch D_shelter_b6_growth_room_8017FC34[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b6_growth_room_8017FC44[6] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 40, 1110, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 40, 1124, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 24, 1129, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 64, 1156, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 64, 1150, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 56, 1139, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_growth_room_8017FCBC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b6_growth_room_8017FCD4[19] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 16, 1203, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 40, 1154, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 16, 1164, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 40, 1175, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 56, 1178, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 8, 1210, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 8, 1210, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 24, 1221, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 40, 1206, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 56, 1205, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 16, 1243, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 32, 1243, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 56, 1236, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 16, 1237, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 32, 1245, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 48, 1300, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 16, 1178, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 32, 1244, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 48, 1290, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_growth_room_8017FE50[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_growth_room_8017FE68[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_growth_room_8017FE78[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_growth_room_8017FE88[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_growth_room_8017FE98[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_growth_room_8017FEA8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b6_growth_room_8017FEB8[8] = {
    { { .empty = D_shelter_b6_growth_room_8017FC34 }, D_shelter_b6_growth_room_8017FC34, NULL },
    { { .elements = D_shelter_b6_growth_room_8017FC44 }, D_shelter_b6_growth_room_8017FCBC, NULL },
    { { .elements = D_shelter_b6_growth_room_8017FCD4 }, D_shelter_b6_growth_room_8017FE50, NULL },
    { { .empty = D_shelter_b6_growth_room_8017FE68 }, D_shelter_b6_growth_room_8017FE68, NULL },
    { { .empty = D_shelter_b6_growth_room_8017FE78 }, D_shelter_b6_growth_room_8017FE78, NULL },
    { { .empty = D_shelter_b6_growth_room_8017FE88 }, D_shelter_b6_growth_room_8017FE88, NULL },
    { { .empty = D_shelter_b6_growth_room_8017FE98 }, D_shelter_b6_growth_room_8017FE98, NULL },
    { { .empty = D_shelter_b6_growth_room_8017FEA8 }, D_shelter_b6_growth_room_8017FEA8, NULL },
};

WorldCoordPointLight D_shelter_b6_growth_room_8017FF18[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2400, -2850, 4050 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 5000, 0x2710 },
};

WorldCoordRoomLights D_shelter_b6_growth_room_8017FF78 = { 0, NULL, ARRAY_SIZE(D_shelter_b6_growth_room_8017FF18), D_shelter_b6_growth_room_8017FF18, 0, NULL };

WorldCollisionTrigger D_shelter_b6_growth_room_8017FF90[12] = {
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

AreaResource D_shelter_b6_growth_room_80180320[2] = {
    { 101, 509, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_450900_80135E78 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_shelter_b6_growth_room_80180338[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017C0E0, D_shelter_b6_growth_room_80180320 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionTrigger D_shelter_b6_growth_room_801803A0[12] = {
    { NULL, NULL, NULL, { 4263, -64, 6519, 0 }, { { -462, 0, -603, 0 }, { 451, 0, -620, 0 }, { -442, 0, 604, 0 }, { 455, 0, 621, 0 } }, { 0, 4114, 0, 0 }, { 4075, 0, 402, 0 }, 768, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5152, -64, 6768, 0 }, { { -1024, 0, -1360, 0 }, { 1024, 0, -1360, 0 }, { -1024, 0, 1360, 0 }, { 1024, 0, 1360, 0 } }, { 0, 4111, 0, 0 }, { -4097, 0, 0, 0 }, 1698, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4032, -64, -3520, 0 }, { { -1024, 0, -608, 0 }, { 1024, 0, -608, 0 }, { -1024, 0, 608, 0 }, { 1024, 0, 608, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1187, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3496, -64, 6403, 0 }, { { 87, 0, -378, 0 }, { 1021, 0, -621, 0 }, { 77, 0, 599, 0 }, { 961, 0, 880, 0 } }, { 0, 4100, 0, 0 }, { 4090, 0, -201, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 736, -64, 5584, 0 }, { { -1262, 0, -2763, 0 }, { 1251, 0, -2780, 0 }, { -1242, 0, 2764, 0 }, { 1255, 0, 2781, 0 } }, { 0, 4101, 0, 0 }, { 4075, 0, 402, 0 }, 3050, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 368, -64, 1072, 0 }, { { -574, 0, -1499, 0 }, { 563, 0, -1516, 0 }, { -554, 0, 1500, 0 }, { 567, 0, 1517, 0 } }, { 0, 4095, 0, 0 }, { 4075, 0, 402, 0 }, 1619, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, 9600, 0 }, { { -574, 0, -1147, 0 }, { 563, 0, -1164, 0 }, { -554, 0, 1148, 0 }, { 567, 0, 1165, 0 } }, { 0, 4096, 0, 0 }, { 4094, 0, 1, 0 }, 1292, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1312, -64, 0x28D0, 0 }, { { -1886, 0, -587, 0 }, { 1875, 0, -604, 0 }, { -1866, 0, 588, 0 }, { 1879, 0, 605, 0 } }, { 0, 4122, 0, 0 }, { 201, 0, -4091, 0 }, 1974, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5152, -64, 8816, 0 }, { { -1024, 0, -656, 0 }, { 1024, 0, -656, 0 }, { -1024, 0, 656, 0 }, { 1024, 0, 656, 0 } }, { 0, 4096, 0, 0 }, { -4097, 0, 0, 0 }, 1214, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5152, -64, 2720, 0 }, { { -1024, 0, -2704, 0 }, { 1024, 0, -2704, 0 }, { -1024, 0, 2704, 0 }, { 1024, 0, 2704, 0 } }, { 0, 4106, 0, 0 }, { -4097, 0, 0, 0 }, 2884, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4064, -64, 0x2F20, 0 }, { { -1086, 0, -475, 0 }, { 1075, 0, -492, 0 }, { -1066, 0, 476, 0 }, { 1079, 0, 493, 0 } }, { 0, 4096, 0, 0 }, { 402, 0, -4075, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1312, -64, -1120, 0 }, { { -1086, 0, -475, 0 }, { 1075, 0, -492, 0 }, { -1066, 0, 476, 0 }, { 1079, 0, 493, 0 } }, { 0, 4096, 0, 0 }, { -1, 0, 4094, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_b6_growth_room_80180730[9] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b6_growth_room_80180730) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 539, 477, 477, 500 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_shelter_b6_growth_room_80180778 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionFootstepSounds D_shelter_b6_growth_room_80180784 = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_shelter_b6_growth_room_80180790[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b6_growth_room_80180798[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b6_growth_room_80180778 },
};

WorldCollisionSurfaceProperties D_shelter_b6_growth_room_801807A0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b6_growth_room_80180784 },
};

WorldCollisionSurfaceProperties* D_shelter_b6_growth_room_801807A8[8] = {
    D_shelter_b6_growth_room_80180790,
    D_shelter_b6_growth_room_80180798,
    D_shelter_b6_growth_room_801807A0,
    D_shelter_b6_growth_room_80180790,
    D_shelter_b6_growth_room_80180790,
    D_shelter_b6_growth_room_80180790,
    D_shelter_b6_growth_room_80180790,
    D_shelter_b6_growth_room_80180790,
};

AreaApplyRec D_shelter_b6_growth_room_801807C8[58] = {
    { 5, 5, 7, 33 },
    { 5, 8, 11, 0 },
    { 5, 10, 3, 1 },
    { 5, 11, 5, 1 },
    { 5, 12, 3, 0 },
    { 5, 13, 5, 1 },
    { 5, 14, 3, 1 },
    { 5, 15, 2, 0 },
    { 5, 17, 4, 1 },
    { 5, 18, 5, 1 },
    { 5, 19, 5, 1 },
    { 5, 21, 5, 1 },
    { 5, 27, 5, 1 },
    { 5, 29, 4, 1 },
    { 5, 30, 3, 1 },
    { 5, 32, 4, 1 },
    { 4, 2, 11, 1 },
    { 4, 5, 11, 1 },
    { 4, 8, 11, 1 },
    { 4, 9, 11, 1 },
    { 4, 10, 11, 1 },
    { 4, 11, 11, 1 },
    { 4, 12, 11, 1 },
    { 4, 14, 11, 17 },
    { 4, 14, 17, 33 },
    { 4, 15, 11, 1 },
    { 4, 17, 11, 1 },
    { 4, 18, 13, 1 },
    { 4, 19, 11, 17 },
    { 4, 19, 17, 33 },
    { 4, 24, 11, 1 },
    { 4, 25, 11, 1 },
    { 4, 27, 11, 1 },
    { 4, 28, 11, 1 },
    { 4, 29, 11, 1 },
    { 4, 30, 11, 1 },
    { 4, 32, 11, 1 },
    { 4, 33, 11, 1 },
    { 4, 34, 11, 1 },
    { 4, 35, 11, 1 },
    { 4, 42, 11, 1 },
    { 4, 43, 11, 1 },
    { 4, 44, 11, 1 },
    { 4, 45, 11, 1 },
    { 4, 46, 11, 1 },
    { 4, 47, 11, 0 },
    { 3, 3, 21, 1 },
    { 3, 5, 21, 1 },
    { 3, 9, 21, 1 },
    { 3, 11, 21, 1 },
    { 3, 12, 21, 1 },
    { 3, 15, 21, 1 },
    { 3, 20, 21, 1 },
    { 3, 22, 21, 1 },
    { 3, 25, 21, 1 },
    { 3, 26, 21, 1 },
    { 3, 32, 21, 1 },
    { 255, 0, 0, 0 },
};

/// Resets the live layout lists from the template: the four-entry vector list
/// and its 12-byte records, then the eight-entry list, which is afterwards
/// raised by 0x7D0 on y when `arg0` is nonzero.
void func_shelter_b6_growth_room_8017D82C(s32 arg0)
{
    WorldCollisionGrid* dst;
    WorldCollisionGrid* src;
    SVECTOR             d;
    s32                 i;

    dst = &D_shelter_b6_growth_room_8017FAF0;
    src = &D_shelter_b6_growth_room_8017F234;

    for (i = 0; i < 4; i++) {
        dst->normals[i].vx = src->normals[i].vx;
        dst->normals[i].vy = src->normals[i].vy;
        dst->normals[i].vz = src->normals[i].vz;
        dst->faces[i]      = src->faces[i];
    }

    for (i = 0; i < 8; i++) {
        dst->vertices[i].vx = src->vertices[i].vx;
        dst->vertices[i].vy = src->vertices[i].vy;
        dst->vertices[i].vz = src->vertices[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
    } else {
        d.vx = 0;
        d.vy = 0x7D0;
    }
    d.vz = 0;

    for (i = 0; i < 8; i++) {
        dst->vertices[i].vx += d.vx;
        dst->vertices[i].vy += d.vy;
        dst->vertices[i].vz += d.vz;
    }
}

void func_shelter_b6_growth_room_8017D9D8(Task* task)
{
    SVECTOR pos;
    s32     angle;
    s32     i;
    s32     z;

    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        if (task->spawnArg1.value < 0x130 && !(gDisplayState.animFrame & 7)) {
            task->spawnArg1.value++;
        }
    }
    if (task->state < 6) {
        task->state = (task->spawnArg1.value >> 4) + 1;
    }
    if (gDisplayState.animFrame % (task->state * 2 + 4) == 0) {
        for (i = 0; i < task->state; i++) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            angle           = ((gRandomLcgState >> 16) & 0x7FF) - 0x400;
            pos.vx          = D_shelter_b6_growth_room_8017F258[i + 30].vx + ((rcos(angle) * 1000) >> 12);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            pos.vy          = -((s32)(gRandomLcgState >> 16) % ((task->spawnArg1.value + 1) * 8));
            pos.vz          = D_shelter_b6_growth_room_8017F258[i + 30].vz + ((rsin(angle) * 1000) >> 12);
            Gp_SpawnEff(EFFECT_GROWTH_ROOM_MIST, NULL, 0x106500, &pos);
        }
    }
    if (!(gDisplayState.animFrame & 1)) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        pos.vx          = -1000;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        pos.vy          = -1200 - (gRandomLcgState >> 16) % 400;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        z               = (gRandomLcgState >> 16) % 400 + 0xDAC;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        pos.vz          = z + ((gRandomLcgState >> 16) & 1) * 1000;
        Gp_SpawnEff(EFFECT_SHELTER_B6_GROWTH_ROOM_DRIFT_PUFF, NULL, 0x183280, &pos);
    }
    func_shelter_b6_growth_room_8017E448(task->spawnArg1.value, (task->spawnArg1.value >> 1) + 0x50);
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[0], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[1], 0x200, 0x400);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[6], 0x180, 0x400);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[7], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[8], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[9], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[10], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[11], 0x180, 0x440);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[12], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[13], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[14], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[15], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[20], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[21], 0x200, 0x444);
            break;
        case 3:
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[0], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[1], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[2], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[3], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[4], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[5], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[6], 0x200, 0x400);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[7], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[12], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[13], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[24], 0x200, 0x444);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[25], 0x200, 0x444);
            break;
        case 4:
            glowDrawDisc(&D_shelter_b6_growth_room_8017F300[0], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F300[1], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F300[2], 0x100, 0x44);
            break;
        case 5:
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[0], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[1], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[2], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[3], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[4], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[5], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[6], 0x200, 0x400);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F258[7], 0x200, 0x44);
            break;
        case 6:
            glowDrawDisc(&D_shelter_b6_growth_room_8017F2C8[0], 0x180, 0x400);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F2C8[1], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F2C8[2], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F2C8[7], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F2C8[8], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F2C8[9], 0x100, 0x44);
            break;
        case 7:
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[0], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[1], 0x200, 0x400);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[2], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[3], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[9], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[10], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[11], 0x180, 0x440);
            glowDrawDisc(&D_shelter_b6_growth_room_8017F298[12], 0x200, 0x44);
            break;
    }
}

#include "../../shared/glow_draw_disc.inc.c"

static void func_shelter_b6_growth_room_8017E448(s16 arg0, s16 arg1)
{
    POLY_G4* prim;

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG4(prim);
    setRGB0(prim, 0, 0, 0);
    setRGB1(prim, 0, 0, 0);
    setRGB2(prim, arg1, arg1, arg1);
    setRGB3(prim, arg1, arg1, arg1);
    setXY4(prim, -160, 120 - arg0, 160, 120 - arg0, -160, 120, 160, 120);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(0x40 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, 0x40);
}

void func_shelter_b6_growth_room_8017E564(Task* task)
{
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    s32         vz;
    s32         t;
    s16         f2a;
    u32         rng2;
    u32         rng3;

    work->age++;
    if (task->state == 0) {
        work->scale = task->spawnArg1.value & 0xFFF;

        if (task->spawnArg1.value & 0xF000) {
            work->period = (task->spawnArg1.value >> 12) & 0x7;
        } else {
            work->period = 1;
        }

        work->age   = 0;
        task->state = 1;

        if (task->spawnArg1.value & 0xFF0000) {
            f2a = (task->spawnArg1.value >> 16) & 0xFF;
        } else {
            f2a = 0x40;
        }

        work->step      = f2a;
        work->move.vy   = 0;
        rng2            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng2;
        work->move.vx   = 0x80 - ((rng2 >> 16) & 0xFF);
        rng3            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng3;
        vz              = 0x80 - ((rng3 >> 16) & 0xFF);
        work->move.vz   = vz;
        VectorNormalSS(&work->move, &work->move);

        gte_lddp(work->step);
        gte_ldsv(&work->move);
        gte_gpf12();
        gte_stsv(&work->move);
    }

    if (work->age < work->period * 10 - 16) {
        if (work->angle < 0x40) {
            work->angle += 4;
        }
    } else {
        t           = work->age + 16;
        work->angle = 0x40 - (t - work->period * 10) * 4;
    }

    func_shelter_b6_growth_room_8017E7F0(coord, work->index, work->scale, work->angle);

    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 0xA) {
            effectKillTask(work, task);
        }
    }
}

static void func_shelter_b6_growth_room_8017E7F0(GfxCoord* coord, u16 arg1, s16 arg2, u16 arg3)
{
    void**              scratch;
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    SVECTOR*            vec;
    s32                 u0;
    s32                 v0;
    s32                 u1;
    s32                 v1;
    s16                 xy;
    u16                 vz;

    scratch                   = SCRATCH_STACK_CURSOR_SLOT;
    head                      = *scratch;
    (head - 1)->worldPoint.vx = (u16)coord->workm.t[0];
    block                     = head - 1;
    block->worldPoint.vy      = (u16)coord->workm.t[1];
    vz                        = (u16)coord->workm.t[2];
    *scratch                  = block;
    block->worldPoint.vz      = vz;
    vec                       = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
        if (block->depth >= 0x41) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            prim->tpage = 0x2C;
            prim->clut  = 0x4386;
            u0          = (arg1 & 1) << 7;
            v0          = (arg1 >> 1) << 5;
            u1          = u0 + 0x7F;
            v1          = v0 + 0x1F;
            setRGB0(prim, arg3, arg3, arg3);
            setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
            setSemiTrans(prim, 1);
            block->extent.corner.x = (arg2 * 127) / block->depth;
            block->extent.corner.y = (arg2 * 31) / block->depth;
            xy                     = block->screenX - (u16)block->extent.corner.x;
            prim->x2               = xy;
            prim->x0               = xy;
            xy                     = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = xy;
            prim->x1               = xy;
            xy                     = block->screenY - (u16)block->extent.corner.y;
            prim->y1               = xy;
            prim->y0               = xy;
            xy                     = block->screenY + (u16)block->extent.corner.y;
            prim->y3               = xy;
            prim->y2               = xy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void func_shelter_b6_growth_room_8017EAC8(Task* task)
{
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    s32         vz;
    s16         f2a;
    u32         rng2;
    u32         rng3;

    work->age++;
    if (task->state == 0) {
        work->scale     = task->spawnArg1.value & 0xFFF;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->angle     = (gRandomLcgState >> 16) & 0xFFF;

        if (task->spawnArg1.value & 0xF000) {
            work->period = (task->spawnArg1.value >> 12) & 0x7;
        } else {
            work->period = 1;
        }

        work->age   = 0;
        task->state = 1;

        if (task->spawnArg1.value & 0xFF0000) {
            f2a = (task->spawnArg1.value >> 16) & 0xFF;
        } else {
            f2a = 0x40;
        }

        work->step      = f2a;
        work->move.vy   = 0;
        rng2            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng2;
        work->move.vx   = ((rng2 >> 16) & 0x7F) + 0x40;
        rng3            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng3;
        vz              = 0x40 - ((rng3 >> 16) & 0x7F);
        work->move.vz   = vz;
        VectorNormalSS(&work->move, &work->move);

        gte_lddp(work->step);
        gte_ldsv(&work->move);
        gte_gpf12();
        gte_stsv(&work->move);
    }

    func_shelter_b6_growth_room_8017ED28(coord, work->index, work->scale, work->angle);

    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->move.vy      += 2;

    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 0xA) {
            effectKillTask(work, task);
        }
    }
}

static void func_shelter_b6_growth_room_8017ED28(GfxCoord* coord, u16 arg1, s16 arg2, s16 arg3)
{
    void**              scratch;
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    SVECTOR*            vec;
    s32                 u0;
    s32                 v0;
    s32                 u1;
    s32                 v1;
    s32                 ang;
    s32                 ang2;
    u16                 vz;

    scratch                   = SCRATCH_STACK_CURSOR_SLOT;
    head                      = *scratch;
    (head - 1)->worldPoint.vx = (u16)coord->workm.t[0];
    block                     = head - 1;
    block->worldPoint.vy      = (u16)coord->workm.t[1];
    vz                        = (u16)coord->workm.t[2];
    *scratch                  = block;
    block->worldPoint.vz      = vz;
    vec                       = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
        if (block->depth >= 0x41) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x2B;
            prim->clut  = 0x4384;
            u0          = (arg1 % 5) * 0x30;
            v0          = (arg1 / 5) * 0x30;
            u1          = u0 + 0x2F;
            v1          = v0 + 0x57;
            v0          = v0 + 0x28;
            setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
            ang                    = arg3;
            block->extent.corner.x = (((arg2 * 47) / block->depth) * rsin(ang)) >> 12;
            block->extent.corner.y = (((arg2 * 47) / block->depth) * rcos(ang)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            ang2                   = ang + 0x400;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((arg2 * 47) / block->depth) * rsin(ang2)) >> 12;
            block->extent.corner.y = (((arg2 * 47) / block->depth) * rcos(ang2)) >> 12;
            prim->x1               = block->screenX + (u16)block->extent.corner.x;
            prim->x2               = block->screenX - (u16)block->extent.corner.x;
            prim->y1               = block->screenY - (u16)block->extent.corner.y;
            prim->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
