#include "common.h"

#include "main/task_types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
s32 D_shelter_b6_training_room_80185C58;

/// The room's tracked task, driven by `func_shelter_b6_training_room_8017D974`,
/// or NULL when none is running.
Task* D_shelter_b6_training_room_80185C5C;

#include "rooms/shelter_b6_training_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "shelter_b6_training_room_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"

#include "mapui/map_neo_ark.h"

static void func_shelter_b6_training_room_8017DBB0(s32 arg0);

extern AreaResource D_shelter_b6_training_room_80185994[6];

u8* D_shelter_b6_training_room_80184418[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b6_training_room_8018441C[2] = { 8, 0 };

DirectionWarpEntry D_shelter_b6_training_room_80184420[2] = {
    { { { .word = 1024 }, 500, 0, 1500 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 500, 0, 1500 }, { 0, 0, 0, 0 }, 0x55190005, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 4500, 0, 9390 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4500, 0, 9390 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB6TrainingRoomCollision07174Normals[7] = {
#include "assets/shelter_b6_training_room_collision_07174_normals.inc"
};

static SVECTOR _gShelterB6TrainingRoomCollision07174Verts[22] = {
#include "assets/shelter_b6_training_room_collision_07174_verts.inc"
};

static WorldCollisionGridFace _gShelterB6TrainingRoomCollision07174Faces[21] = {
#include "assets/shelter_b6_training_room_collision_07174_faces.inc"
};

static s16 _gShelterB6TrainingRoomCollision07174Cells[84] = {
#include "assets/shelter_b6_training_room_collision_07174_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB6TrainingRoomCollision07174Cells[i])
static s16* _gShelterB6TrainingRoomCollision07174Table[6] = {
#include "assets/shelter_b6_training_room_collision_07174_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b6_training_room_80184734 = { NULL, _gShelterB6TrainingRoomCollision07174Normals, _gShelterB6TrainingRoomCollision07174Verts, _gShelterB6TrainingRoomCollision07174Faces, _gShelterB6TrainingRoomCollision07174Table, 0, -750, 2, 3, 4000, 21 };

ViewCamera D_shelter_b6_training_room_80184758[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -2500, 0x61A8, -5250 } }, 380 },
    { { { { -4058, 0, 553 }, { 56, 4074, 413 }, { -550, 417, -4037 } }, { -3120, 1960, -5900 } }, 257 },
    { { { { 4069, 0, 468 }, { -98, 4004, 855 }, { -457, -861, 3978 } }, { -3190, 870, 2460 } }, 257 },
    { { { { 4064, 0, 507 }, { -119, 3980, 959 }, { -492, -967, 3949 } }, { -3240, 820, -590 } }, 257 },
    { { { { 4078, 0, 377 }, { -7, 4095, 76 }, { -376, -77, 4077 } }, { -3110, 1500, -4690 } }, 257 },
    { { { { 831, 0, 4010 }, { 365, 4078, -75 }, { -3994, 373, 828 } }, { -4510, 1400, -8940 } }, 289 },
    { { { { -4087, 0, 257 }, { 5, 4095, 80 }, { -257, 80, -4087 } }, { -2730, 1400, -0x2846 } }, 329 },
    { { { { 4085, 0, -286 }, { -138, 3587, -1972 }, { 251, 1977, 3578 } }, { -2490, 2380, -6300 } }, 329 },
};

SpriteBatch D_shelter_b6_training_room_80184878[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_training_room_80184888[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b6_training_room_80184898[28] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 64, 1565, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 88, 1359, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, -24, 1567, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, 0, 1707, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, 32, 1697, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 64, 1684, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 64, 1688, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 32, 1731, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 0, 1775, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, -32, 1693, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, -24, 1821, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, -40, 1515, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, -32, 1632, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, 8, 1746, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 48, 1711, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 72, 1577, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 48, 1430, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 72, 1405, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, -32, 1744, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 0, 1906, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 32, 1867, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 64, 1834, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -24, 1866, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 0, 1934, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 24, 1895, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 48, 1859, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 72, 1821, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -16, 1951, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_training_room_80184AC8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 1, 0 } },
    { 12, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_training_room_80184AE8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_training_room_80184AF8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_training_room_80184B08[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b6_training_room_80184B18[28] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, -80, 1589, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, -40, 1643, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, 0, 1643, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, 40, 1514, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, -72, 1711, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, -40, 1762, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 0, 1758, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, 40, 1650, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, -72, 1808, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, -40, 1832, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 0, 1836, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 40, 1680, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 40, 1784, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, -72, 1806, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -88, 1507, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -48, 1606, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -8, 1619, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, 32, 1589, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -80, 1616, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -152, -48, 1708, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -152, -8, 1708, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -152, 32, 1689, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, -80, 1680, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -48, 1743, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -8, 1747, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 32, 1752, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 48, 1722, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, -72, 1742, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_training_room_80184D48[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 1, 0 } },
    { 14, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_training_room_80184D68[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b6_training_room_80184D78[8] = {
    { { .empty = D_shelter_b6_training_room_80184878 }, D_shelter_b6_training_room_80184878, NULL },
    { { .empty = D_shelter_b6_training_room_80184888 }, D_shelter_b6_training_room_80184888, NULL },
    { { .elements = D_shelter_b6_training_room_80184898 }, D_shelter_b6_training_room_80184AC8, NULL },
    { { .empty = D_shelter_b6_training_room_80184AE8 }, D_shelter_b6_training_room_80184AE8, NULL },
    { { .empty = D_shelter_b6_training_room_80184AF8 }, D_shelter_b6_training_room_80184AF8, NULL },
    { { .empty = D_shelter_b6_training_room_80184B08 }, D_shelter_b6_training_room_80184B08, NULL },
    { { .elements = D_shelter_b6_training_room_80184B18 }, D_shelter_b6_training_room_80184D48, NULL },
    { { .empty = D_shelter_b6_training_room_80184D68 }, D_shelter_b6_training_room_80184D68, NULL },
};

WorldCoordPointLight D_shelter_b6_training_room_80184DD8[12] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2500, -2750, 7500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 1638, 1392 }, { 0, 0 } }, 3000, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2500, -2750, 3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 1638, 1392 }, { 0, 0 } }, 3000, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 250, -2000, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 2457, 1228 }, { 0, 0 } }, 750, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4750, -2000, 9500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 2457, 1228 }, { 0, 0 } }, 750, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2500, -2750, 3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1310, 1228, 983 }, { 0, 0 } }, 1000, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2500, -2750, 7500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1310, 1228, 983 }, { 0, 0 } }, 1000, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 250, -2250, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 2457, 1228 }, { 0, 0 } }, 750, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4750, -2250, 9500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 2457, 1228 }, { 0, 0 } }, 750, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1030, -1417, 0x2710 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 245, 1638, 1474 }, { 0, 0 } }, 100, 700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2070, -1417, 0x2742 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 245, 1638, 1474 }, { 0, 0 } }, 100, 600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 550, -1417, 0x2710 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 245, 1638, 1474 }, { 0, 0 } }, 100, 600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1785, -1417, 0x27A6 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1474, 245, 0 }, { 0, 0 } }, 100, 400 },
};

