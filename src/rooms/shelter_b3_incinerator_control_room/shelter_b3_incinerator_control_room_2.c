#include "rooms/shelter_b3_incinerator_control_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "shelter_b3_incinerator_control_room_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag_ids.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/task_types.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_cutscene.h"

#define D_shelter_b3_incinerator_control_room_801818E8 (D_shelter_b3_incinerator_control_room_80181888 + 12)

/// Glow positions `func_shelter_b3_incinerator_control_room_8017FD10` draws
/// per view.
extern SVECTOR D_shelter_b3_incinerator_control_room_80181868[];

SVECTOR D_shelter_b3_incinerator_control_room_80181868[4] = {
    { -1543, -2132, -2753, 0 },
    { -365, -2132, -2753, 0 },
    { -1543, -2042, -2677, 0 },
    { -365, -2042, -2677, 0 },
};

// Indexed views below share one contiguous table.
SVECTOR D_shelter_b3_incinerator_control_room_80181888[19] = {
    { -3967, -562, -778, 0 },
    { -3967, -562, -70, 0 },
    { -3967, -562, 1570, 0 },
    { -3967, -562, 2278, 0 },
    { -3967, -680, -778, 0 },
    { -3967, -680, -70, 0 },
    { -3967, -680, 1570, 0 },
    { -3967, -680, 2278, 0 },
    { -7049, -562, 1570, 0 },
    { -7049, -562, 2278, 0 },
    { -7049, -680, 1570, 0 },
    { -7049, -680, 2278, 0 },
    { -7049, -562, -778, 0 },
    { -7049, -562, -70, 0 },
    { -7049, -680, -778, 0 },
    { -7049, -680, -70, 0 },
    { -5786, -2150, 3559, 0 },
    { -5228, -2150, 3559, 0 },
    { -4439, -1161, -1751, 0 },
};

u8* D_shelter_b3_incinerator_control_room_80181920[2] = {
    gViewIdentityMap,
    gViewIdentityMap,
};

ViewCount D_shelter_b3_incinerator_control_room_80181928[2] = { 8, 8 };

DirectionWarpEntry D_shelter_b3_incinerator_control_room_8018192C[4] = {
    { { { .word = 2048 }, -5333, 0, 2984 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -5333, 0, 2984 }, { 0, 0, 0, 0 }, 0x54290008, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 820, 0, -2540 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 820, 0, -3100 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_RESERVOIR_1BF },
    { { { .word = 0 }, -6048, 0, -3020 }, { 0, 0, 0, 0 }, { { .word = 256 }, -5200, 0, -3020 }, { 0, 0, 0, 0 }, 0x54290006, 0x54290005, 0x54290007, 4, DIRECTION_WARP_FLAG_NONE, 459 },
    { { { .word = 2048 }, -5333, 0, 2984 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -5333, 0, 2984 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB3IncineratorControlRoomCollision04700Normals[10] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_normals.inc"
};

static SVECTOR _gShelterB3IncineratorControlRoomCollision04700Verts[27] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_verts.inc"
};

static WorldCollisionGridFace _gShelterB3IncineratorControlRoomCollision04700Faces[19] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_faces.inc"
};

static s16 _gShelterB3IncineratorControlRoomCollision04700Cells[68] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB3IncineratorControlRoomCollision04700Cells[i])
static s16* _gShelterB3IncineratorControlRoomCollision04700Table[8] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b3_incinerator_control_room_80181CC0 = { NULL, _gShelterB3IncineratorControlRoomCollision04700Normals, _gShelterB3IncineratorControlRoomCollision04700Verts, _gShelterB3IncineratorControlRoomCollision04700Faces, _gShelterB3IncineratorControlRoomCollision04700Table, 6742, 3500, 4, 2, 4000, 19 };

