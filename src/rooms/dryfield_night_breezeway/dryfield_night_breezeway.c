#include "rooms/dryfield_night_breezeway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

#define D_dryfield_night_breezeway_8017E6AC (D_dryfield_night_breezeway_8017E6A4 + 1)
#define D_dryfield_night_breezeway_8017E6C4 (D_dryfield_night_breezeway_8017E6A4[4])

static void func_dryfield_night_breezeway_8017D634(Task* task);
static void func_dryfield_night_breezeway_8017D678(Task* task);

/// The room's message table: 0x13EE, 0x13F1, 0x13EF and 0x13F0 to their
/// handlers, terminated by `TASK_MESSAGE_TABLE_END`.
extern TaskMessageEntry D_dryfield_night_breezeway_8017E67C[];

/// The anchor points of the room's lights: one run of eight `SVECTOR`s. The
/// draw below reaches some points through their own address and others by
/// indexing from an earlier one, so the run carries three names.

/// The room's event task states: open the message table, idle, then kill the
/// task.
static const TaskFuncTable3 D_dryfield_night_breezeway_8017D5C4 = {
    { func_dryfield_night_breezeway_8017D634, func_dryfield_night_breezeway_8017D678, taskKill },
};

// Indexed views below share one contiguous table.
s32 func_dryfield_night_breezeway_8017D5D0(Task*, s32, s32, s32);
s32 func_dryfield_night_breezeway_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_night_breezeway_8017D600(Task*, s32, s32, s32);
s32 func_dryfield_night_breezeway_8017D62C(Task*, s32, s32, s32);

extern WorldCollisionGrid    D_dryfield_night_breezeway_8017EBC4[1];
extern WorldCollisionTrigger D_dryfield_night_breezeway_80180170[4];
extern WorldCollisionTrigger D_dryfield_night_breezeway_801802A0[3];
extern WorldCoordRoomLights  D_dryfield_night_breezeway_80180158[1];

extern TaskDesc D_8014D8A4;

TaskMessageEntry D_dryfield_night_breezeway_8017E67C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_breezeway_8017D5D8 },
    { 5105, func_dryfield_night_breezeway_8017D5D0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_breezeway_8017D62C },
    { ROOM_MESSAGE_COMMAND, func_dryfield_night_breezeway_8017D600 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_dryfield_night_breezeway_8017E6A4[8] = {
    { 17990, -1365, 2825, 0 },
    { 4290, -3080, 1810, 0 },
    { 17500, -2850, 2680, 0 },
    { 17870, -2600, 2680, 0 },
    { 14000, -2600, 1140, 0 },
    { 14000, -2850, 1500, 0 },
    { 10000, -2850, 1500, 0 },
    { 10000, -2600, 1140, 0 },
};

WorldCoordRoomLighting D_dryfield_night_breezeway_8017E6E4[1] = {
    { D_dryfield_night_breezeway_80180158, NULL },
};

WorldCollisionRoomResources D_dryfield_night_breezeway_8017E6EC[1] = {
    { D_dryfield_night_breezeway_8017EBC4, D_dryfield_night_breezeway_80180170, D_dryfield_night_breezeway_801802A0, NULL },
};

u8* D_dryfield_night_breezeway_8017E6FC[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_night_breezeway_8017E700[1] = { 6 };

DirectionWarpEntry D_dryfield_night_breezeway_8017E704[2] = {
    { { { .word = 1024 }, 6656, 1, 1568 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 7940, 1, 1590 }, { 0, 0, 0, 0 }, 0x53160002, 0x53160001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 476 },
    { { { .word = 3072 }, 0x44F3, 1, 2075 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x44F3, 1, 2075 }, { 0, 0, 0, 0 }, 0x53160004, 0x53160003, 0x53160005, 4, DIRECTION_WARP_FLAG_NONE, 475 },
};

static SVECTOR _gDryfieldNightBreezewayCollision01604Normals[16] = {
#include "assets/dryfield_night_breezeway_collision_01604_normals.inc"
};

static SVECTOR _gDryfieldNightBreezewayCollision01604Verts[48] = {
#include "assets/dryfield_night_breezeway_collision_01604_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightBreezewayCollision01604Faces[25] = {
#include "assets/dryfield_night_breezeway_collision_01604_faces.inc"
};

static s16 _gDryfieldNightBreezewayCollision01604Cells[130] = {
#include "assets/dryfield_night_breezeway_collision_01604_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightBreezewayCollision01604Cells[i])
static s16* _gDryfieldNightBreezewayCollision01604Table[8] = {
#include "assets/dryfield_night_breezeway_collision_01604_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_breezeway_8017EBC4[1] = {
    { NULL, _gDryfieldNightBreezewayCollision01604Normals, _gDryfieldNightBreezewayCollision01604Verts, _gDryfieldNightBreezewayCollision01604Faces, _gDryfieldNightBreezewayCollision01604Table, -5000, 1000, 4, 2, 4000, 25 },
};

ViewCamera D_dryfield_night_breezeway_8017EBE8[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2EE0, 0x5DC0, 0 } }, 322 },
    { { { { 0, 0, 4096 }, { -1081, 3950, 0 }, { -3950, -1081, 0 } }, { -0x38A4, 200, -1600 } }, 257 },
    { { { { 0, 0, -4095 }, { 831, 4010, 0 }, { 4010, -831, 0 } }, { -9800, 300, -1600 } }, 257 },
    { { { { 1090, 0, -3948 }, { 702, 4030, 194 }, { 3885, -729, 1072 } }, { -0x35CA, 500, -1150 } }, 257 },
    { { { { 1589, 0, -3775 }, { -1365, 3818, -575 }, { 3519, 1481, 1481 } }, { -0x459C, 1450, -2780 } }, 257 },
    { { { { 0, 0, -4096 }, { 0, 4096, 0 }, { 4096, 0, 0 } }, { -0x41AC, 295, -3414 } }, 680 },
};

