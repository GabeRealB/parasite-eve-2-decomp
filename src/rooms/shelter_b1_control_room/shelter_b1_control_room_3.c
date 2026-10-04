#include "actors/task_tables.h"

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
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/shelter_b1_control_room.h"

#include "shelter_b1_control_room_private.h"

#include "common.h"

#include "../../shared/room_visual_effects.h"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flying_tasks.inc.c"

#include "../../shared/room_visual_effects_disc_data.inc.c"

u8* D_shelter_b1_control_room_80181C70[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b1_control_room_80181C74[1] = { 8 };

DirectionWarpEntry D_shelter_b1_control_room_80181C78[3] = {
    { { { .word = 3072 }, 0x283C, -600, -3700 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x283C, -600, -3700 }, { 0, 0, 0, 0 }, 0x54120002, 0x54120001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, 9120, -672, -3000 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 9120, -672, -3000 }, { 0, 0, 0, 0 }, 0x54120004, 0x54120003, 0x54120005, 2, DIRECTION_WARP_FLAG_NONE, 453 },
    { { { .word = 3072 }, 0x283C, -600, -3700 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x283C, -600, -3700 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB1ControlRoomCollision04B38Normals[7] = {
#include "assets/shelter_b1_control_room_collision_04B38_normals.inc"
};

static SVECTOR _gShelterB1ControlRoomCollision04B38Verts[56] = {
#include "assets/shelter_b1_control_room_collision_04B38_verts.inc"
};

static WorldCollisionGridFace _gShelterB1ControlRoomCollision04B38Faces[29] = {
#include "assets/shelter_b1_control_room_collision_04B38_faces.inc"
};

static s16 _gShelterB1ControlRoomCollision04B38Cells[60] = {
#include "assets/shelter_b1_control_room_collision_04B38_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1ControlRoomCollision04B38Cells[i])
static s16* _gShelterB1ControlRoomCollision04B38Table[3] = {
#include "assets/shelter_b1_control_room_collision_04B38_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_control_room_801820F8 = { NULL, _gShelterB1ControlRoomCollision04B38Normals, _gShelterB1ControlRoomCollision04B38Verts, _gShelterB1ControlRoomCollision04B38Faces, _gShelterB1ControlRoomCollision04B38Table, 350, 6000, 3, 1, 4000, 29 };

ViewCamera D_shelter_b1_control_room_8018211C[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4900, 0x61A8, 4250 } }, 541 },
    { { { { 1218, 0, -3910 }, { 280, 4085, 87 }, { 3900, -294, 1214 } }, { -3730, 1370, 5270 } }, 257 },
    { { { { 1209, 0, -3913 }, { 41, 4095, 12 }, { 3912, -43, 1209 } }, { -50, 1330, 5390 } }, 257 },
    { { { { 3774, 0, 1590 }, { -11, 4095, 26 }, { -1590, -28, 3774 } }, { -5160, 2130, 4920 } }, 289 },
    { { { { 2189, 0, 3461 }, { -2880, 2271, 1822 }, { -1919, -3408, 1214 } }, { -1730, 20, 4840 } }, 329 },
    { { { { 1221, 0, 3909 }, { -29, 4095, 9 }, { -3909, -30, 1221 } }, { -6380, 1330, 5390 } }, 257 },
    { { { { 4095, 0, -35 }, { -6, 4017, -799 }, { 34, 799, 4016 } }, { -3280, 2730, 3840 } }, 289 },
    { { { { 4095, 0, -35 }, { -6, 4017, -799 }, { 34, 799, 4016 } }, { -3280, 2730, 3840 } }, 289 },
};

SpriteBatch D_shelter_b1_control_room_8018223C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_control_room_8018224C[99] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 16, 598, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 56, 512, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -8, 667, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, -8, 656, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, -8, 644, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, -8, 633, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 128, -8, 621, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 8, 623, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 8, 651, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 0, 682, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 0, 639, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 24, 579, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 32, 551, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 40, 530, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 48, 511, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 16, 684, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 32, 681, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 48, 680, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 32, 665, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, 32, 648, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 32, 631, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 32, 616, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 40, 614, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 40, 646, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 56, 440, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 72, 439, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 96, 438, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 56, 447, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 56, 455, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 56, 463, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 56, 474, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 56, 481, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 56, 500, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 56, 582, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 72, 689, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 88, 669, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 72, 502, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 88, 501, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 104, 499, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 104, 488, { .fields = { 40, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 96, 482, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 104, 472, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 72, 658, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 88, 635, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 104, 462, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 104, 454, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 72, 625, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 88, 625, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 120, 104, 446, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 24, 813, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 0, 852, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, 0, 829, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 0, 811, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 0, 794, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 0, 758, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 8, 846, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 24, 844, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 24, 823, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 8, 814, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 8, 707, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 16, 694, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 24, 697, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 24, 656, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 32, 630, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 32, 739, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 40, 611, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 40, 714, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 40, 639, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 40, 625, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 48, 786, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, 64, 782, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 48, 684, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 48, 625, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 56, 661, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 56, 625, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 64, 625, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 72, 617, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 80, 606, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 40, 613, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, 48, 595, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 56, 592, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 72, 590, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 56, 849, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 88, 590, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 104, 585, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 72, 850, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, 80, 598, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 80, 610, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 80, 629, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, -120, 1277, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -24, -120, 1288, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, -120, 1351, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, -96, 1318, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -16, -96, 1303, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -96, 1423, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, -72, 1491, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, -72, 1337, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -72, 1483, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -56, 1457, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_control_room_80182A08[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 49, 0, 0, { 1, 0 } },
    { 49, 40, 0, 0, { 2, 0 } },
    { 89, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_control_room_80182A30[43] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -8, 1561, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -8, 1515, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, -8, 1542, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 8, 1335, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 24, 1335, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 40, 1330, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 64, 1319, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 8, 1450, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 24, 1359, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, -8, 1533, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 0, 1465, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 8, 1409, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 16, 1399, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 24, 1399, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 8, 1450, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 24, 1379, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 56, 1388, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 1394, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -8, 1734, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 8, 1725, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 24, 1536, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 40, 1545, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 56, 1527, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -8, 1625, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 8, 1625, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -16, 24, 1507, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, -8, 1625, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 0, 1555, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 8, 1500, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 16, 1543, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 24, 1685, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 32, 1486, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 48, 1485, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -96, 2017, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -96, 2048, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -80, 2186, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -64, 2306, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -80, 2141, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -64, 2161, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -48, 2342, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, -96, 2059, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, -80, 2208, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, -64, 2286, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_control_room_80182D8C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 1, 0 } },
    { 18, 15, 0, 0, { 2, 0 } },
    { 33, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_control_room_80182DB4[29] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 56, 850, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, -120, 500, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, -88, 500, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, -56, 500, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, -24, 500, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 8, 500, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 40, 500, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 72, 500, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 64, 864, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 814, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 64, 823, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -128, 96, 804, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 72, 533, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 96, 509, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 72, 533, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 96, 502, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, 72, 533, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, 96, 502, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -24, 80, 500, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -24, 96, 502, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, 80, 500, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, 96, 500, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, 80, 500, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, 96, 500, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 48, 88, 500, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 72, 88, 500, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, 88, 500, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 120, 96, 500, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 96, 500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_control_room_80182FF8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 29, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_control_room_80183010[21] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -16, 286, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -40, 350, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -32, 275, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -24, 269, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -24, 276, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -16, 281, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, -24, 282, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -16, 255, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 0, -8, 253, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, -8, 259, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, -8, 288, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -24, 0, 242, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -56, 0, 265, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, -16, 322, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 8, 342, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -72, 24, 245, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -32, 24, 201, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 8, 24, 201, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, 24, 215, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, 32, 345, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 56, 396, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_control_room_801831B4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_control_room_801831CC[22] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -40, 1284, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, -40, 1301, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -40, 1320, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, -16, 1350, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, -16, 1369, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -8, 1364, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 0, 1337, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -40, 16, 1282, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 16, 1284, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, 16, 1318, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -24, 32, 1288, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 56, 1307, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 64, 1276, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 16, 1295, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -16, 1337, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -32, 1437, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -8, 1435, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 16, 1433, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, 32, 1277, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, -8, 1362, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 40, 1431, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 56, 1386, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_control_room_80183384[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_control_room_8018339C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_control_room_801833AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b1_control_room_801833BC[8] = {
    { { .empty = D_shelter_b1_control_room_8018223C }, D_shelter_b1_control_room_8018223C, NULL },
    { { .elements = D_shelter_b1_control_room_8018224C }, D_shelter_b1_control_room_80182A08, NULL },
    { { .elements = D_shelter_b1_control_room_80182A30 }, D_shelter_b1_control_room_80182D8C, NULL },
    { { .elements = D_shelter_b1_control_room_80182DB4 }, D_shelter_b1_control_room_80182FF8, NULL },
    { { .elements = D_shelter_b1_control_room_80183010 }, D_shelter_b1_control_room_801831B4, NULL },
    { { .elements = D_shelter_b1_control_room_801831CC }, D_shelter_b1_control_room_80183384, NULL },
    { { .empty = D_shelter_b1_control_room_8018339C }, D_shelter_b1_control_room_8018339C, NULL },
    { { .empty = D_shelter_b1_control_room_801833AC }, D_shelter_b1_control_room_801833AC, NULL },
};

WorldCoordPointLight D_shelter_b1_control_room_8018341C[2] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2870, -650, -4290 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1802, 2621, 2375 }, { 0, 0 } }, 5000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9530, -2550, -4380 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1228, 2129, 1966 }, { 0, 0 } }, 2000, 2750 },
};

