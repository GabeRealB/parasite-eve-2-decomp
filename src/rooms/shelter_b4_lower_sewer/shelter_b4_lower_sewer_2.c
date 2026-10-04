#include "rooms/shelter_b4_lower_sewer.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b4_lower_sewer_private.h"

#include "actors/task_tables.h"

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

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
/// Empty presence flag so `water_effects.h` declares the shared
/// `waterDrawSpinU16` and `waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
#include "../../shared/water_effects.h"

extern SVECTOR D_shelter_b4_lower_sewer_80181EA4[];
extern SVECTOR D_shelter_b4_lower_sewer_80181F04[];
extern SVECTOR D_shelter_b4_lower_sewer_80181F14[];

/// The two points the ribbon trail is emitted from, relative to the effect's
/// parent: the first positions the effect's own coordinate, the second is the
/// other end of the trail.
/// The second of those points, which the trail's per-frame state reaches
/// through its own label rather than by indexing the pair.

extern TaskDesc Actor04400_D107E4;
extern TaskDesc D_actor_100400_80147E48;

SVECTOR D_shelter_b4_lower_sewer_80181EA4[12] = {
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
};

SVECTOR D_shelter_b4_lower_sewer_80181F04[2] = {
    { 6730, -5090, 2290, 0 },
    { 7930, -5090, 2290, 0 },
};

SVECTOR D_shelter_b4_lower_sewer_80181F14[16] = {
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

#include "../../shared/room_visual_effects_trail_data.inc.c"

u8* D_shelter_b4_lower_sewer_80181FA4[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b4_lower_sewer_80181FA8[1] = { 9 };

DirectionWarpEntry D_shelter_b4_lower_sewer_80181FAC[4] = {
    { { { .word = 1024 }, -7821, 0, 2453 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -7800, 0, 1620 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_RESERVOIR_1BF },
    { { { .word = 3072 }, 0x3075, 0, 1820 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x3075, 0, 1180 }, { 0, 0, 0, 0 }, 0x542B0002, 0x542B0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -6000, -2000, -1180 }, { 0, 0, 0, 0 }, { { .word = 768 }, -5400, -2000, -400 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 9, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, 0x37DC, -2000, -410 }, { 0, 0, 0, 0 }, { { .word = 3328 }, 0x3908, -2000, -1400 }, { 0, 0, 0, 0 }, 0x542B0006, 0x542B0005, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_shelter_b4_lower_sewer_8018208C[8] = {
    { 8256, 1000, 1000, 0 },
    { 3584, 1000, 1000, 0 },
    { -2592, 1000, 1000, 0 },
    { -6528, 1000, 1000, 0 },
    { -1700, 1000, 1000, 0 },
    { 2000, 1000, 1000, 0 },
    { 7300, 1000, 1000, 0 },
    { 0x2CEC, 1000, 1000, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_shelter_b4_lower_sewer_801820CC[8] = {
    { 8256, 1000, 2000, 0 },
    { 3584, 1000, 2000, 0 },
    { -2592, 1000, 2000, 0 },
    { -6528, 1000, 2000, 0 },
    { -1700, 1000, 2000, 0 },
    { 2000, 1000, 2000, 0 },
    { 7300, 1000, 2000, 0 },
    { 0x2CEC, 1000, 2000, 0 },
};

// Retained data: Four retained references to the preceding eight-point coordinate pools; no runtime owner found.
SVECTOR* D_shelter_b4_lower_sewer_8018210C[4] = {
    D_shelter_b4_lower_sewer_8018208C,
    D_shelter_b4_lower_sewer_801820CC,
    D_shelter_b4_lower_sewer_8018208C,
    D_shelter_b4_lower_sewer_801820CC,
};

// Retained data: Adjacent retained point data, ending with fourth halfword -1; no runtime owner found.
SVECTOR D_shelter_b4_lower_sewer_8018211C[10] = {
    { 0, 0, 0, 0 },
    { -4900, 1000, 1000, 0 },
    { -2600, 1000, 1000, 0 },
    { 200, 1000, 1000, 0 },
    { 2000, 1000, 1000, 0 },
    { 3800, 1000, 1000, 0 },
    { 5400, 1000, 1000, 0 },
    { 8400, 1000, 1000, 0 },
    { 0x2CEC, 1000, 1000, 0 },
    { 0, 0, 0, -1 },
};

static SVECTOR _gShelterB4LowerSewerCollision05324Normals[16] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_normals.inc"
};

static SVECTOR _gShelterB4LowerSewerCollision05324Verts[100] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_verts.inc"
};

static WorldCollisionGridFace _gShelterB4LowerSewerCollision05324Faces[33] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_faces.inc"
};

static s16 _gShelterB4LowerSewerCollision05324Cells[262] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB4LowerSewerCollision05324Cells[i])
static s16* _gShelterB4LowerSewerCollision05324Table[16] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b4_lower_sewer_801828E4 = { NULL, _gShelterB4LowerSewerCollision05324Normals, _gShelterB4LowerSewerCollision05324Verts, _gShelterB4LowerSewerCollision05324Faces, _gShelterB4LowerSewerCollision05324Table, 0x39A8, 3210, 8, 2, 4000, 33 };

ViewCamera D_shelter_b4_lower_sewer_80182908[9] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, 0 } }, 207 },
    { { { { -855, 0, -4005 }, { -579, 4052, 123 }, { 3963, 592, -846 } }, { -4200, 2980, -1880 } }, 257 },
    { { { { -601, 0, -4051 }, { -787, 4017, 116 }, { 3974, 795, -589 } }, { 3545, 3080, -2150 } }, 257 },
    { { { { -804, 0, 4016 }, { 783, 4017, 156 }, { -3939, 799, -788 } }, { -5335, 3080, -2150 } }, 257 },
    { { { { -384, 0, 4077 }, { 623, 4047, 58 }, { -4029, 626, -379 } }, { 2470, 2980, -1245 } }, 257 },
    { { { { 842, 0, -4008 }, { -693, 4034, -145 }, { 3947, 709, 829 } }, { -7260, 4330, 1250 } }, 257 },
    { { { { -1021, 0, -3966 }, { 547, 4056, -140 }, { 3928, -565, -1011 } }, { 0, 2490, -130 } }, 257 },
    { { { { -780, 0, 4020 }, { -569, 4054, -110 }, { -3980, -580, -772 } }, { -4910, 2670, -90 } }, 257 },
    { { { { -974, 0, 3978 }, { 865, 3997, 211 }, { -3883, 891, -950 } }, { 410, 4410, -280 } }, 257 },
};