SpriteBatch D_dryfield_night_breezeway_8017ECC0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_breezeway_8017ECD0[96] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -120, 426, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -120, 420, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -112, 431, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -112, 424, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -104, 435, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -104, 427, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -96, 439, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -96, 430, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -88, 442, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -88, 433, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -80, 446, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -80, 437, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -72, 453, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -72, 441, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -64, 463, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -64, 450, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -56, 474, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -56, 461, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -48, 488, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -48, 474, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -40, 503, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -40, 487, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -32, 505, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -32, 489, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -24, 488, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -24, 361, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -16, 366, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -16, 344, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -8, 350, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -8, 331, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 0, 344, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 0, 296, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 8, 301, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 8, 289, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 16, 299, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 16, 287, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 24, 297, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 24, 286, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 32, 276, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 32, 269, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 40, 1833, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 40, 271, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 40, 266, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 48, 267, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 48, 261, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 56, 261, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 56, 256, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 64, 253, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 64, 249, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 72, 236, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 72, 233, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 80, 230, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 80, 224, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 88, 227, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 88, 222, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 96, 225, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 96, 220, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 104, 223, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 104, 213, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 112, 221, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 112, 216, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, -120, 516, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, -40, 728, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, 40, 254, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, 40, 266, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, -40, 1002, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, -120, 566, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 64, -120, 687, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 64, -40, 1190, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 64, 40, 287, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 56, 40, 396, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 56, -40, 1253, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 56, -120, 1143, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 48, -120, 1251, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 48, -40, 1429, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, 40, 426, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 40, -40, 2500, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 40, -120, 1353, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 32, -112, 1483, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 32, -40, 2500, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 24, -40, 2500, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, -96, 1676, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -80, 1846, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 16, -40, 2500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, 40, 2500, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 40, 2069, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 72, 600, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 40, 2500, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, 56, 608, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 40, 48, 463, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 64, 1623, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 64, 652, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 64, 1167, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 72, 857, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 72, 634, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 72, 626, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_breezeway_8017F450[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 90, 0, 0, { 1, 0 } },
    { 90, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_breezeway_8017F470[46] = {
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -80, -120, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -72, -120, 750, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -56, -120, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -64, -120, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, -120, 750, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -120, 750, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -16, 750, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -80, 48, 450, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, 48, 450, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -72, -16, 750, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -160, -120, 464, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -160, -32, 379, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, 48, 375, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -144, 48, 375, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -128, 48, 375, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -112, 48, 375, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -96, 48, 375, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -144, -32, 409, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -128, -32, 426, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -112, -32, 446, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -32, 625, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -16, 750, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -64, 24, 450, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -56, 32, 450, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -48, 48, 450, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -40, 48, 450, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -32, 56, 450, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 64, 450, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -144, -120, 490, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -120, 512, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, -120, 535, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -96, -120, 500, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 56, 250, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 80, 250, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 250, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -144, 64, 250, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -128, 64, 250, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, 32, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -96, 48, 250, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -80, 32, 250, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -72, 24, 250, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -64, 32, 250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -56, 40, 250, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 80, 250, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 80, 250, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -32, 96, 250, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_breezeway_8017F808[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 32, 0, 0, { 1, 0 } },
    { 32, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_breezeway_8017F828[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_breezeway_8017F838[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_breezeway_8017F848[60] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 8, 298, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 8, 296, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 16, 298, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 16, 296, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 24, 298, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 24, 296, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 32, 298, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 32, 296, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 40, 298, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 40, 296, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 48, 298, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 48, 296, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 56, 298, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 56, 296, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 64, 298, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 64, 296, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 72, 296, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 72, 299, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 80, 297, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 80, 297, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -144, -96, 296, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, -160, -8, 269, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -144, 8, 260, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -128, 8, 282, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -112, 8, 271, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -96, 8, 296, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -80, 8, 298, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -64, 96, 244, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, -128, -88, 298, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -64, 8, 297, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -48, 8, 298, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -32, 8, 298, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -16, 8, 298, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 0, 8, 298, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, -112, -88, 298, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -96, -88, 298, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -80, -88, 298, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -64, -88, 298, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -48, -88, 298, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -32, -88, 298, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -16, -88, 298, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 0, -88, 298, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 16, -88, 298, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, 32, -88, 298, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, 48, -88, 298, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, 64, -88, 297, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, -88, 301, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, -88, 301, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, -88, 301, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 80, -72, 297, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, -56, 298, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, -32, 294, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 16, 8, 298, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 128, -96, 298, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 32, 8, 298, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 48, 8, 298, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 64, 8, 298, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 80, 8, 298, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 96, 8, 298, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 112, 8, 298, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_breezeway_8017FCF8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 60, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_breezeway_8017FD10[6] = {
    { { .empty = D_dryfield_night_breezeway_8017ECC0 }, D_dryfield_night_breezeway_8017ECC0, NULL },
    { { .elements = D_dryfield_night_breezeway_8017ECD0 }, D_dryfield_night_breezeway_8017F450, NULL },
    { { .elements = D_dryfield_night_breezeway_8017F470 }, D_dryfield_night_breezeway_8017F808, NULL },
    { { .empty = D_dryfield_night_breezeway_8017F828 }, D_dryfield_night_breezeway_8017F828, NULL },
    { { .empty = D_dryfield_night_breezeway_8017F838 }, D_dryfield_night_breezeway_8017F838, NULL },
    { { .elements = D_dryfield_night_breezeway_8017F848 }, D_dryfield_night_breezeway_8017FCF8, NULL },
};

/// Four directional model-shading lights shared by every nighttime breezeway view.
///
/// Local translations hold direction vectors, normalized when shading, with
/// equal RGB intensities of 750 in ONE-based fixed point. Coordinate updates
/// and lighting queries mutate the transform caches, parent and attenuation;
/// the room light collection borrows this array while the overlay is loaded.
static WorldCoordLight _gDryfieldNightBreezewayDirectionalLights[] = {
    {
        .transform = { .lighting = {
                           .composeStamp = GRAPHICS_COORD_DIRTY,
                           .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -1000, 0 } },
                           .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                           .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                       } },
        .color     = { 750, 750, 750 },
    },
    {
        .transform = { .lighting = {
                           .composeStamp = GRAPHICS_COORD_DIRTY,
                           .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -1000, 0 } },
                           .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                           .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                       } },
        .color     = { 750, 750, 750 },
    },
    {
        .transform = { .lighting = {
                           .composeStamp = GRAPHICS_COORD_DIRTY,
                           .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, 1000 } },
                           .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                           .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                       } },
        .color     = { 750, 750, 750 },
    },
    {
        .transform = { .lighting = {
                           .composeStamp = GRAPHICS_COORD_DIRTY,
                           .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, -1000 } },
                           .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                           .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                       } },
        .color     = { 750, 750, 750 },
    },
};