WorldCoordSpotLight D_shelter_b6_training_room_80185258[12] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 200, -5750, 740 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4014, 3686 }, { 0, 0 } }, { 564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4800, -5750, 6240 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4014, 3686 }, { 0, 0 } }, { -564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4800, -5750, 7740 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4014, 3686 }, { 0, 0 } }, { -564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4800, -5750, 9740 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4014, 3686 }, { 0, 0 } }, { -564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 200, -5750, 2740 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4014, 3686 }, { 0, 0 } }, { 564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 200, -5750, 4240 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4014, 3686 }, { 0, 0 } }, { 564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4800, -5750, 740 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4014, 3686 }, { 0, 0 } }, { -564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4800, -5750, 2740 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4014, 3686 }, { 0, 0 } }, { -564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4800, -5750, 4240 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4014, 3686 }, { 0, 0 } }, { -564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 200, -5750, 6240 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4014, 3686 }, { 0, 0 } }, { 564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 200, -5750, 7740 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4014, 3686 }, { 0, 0 } }, { 564, 4056, 7, 0 }, 5000, 6500, 375 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 200, -5750, 9740 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4014, 3686 }, { 0, 0 } }, { 564, 4056, 7, 0 }, 5000, 6500, 375 },
};

WorldCoordRoomLights D_shelter_b6_training_room_80185768 = { 0, NULL, ARRAY_SIZE(D_shelter_b6_training_room_80184DD8), D_shelter_b6_training_room_80184DD8, ARRAY_SIZE(D_shelter_b6_training_room_80185258), D_shelter_b6_training_room_80185258 };

