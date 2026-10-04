#include "rooms/mine_tunnel_entrance.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
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
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

/// The room's message table, which the room task answers messages with.
extern TaskMessageEntry D_mine_tunnel_entrance_8017DAF0[];

/// The tunnel's per-view quad positions, one `SVECTOR` per position, 8 bytes
/// apart. The runs overlap: `DB18[5]`, `DB30[2]` and `DB38[1]` are all the
/// same point, reached through whichever base the view's case names.
extern SVECTOR D_mine_tunnel_entrance_8017DB18[];
extern SVECTOR D_mine_tunnel_entrance_8017DB30[];
extern SVECTOR D_mine_tunnel_entrance_8017DB38[];
extern SVECTOR D_mine_tunnel_entrance_8017DB48[];

static void func_mine_tunnel_entrance_8017D644(Task* arg0);
static void func_mine_tunnel_entrance_8017D690(Task* task);
static void func_mine_tunnel_entrance_8017D6B4(Task* task);

/// State handlers of the room task `func_mine_tunnel_entrance_8017D6BC` runs:
/// set-up, the scene-event state, an idle state and `taskKill`.
static const TaskFuncTable4 D_mine_tunnel_entrance_8017D5C4 = {
    func_mine_tunnel_entrance_8017D644,
    func_mine_tunnel_entrance_8017D690,
    func_mine_tunnel_entrance_8017D6B4,
    taskKill,
};

s32 func_mine_tunnel_entrance_8017D5E8(Task*, s32, s32, s32);
s32 func_mine_tunnel_entrance_8017D5F0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_mine_tunnel_entrance_8017D634(Task*, s32, s32, s32);
s32 func_mine_tunnel_entrance_8017D63C(Task*, s32, s32, s32);

extern WorldCollisionGrid         D_mine_tunnel_entrance_8017E0C0[1];
extern WorldCollisionTrigger      D_mine_tunnel_entrance_8017ECEC[8];
extern WorldCollisionTrigger      D_mine_tunnel_entrance_8017EF4C[4];
extern WorldCoordRoomAmbientEntry D_mine_tunnel_entrance_8017F38C[7];
extern WorldCoordRoomLights       D_mine_tunnel_entrance_8017ECD4[1];

extern TaskDesc D_8014D8A4;