WorldCoordRoomLights D_shelter_b1_control_room_801834DC = { 0, NULL, ARRAY_SIZE(D_shelter_b1_control_room_8018341C), D_shelter_b1_control_room_8018341C, 0, NULL };

WorldCollisionTrigger D_shelter_b1_control_room_801834F4[4] = {
    { NULL, NULL, NULL, { 6251, -1536, -4211, 0 }, { { -2, -2016, -2627, 0 }, { 2, -2016, 2628, 0 }, { -2, 2016, -2627, 0 }, { 2, 2016, 2628, 0 } }, { 4096, 0, -4, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6331, -1504, -4308, 0 }, { { -5, -2016, 2589, 0 }, { 5, -2016, -2589, 0 }, { -5, 2016, 2589, 0 }, { 5, 2016, -2589, 0 } }, { -4099, 0, -9, 0 }, { 0, 0, 4096, 0 }, 3278, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2848, -1536, -4224, 0 }, { { 252, -2016, -2617, 0 }, { -258, -2016, 2612, 0 }, { 252, 2016, -2617, 0 }, { -258, 2016, 2612, 0 } }, { 4076, 0, 397, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 3, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2975, -1600, -4225, 0 }, { { -259, -2016, 2614, 0 }, { 258, -2016, -2616, 0 }, { -259, 2016, 2614, 0 }, { 258, 2016, -2616, 0 } }, { -4078, 0, -404, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 6, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_control_room_80183624[8] = {
    { NULL, NULL, NULL, { 0x28A0, -640, -3712, 0 }, { { -448, 0, -1024, 0 }, { 448, 0, -1024, 0 }, { -448, 0, 1024, 0 }, { 448, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9248, -640, -3168, 0 }, { { -928, 0, 448, 0 }, { -928, 0, -448, 0 }, { 928, 0, 448, 0 }, { 928, 0, -448, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6720, -640, -4288, 0 }, { { -224, 0, -320, 0 }, { 224, 0, -320, 0 }, { -224, 0, 320, 0 }, { 224, 0, 320, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 390, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 35, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5760, -48, -4320, 0 }, { { -224, 0, -320, 0 }, { 224, 0, -320, 0 }, { -224, 0, 320, 0 }, { 224, 0, 320, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 390, WORLD_COLLISION_TRIGGER_ACTION_FACING, 35, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2624, -64, -2976, 0 }, { { -2176, 0, 320, 0 }, { -2176, 0, -320, 0 }, { 2176, 0, 320, 0 }, { 2176, 0, -320, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 2187, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2560, -64, -5600, 0 }, { { -2176, 0, 320, 0 }, { -2176, 0, -320, 0 }, { 2176, 0, 320, 0 }, { 2176, 0, -320, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, 4096, 0 }, 2187, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1504, -64, -4336, 0 }, { { -448, 0, -368, 0 }, { 448, 0, -368, 0 }, { -448, 0, 368, 0 }, { 448, 0, 368, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 579, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -4288, 0 }, { { -320, 0, -1696, 0 }, { 320, 0, -1696, 0 }, { -320, 0, 1696, 0 }, { 320, 0, 1696, 0 } }, { 0, 4112, 0, 0 }, { 4096, 0, 0, 0 }, 1722, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b1_control_room_80183884[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80135C30 },
    { 24, 24, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_202400_8014E47C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_control_room_801838A8[2] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80135C30 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_control_room_801838C0[2] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80135C30 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_control_room_801838D8[2] = {
    { 34, 504, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_150400_8013C8F4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_control_room_801838F0[2] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80135C30 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_control_room_80183908[8] = {
    { 21, 3, 0, -350, -3000, -3800, 1024, 0, -1, 2, 1 },
    { 21, 3, 0, -350, -3000, -4800, 1024, 0, -1, 2, 1 },
    { 21, 1, 0, -350, -2200, -3000, 1024, 0, -1, 2, 1 },
    { 21, 1, 0, -350, -2200, -5600, 1024, 0, -1, 2, 1 },
    { 24, 0, 0, 3500, 0, -3400, 2000, 0, 0, 4, 0 },
    { 24, 0, 0, 3400, 0, -5100, 2450, 0, 0, 4, 0 },
    { 24, 0, 0, 2800, 0, -4300, 1024, 0, 0, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_control_room_80183988[4] = {
    { 21, 0, 0, -600, -2000, -3500, 1024, 0, 0, 2, 0 },
    { 21, 0, 0, -600, -2000, -5000, 1024, 0, 0, 2, 0 },
    { 21, 0, 0, 0x2A94, -2300, -4800, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_control_room_801839C8[4] = {
    { 21, 0, 0, -600, -2000, -3500, 1024, 0, 0, 2, 0 },
    { 21, 0, 0, -600, -2000, -5000, 1024, 0, 0, 2, 0 },
    { 21, 0, 0, 0x2A94, -2300, -4800, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_control_room_80183A08[2] = {
    { 34, 0, 0, 4258, 0, 2700, 0, 0, 4, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_control_room_80183A28[7] = {
    { 21, 2, 0, -350, -3000, -3800, 1024, 0, 0, 2, 4 },
    { 21, 2, 0, -350, -3000, -4800, 1024, 0, 0, 2, 4 },
    { 21, 1, 0, -350, -2200, -2900, 1024, 0, 0, 2, 4 },
    { 21, 1, 0, -350, -2200, -5700, 1024, 0, 0, 2, 4 },
    { 21, 1, 0, -350, -2200, -3600, 1024, 0, 0, 2, 4 },
    { 21, 1, 0, -350, -2200, -5000, 1024, 0, 0, 2, 4 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b1_control_room_80183A98[22] = {
    { NULL, NULL },
    { D_shelter_b1_control_room_80183908, D_shelter_b1_control_room_80183884 },
    { D_shelter_b1_control_room_80183988, D_shelter_b1_control_room_801838A8 },
    { D_shelter_b1_control_room_801839C8, D_shelter_b1_control_room_801838C0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_control_room_80183A08, D_shelter_b1_control_room_801838D8 },
    { NULL, NULL },
    { D_shelter_b1_control_room_80183A28, D_shelter_b1_control_room_801838F0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCoordRoomAmbientEntry D_shelter_b1_control_room_80183B48[9] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b1_control_room_80183B48) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 582, 925, 780, 778 } },
    { .color = { 579, 901, 799, 767 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 614, 899, 793, 778 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_shelter_b1_control_room_80183B90 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionFootstepSounds D_shelter_b1_control_room_80183B9C = {
    0x10000015,
    0x10000017,
    0x10000019,
};

WorldCollisionSurfaceProperties D_shelter_b1_control_room_80183BA8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b1_control_room_80183BB0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_control_room_80183B90 },
};

WorldCollisionSurfaceProperties D_shelter_b1_control_room_80183BB8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_control_room_80183B9C },
};

WorldCollisionSurfaceProperties* D_shelter_b1_control_room_80183BC0[8] = {
    D_shelter_b1_control_room_80183BA8,
    D_shelter_b1_control_room_80183BB0,
    D_shelter_b1_control_room_80183BB8,
    D_shelter_b1_control_room_80183BA8,
    D_shelter_b1_control_room_80183BA8,
    D_shelter_b1_control_room_80183BA8,
    D_shelter_b1_control_room_80183BA8,
    D_shelter_b1_control_room_80183BA8,
};

AreaApplyRec D_shelter_b1_control_room_80183BE0[2] = {
    { 4, 18, 12, 0 },
    { 255, 0, 0, 0 },
};

void func_shelter_b1_control_room_8017FF80(Task* arg0)
{
    RoomFx_GlowDiscTask(arg0);
}

void func_shelter_b1_control_room_801804D8(Task* task)
{
    RoomFx_FlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void func_shelter_b1_control_room_80181138(Task* arg0)
{
    RoomFx_OrangeBurst2Task(arg0);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