WorldCollisionTrigger D_shelter_b6_training_room_80185780[7] = {
    { NULL, NULL, NULL, { 2497, -2912, 4352, 0 }, { { 3360, -3312, 0, 0 }, { -3360, -3312, 0, 0 }, { 3360, 3312, 0, 0 }, { -3360, 3312, 0, 0 } }, { 0, 0, 4106, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2480, -2929, 4480, 0 }, { { -3248, -3328, 0, 0 }, { 3248, -3328, 0, 0 }, { -3248, 3328, 0, 0 }, { 3248, 3328, 0, 0 } }, { 0, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4753, -2401, 8944, 0 }, { { -2390, -3328, -575, 0 }, { 2384, -3328, 569, 0 }, { -2390, 3328, -575, 0 }, { 2384, 3328, 569, 0 } }, { 955, 0, -3989, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4800, -2240, 8785, 0 }, { { 2446, -3312, 610, 0 }, { -2448, -3312, -612, 0 }, { 2446, 3312, 610, 0 }, { -2448, 3312, -612, 0 } }, { -995, 0, 3980, 0 }, { 0, 0, 4096, 0 }, 4159, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 173, -2304, 8607, 0 }, { { 2351, -3312, -432, 0 }, { -2352, -3312, 432, 0 }, { 2351, 3312, -432, 0 }, { -2352, 3312, 432, 0 } }, { 740, 0, 4029, 0 }, { 0, 0, 4096, 0 }, 4079, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 186, -2273, 8811, 0 }, { { -2336, -3328, 448, 0 }, { 2335, -3328, -449, 0 }, { -2336, 3328, 448, 0 }, { 2335, 3328, -449, 0 } }, { -774, 0, -4030, 0 }, { 0, 0, 4096, 0 }, 4087, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2479, -2144, 2047, 0 }, { { -4031, -3312, -3, 0 }, { 4031, -3312, 3, 0 }, { -4031, 3312, -3, 0 }, { 4031, 3312, 3, 0 } }, { 2, 0, -4097, 0 }, { 0, 0, 4096, 0 }, 5196, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b6_training_room_80185994[6] = {
    { 131, 507, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, &D_actor_350700_80169D10 },
    { 140, 507, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_350700_801708DC },
    { 51, 51, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_105100_80141464 },
    { 52, 52, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205200_8014CA60 },
    { 60, 60, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_205200_801567C4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_shelter_b6_training_room_801859DC[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017C140, D_shelter_b6_training_room_80185994 },
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

WorldCollisionTrigger D_shelter_b6_training_room_80185A44[5] = {
    { NULL, NULL, NULL, { -1056, -48, 2416, 0 }, { { -352, 0, -592, 0 }, { 352, 0, -592, 0 }, { -352, 0, 592, 0 }, { 352, 0, 592, 0 } }, { 0, 4107, 0, 0 }, { 4096, 0, 0, 0 }, 686, WORLD_COLLISION_TRIGGER_ACTION_WARP, 24, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4608, -48, 9600, 0 }, { { -352, 0, -592, 0 }, { 352, 0, -592, 0 }, { -352, 0, 592, 0 }, { 352, 0, 592, 0 } }, { 0, 4107, 0, 0 }, { -4096, 0, 0, 0 }, 686, WORLD_COLLISION_TRIGGER_ACTION_WARP, 22, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 223, -64, 1825, 0 }, { { -352, 0, -592, 0 }, { 352, 0, -592, 0 }, { -352, 0, 592, 0 }, { 352, 0, 592, 0 } }, { 0, 4107, 0, 0 }, { 4096, 0, 0, 0 }, 686, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 224, -64, 4368, 0 }, { { -352, 0, -1088, 0 }, { 352, 0, -1088, 0 }, { -352, 0, 1088, 0 }, { 352, 0, 1088, 0 } }, { 0, 4103, 0, 0 }, { 4096, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4671, -64, 4352, 0 }, { { -352, 0, -1088, 0 }, { 352, 0, -1088, 0 }, { -352, 0, 1088, 0 }, { 352, 0, 1088, 0 } }, { 0, 4103, 0, 0 }, { -4091, 0, 201, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_b6_training_room_80185BC0[9] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b6_training_room_80185BC0) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 457, 493, 457, 475 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_shelter_b6_training_room_80185C08 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionFootstepSounds D_shelter_b6_training_room_80185C14 = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_shelter_b6_training_room_80185C20[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b6_training_room_80185C28[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b6_training_room_80185C08 },
};

WorldCollisionSurfaceProperties D_shelter_b6_training_room_80185C30[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b6_training_room_80185C14 },
};