ViewCamera D_shelter_b3_incinerator_control_room_80181CE4[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 1000, 0x7530, 0 } }, 541 },
    { { { { 607, 0, 4050 }, { 1065, 3951, -159 }, { -3907, 1077, 586 } }, { -3319, 1397, 3178 } }, 212 },
    { { { { 607, 0, 4050 }, { 1065, 3951, -159 }, { -3907, 1077, 586 } }, { -3319, 1397, 3178 } }, 212 },
    { { { { -3911, 0, -1216 }, { -52, 4092, 170 }, { 1214, 178, -3907 } }, { 6542, 1367, -3406 } }, 230 },
    { { { { 3970, 0, -1006 }, { -3, 4095, -12 }, { 1006, 13, 3970 } }, { 6416, 1197, 2136 } }, 230 },
    { { { { -2634, 0, 3136 }, { 128, 4092, 108 }, { -3133, 168, -2632 } }, { 4213, 1411, -2685 } }, 498 },
    { { { { 4052, 0, -598 }, { -38, 4087, -263 }, { 597, 266, 4043 } }, { 6547, 1635, 2355 } }, 1104 },
    { { { { -1234, 0, -3905 }, { -938, 3975, 296 }, { 3791, 984, -1198 } }, { 5487, 1464, 1249 } }, 230 },
};

SpriteBatch D_shelter_b3_incinerator_control_room_80181E04[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_incinerator_control_room_80181E14[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_incinerator_control_room_80181E24[13] = {
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 8, -80, 1558, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, -40, 1788, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, -40, 1863, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -40, 1707, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -64, 1794, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, -64, 1783, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -64, 1517, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -80, 1828, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, -80, 1703, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -80, 1729, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, 16, -80, 1725, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 64, -80, 1725, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, 104, -80, 1725, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_incinerator_control_room_80181F28[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_incinerator_control_room_80181F40[18] = {
    { 143, 0x3FC0, { .fields = { 64, 72 } }, -160, -32, 1586, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -32, -32, 1586, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 24, 1586, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -32, 1586, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, -16, 1339, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 8, 1344, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -96, -32, 1586, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, -32, 1258, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -16, 1333, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, -16, 1249, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 8, 1339, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 8, 1254, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 24, 1310, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 24, 1256, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, -24, 1399, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -40, -24, 1339, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 88, -64, 706, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 88, 16, 713, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_incinerator_control_room_801820A8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 1, 0 } },
    { 14, 2, 0, 0, { 2, 0 } },
    { 16, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_incinerator_control_room_801820D0[2] = {
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -120, 16, 753, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -120, -56, 753, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_incinerator_control_room_801820F8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_incinerator_control_room_80182110[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_incinerator_control_room_80182120[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_incinerator_control_room_80182130[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b3_incinerator_control_room_80182140[8] = {
    { { .empty = D_shelter_b3_incinerator_control_room_80181E04 }, D_shelter_b3_incinerator_control_room_80181E04, NULL },
    { { .empty = D_shelter_b3_incinerator_control_room_80181E14 }, D_shelter_b3_incinerator_control_room_80181E14, NULL },
    { { .elements = D_shelter_b3_incinerator_control_room_80181E24 }, D_shelter_b3_incinerator_control_room_80181F28, NULL },
    { { .elements = D_shelter_b3_incinerator_control_room_80181F40 }, D_shelter_b3_incinerator_control_room_801820A8, NULL },
    { { .elements = D_shelter_b3_incinerator_control_room_801820D0 }, D_shelter_b3_incinerator_control_room_801820F8, NULL },
    { { .empty = D_shelter_b3_incinerator_control_room_80182110 }, D_shelter_b3_incinerator_control_room_80182110, NULL },
    { { .empty = D_shelter_b3_incinerator_control_room_80182120 }, D_shelter_b3_incinerator_control_room_80182120, NULL },
    { { .empty = D_shelter_b3_incinerator_control_room_80182130 }, D_shelter_b3_incinerator_control_room_80182130, NULL },
};

WorldCoordPointLight D_shelter_b3_incinerator_control_room_801821A0[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -591, -1844, -2881 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3295, -3263, 141 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3734, 3791, 3811 }, { 0, 0 } }, 1000, 0x2D03 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6412, -2764, -1784 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2370, 2309, 2426 }, { 0, 0 } }, 1000, 5940 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4412, -2882, 2029 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2128, 2126, 2128 }, { 0, 0 } }, 1000, 2477 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6584, 160, 1519 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2553, 2457, 2457 }, { 0, 0 } }, 1000, 1297 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4412, -321, 1461 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1000, 3981 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6679, 0, -121 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1000, 2100 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4352, 0, -121 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1000, 2100 },
};