TaskMessageEntry D_mine_tunnel_entrance_8017DAF0[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_mine_tunnel_entrance_8017D5F0 },
    { 5105, func_mine_tunnel_entrance_8017D5E8 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_mine_tunnel_entrance_8017D63C },
    { ROOM_MESSAGE_COMMAND, func_mine_tunnel_entrance_8017D634 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_mine_tunnel_entrance_8017DB18[3] = {
    { 0x5B4A, -600, 3070, 0 },
    { 0x5B4A, -600, 1560, 0 },
    { 0x3BE2, -1920, 3770, 0 },
};

SVECTOR D_mine_tunnel_entrance_8017DB30[1] = {
    { 6970, -1770, 3440, 0 },
};

SVECTOR D_mine_tunnel_entrance_8017DB38[2] = {
    { 200, -1960, 2090, 0 },
    { 0x302A, -1830, 530, 0 },
};

SVECTOR D_mine_tunnel_entrance_8017DB48[2] = {
    { 2250, -1500, -840, 0 },
    { 3000, -1870, -3060, 0 },
};

WorldCoordRoomLighting D_mine_tunnel_entrance_8017DB58[1] = {
    { D_mine_tunnel_entrance_8017ECD4, D_mine_tunnel_entrance_8017F38C },
};

WorldCollisionRoomResources D_mine_tunnel_entrance_8017DB60[1] = {
    { D_mine_tunnel_entrance_8017E0C0, D_mine_tunnel_entrance_8017ECEC, D_mine_tunnel_entrance_8017EF4C, NULL },
};

u8* D_mine_tunnel_entrance_8017DB70[1] = {
    gViewIdentityMap,
};

ViewCount D_mine_tunnel_entrance_8017DB74[1] = { 6 };

DirectionWarpEntry D_mine_tunnel_entrance_8017DB78[3] = {
    { { { .word = 3072 }, 0x44C0, 0, 1750 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x44C0, 0, 1750 }, { 0, 0, 0, 0 }, 0x54030002, 0x54030001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, 2873, 0, -448 }, { 0, 0, 0, 0 }, { { .word = 0 }, 2873, 0, -448 }, { 0, 0, 0, 0 }, 0x54030004, 0x54030003, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, 417, 0, 1571 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 417, 0, 1571 }, { 0, 0, 0, 0 }, 0x54030006, 0x54030005, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gMineTunnelEntranceCollision00B00Normals[23] = {
#include "assets/mine_tunnel_entrance_collision_00B00_normals.inc"
};

static SVECTOR _gMineTunnelEntranceCollision00B00Verts[54] = {
#include "assets/mine_tunnel_entrance_collision_00B00_verts.inc"
};

static WorldCollisionGridFace _gMineTunnelEntranceCollision00B00Faces[30] = {
#include "assets/mine_tunnel_entrance_collision_00B00_faces.inc"
};

static s16 _gMineTunnelEntranceCollision00B00Cells[84] = {
#include "assets/mine_tunnel_entrance_collision_00B00_cells.inc"
};

#define GRID_CELL(i) (&_gMineTunnelEntranceCollision00B00Cells[i])
static s16* _gMineTunnelEntranceCollision00B00Table[10] = {
#include "assets/mine_tunnel_entrance_collision_00B00_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mine_tunnel_entrance_8017E0C0[1] = {
    { NULL, _gMineTunnelEntranceCollision00B00Normals, _gMineTunnelEntranceCollision00B00Verts, _gMineTunnelEntranceCollision00B00Faces, _gMineTunnelEntranceCollision00B00Table, 630, 1791, 5, 2, 4000, 30 },
};

ViewCamera D_mine_tunnel_entrance_8017E0E4[6] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { -9000, 0x4A38, -2000 } }, 269 },
    { { { { 1067, 0, -3954 }, { -511, 4061, -138 }, { 3921, 529, 1058 } }, { -0x27B4, 1646, -704 } }, 289 },
    { { { { 829, 0, 4011 }, { 668, 4038, -138 }, { -3954, 683, 817 } }, { -0x419D, 1790, -1202 } }, 269 },
    { { { { 841, 0, 4008 }, { 629, 4045, -132 }, { -3959, 643, 830 } }, { -0x2979, 1679, -913 } }, 269 },
    { { { { 837, 0, 4009 }, { -171, 4092, 35 }, { -4005, -174, 836 } }, { -5417, 1079, -913 } }, 269 },
    { { { { -3780, 0, 1577 }, { -235, 4050, -564 }, { -1559, -611, -3737 } }, { -3904, 535, -2862 } }, 246 },
};