WorldCollisionSurfaceProperties* D_shelter_b6_training_room_80185C38[8] = {
    D_shelter_b6_training_room_80185C20,
    D_shelter_b6_training_room_80185C28,
    D_shelter_b6_training_room_80185C30,
    D_shelter_b6_training_room_80185C20,
    D_shelter_b6_training_room_80185C20,
    D_shelter_b6_training_room_80185C20,
    D_shelter_b6_training_room_80185C20,
    D_shelter_b6_training_room_80185C20,
};

u8 D_shelter_b6_training_room_80185C60[3][16];

GfxCoord* D_shelter_b6_training_room_80185C90;

GfxCoord* D_shelter_b6_training_room_80185C94;

u16 D_shelter_b6_training_room_80185C98;

static void func_shelter_b6_training_room_8017D7D4(Task* arg0);
static void func_shelter_b6_training_room_8017D874(Task* task);

/// The room's handler for message 0x13F1, which does nothing and returns 0.
s32 func_shelter_b6_training_room_8017D638(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming `RoomEventMsg` onto
/// the outgoing one, passes both to `func_map_neo_ark_80179B14`, and returns 1.
s32 func_shelter_b6_training_room_8017D640(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

s32 func_shelter_b6_training_room_8017D684(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 5:
            if (gameFlagGetNibble(GAME_FLAG_153) != 0) {
                Gp_RunCapCmd1(7);
            } else if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                Gp_RunCapCmd1(5);
            } else {
                Gp_RunCapCmd1(7);
            }
            break;
        case 6:
            if (gameFlagGetNibble(GAME_FLAG_154) != 0) {
                Gp_RunCapCmd1(8);
            } else if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                Gp_RunCapCmd1(6);
            } else {
                Gp_RunCapCmd1(8);
            }
            break;
        case 4:
            if (gameFlagGetNibble(GAME_FLAG_B6_CORRIDOR_EVE_PART_2_DOWN) != 0) {
                Gp_RunCapCmd1(7);
            } else if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
                Gp_RunCapCmd1(0xA);
            } else {
                Gp_RunCapCmd1(4);
            }
            break;
    }
    return 0;
}

/// The room's handler for message 0x13EF, which does nothing and returns 0.
s32 func_shelter_b6_training_room_8017D75C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b6_training_room_8017D764(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    gGameSession->flowFlags |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
    func_800E8634(D_shelter_b6_training_room_80183BB4, 0, D_shelter_b6_training_room_80184124);
    TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(3), ACTOR_COMMAND_MESSAGE_APPLY, &D_shelter_b6_training_room_80182B24, 0);
    D_shelter_b6_training_room_80185C58 = 1;
    return 0;
}