/// Seven point-light records for model shading and light queries in every nighttime breezeway view.
///
/// The first three start white at ONE-based RGB intensity; the remaining four
/// start black with inner/outer radii of 0/1. Positions and radii use integer
/// world units. Coordinate updates and lighting queries mutate the parents,
/// transform caches and attenuation. The room light collection borrows this
/// writable array while the overlay is loaded.
static WorldCoordPointLight _gDryfieldNightBreezewayPointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 10000, -1896, 1500 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1278,
        .outer = 2239,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 14000, -1896, 1500 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1278,
        .outer = 2239,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 17500, -2278, 2700 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 2001,
        .outer = 2203,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7500, -2000, 1000 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 0, 0, 0 },
        },
        .inner = 0,
        .outer = 1,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 12500, -2000, 1000 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 0, 0, 0 },
        },
        .inner = 0,
        .outer = 1,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 15000, -2000, 1000 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 0, 0, 0 },
        },
        .inner = 0,
        .outer = 1,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7500, -2000, 1000 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 0, 0, 0 },
        },
        .inner = 0,
        .outer = 1,
    },
};

WorldCoordRoomLights D_dryfield_night_breezeway_80180158[1] = {
    { ARRAY_SIZE(_gDryfieldNightBreezewayDirectionalLights), _gDryfieldNightBreezewayDirectionalLights, ARRAY_SIZE(_gDryfieldNightBreezewayPointLights), _gDryfieldNightBreezewayPointLights, 0, NULL },
};