SpriteBatch D_mine_tunnel_entrance_8017E1BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_tunnel_entrance_8017E1CC[13] = {
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 88, -104, 499, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, -40, 505, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 0, 489, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 40, 474, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 112, -120, 437, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 120, -104, 480, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, -104, 438, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, -72, 443, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -40, 450, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 0, 457, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 40, 464, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 80, 555, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 80, 474, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_tunnel_entrance_8017E2D0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_tunnel_entrance_8017E2E8[39] = {
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 0, -120, 1132, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 32, -120, 1153, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 64, -120, 1113, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 64, -80, 1146, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 64, -24, 1179, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 64, 16, 1211, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, -120, 996, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -80, -120, 1051, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -144, -120, 956, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -128, 48, 1000, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, -80, 1036, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 48, 1105, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, -32, 1078, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, 8, 1070, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 8, 1031, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -136, -32, 1004, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -136, -80, 980, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -56, -96, 2310, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -24, -96, 2452, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 0, -96, 2453, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 0, -64, 2487, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 0, -32, 2533, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 0, 0, 2487, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, -56, 2191, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, -24, 2193, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -80, -80, 2302, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, -80, 2258, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, -96, 2233, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, -96, 2264, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 112, 713, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -96, -120, 2200, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -32, 2080, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 8, 2000, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 120 } }, -160, -8, 850, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, -160, -120, 955, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -128, -120, 1580, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -128, -16, 1500, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -112, -120, 1995, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, -24, 1900, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_tunnel_entrance_8017E5F4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 1, 0 } },
    { 17, 12, 0, 0, { 2, 0 } },
    { 29, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_tunnel_entrance_8017E61C[39] = {
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 104, 64, 1095, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 56, 1117, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 128, 40, 1056, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 96, 40, 1064, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 104, -8, 1114, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 104, -64, 1114, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, -88, 1047, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 88, -120, 1003, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, -120, 958, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, -88, 977, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, -64, 1010, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 128, -8, 1023, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, -64, 1000, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 136, -16, 1032, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -136, 48, 856, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 48, 877, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 80, 856, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 80, 810, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -112, -16, 897, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -120, -80, 812, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -136, -16, 833, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -144, -80, 750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -144, -120, 780, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -120, 809, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -104, -120, 862, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -120, 778, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -144, -80, 1377, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, -120, 779, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -128, 0, 837, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -128, 48, 783, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 24, 849, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 64, 670, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 523, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 8, 860, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -32, 1293, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 24, 571, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -24, 849, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -72, 1163, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -120, 958, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_tunnel_entrance_8017E928[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 25, 0, 0, { 1, 0 } },
    { 25, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mine_tunnel_entrance_8017E948[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_tunnel_entrance_8017E958[11] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, 40, 960, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -80, -32, 970, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -80, 24, 941, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -24, 1006, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, 24, 980, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -16, 1030, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, 24, 1006, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, -16, 1054, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, 24, 1026, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -16, -24, 1069, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -16, 24, 1040, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_tunnel_entrance_8017EA34[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_mine_tunnel_entrance_8017EA4C[6] = {
    { { .empty = D_mine_tunnel_entrance_8017E1BC }, D_mine_tunnel_entrance_8017E1BC, NULL },
    { { .elements = D_mine_tunnel_entrance_8017E1CC }, D_mine_tunnel_entrance_8017E2D0, NULL },
    { { .elements = D_mine_tunnel_entrance_8017E2E8 }, D_mine_tunnel_entrance_8017E5F4, NULL },
    { { .elements = D_mine_tunnel_entrance_8017E61C }, D_mine_tunnel_entrance_8017E928, NULL },
    { { .empty = D_mine_tunnel_entrance_8017E948 }, D_mine_tunnel_entrance_8017E948, NULL },
    { { .elements = D_mine_tunnel_entrance_8017E958 }, D_mine_tunnel_entrance_8017EA34, NULL },
};

WorldCoordPointLight D_mine_tunnel_entrance_8017EA94[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9530, -3860, 1830 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1155, 1608, 1468 }, { 0, 0 } }, 0, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3BEC, -2000, 3640 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3522, 2539 }, { 0, 0 } }, 800, 3600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3016, -2000, 700 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3522, 2457 }, { 0, 0 } }, 800, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7110, -2000, 3290 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2621 }, { 0, 0 } }, 0, 4600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 740, -2000, 2380 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3276, 2539 }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2140, -2000, -90 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3112, 2211 }, { 0, 0 } }, 500, 3000 },
};

WorldCoordRoomLights D_mine_tunnel_entrance_8017ECD4[1] = {
    { 0, NULL, ARRAY_SIZE(D_mine_tunnel_entrance_8017EA94), D_mine_tunnel_entrance_8017EA94, 0, NULL },
};

