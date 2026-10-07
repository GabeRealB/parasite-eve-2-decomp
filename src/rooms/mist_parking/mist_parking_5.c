#include "rooms/mist_parking.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "mist_parking_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_cutscene.h"

/// World-space points the room's glow markers are drawn at. Every view draws
/// its markers from this one table, by index.

extern WorldCollisionTrigger      D_mist_parking_80193A8C[12];
extern WorldCollisionTrigger      D_mist_parking_80193E1C[13];
extern WorldCollisionTrigger      D_mist_parking_801941F8[14];
extern WorldCoordRoomAmbientEntry D_mist_parking_8019521C[21];
extern WorldCoordRoomLights       D_mist_parking_801950A0[1];
extern WorldCoordRoomLights       D_mist_parking_80195178[1];

EvsCommand D_mist_parking_80191154[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_80190870 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8019088C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_80190BC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_80190C38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_80190BC0 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_80191214[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_80190870 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackResult = shopOpenSession }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8019088C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_80190C4C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_80190C60 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_80190C10 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_80191304[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_80190870 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8019088C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_80190C4C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_80190C60 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_80190BC0 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_801913C4[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_80190870 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8019088C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_80190C4C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_80190C60 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_80190C10 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

SVECTOR D_mist_parking_80191484[27] = {
    { 11430, -2690, -4750, 0 },
    { 10230, -2690, -4750, 0 },
    { 8070, -2690, -4750, 0 },
    { 6860, -2690, -4750, 0 },
    { 3870, -3190, -4840, 0 },
    { 3870, -3190, -3690, 0 },
    { 3870, -3190, -1600, 0 },
    { 3870, -3190, -450, 0 },
    { 3870, -3190, 1020, 0 },
    { 3870, -3190, 2180, 0 },
    { -2000, -3190, -4840, 0 },
    { -2000, -3190, -3690, 0 },
    { -2000, -3190, -1600, 0 },
    { -2000, -3190, -450, 0 },
    { -2000, -3190, 1020, 0 },
    { -2000, -3190, 2180, 0 },
    { -5870, -3190, -4840, 0 },
    { -5870, -3190, -3690, 0 },
    { -5870, -3190, -1600, 0 },
    { -5870, -3190, -450, 0 },
    { -5870, -3190, 1020, 0 },
    { -5870, -3190, 2180, 0 },
    { -7780, -3190, -2000, 0 },
    { -8940, -3190, -2000, 0 },
    { -7780, -3190, -5000, 0 },
    { -8940, -3190, -5000, 0 },
    { -7412, -1345, 773, 0 },
};

WorldCollisionRoomResources D_mist_parking_8019155C[4] = {
    { &D_mist_parking_80192204, D_mist_parking_80193A8C, D_mist_parking_80193E1C, NULL },
    { &D_mist_parking_80192204, D_mist_parking_80193A8C, D_mist_parking_80193E1C, NULL },
    { &D_mist_parking_80192204, D_mist_parking_80193A8C, D_mist_parking_801941F8, NULL },
    { &D_mist_parking_80192204, D_mist_parking_80193A8C, D_mist_parking_801941F8, NULL },
};

u8 D_mist_parking_8019159C[20] = {
    1,
    2,
    3,
    4,
    18,
    19,
    7,
    20,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    5,
    6,
    8,
};

u8* D_mist_parking_801915B0[4] = {
    D_mist_parking_8019159C,
    D_mist_parking_8019159C,
    gViewIdentityMap,
    gViewIdentityMap,
};

ViewCount D_mist_parking_801915C0[4] = { 20, 20, 20, 20 };

WorldCoordRoomLighting D_mist_parking_801915C8[4] = {
    { D_mist_parking_801950A0, NULL },
    { D_mist_parking_80195178, D_mist_parking_8019521C },
    { D_mist_parking_801950A0, NULL },
    { D_mist_parking_80195178, D_mist_parking_8019521C },
};

DirectionWarpEntry D_mist_parking_801915E8[4] = {
    { { { .word = 3072 }, 0x2EEC, 0, -5053 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x2EEC, 0, -5053 }, { 0, 0, 0, 0 }, 0x51130015, 0x51130014, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, 8448, 0, -3014 }, { 0, 0, 0, 0 }, { { .word = 0 }, 8448, 0, -3014 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 0x2EEC, 0, -5053 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x2EEC, 0, -5053 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 10, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, 5022, 0, -5260 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 5022, 0, -5260 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gMistParkingCollision14C44Normals[13] = {
#include "assets/mist_parking_collision_14C44_normals.inc"
};

static SVECTOR _gMistParkingCollision14C44Verts[125] = {
#include "assets/mist_parking_collision_14C44_verts.inc"
};

static WorldCollisionGridFace _gMistParkingCollision14C44Faces[66] = {
#include "assets/mist_parking_collision_14C44_faces.inc"
};

static s16 _gMistParkingCollision14C44Cells[434] = {
#include "assets/mist_parking_collision_14C44_cells.inc"
};

#define GRID_CELL(i) (&_gMistParkingCollision14C44Cells[i])
static s16* _gMistParkingCollision14C44Table[28] = {
#include "assets/mist_parking_collision_14C44_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mist_parking_80192204 = { NULL, _gMistParkingCollision14C44Normals, _gMistParkingCollision14C44Verts, _gMistParkingCollision14C44Faces, _gMistParkingCollision14C44Table, 0x2AF8, 7000, 7, 4, 4000, 66 };

ViewCamera D_mist_parking_80192228[20] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x6978, 0 } }, 257 },
    { { { { 1185, 0, -3920 }, { -1131, 3921, -341 }, { 3754, 1181, 1134 } }, { -4650, 1900, 5340 } }, 257 },
    { { { { -3872, 0, -1335 }, { -882, 3074, 2558 }, { 1002, 2706, -2906 } }, { -7880, 2620, 660 } }, 207 },
    { { { { 3928, 0, -1157 }, { -680, 3313, -2310 }, { 936, 2408, 3177 } }, { -7810, 2620, 3170 } }, 207 },
    { { { { 1525, 0, -3801 }, { -1441, 3789, -578 }, { 3517, 1553, 1411 } }, { 2220, 3050, 5330 } }, 257 },
    { { { { 1940, 0, 3607 }, { 848, 3981, -456 }, { -3506, 963, 1885 } }, { -4330, 2770, 5440 } }, 257 },
    { { { { 3908, 0, 1223 }, { 316, 3956, -1011 }, { -1182, 1059, 3775 } }, { 2600, 2770, 4330 } }, 257 },
    { { { { 3944, 0, -1103 }, { -319, 3920, -1143 }, { 1055, 1187, 3775 } }, { -2890, 2770, 4790 } }, 257 },
    { { { { 1474, 0, 3821 }, { 1397, 3812, -539 }, { -3556, 1497, 1372 } }, { 6830, 1450, -490 } }, 257 },
    { { { { 2493, 0, -3249 }, { 2259, 2943, 1733 }, { 2335, -2847, 1791 } }, { -2570, 630, 4110 } }, 257 },
    { { { { 4040, 0, -670 }, { -557, 2276, -3359 }, { 372, 3405, 2245 } }, { -3370, 3940, 5780 } }, 257 },
    { { { { -1614, 0, -3764 }, { -140, 4093, 60 }, { 3761, 152, -1613 } }, { -1730, 1590, 2750 } }, 680 },
    { { { { 2089, 0, -3522 }, { -459, 4060, -272 }, { 3492, 534, 2072 } }, { -2680, 1720, 5510 } }, 541 },
    { { { { 4024, 0, 759 }, { -97, 4061, 518 }, { -753, -527, 3991 } }, { -5100, 1320, 5320 } }, 257 },
    { { { { -812, 0, -4014 }, { -303, 4084, 61 }, { 4003, 309, -810 } }, { -3440, 1640, 3930 } }, 541 },
    { { { { 958, 0, 3982 }, { 680, 4035, -163 }, { -3923, 700, 944 } }, { -6690, 1740, 5270 } }, 257 },
    { { { { 2152, 0, -3484 }, { -38, 4095, -23 }, { 3484, 45, 2152 } }, { 2390, 510, 4370 } }, 329 },
    { { { { 1525, 0, -3801 }, { -1441, 3789, -578 }, { 3517, 1553, 1411 } }, { 2220, 3050, 5330 } }, 257 },
    { { { { 1940, 0, 3607 }, { 848, 3981, -456 }, { -3506, 963, 1885 } }, { -4330, 2770, 5440 } }, 257 },
    { { { { 3944, 0, -1103 }, { -319, 3920, -1143 }, { 1055, 1187, 3775 } }, { -2890, 2770, 4790 } }, 257 },
};

SpriteBatch D_mist_parking_801924F8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_parking_80192508[21] = {
    { 143, 0x3FC0, { .fields = { 48, 128 } }, -160, -120, 725, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 112 } }, -160, 8, 725, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -112, -120, 725, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 112 } }, -112, 8, 725, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -88, -120, 725, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -88, -48, 725, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -88, 8, 725, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -88, 64, 725, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -72, -120, 725, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -48, 725, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, 8, 725, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, 64, 725, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -112, 775, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, -48, 775, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, 8, 775, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, 64, 775, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, 8, 825, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, 56, 825, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 24, 875, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 56, 875, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -40, 32, 925, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_parking_801926AC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_parking_801926C4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_parking_801926D4[26] = {
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -120, -8, 720, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -56, -8, 802, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -16, -8, 799, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, -16, -120, 650, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 32 } }, -88, -120, 637, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 32 } }, -160, -120, 625, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -152, -88, 637, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 16 } }, -88, -88, 650, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -16, -88, 662, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 24 } }, -144, -72, 650, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 24 } }, -80, -72, 662, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -16, -72, 675, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -136, -48, 614, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -80, -48, 671, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -16, -48, 731, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -16, -32, 766, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -72, -32, 715, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -128, -32, 663, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -8, -24, 799, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -48, -16, 769, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -80, -8, 750, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -112, -8, 723, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 16, 840, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 0, 16, 925, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 16, 905, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -48, 24, 875, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_parking_801928DC[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 18, 0, 0, { 2, 0 } },
    { 18, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_parking_80192904[39] = {
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -160, -56, 1050, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -152, -48, 1075, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -40, 1125, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, -32, 1150, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 16, 1375, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 112 } }, -160, -40, 1050, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -136, -40, 1150, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -136, 16, 1100, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, -48, 1200, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -120, 0, 1150, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -48, 1250, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, 0, 1200, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -88, -48, 1250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -88, 0, 1250, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, -48, 1375, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 0, 1375, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -72, -24, 1375, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 0, 1375, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, 0, 1375, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 96 } }, -160, 24, 1050, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, -104, 8, 1050, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, -72, 0, 1050, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -48, 0, 1050, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -16, 1375, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -24, -32, 1375, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -8, -32, 1350, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 8, -24, 1350, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -24, 1350, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, -24, 1350, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, 0, 1100, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -16, 0, 1150, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 0, 0, 1200, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 16, 0, 1275, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, 8, 1350, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 40, 8, 1350, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 16, 40, 1275, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 0, 48, 1200, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -16, 48, 1150, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -32, 48, 1100, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_parking_80192C10[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 15, 0, 0, { 2, 0 } },
    { 19, 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_parking_80192C38[37] = {
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, -24, 1250, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 128, -24, 1250, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 104, -16, 1250, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, -16, 1250, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, 8, 1250, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 72, -16, 1250, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 32, 1250, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 80, -16, 1250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 80, 32, 1250, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -16, 1200, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 88, 40, 1200, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, 104, -16, 1150, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 104, 40, 1150, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 128, -8, 1050, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 128, 56, 1050, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 24 } }, 16, -120, 1250, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, 24, -96, 1250, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, 16, -24, 1250, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, 16, 1625, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, 16, 1500, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -48, 8, 1500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -48, 48, 1500, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -40, 8, 1500, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -40, 48, 1500, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -24, 8, 1500, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, 8, 1500, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 8, 8, 1450, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, 8, 1400, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -24, 48, 1500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -8, 48, 1500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 8, 48, 1450, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 24, 48, 1400, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 40, 24, 1350, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 40, 72, 1350, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 32, 1300, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 56, 72, 1300, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 72 } }, 72, 48, 1250, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_parking_80192F1C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { 4, 1, 0, 0, { 3, 0 } },
    { 5, 10, 0, 0, { 2, 0 } },
    { 15, 3, 0, 0, { 4, 0 } },
    { 18, 19, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_parking_80192F54[9] = {
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, -120, 1425, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, -56, 1325, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, 8, 1412, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -144, 8, 1412, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -144, -56, 1350, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -144, -112, 1437, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -136, -56, 1375, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -136, 8, 1425, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -128, 8, 1425, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_parking_80193008[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_parking_80193020[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_parking_80193030[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_parking_80193040[25] = {
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -160, -120, 125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -160, -56, 125, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -128, -120, 112, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -128, -64, 112, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -96, -120, 100, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, -80, 100, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, -120, 87, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -120, 75, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, -120, 50, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, -72, 50, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, -120, 50, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, -88, 50, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -120, 50, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 72, 42, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 24, 42, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 80, 45, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 40, 45, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 64, 88, 50, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 64, 56, 50, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 32, 96, 55, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 32, 72, 55, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 88, 57, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -8, 96, 62, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 104, 67, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 112, 75, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_parking_80193234[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 25, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_parking_8019324C[4] = {
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -88, -64, 675, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -104, -56, 675, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -112, -32, 700, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 8, 725, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_parking_8019329C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_parking_801932B4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_parking_801932C4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_parking_801932D4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_parking_801932E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_parking_801932F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_parking_80193304[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_parking_80193314[35] = {
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -160, -56, 1050, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -152, -48, 1075, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -40, 1125, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, -32, 115, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 16, 936, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 112 } }, -160, -40, 1050, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -136, -40, 1150, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -136, 16, 1100, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, -48, 1200, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -120, 0, 1150, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -48, 1250, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, 0, 1200, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -88, -48, 1250, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -88, 0, 1250, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, -48, 1375, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 0, 1375, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -24, 1375, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 0, 1375, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 0, 1375, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 8, 1350, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, 0, 1350, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 96 } }, -160, 24, 1050, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, -104, 8, 1050, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, -72, 0, 1050, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -48, 0, 1050, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, 0, 1100, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -32, 48, 1100, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -16, 48, 1150, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -16, 0, 1150, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 0, 0, 1200, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 0, 48, 1200, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 16, 40, 1275, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 16, 0, 1275, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -8, 1375, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -16, -8, 1453, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_parking_801935D0[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 15, 0, 0, { 2, 0 } },
    { 19, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_parking_801935F8[43] = {
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, -24, 1250, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 128, -24, 1250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, -16, 1250, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 104, -16, 1250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, 8, 1995, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 128, -8, 1050, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 128, 56, 1098, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, 104, -16, 1150, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, 104, 40, 1150, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 88, -16, 1200, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 88, 40, 1200, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -16, 1250, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, -16, 1250, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 72, 32, 1250, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 80, 32, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 40, 949, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 40, 938, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 40, 947, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 40, 958, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 40, 978, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 40, 1775, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 24 } }, 16, -120, 1341, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, 24, -96, 1250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, 16, -24, 1250, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -64, 16, 1414, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, 16, 1525, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -48, 8, 1500, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -48, 48, 1500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 8, 1500, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -40, 48, 1500, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -24, 8, 1500, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -24, 48, 1500, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, 8, 1500, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -8, 48, 1500, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 8, 8, 1450, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 24, 8, 1400, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 24, 48, 1400, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 8, 48, 1450, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 24, 1350, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 72, 1350, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, 32, 1300, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 72, 1300, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 72 } }, 72, 48, 1250, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_parking_80193954[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { 4, 1, 0, 0, { 3, 0 } },
    { 5, 10, 0, 0, { 2, 0 } },
    { 15, 9, 0, 0, { 4, 0 } },
    { 24, 19, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_parking_8019398C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_mist_parking_8019399C[20] = {
    { { .empty = D_mist_parking_801924F8 }, D_mist_parking_801924F8, NULL },
    { { .elements = D_mist_parking_80192508 }, D_mist_parking_801926AC, NULL },
    { { .empty = D_mist_parking_801926C4 }, D_mist_parking_801926C4, NULL },
    { { .elements = D_mist_parking_801926D4 }, D_mist_parking_801928DC, NULL },
    { { .elements = D_mist_parking_80192904 }, D_mist_parking_80192C10, NULL },
    { { .elements = D_mist_parking_80192C38 }, D_mist_parking_80192F1C, NULL },
    { { .elements = D_mist_parking_80192F54 }, D_mist_parking_80193008, NULL },
    { { .empty = D_mist_parking_80193020 }, D_mist_parking_80193020, NULL },
    { { .empty = D_mist_parking_80193030 }, D_mist_parking_80193030, NULL },
    { { .elements = D_mist_parking_80193040 }, D_mist_parking_80193234, NULL },
    { { .elements = D_mist_parking_8019324C }, D_mist_parking_8019329C, NULL },
    { { .empty = D_mist_parking_801932B4 }, D_mist_parking_801932B4, NULL },
    { { .empty = D_mist_parking_801932C4 }, D_mist_parking_801932C4, NULL },
    { { .empty = D_mist_parking_801932D4 }, D_mist_parking_801932D4, NULL },
    { { .empty = D_mist_parking_801932E4 }, D_mist_parking_801932E4, NULL },
    { { .empty = D_mist_parking_801932F4 }, D_mist_parking_801932F4, NULL },
    { { .empty = D_mist_parking_80193304 }, D_mist_parking_80193304, NULL },
    { { .elements = D_mist_parking_80193314 }, D_mist_parking_801935D0, NULL },
    { { .elements = D_mist_parking_801935F8 }, D_mist_parking_80193954, NULL },
    { { .empty = D_mist_parking_8019398C }, D_mist_parking_8019398C, NULL },
};

WorldCollisionTrigger D_mist_parking_80193A8C[12] = {
    { NULL, NULL, NULL, { 6687, -2240, -4768, 0 }, { { -114, -3264, -1385, 0 }, { 113, -3264, 1384, 0 }, { -114, 3264, -1385, 0 }, { 113, 3264, 1384, 0 } }, { 4103, 0, -337, 0 }, { 0, 0, 4096, 0 }, 3547, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6800, -1648, -4864, 0 }, { { 171, -2672, 1615, 0 }, { -172, -2672, -1616, 0 }, { 171, 2672, 1615, 0 }, { -172, 2672, -1616, 0 } }, { -4090, 0, 433, 0 }, { 0, 0, 4096, 0 }, 3124, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8543, -2160, -3041, 0 }, { { -1624, -3184, 13, 0 }, { 1625, -3184, -12, 0 }, { -1624, 3184, 13, 0 }, { 1625, 3184, -12, 0 } }, { -32, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 3565, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8542, -1984, -3298, 0 }, { { 1625, -3008, -12, 0 }, { -1624, -3008, 13, 0 }, { 1625, 3008, -12, 0 }, { -1624, 3008, 13, 0 } }, { 31, 0, 4115, 0 }, { 0, 0, 4096, 0 }, 3415, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8542, -1792, -1922, 0 }, { { -1611, -2816, -229, 0 }, { 1605, -2816, 223, 0 }, { -1611, 2816, -229, 0 }, { 1605, 2816, 223, 0 } }, { 569, 0, -4058, 0 }, { 0, 0, 4096, 0 }, 3248, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8543, -1648, -1953, 0 }, { { 1606, -2672, 224, 0 }, { -1610, -2672, -228, 0 }, { 1606, 2672, 224, 0 }, { -1610, 2672, -228, 0 } }, { -573, 0, 4069, 0 }, { 0, 0, 4096, 0 }, 3124, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 703, -1616, -5185, 0 }, { { 12, -2640, 1624, 0 }, { -13, -2640, -1625, 0 }, { 12, 2640, 1624, 0 }, { -13, 2640, -1625, 0 } }, { -4125, 0, 31, 0 }, { 0, 0, 4096, 0 }, 3093, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 543, -1712, -5217, 0 }, { { -12, -2736, -1624, 0 }, { 13, -2736, 1625, 0 }, { -12, 2736, -1624, 0 }, { 13, 2736, 1625, 0 } }, { 4119, 0, -33, 0 }, { 0, 0, 4096, 0 }, 3176, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4223, -1680, -1154, 0 }, { { -1624, -2704, 13, 0 }, { 1625, -2704, -12, 0 }, { -1624, 2704, 13, 0 }, { 1625, 2704, -12, 0 } }, { -33, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 5, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4223, -1728, -1474, 0 }, { { 1625, -2752, -12, 0 }, { -1624, -2752, 13, 0 }, { 1625, 2752, -12, 0 }, { -1624, 2752, 13, 0 } }, { 31, 0, 4114, 0 }, { 0, 0, 4096, 0 }, 3187, 0, 8, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4257, -1936, -1089, 0 }, { { 2937, -2960, -204, 0 }, { -2936, -2960, 205, 0 }, { 2937, 2960, -204, 0 }, { -2936, 2960, 205, 0 } }, { 284, 0, 4086, 0 }, { 0, 0, 4096, 0 }, 4159, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4321, -2208, -801, 0 }, { { -2936, -3232, 205, 0 }, { 2937, -3232, -204, 0 }, { -2936, 3232, 205, 0 }, { 2937, 3232, -204, 0 } }, { -286, 0, -4094, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_mist_parking_80193E1C[13] = {
    { NULL, NULL, NULL, { 4960, -64, 2664, 0 }, { { 925, 0, 1264, 0 }, { -964, 0, 1259, 0 }, { 917, 0, -1274, 0 }, { -876, 0, -1247, 0 } }, { 0, 4108, 0, 0 }, { -4051, 0, -600, 0 }, 1583, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 480, -64, -4193, 0 }, { { -2310, 0, 1056, 0 }, { -2511, 0, -726, 0 }, { 2512, 0, 727, 0 }, { 2311, 0, -1055, 0 } }, { 0, 4099, 0, 0 }, { -601, 0, -4051, 0 }, 2610, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 800, -64, 2047, 0 }, { { -2977, 0, 1622, 0 }, { -2953, 0, -1643, 0 }, { 2985, 0, 1676, 0 }, { 2945, 0, -1654, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4095, 0 }, 3415, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6976, -64, 479, 0 }, { { -865, 0, 918, 0 }, { -841, 0, -875, 0 }, { 841, 0, 876, 0 }, { 865, 0, -918, 0 } }, { 0, 4108, 0, 0 }, { 4095, 0, 0, 0 }, 1260, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9736, -96, -3553, 0 }, { { -761, 0, 2422, 0 }, { -769, 0, -2443, 0 }, { 753, 0, 2444, 0 }, { 777, 0, -2422, 0 } }, { 0, 4098, 0, 0 }, { 4075, 0, -401, 0 }, 2560, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4513, -96, 3775, 0 }, { { 2427, 0, 747, 0 }, { -2438, 0, 784, 0 }, { 2440, 0, -767, 0 }, { -2426, 0, -762, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, -4075, 0 }, 2560, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8639, -48, -3854, 0 }, { { 634, 0, -368, 0 }, { 623, 0, 369, 0 }, { -623, 0, -369, 0 }, { -634, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { -202, 0, -4092, 0 }, 732, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8448, -32, -2560, 0 }, { { 634, 0, -368, 0 }, { 623, 0, 369, 0 }, { -623, 0, -369, 0 }, { -634, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { -200, 0, 4091, 0 }, 732, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4752, -64, -5905, 0 }, { { -1219, 0, 1639, 0 }, { -1477, 0, -281, 0 }, { 1253, 0, 1561, 0 }, { 1444, 0, -167, 0 } }, { 0, 4109, 0, 0 }, { 4051, 0, -601, 0 }, 2039, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8432, -64, -1505, 0 }, { { -1070, 0, 252, 0 }, { -1051, 0, -453, 0 }, { 1052, 0, 455, 0 }, { 1071, 0, -251, 0 } }, { 0, 4107, 0, 0 }, { -201, 0, -4091, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x30C0, -64, -4656, 0 }, { { -878, 0, 1036, 0 }, { -859, 0, -1045, 0 }, { 860, 0, 1047, 0 }, { 879, 0, -1035, 0 } }, { 0, 4099, 0, 0 }, { -4091, 0, 201, 0 }, 1354, WORLD_COLLISION_TRIGGER_ACTION_WARP, 20, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 544, -64, -3296, 0 }, { { -3238, 0, 1632, 0 }, { -3631, 0, -1526, 0 }, { 3472, 0, 1143, 0 }, { 3079, 0, -2015, 0 } }, { 0, 4113, 0, 0 }, { -601, 0, -4051, 0 }, 3932, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -8464, -32, -1408, 0 }, { { 1386, 32, -368, 0 }, { 1375, 32, 369, 0 }, { -1375, -32, -369, 0 }, { -1386, -32, 368, 0 } }, { -100, 4097, -9, 0 }, { -202, 0, -4092, 0 }, 1431, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_mist_parking_801941F8[14] = {
    { NULL, NULL, NULL, { 2879, -64, -3537, 0 }, { { -1055, 0, -1286, 0 }, { 727, 0, -1487, 0 }, { -726, 0, 1488, 0 }, { 1056, 0, 1287, 0 } }, { 0, 4099, 0, 0 }, { 4051, 0, -601, 0 }, 1664, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4960, -64, 2664, 0 }, { { 925, 0, 1264, 0 }, { -964, 0, 1259, 0 }, { 917, 0, -1274, 0 }, { -876, 0, -1247, 0 } }, { 0, 4108, 0, 0 }, { -4051, 0, -600, 0 }, 1583, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -16, -64, -4193, 0 }, { { -1782, 0, 1056, 0 }, { -1983, 0, -726, 0 }, { 1984, 0, 727, 0 }, { 1783, 0, -1055, 0 } }, { 0, 4107, 0, 0 }, { -601, 0, -4051, 0 }, 2111, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 800, -64, 2047, 0 }, { { -2977, 0, 1622, 0 }, { -2953, 0, -1643, 0 }, { 2985, 0, 1676, 0 }, { 2945, 0, -1654, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4095, 0 }, 3415, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6976, -64, 479, 0 }, { { -865, 0, 918, 0 }, { -841, 0, -875, 0 }, { 841, 0, 876, 0 }, { 865, 0, -918, 0 } }, { 0, 4108, 0, 0 }, { 4095, 0, 0, 0 }, 1260, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9736, -96, -3553, 0 }, { { -761, 0, 2422, 0 }, { -769, 0, -2443, 0 }, { 753, 0, 2444, 0 }, { 777, 0, -2422, 0 } }, { 0, 4098, 0, 0 }, { 4075, 0, -401, 0 }, 2560, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4513, -96, 3775, 0 }, { { 2427, 0, 747, 0 }, { -2438, 0, 784, 0 }, { 2440, 0, -767, 0 }, { -2426, 0, -762, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, -4075, 0 }, 2560, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8751, -48, -3694, 0 }, { { 682, 0, -368, 0 }, { 671, 0, 369, 0 }, { -671, 0, -369, 0 }, { -682, 0, 368, 0 } }, { 0, 4100, 0, 0 }, { -202, 0, -4092, 0 }, 773, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8480, -32, -2656, 0 }, { { 634, 0, -368, 0 }, { 623, 0, 369, 0 }, { -623, 0, -369, 0 }, { -634, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { -200, 0, 4091, 0 }, 732, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4752, -64, -5905, 0 }, { { -1219, 0, 1639, 0 }, { -1477, 0, -281, 0 }, { 1253, 0, 1561, 0 }, { 1444, 0, -167, 0 } }, { 0, 4109, 0, 0 }, { 4051, 0, -601, 0 }, 2039, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8464, -64, -1425, 0 }, { { -1038, 0, 236, 0 }, { -1019, 0, -469, 0 }, { 1020, 0, 471, 0 }, { 1039, 0, -235, 0 } }, { 0, 4096, 0, 0 }, { -201, 0, -4091, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x30C0, -64, -4656, 0 }, { { -878, 0, 1036, 0 }, { -859, 0, -1045, 0 }, { 860, 0, 1047, 0 }, { 879, 0, -1035, 0 } }, { 0, 4099, 0, 0 }, { -4091, 0, 201, 0 }, 1354, WORLD_COLLISION_TRIGGER_ACTION_WARP, 20, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -3232, 0 }, { { -2934, 0, 1184, 0 }, { -3263, 0, -1590, 0 }, { 2400, 0, 631, 0 }, { 2071, 0, -2143, 0 } }, { 0, 4104, 0, 0 }, { -601, 0, -4051, 0 }, 3629, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -8784, -64, -1328, 0 }, { { 1530, 0, -544, 0 }, { 1519, 0, 545, 0 }, { -1519, 0, -545, 0 }, { -1530, 0, 544, 0 } }, { 0, 4097, 0, 0 }, { -202, 0, -4092, 0 }, 1619, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordPointLight D_mist_parking_80194620[28] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x4FB0, -4600, -1460 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x4FB0, -4600, -5530 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x5F64, -5400, -5530 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x5F64, -5400, -1460 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x6F04, -6200, -1460 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x6F04, -6200, -5530 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3870, -3000, 1630 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3870, -3000, -990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3870, -3000, -4270 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2000, -3000, -4270 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2000, -3000, -990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2000, -3000, 1630 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5870, -3000, 1630 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5870, -3000, -990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5870, -3000, -4270 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -8370, -3000, -5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -8370, -3000, -2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5143, -2876, -4752 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3428, 4033, 3661 }, { 0, 0 } }, 3221, 4203 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8510, -2320, -590 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7518, -2500, -4749 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 2048, 2457 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4257, -3000, 6007 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 4000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x289F, -2500, -4746 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 2048, 2457 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x7404, -6000, -3500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3174, 3788, 3788 }, { 0, 0 } }, 7000, 0x4650 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x3052, -3000, -1460 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x3052, -3000, -5530 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x4024, -3800, -5530 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x4024, -3800, -1460 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 869, -3000, -5187 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
};