WorldCollisionTrigger D_dryfield_night_breezeway_80180170[4] = {
    { NULL, NULL, NULL, { 0x2E60, -2880, 1440, 0 }, { { 0, -3568, -1024, 0 }, { 0, -3568, 1024, 0 }, { 0, 3568, -1024, 0 }, { 0, 3568, 1024, 0 } }, { 4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3709, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2F20, -2816, 1472, 0 }, { { 0, -3280, 1024, 0 }, { 0, -3280, -1024, 0 }, { 0, 3280, 1024, 0 }, { 0, 3280, -1024, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3434, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3F8D, -2528, 2138, 0 }, { { -70, -3632, 1554, 0 }, { 56, -3632, -1563, 0 }, { -70, 3632, 1554, 0 }, { 56, 3632, -1563, 0 } }, { -4106, 0, -167, 0 }, { 0, 0, 4096, 0 }, 3949, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3ECD, -2576, 2122, 0 }, { { 78, -3392, -1520, 0 }, { -88, -3392, 1512, 0 }, { 78, 3392, -1520, 0 }, { -88, 3392, 1512, 0 } }, { 4099, 0, 223, 0 }, { 0, 0, 4096, 0 }, 3709, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_breezeway_801802A0[3] = {
    { NULL, NULL, NULL, { 6416, -48, 1680, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_WARP, 20, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4580, -64, 1760, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { -4091, 0, 201, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_WARP, 23, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x45A0, -64, 2816, 0 }, { { -368, 0, -528, 0 }, { 368, 0, -528, 0 }, { -368, 0, 528, 0 }, { 368, 0, 528, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 643, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_dryfield_night_breezeway_80180384[2] = {
    { 37, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103700_80139DAC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_breezeway_8018039C[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_8014D8A4 },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_300700_80165B88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_breezeway_801803C0[3] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_801379A8 },
    { 15, 15, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_80153E28 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_breezeway_801803E4[22] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017C2A8, D_dryfield_night_breezeway_80180384 },
    { NULL, NULL },
    { D_map_dryfield_full_8017C378, D_dryfield_night_breezeway_8018039C },
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
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017C448, D_dryfield_night_breezeway_801803C0 },
};

WorldCollisionFootstepSounds D_dryfield_night_breezeway_80180494 = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

WorldCollisionSurfaceProperties D_dryfield_night_breezeway_801804A0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_breezeway_801804A8[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_breezeway_801804B0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_breezeway_80180494 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_breezeway_801804B8[8] = {
    D_dryfield_night_breezeway_801804A0,
    D_dryfield_night_breezeway_801804A8,
    D_dryfield_night_breezeway_801804B0,
    D_dryfield_night_breezeway_801804A0,
    D_dryfield_night_breezeway_801804A0,
    D_dryfield_night_breezeway_801804A0,
    D_dryfield_night_breezeway_801804A0,
    D_dryfield_night_breezeway_801804A0,
};

/// The room's 0x13F1 message handler: answers 0 without looking at the
/// message.
s32 func_dryfield_night_breezeway_8017D5D0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// The room's 0x13EE message handler: copies the incoming location onto the
/// outgoing record and answers 1.
s32 func_dryfield_night_breezeway_8017D5D8(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    return 1;
}

/// The room's 0x13F0 message handler: when `arg2` is 1, spawns the gameplay
/// event task (`Gp_SpawnIfCapIdle(1, 1)`) unless the cap interpreter is busy.
/// Always answers 0.
s32 func_dryfield_night_breezeway_8017D600(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        Gp_SpawnIfCapIdle(1, 1);
    }
    return 0;
}

/// The room's 0x13EF message handler: answers 0 without looking at the
/// message.
s32 func_dryfield_night_breezeway_8017D62C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// State 0 of the room's event task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7 and advances to the
/// next state.
static void func_dryfield_night_breezeway_8017D634(Task* task)
{
    task->msgTable = D_dryfield_night_breezeway_8017E67C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's event task: does nothing.
static void func_dryfield_night_breezeway_8017D678(Task* task)
{
}

/// The room's event task: copies the three-state table onto the stack and
/// calls the entry for the task's current state.
void func_dryfield_night_breezeway_8017D680(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_breezeway_8017D5C4;
    sp.funcs[task->state](task);
}

#include "../../shared/glow_draw_pulsing_star.inc.c"

#include "../../shared/glow_draw_shaft.inc.c"

#include "../../shared/glow_draw_flare.inc.c"

/// The room's light draw: sets `gRoomEffectState->roomEffectMode` to 2, then draws
/// the lights the current camera view (`gGameSession->location.loc.view`) can see.
/// View 2 draws a sprite and a beam; view 3 draws a beam and then everything
/// view 4 draws, a pulsing star and a second beam. Other views draw nothing.
void func_dryfield_night_breezeway_8017E5BC(Task* unused)
{
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    switch (gGameSession->location.loc.view) {
        case 2:
            glowDrawFlare(&D_dryfield_night_breezeway_8017E6AC[0], 2, 0x400);
            glowDrawShaft(&D_dryfield_night_breezeway_8017E6AC[5], 0x180);
            break;
        case 3:
            glowDrawShaft(&D_dryfield_night_breezeway_8017E6C4, 0x180);
            /* fallthrough */
        case 4:
            glowDrawPulsingStar(&D_dryfield_night_breezeway_8017E6A4[0], 0x600, 0x80);
            glowDrawShaft(&D_dryfield_night_breezeway_8017E6A4[2], 0x180);
            break;
    }
}