WorldCollisionTrigger D_mine_tunnel_entrance_8017ECEC[8] = {
    { NULL, NULL, NULL, { 0x3500, -1632, 1441, 0 }, { { 0, -2368, 3424, 0 }, { 0, -2368, -3424, 0 }, { 0, 2368, 3424, 0 }, { 0, 2368, -3424, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4159, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3461, -1697, 1504, 0 }, { { 0, -2368, -3456, 0 }, { 0, -2368, 3456, 0 }, { 0, 2368, -3456, 0 }, { 0, 2368, 3456, 0 } }, { 4101, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4159, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7361, -1665, 1376, 0 }, { { -339, -2368, 3402, 0 }, { 331, -2368, -3412, 0 }, { -339, 2368, 3402, 0 }, { 331, 2368, -3412, 0 } }, { -4077, 0, -401, 0 }, { 0, 0, 4096, 0 }, 4159, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7136, -1697, 1280, 0 }, { { 336, -2368, -3442, 0 }, { -342, -2368, 3436, 0 }, { 336, 2368, -3442, 0 }, { -342, 2368, 3436, 0 } }, { 4080, 0, 401, 0 }, { 0, 0, 4096, 0 }, 4159, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4657, -1600, -113, 0 }, { { 1741, -2368, -259, 0 }, { -1742, -2368, 258, 0 }, { 1741, 2368, -259, 0 }, { -1742, 2368, 258, 0 } }, { 601, 0, 4058, 0 }, { 0, 0, 4096, 0 }, 2941, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4879, -1632, 48, 0 }, { { -1916, -2368, 284, 0 }, { 1915, -2368, -285, 0 }, { -1916, 2368, 284, 0 }, { 1915, 2368, -285, 0 } }, { -604, 0, -4065, 0 }, { 0, 0, 4096, 0 }, 3050, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1183, -1664, 95, 0 }, { { -1926, -2368, -189, 0 }, { 1927, -2368, 190, 0 }, { -1926, 2368, -189, 0 }, { 1927, 2368, 190, 0 } }, { 401, 0, -4089, 0 }, { 0, 0, 4096, 0 }, 3050, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1182, -1696, -66, 0 }, { { 1927, -2368, 190, 0 }, { -1926, -2368, -189, 0 }, { 1927, 2368, 190, 0 }, { -1926, 2368, -189, 0 } }, { -403, 0, 4087, 0 }, { 0, 0, 4096, 0 }, 3050, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_mine_tunnel_entrance_8017EF4C[4] = {
    { NULL, NULL, NULL, { 0x44A0, -48, 2112, 0 }, { { -512, 0, -1792, 0 }, { 512, 0, -1792, 0 }, { -512, 0, 1792, 0 }, { 512, 0, 1792, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1863, WORLD_COLLISION_TRIGGER_ACTION_WARP, 1, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 544, -48, 1088, 0 }, { { -512, 0, -1024, 0 }, { 512, 0, -1024, 0 }, { -512, 0, 1024, 0 }, { 512, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_WARP, 4, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2992, -48, -624, 0 }, { { 816, 0, -336, 0 }, { 816, 0, 336, 0 }, { -816, 0, -336, 0 }, { -816, 0, 336, 0 } }, { 0, 4113, 0, 0 }, { 0, 0, 4096, 0 }, 882, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6496, -64, 3840, 0 }, { { -864, 0, -1024, 0 }, { 864, 0, -1024, 0 }, { -864, 0, 1024, 0 }, { 864, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { 201, 0, -4091, 0 }, 1336, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_mine_tunnel_entrance_8017F07C[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_8014D8A4 },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_300700_80165B88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_tunnel_entrance_8017F0A0[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_801445DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_tunnel_entrance_8017F0B8[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_actor_400600_80151B10 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_tunnel_entrance_8017F0D0[3] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_801379A8 },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, Actor00700_D075A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_tunnel_entrance_8017F0F4[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_actor_400600_80151B10 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_mine_tunnel_entrance_8017F10C[8] = {
    { 1, 0, 1, 0x2710, 0, 1050, 1024, 0, 0, 2, 0 },
    { 1, 0, 1, 6650, 0, 2500, 2950, 0, 0, 2, 0 },
    { 8, 0, 0, 0x2CEC, -2000, 800, 1650, 0, 2, 4, 3 },
    { 8, 0, 0, 0x2DB4, -2200, 900, 2048, 0, 2, 4, 3 },
    { 8, 0, 0, 0x2E4A, -1900, 800, 2250, 0, 2, 4, 3 },
    { 8, 0, 0, 0x3AFC, -1700, 3350, 400, 0, 2, 4, 3 },
    { 8, 0, 0, 0x3DB8, -2000, 3400, 3800, 0, 2, 4, 3 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_tunnel_entrance_8017F18C[6] = {
    { 16, 0, 0, 4000, 0, 2300, 2500, 0, 0, 2, 0 },
    { 16, 0, 0, 5450, 0, 350, 3500, 0, 0, 2, 0 },
    { 16, 0, 0, 6550, 0, 600, 4000, 0, 0, 2, 0 },
    { 16, 0, 0, 8950, 0, 600, 2600, 0, 0, 2, 0 },
    { 16, 0, 0, 0x2CBA, 0, 3100, 2050, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_tunnel_entrance_8017F1EC[4] = {
    { 6, 0, 0, 8100, 0, 2500, 2750, 0, 0, 2, 0 },
    { 6, 0, 0, 8050, 0, 900, 3250, 0, 0, 2, 0 },
    { 6, 0, 0, 0x30D4, 0, 1400, 950, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_tunnel_entrance_8017F22C[13] = {
    { 25, 0, 0, 2900, 0, 2500, 2850, 0, 0, 2, 2 },
    { 25, 0, 0, 5550, 0, 450, 3350, 0, 0, 2, 2 },
    { 25, 0, 0, 6850, 0, 2950, 2200, 0, 0, 2, 2 },
    { 25, 0, 0, 8250, 0, 1500, 2900, 0, 0, 2, 2 },
    { 25, 0, 0, 0x2CEC, 0, 3000, 1400, 0, 0, 2, 2 },
    { 25, 0, 0, 0x3552, 0, 900, 450, 0, 0, 2, 2 },
    { 8, 0, 0, 6250, -1800, 3200, 300, 0, 2, 4, 3 },
    { 8, 0, 0, 6900, -1500, 2950, 50, 0, 2, 4, 3 },
    { 8, 0, 0, 7250, -1600, 3100, 3550, 0, 2, 4, 3 },
    { 8, 0, 0, 9800, -2000, 600, 1500, 0, 2, 4, 3 },
    { 8, 0, 0, 0x29FE, -1800, 800, 2050, 0, 2, 4, 3 },
    { 8, 0, 0, 0x3C8C, -1600, 3400, 200, 0, 2, 4, 3 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_tunnel_entrance_8017F2FC[3] = {
    { 6, 0, 0, 0x2710, 0, 1600, 1024, 0, 0, 2, 0 },
    { 6, 0, 0, 6000, 0, 2500, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_mine_tunnel_entrance_8017F32C[12] = {
    { NULL, NULL },
    { D_mine_tunnel_entrance_8017F10C, D_mine_tunnel_entrance_8017F07C },
    { D_mine_tunnel_entrance_8017F18C, D_mine_tunnel_entrance_8017F0A0 },
    { D_mine_tunnel_entrance_8017F1EC, D_mine_tunnel_entrance_8017F0B8 },
    { D_mine_tunnel_entrance_8017F22C, D_mine_tunnel_entrance_8017F0D0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_mine_tunnel_entrance_8017F2FC, D_mine_tunnel_entrance_8017F0F4 },
};

WorldCoordRoomAmbientEntry D_mine_tunnel_entrance_8017F38C[7] = {
    { .viewCount = ARRAY_SIZE(D_mine_tunnel_entrance_8017F38C) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 400, 380, 300, 377 } },
    { .color = { 560, 530, 500, 537 } },
    { .color = { 250, 50, 160, 138 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_mine_tunnel_entrance_8017F3C4 = {
    0x1000001D,
    0x1000001F,
    0x1000001D,
};

WorldCollisionSurfaceProperties D_mine_tunnel_entrance_8017F3D0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_mine_tunnel_entrance_8017F3D8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_mine_tunnel_entrance_8017F3E0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_mine_tunnel_entrance_8017F3C4 },
};

WorldCollisionSurfaceProperties* D_mine_tunnel_entrance_8017F3E8[8] = {
    D_mine_tunnel_entrance_8017F3D0,
    D_mine_tunnel_entrance_8017F3D8,
    D_mine_tunnel_entrance_8017F3E0,
    D_mine_tunnel_entrance_8017F3D0,
    D_mine_tunnel_entrance_8017F3D0,
    D_mine_tunnel_entrance_8017F3D0,
    D_mine_tunnel_entrance_8017F3D0,
    D_mine_tunnel_entrance_8017F3D0,
};

s32 func_mine_tunnel_entrance_8017D5E8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message handler 0x13EE of the room's message table: copies the incoming
/// record onto the outgoing one, passes both to `func_map_shelter_80179A04`, and returns 1.
s32 func_mine_tunnel_entrance_8017D5F0(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

s32 func_mine_tunnel_entrance_8017D634(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_mine_tunnel_entrance_8017D63C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// State 0 of the room task: installs the room's message table, publishes the
/// task in pointer slot 7, advances to the next state and selects scene music entry 1.
static void func_mine_tunnel_entrance_8017D644(Task* arg0)
{
    arg0->msgTable = D_mine_tunnel_entrance_8017DAF0;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    arg0->state           = (s32)(arg0->state + 1);
    gStageSceneMusicEntry = 1;
}

/// State 1 of the room task: moves the saved scene event from 9 on to 10.
static void func_mine_tunnel_entrance_8017D690(Task* task)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == 9) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0xA;
    }
}

static void func_mine_tunnel_entrance_8017D6B4(Task* task)
{
}

/// Per-frame entry of the room task: copies the state table onto the stack
/// and runs the handler for the task's current state.
void func_mine_tunnel_entrance_8017D6BC(Task* task)
{
    TaskFuncTable4 states;

    states = D_mine_tunnel_entrance_8017D5C4;
    states.funcs[task->state](task);
}

/// Sets `gRoomEffectState->roomEffectMode` to 2, then draws the quads the current
/// camera view shows, one `glowDrawFlare` call per
/// position with UV column 0 or 1 and half-extent 0x300 (0x200 for view 6's
/// second quad). Other views draw nothing.
void func_mine_tunnel_entrance_8017D720(Task* unused)
{
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB18;
            glowDrawFlare(&p[0], 0, 0x300);
            glowDrawFlare(&p[1], 0, 0x300);
            glowDrawFlare(&p[2], 1, 0x300);
            glowDrawFlare(&p[5], 1, 0x300);
            break;
        }
        case 3: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB30;
            glowDrawFlare(&p[0], 1, 0x300);
            glowDrawFlare(&p[1], 1, 0x300);
            glowDrawFlare(&p[2], 1, 0x300);
            break;
        }
        case 4: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB30;
            glowDrawFlare(&p[0], 1, 0x300);
            glowDrawFlare(&p[1], 1, 0x300);
            break;
        }
        case 5: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB38;
            glowDrawFlare(&p[0], 1, 0x300);
            break;
        }
        case 6: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB48;
            glowDrawFlare(&p[0], 1, 0x300);
            glowDrawFlare(&p[1], 1, 0x200);
            break;
        }
    }
}

#include "../../shared/glow_draw_flare.inc.c"