static void func_shelter_b6_training_room_8017D7D4(Task* arg0)
{
    u16* ptr;
    s32  i;

    arg0->msgTable = D_shelter_b6_training_room_80182AF4;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    ptr = (u16*)Fs_ImgBuffers;
    i   = 0;
    do {
        *ptr = (u16)(*ptr | FILE_SYSTEM_IMAGE_PIXEL_MASK);
        i   += 1;
        ptr += 1;
    } while (i <= FILE_SYSTEM_IMAGE_STRIP_COUNT * FILE_SYSTEM_IMAGE_STRIP_WORDS * 2 - 1);
    gameFlagSetNibble(GAME_FLAG_COMPANION_3_SCHEDULE, 1);
    gStageSceneMusicEntry = 0xA;
    func_shelter_b6_training_room_8017DBB0(0);
    arg0->state                         = (s32)(arg0->state + 1);
    D_shelter_b6_training_room_80185C58 = 0;
}

static void func_shelter_b6_training_room_8017D874(Task* task)
{
    u8 place;

    gCdCmdQueue.imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
    place                     = gGameSession->location.loc.variant;
    if (place == 1 && gGameSession->eventState == 0 && D_shelter_b6_training_room_80185C58 == place) {
        func_800E8614(D_shelter_b6_training_room_80184274, 0);
        D_shelter_b6_training_room_80185C58 = 2;
    }
}

/// State handlers of the room task: set-up, the per-frame tick and `taskKill`.
static const TaskFuncTable3 D_shelter_b6_training_room_8017D5C4 = {
    { func_shelter_b6_training_room_8017D7D4, func_shelter_b6_training_room_8017D874, taskKill },
};

/// Runs a task through the room's three-state table, copied onto the stack
/// first and indexed by the task's state.
void func_shelter_b6_training_room_8017D8E8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b6_training_room_8017D5C4;
    sp.funcs[task->state](task);
}

void func_shelter_b6_training_room_8017D940(void)
{
    D_shelter_b6_training_room_80185C5C = taskSpawnFromTable(&D_shelter_b6_training_room_801839A8, 0, 0, 0);
}

/// Drives the room's tracked task: an argument in 0..1 becomes its
/// `spawnArg1`; anything else kills the task and clears the pointer. Does
/// nothing when no task is tracked.
void func_shelter_b6_training_room_8017D974(s32 arg0)
{
    Task* t = D_shelter_b6_training_room_80185C5C;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1.value = arg0;
    return;
kill:
    taskKill(D_shelter_b6_training_room_80185C5C);
    D_shelter_b6_training_room_80185C5C = NULL;
}

void func_shelter_b6_training_room_8017D9C8(Task* task)
{
    Enemy* enemy;
    u16    tick;

    if (D_801156F9 == 0) {
        if (task->state == 0) {
            if (task->spawnArg1.value != 0) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            enemy = Gp_FindWorkById(gGameSession->location.loc.area | ((gGameSession->location.loc.stage << 8) | 0x1000));
            func_800B0928(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), enemy->task, 0x200, 0x100, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}

/// Spawns the task that starts the room's stream playback.
void func_shelter_b6_training_room_8017DAC8(void)
{
    taskSpawnFromTable(D_shelter_b6_training_room_8018431C, 0, 0, 0);
}

void func_shelter_b6_training_room_8017DAF8(s32 arg0)
{
    gGameSession->flowFlags |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
    if (arg0 != 0) {
        gSceneCombatState.signals.bytes.endDelayFrames = arg0;
    }
}

/// Sets the saved location to area 0x16, warp 1, room 1 and spawns task 0x11.
void func_shelter_b6_training_room_8017DB28(void)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B6_NURSERY;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 1;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 1;
    Task_Spawn(0, 0x11, 0, 0);
}

void func_shelter_b6_training_room_8017DB70(void)
{
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    Gp_PulseState1C();
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
}

static void func_shelter_b6_training_room_8017DBB0(s32 arg0)
{
    D_shelter_b6_training_room_80185C5C = NULL;
}