WorldCoordRoomLights D_mist_parking_801950A0[1] = {
    { 0, NULL, ARRAY_SIZE(D_mist_parking_80194620), D_mist_parking_80194620, 0, NULL },
};

WorldCoordPointLight D_mist_parking_801950B8[2] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8510, -2000, 209 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2252, 2048 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8510, -2000, -1857 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2252, 2048 }, { 0, 0 } }, 1500, 2500 },
};

WorldCoordRoomLights D_mist_parking_80195178[1] = {
    { 0, NULL, ARRAY_SIZE(D_mist_parking_801950B8), D_mist_parking_801950B8, 0, NULL },
};

AreaResource D_mist_parking_80195190[3] = {
    { 143, 131, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_113100_80144308 },
    { 115, 131, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_213100_801521A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_mist_parking_801951B4[13] = {
    { NULL, NULL },
    { D_map_akropolis_8017BDBC, D_mist_parking_80195190 },
    { D_map_akropolis_8017BDBC, D_mist_parking_80195190 },
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

WorldCoordRoomAmbientEntry D_mist_parking_8019521C[21] = {
    { .viewCount = ARRAY_SIZE(D_mist_parking_8019521C) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1000, 1000, 1000, 1000 } },
    { .color = { 1500, 1500, 1500, 1500 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_mist_parking_801952C4 = {
    0x10000011,
    0x10000013,
    0x10000011,
};

WorldCollisionSurfaceProperties D_mist_parking_801952D0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_mist_parking_801952D8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_mist_parking_801952C4 },
};

WorldCollisionSurfaceProperties D_mist_parking_801952E0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_mist_parking_801952E8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_mist_parking_801952C4 },
};

WorldCollisionSurfaceProperties* D_mist_parking_801952F0[8] = {
    D_mist_parking_801952D0,
    D_mist_parking_801952D8,
    D_mist_parking_801952E0,
    D_mist_parking_801952E8,
    D_mist_parking_801952D0,
    D_mist_parking_801952D0,
    D_mist_parking_801952D0,
    D_mist_parking_801952D0,
};

s32 Shop_Data_80187628 = 0;

const EquipmentWeaponSupply* Shop_Data_8018762C = NULL;

Task* gRoomCutsceneSoundTask = NULL;

s32 D_mist_parking_8019531C = 0;

Task* D_mist_parking_80195320 = NULL;

Task* D_mist_parking_80195324 = NULL;

MistParkingPrizeAnnouncementState D_mist_parking_80195328 = { 0 };

MistParkingHeadAimHandle D_mist_parking_8019532C = { 0 };

MistParkingShopTalkState D_mist_parking_80195334 = { 0, 0, 0, 0 };

RoomCutsceneRec D_mist_parking_8019533C = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);
static void _glowDrawDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _glowDrawPulsingDisc(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);