WorldCoordRoomLights D_shelter_b3_incinerator_control_room_801824A0 = { 0, NULL, ARRAY_SIZE(D_shelter_b3_incinerator_control_room_801821A0), D_shelter_b3_incinerator_control_room_801821A0, 0, NULL };

WorldCollisionTrigger D_shelter_b3_incinerator_control_room_801824B8[4] = {
    { NULL, NULL, NULL, { -5537, -1536, 687, 0 }, { { -2101, -2016, -224, 0 }, { 2099, -2016, 222, 0 }, { -2101, 2016, -224, 0 }, { 2099, 2016, 222, 0 } }, { 435, 0, -4103, 0 }, { 0, 0, 4096, 0 }, 2918, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5456, -1536, 592, 0 }, { { 2067, -2016, 219, 0 }, { -2069, -2016, -221, 0 }, { 2067, 2016, 219, 0 }, { -2069, 2016, -221, 0 } }, { -435, 0, 4078, 0 }, { 0, 0, 4096, 0 }, 2896, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4305, -1248, -3633, 0 }, { { -318, -2016, -2024, 0 }, { 315, -2016, 2021, 0 }, { -318, 2016, -2024, 0 }, { 315, 2016, 2021, 0 } }, { 4050, 0, -635, 0 }, { 0, 0, 4096, 0 }, 2873, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4145, -1280, -3648, 0 }, { { 298, -2016, 1911, 0 }, { -301, -2016, -1914, 0 }, { 298, 2016, 1911, 0 }, { -301, 2016, -1914, 0 } }, { -4053, 0, 633, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b3_incinerator_control_room_801825E8[2] = {
    { 101, 426, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_142600_80135E24 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b3_incinerator_control_room_80182600[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b3_incinerator_control_room_80182610[11] = {
    { NULL, NULL },
    { D_shelter_b3_incinerator_control_room_80182600, D_shelter_b3_incinerator_control_room_801825E8 },
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

WorldCollisionTrigger D_shelter_b3_incinerator_control_room_80182668[5] = {
    { NULL, NULL, NULL, { 1201, -48, -2720, 0 }, { { -400, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 400, 0, 1024, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 38, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1792, -304, -2704, 0 }, { { 0, -2560, -1328, 0 }, { 0, 2560, -1328, 0 }, { 0, -2560, 1328, 0 }, { 0, 2560, 1328, 0 } }, { -4099, 0, 0, 0 }, { -4096, 0, 0, 0 }, 2873, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 43, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5903, -48, -3136, 0 }, { { 752, 0, -400, 0 }, { 752, 0, 400, 0 }, { -752, 0, -400, 0 }, { -752, 0, 400, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, 4096, 0 }, 851, WORLD_COLLISION_TRIGGER_ACTION_WARP, 42, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5440, -64, 3136, 0 }, { { -1424, 0, -384, 0 }, { 1424, 0, -384, 0 }, { -1424, 0, 384, 0 }, { 1424, 0, 384, 0 } }, { 0, 4113, 0, 0 }, { 0, 0, -4096, 0 }, 1470, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4576, -64, -1584, 0 }, { { -400, 0, -752, 0 }, { 400, 0, -752, 0 }, { -400, 0, 752, 0 }, { 400, 0, 752, 0 } }, { 0, 4115, 0, 0 }, { -4096, 0, 0, 0 }, 851, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b3_incinerator_control_room_801827E4[6] = {
    { NULL, NULL, NULL, { 720, -48, -2720, 0 }, { { -400, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 400, 0, 1024, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1792, -304, -2704, 0 }, { { 0, -2560, -1328, 0 }, { 0, 2560, -1328, 0 }, { 0, -2560, 1328, 0 }, { 0, 2560, 1328, 0 } }, { -4099, 0, 0, 0 }, { -4096, 0, 0, 0 }, 2873, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 43, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5600, -48, -3136, 0 }, { { 1024, 0, -400, 0 }, { 1024, 0, 400, 0 }, { -1024, 0, -400, 0 }, { -1024, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_WARP, 42, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4608, -64, -1504, 0 }, { { -400, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 400, 0, 1024, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5504, -64, 3168, 0 }, { { 1024, 0, -400, 0 }, { 1024, 0, 400, 0 }, { -1024, 0, -400, 0 }, { -1024, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4288, -64, -2080, 0 }, { { 832, 0, -1008, 0 }, { 832, 0, 1008, 0 }, { -832, 0, -1008, 0 }, { -832, 0, 1008, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 1305, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_shelter_b3_incinerator_control_room_801829AC[1] = {
    { NULL, NULL, { -1312, -95, 368, 0 }, { { -2752, 4576, -2416, 0 }, { 2752, 4576, 2416, 0 }, { -2752, -4576, -2416, 0 }, { 2752, -4576, 2416, 0 } }, { -2705, 0, 3080, 0 }, 5860, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_shelter_b3_incinerator_control_room_801829E8 = {
    0x10000041,
    0x10000043,
    0x10000041,
};

WorldCollisionFootstepSounds D_shelter_b3_incinerator_control_room_801829F4 = {
    0x10000015,
    0x10000017,
    0x10000019,
};

WorldCollisionSurfaceProperties D_shelter_b3_incinerator_control_room_80182A00[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b3_incinerator_control_room_80182A08[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b3_incinerator_control_room_801829E8 },
};

WorldCollisionSurfaceProperties D_shelter_b3_incinerator_control_room_80182A10[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b3_incinerator_control_room_801829F4 },
};

WorldCollisionSurfaceProperties D_shelter_b3_incinerator_control_room_80182A18[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b3_incinerator_control_room_801829E8 },
};

WorldCollisionSurfaceProperties* D_shelter_b3_incinerator_control_room_80182A20[8] = {
    D_shelter_b3_incinerator_control_room_80182A00,
    D_shelter_b3_incinerator_control_room_80182A08,
    D_shelter_b3_incinerator_control_room_80182A10,
    D_shelter_b3_incinerator_control_room_80182A18,
    D_shelter_b3_incinerator_control_room_80182A00,
    D_shelter_b3_incinerator_control_room_80182A00,
    D_shelter_b3_incinerator_control_room_80182A00,
    D_shelter_b3_incinerator_control_room_80182A00,
};

AreaApplyRec D_shelter_b3_incinerator_control_room_80182A40[5] = {
    { 4, 39, 2, 0 },
    { 4, 40, 2, 0 },
    { 4, 43, 7, 33 },
    { 4, 44, 7, 33 },
    { 255, 0, 0, 0 },
};

Task* gRoomCutsceneSoundTask = NULL;

RoomCutsceneRec D_shelter_b3_incinerator_control_room_80182A58;

/// Draws the glows of the current camera view at the room's fixed world
/// points; views without an entry draw nothing.
void func_shelter_b3_incinerator_control_room_8017FD10(Task* unused)
{
    u8 view;

    view = viewGetMappedIndex();
    switch (view) {
        case 2:
        case 3:
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181868[0], 0x180, 0x111);
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181868[2], 0x180, 0x111);
            break;
        case 4:
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181888[0], 0x180, 0x111);
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181888[4], 0x180, 0x111);
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181888[8], 0x180, 0x111);
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181888[10], 0x180, 0x111);
            glowDrawDiamond(&D_shelter_b3_incinerator_control_room_80181888[18], 0x60, 0x80);
            break;
        case 5:
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181888[0], 0x180, 0x111);
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181888[4], 0x180, 0x111);
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181888[2], 0x180, 0x111);
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181888[6], 0x180, 0x111);
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181888[12], 0x180, 0x111);
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181888[14], 0x180, 0x111);
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181888[16], 0x180, 0x421);
            break;
        case 6:
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_801818E8[0], 0x180, 0x111);
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_801818E8[2], 0x180, 0x111);
            break;
        case 8:
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181888[0], 0x180, 0x111);
            glowDrawCapsule(&D_shelter_b3_incinerator_control_room_80181888[4], 0x180, 0x111);
            glowDrawPulsingDisc(&D_shelter_b3_incinerator_control_room_80181888[18], 0x60, 0x80);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_diamond.inc.c"

#include "../../shared/glow_draw_pulsing_disc.inc.c"