SpriteBatch D_shelter_b4_lower_sewer_80182A4C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_lower_sewer_80182A5C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_lower_sewer_80182A6C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b4_lower_sewer_80182A7C[3] = {
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 40, -72, 3800, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 32, -48, 3775, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 32, -16, 3750, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_lower_sewer_80182AB8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b4_lower_sewer_80182AD0[16] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -72, 1800, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, -48, 1925, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 24 } }, 40, 64, 1625, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, 64, 1420, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, 40, 48, 1675, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 48, 1438, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, 40, 32, 1725, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 32, 1488, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, 40, 16, 1775, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 16, 1506, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 40, 0, 1825, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 0, 1700, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 40, -16, 1875, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -16, 1750, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, 88, 1377, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 88, 1450, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_lower_sewer_80182C10[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b4_lower_sewer_80182C28[9] = {
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 48, 1725, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 56, 1618, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -16, 64, 1575, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -24, 72, 1452, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -24, 80, 1425, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -32, 88, 1350, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -32, 96, 1275, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -40, 104, 1200, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -40, 112, 1125, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_lower_sewer_80182CDC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_lower_sewer_80182CF4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_lower_sewer_80182D04[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b4_lower_sewer_80182D14[17] = {
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 16, 1690, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 24, 1686, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -16, 32, 1626, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -16, 40, 1625, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -32, 48, 1500, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -32, 56, 1425, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -32, 64, 1400, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -32, 72, 1250, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -32, 80, 1175, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -32, 88, 1125, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -40, 96, 1075, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -40, 104, 1025, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -40, 112, 975, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 8, 1742, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 0, 1808, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, -8, 1873, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, -16, 2005, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_lower_sewer_80182E68[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b4_lower_sewer_80182E80[9] = {
    { { .empty = D_shelter_b4_lower_sewer_80182A4C }, D_shelter_b4_lower_sewer_80182A4C, NULL },
    { { .empty = D_shelter_b4_lower_sewer_80182A5C }, D_shelter_b4_lower_sewer_80182A5C, NULL },
    { { .empty = D_shelter_b4_lower_sewer_80182A6C }, D_shelter_b4_lower_sewer_80182A6C, NULL },
    { { .elements = D_shelter_b4_lower_sewer_80182A7C }, D_shelter_b4_lower_sewer_80182AB8, NULL },
    { { .elements = D_shelter_b4_lower_sewer_80182AD0 }, D_shelter_b4_lower_sewer_80182C10, NULL },
    { { .elements = D_shelter_b4_lower_sewer_80182C28 }, D_shelter_b4_lower_sewer_80182CDC, NULL },
    { { .empty = D_shelter_b4_lower_sewer_80182CF4 }, D_shelter_b4_lower_sewer_80182CF4, NULL },
    { { .empty = D_shelter_b4_lower_sewer_80182D04 }, D_shelter_b4_lower_sewer_80182D04, NULL },
    { { .elements = D_shelter_b4_lower_sewer_80182D14 }, D_shelter_b4_lower_sewer_80182E68, NULL },
};

WorldCoordPointLight D_shelter_b4_lower_sewer_80182EEC[14] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7100, -1900, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 3662, 5242 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x3908, -4600, 2199 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 2500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3700, -4000, -430 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -99, -4000, -430 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 4341, 4920 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3400, -4000, -430 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7400, -4000, -430 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 4162, 5341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2C88, -4000, -430 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 4501, 4898 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7100, -4000, -430 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2EE5, -1800, 749 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 3859, 3860 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2C88, -1900, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7400, -1900, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 3121, 4161 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3400, -1900, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 2481, 4983 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -99, -1900, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 2300, 3921 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3700, -1900, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 2300, 5121 },
};

WorldCoordRoomLights D_shelter_b4_lower_sewer_8018342C = { 0, NULL, ARRAY_SIZE(D_shelter_b4_lower_sewer_80182EEC), D_shelter_b4_lower_sewer_80182EEC, 0, NULL };

WorldCollisionTrigger D_shelter_b4_lower_sewer_80183444[12] = {
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

WorldCollisionTrigger D_shelter_b4_lower_sewer_801837D4[12] = {
    { NULL, NULL, NULL, { -8288, -48, 2976, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_FACING, 53, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -8736, -1888, 2384, 0 }, { { 0, 1600, -784, 0 }, { 0, -1600, -784, 0 }, { 0, 1600, 784, 0 }, { 0, -1600, 784, 0 } }, { 4109, 0, 0, 0 }, { 4096, 0, 0, 0 }, 1778, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 41, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3200, -48, 1472, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_WARP, 44, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3760, -2080, -224, 0 }, { { 1024, 0, -544, 0 }, { 1024, 0, 544, 0 }, { -1024, 0, -544, 0 }, { -1024, 0, 544, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_WARP, 44, 68, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6528, -2048, -1024, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_FACING, 85, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7072, -2737, -992, 0 }, { { 0, 1296, -1024, 0 }, { 0, -1295, -1024, 0 }, { 0, 1296, 1024, 0 }, { 0, -1295, 1024, 0 } }, { 4102, 0, 0, 0 }, { 4096, 0, 0, 0 }, 1649, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 42, 50, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x30E0, -64, 736, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -176, -64, 304, 0 }, { { -1072, 0, -720, 0 }, { 1072, 0, -720, 0 }, { -1072, 0, 720, 0 }, { 1072, 0, 720, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1286, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 704, -64, 2496, 0 }, { { -1360, 0, -720, 0 }, { 1360, 0, -720, 0 }, { -1360, 0, 720, 0 }, { 1360, 0, 720, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1536, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2528, -2048, -2304, 0 }, { { -1360, 0, -720, 0 }, { 1360, 0, -720, 0 }, { -1360, 0, 720, 0 }, { 1360, 0, 720, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1536, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6976, -64, 2528, 0 }, { { -1360, 0, -720, 0 }, { 1360, 0, -720, 0 }, { -1360, 0, 720, 0 }, { 1360, 0, 720, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1536, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2784, -64, 2560, 0 }, { { -1360, 0, -720, 0 }, { 1360, 0, -720, 0 }, { -1360, 0, 720, 0 }, { 1360, 0, 720, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1536, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b4_lower_sewer_80183B64[2] = {
    { 44, 44, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor04400_D107E4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_lower_sewer_80183B7C[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_lower_sewer_80183B94[4] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { 72, 72, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207200_80153EC8 },
    { 73, 73, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207200_8014E7A4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_lower_sewer_80183BC4[2] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, Actor02000_D15FD0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_lower_sewer_80183BDC[2] = {
    { 23, 23, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102300_80147AB8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_lower_sewer_80183BF4[3] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { 49, 49, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_201100_8015F400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b4_lower_sewer_80183C18[3] = {
    { 44, 0, 0, -500, 0, 700, 2700, 0, 0, 2, 0 },
    { 44, 0, 0, 4500, 0, 2500, 3400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_lower_sewer_80183C48[2] = {
    { 4, 0, 1, 0x2CEC, 1000, 1000, 1280, 0, 0, 2, 4 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_lower_sewer_80183C68[6] = {
    { 4, 0, 1, 0x2CEC, 1000, 1000, 3072, 0, 0, 2, 2 },
    { 4, 0, 17, -5000, 1000, 2000, 1024, 0, 0, 2, 2 },
    { 72, 0, 0, 0, -2000, -930, 1024, 0, 2, 4, 0 },
    { 73, 0, 1, -3600, -2000, -1400, 1200, 0, 2, 4, 0 },
    { 73, 0, 1, 2250, -2000, -400, 700, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_lower_sewer_80183CC8[2] = {
    { 20, 4, 1, -2000, 0, 1600, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_lower_sewer_80183CE8[2] = {
    { 23, 5, 1, 7000, -2000, -1000, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_lower_sewer_80183D08[4] = {
    { 4, 0, 1, 0x2CEC, 1000, 1000, 3072, 0, 0, 2, 3 },
    { 4, 0, 17, -5000, 1000, 2000, 1024, 0, 0, 2, 3 },
    { 49, 1, 0, 1950, -2000, -1300, 200, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b4_lower_sewer_80183D48[12] = {
    { NULL, NULL },
    { D_shelter_b4_lower_sewer_80183C18, D_shelter_b4_lower_sewer_80183B64 },
    { D_shelter_b4_lower_sewer_80183C48, D_shelter_b4_lower_sewer_80183B7C },
    { D_shelter_b4_lower_sewer_80183C68, D_shelter_b4_lower_sewer_80183B94 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b4_lower_sewer_80183CC8, D_shelter_b4_lower_sewer_80183BC4 },
    { D_shelter_b4_lower_sewer_80183CE8, D_shelter_b4_lower_sewer_80183BDC },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b4_lower_sewer_80183D08, D_shelter_b4_lower_sewer_80183BF4 },
};

WorldCollisionFootstepSounds D_shelter_b4_lower_sewer_80183DA8 = {
    0x10000041,
    0x10000043,
    0x10000055,
};

WorldCollisionFootstepSounds D_shelter_b4_lower_sewer_80183DB4 = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionFootstepSounds D_shelter_b4_lower_sewer_80183DC0 = {
    0x10000015,
    0x10000017,
    0x10000019,
};

WorldCollisionSurfaceProperties D_shelter_b4_lower_sewer_80183DCC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b4_lower_sewer_80183DD4[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b4_lower_sewer_80183DDC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b4_lower_sewer_80183DB4 },
};

WorldCollisionSurfaceProperties D_shelter_b4_lower_sewer_80183DE4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b4_lower_sewer_80183DC0 },
};

WorldCollisionSurfaceProperties D_shelter_b4_lower_sewer_80183DEC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b4_lower_sewer_80183DA8 },
};

WorldCollisionSurfaceProperties* D_shelter_b4_lower_sewer_80183DF4[8] = {
    D_shelter_b4_lower_sewer_80183DCC,
    D_shelter_b4_lower_sewer_80183DD4,
    D_shelter_b4_lower_sewer_80183DDC,
    D_shelter_b4_lower_sewer_80183DE4,
    D_shelter_b4_lower_sewer_80183DCC,
    D_shelter_b4_lower_sewer_80183DEC,
    D_shelter_b4_lower_sewer_80183DCC,
    D_shelter_b4_lower_sewer_80183DCC,
};

u8* D_shelter_b4_lower_sewer_80183E14 = NULL;

/// Per-frame task drawing the room's glowing capsules. On its first tick it
/// stores the values 0x600ED, 0x600EE and 0x600EF in three gameplay globals,
/// and 0x6016E and 0x6016F in two more when GameFlag nibble 0xB7 is 1. Each
/// tick it then draws, through `glowDrawCapsule`, the
/// capsules visible from the current camera view, picked from the point-pair
/// lists `D_shelter_b4_lower_sewer_80181EA4`, `D_shelter_b4_lower_sewer_80181F04`
/// and `D_shelter_b4_lower_sewer_80181F14`.
void func_shelter_b4_lower_sewer_8017E400(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectFlashId      = EFFECT_SHELTER_B4_LOWER_SEWER_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_SHELTER_B4_LOWER_SEWER_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_SHELTER_B4_LOWER_SEWER_SPARK_BURST;
        if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) == 1) {
            gRoomEffectWaterRippleId = EFFECT_SHELTER_B4_LOWER_SEWER_WATER_RIPPLE;
            gRoomEffectWaterSprayId  = EFFECT_SHELTER_B4_LOWER_SEWER_WATER_SPRAY;
        }
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
        case 6: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181F14;
            glowDrawCapsule(&p[0], 0x200, 0x222);
            glowDrawCapsule(&p[2], 0x200, 0x222);
            break;
        }
        case 3: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181F04;
            glowDrawCapsule(&p[0], 0x200, 0x222);
            glowDrawCapsule(&p[2], 0x200, 0x222);
            glowDrawCapsule(&p[4], 0x200, 0x222);
            glowDrawCapsule(&p[6], 0x200, 0x222);
            break;
        }
        case 4: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            glowDrawCapsule(&p[0], 0x200, 0x222);
            glowDrawCapsule(&p[2], 0x200, 0x222);
            glowDrawCapsule(&p[4], 0x200, 0x222);
            glowDrawCapsule(&p[6], 0x200, 0x222);
            glowDrawCapsule(&p[24], 0x200, 0x222);
            glowDrawCapsule(&p[26], 0x200, 0x222);
            glowDrawCapsule(&p[28], 0x200, 0x222);
            break;
        }
        case 5: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            glowDrawCapsule(&p[0], 0x200, 0x222);
            glowDrawCapsule(&p[2], 0x200, 0x222);
            glowDrawCapsule(&p[28], 0x200, 0x222);
            break;
        }
        case 7: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181F04;
            glowDrawCapsule(&p[0], 0x200, 0x222);
            glowDrawCapsule(&p[2], 0x200, 0x222);
            glowDrawCapsule(&p[4], 0x200, 0x222);
            glowDrawCapsule(&p[6], 0x200, 0x222);
            glowDrawCapsule(&p[8], 0x200, 0x222);
            break;
        }
        case 8: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            glowDrawCapsule(&p[0], 0x200, 0x222);
            glowDrawCapsule(&p[2], 0x200, 0x222);
            glowDrawCapsule(&p[4], 0x200, 0x222);
            glowDrawCapsule(&p[6], 0x200, 0x222);
            glowDrawCapsule(&p[8], 0x200, 0x222);
            glowDrawCapsule(&p[22], 0x200, 0x222);
            glowDrawCapsule(&p[24], 0x200, 0x222);
            glowDrawCapsule(&p[26], 0x200, 0x222);
            glowDrawCapsule(&p[28], 0x200, 0x222);
            break;
        }
        case 9: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            glowDrawCapsule(&p[0], 0x200, 0x222);
            glowDrawCapsule(&p[2], 0x200, 0x222);
            glowDrawCapsule(&p[4], 0x200, 0x222);
            glowDrawCapsule(&p[24], 0x200, 0x222);
            glowDrawCapsule(&p[26], 0x200, 0x222);
            glowDrawCapsule(&p[28], 0x200, 0x222);
            break;
        }
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/water_ripple_task.inc.c"

void func_shelter_b4_lower_sewer_8017EEE4(Task* task)
{
    waterRippleTask(task);
}

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task_u16.inc.c"

void func_shelter_b4_lower_sewer_8017F36C(Task* task)
{
    waterDriftTaskU16(task);
}

#include "../../shared/water_spin_u16.inc.c"

#include "../../shared/water_tile_u16.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_b4_lower_sewer_8017FEB0(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b4_lower_sewer_80180914(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b4_lower_sewer_801811FC(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