/// Draws one parking-area row of three capsule glows.
///
/// Borrows six contiguous, word-aligned `worldEndpoints`, pairing entries
/// 0/1, 2/3 and 4/5 in order. A negative GTE flag at either endpoint skips
/// that capsule; the other pairs are still drawn. The signed low halfword of
/// `radiusScale` gives each endpoint's pixel radius as that value times 64
/// divided by camera Z / 4, which must be nonzero for accepted projections.
/// `rgbNibbles` bits 8..11, 4..7 and 0..3 supply RGB channels scaled by 16;
/// odd animation frames set intensity bit 3 in each channel.
///
/// Requires the current view, an initialized scratch stack with room for one
/// `OverlayPointPairScratch`, and a current ordering table and packet arena.
/// Queues up to eighteen additive Gouraud quads plus their blend commands.
/// Retains no endpoint pointer; packets use the frame arena until GPU completion.
static inline void _mistParkingDrawGlowRow(const SVECTOR worldEndpoints[6], s32 radiusScale, s32 rgbNibbles)
{
    _glowDrawCapsule(worldEndpoints, radiusScale, rgbNibbles);
    _glowDrawCapsule(worldEndpoints + 2, radiusScale, rgbNibbles);
    _glowDrawCapsule(worldEndpoints + 4, radiusScale, rgbNibbles);
}

void mistParkingDrawGlowsTask(Task* unused)
{
    enum {
        MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE       = 0x200, // Pixel radius = scale * 64 / (camera Z / 4)
        MIST_PARKING_GLOW_GREY_RGB_NIBBLES           = 0x444, // Each channel is 64, with bit 3 set on odd animation frames
        MIST_PARKING_GLOW_POINT_INDEX                = 26,    // Single-point glow, separate from the capsule endpoint pairs
        MIST_PARKING_GLOW_POINT_PULSE_RATE           = 0x60,  // 4096 angle units per turn, per animation frame
        MIST_PARKING_GLOW_DIAMOND_RADIUS_SCALE       = 0x100, // Pixel half-extent = scale * 32 / (camera Z / 4)
        MIST_PARKING_GLOW_VIEW7_DIAMOND_RADIUS_SCALE = 0xE0,
        MIST_PARKING_GLOW_DISC_RADIUS_SCALE          = 0x40   // Outer pixel radius = scale * 64 / (camera Z / 4)
    };

    u8 mappedViewIndex;

    // Select by mapped camera index; logical views can use a different ordering.
    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 2:
            _glowDrawCapsule(&D_mist_parking_80191484[0], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 18:
            _mistParkingDrawGlowRow(&D_mist_parking_80191484[4], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 7:
            _glowDrawCapsule(&D_mist_parking_80191484[12], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(&D_mist_parking_80191484[14], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(&D_mist_parking_80191484[18], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(&D_mist_parking_80191484[20], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawDiamond(&D_mist_parking_80191484[MIST_PARKING_GLOW_POINT_INDEX], MIST_PARKING_GLOW_POINT_PULSE_RATE, MIST_PARKING_GLOW_VIEW7_DIAMOND_RADIUS_SCALE);
            break;
        case 20:
            _glowDrawCapsule(&D_mist_parking_80191484[6], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(&D_mist_parking_80191484[8], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 9:
            _glowDrawPulsingDisc(&D_mist_parking_80191484[MIST_PARKING_GLOW_POINT_INDEX], MIST_PARKING_GLOW_POINT_PULSE_RATE, MIST_PARKING_GLOW_DISC_RADIUS_SCALE);
            break;
        case 10:
            _glowDrawCapsule(&D_mist_parking_80191484[4], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(&D_mist_parking_80191484[6], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 11:
            _glowDrawCapsule(&D_mist_parking_80191484[4], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 14:
            _glowDrawCapsule(&D_mist_parking_80191484[6], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(&D_mist_parking_80191484[8], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(&D_mist_parking_80191484[14], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 15:
            _glowDrawCapsule(&D_mist_parking_80191484[0], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 16:
            _mistParkingDrawGlowRow(&D_mist_parking_80191484[10], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(&D_mist_parking_80191484[16], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(&D_mist_parking_80191484[18], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(&D_mist_parking_80191484[22], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(&D_mist_parking_80191484[24], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 5:
            _mistParkingDrawGlowRow(&D_mist_parking_80191484[4], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 6:
        case 19:
            _mistParkingDrawGlowRow(&D_mist_parking_80191484[10], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _mistParkingDrawGlowRow(&D_mist_parking_80191484[16], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(&D_mist_parking_80191484[22], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(&D_mist_parking_80191484[24], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawDiamond(&D_mist_parking_80191484[MIST_PARKING_GLOW_POINT_INDEX], MIST_PARKING_GLOW_POINT_PULSE_RATE, MIST_PARKING_GLOW_DIAMOND_RADIUS_SCALE);
            break;
        case 8:
            _glowDrawCapsule(&D_mist_parking_80191484[6], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(&D_mist_parking_80191484[8], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
    }
}

#include "../../shared/glow_draw_diamond.inc.c"

#include "../../shared/glow_draw_pulsing_disc.inc.c"

#include "../../shared/glow_draw_capsule.inc.c"
