#include "rooms/neo_ark_pyramid.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"

/// Angle of the room's rotating quad, set by `func_neo_ark_pyramid_8017DAC0`.
extern s32      D_neo_ark_pyramid_801818A4;
extern TaskDesc D_neo_ark_pyramid_8017FC0C;

/// The room's message table: handlers for messages 0x13EE, 0x13F1, 0x13EF
/// and 0x13F0, closed by a `TASK_MESSAGE_TABLE_END` entry.
extern TaskMessageEntry D_neo_ark_pyramid_8017FBE4[];

/// The two points on the spawner's parent coordinate that the ribbon task
/// trails: the first entry places the effect, the second (reached here both
/// as `[1]` and under its own label) is the ribbon's other edge.

static void func_neo_ark_pyramid_8017DAC0(s32 arg0);
static void func_neo_ark_pyramid_8017DB18(Task* task);
static void func_neo_ark_pyramid_8017DB5C(Task* task);

/// State handlers of the room's entry task, indexed by its state through
/// `func_neo_ark_pyramid_8017DB98`: set-up, per-frame draw, then kill.
static const TaskFuncTable3 D_neo_ark_pyramid_8017D5C4 = {
    { func_neo_ark_pyramid_8017DB18, func_neo_ark_pyramid_8017DB5C, taskKill }
};

void func_neo_ark_pyramid_8017D600(Task*);
s32  func_neo_ark_pyramid_8017D9F0(Task*, s32, s32, s32);
s32  func_neo_ark_pyramid_8017D9F8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_neo_ark_pyramid_8017DA3C(Task*, s32, s32, s32);
s32  func_neo_ark_pyramid_8017DA44(Task* task, s32 msgId, const void* firstArg, s32 arg3);

extern WorldCollisionGrid     D_neo_ark_pyramid_801802C4[1];
extern WorldCollisionOccluder D_neo_ark_pyramid_80181790[3];
extern WorldCollisionTrigger  D_neo_ark_pyramid_801812B0[6];
extern WorldCollisionTrigger  D_neo_ark_pyramid_80181478[7];
extern WorldCoordRoomLights   D_neo_ark_pyramid_80181298[1];

TaskMessageEntry D_neo_ark_pyramid_8017FBE4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_pyramid_8017D9F8 },
    { 5105, func_neo_ark_pyramid_8017D9F0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_pyramid_8017DA44 },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_pyramid_8017DA3C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_neo_ark_pyramid_8017FC0C = { { { TASK_BODY_NONE, 32 } }, func_neo_ark_pyramid_8017D600, { .value = 0 } };

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_neo_ark_pyramid_8017FC28[2] = {
    { D_neo_ark_pyramid_801802C4, D_neo_ark_pyramid_801812B0, D_neo_ark_pyramid_80181478, D_neo_ark_pyramid_80181790 },
    { D_neo_ark_pyramid_801802C4, D_neo_ark_pyramid_801812B0, D_neo_ark_pyramid_80181478, D_neo_ark_pyramid_80181790 },
};

WorldCoordRoomLighting D_neo_ark_pyramid_8017FC48[2] = {
    { D_neo_ark_pyramid_80181298, NULL },
    { D_neo_ark_pyramid_80181298, NULL },
};

u8 D_neo_ark_pyramid_8017FC58[8] = {
    1,
    2,
    3,
    6,
    5,
    4,
    7,
    8,
};

u8* D_neo_ark_pyramid_8017FC60[2] = {
    gViewIdentityMap,
    D_neo_ark_pyramid_8017FC58,
};

ViewCount D_neo_ark_pyramid_8017FC68[2] = { 8, 8 };

DirectionWarpEntry D_neo_ark_pyramid_8017FC6C[2] = {
    { { { .word = 2048 }, -2944, 0, -2035 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -2944, 0, -2035 }, { 0, 0, 0, 0 }, 0x55200002, 0x55200001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 3200, -650, -7490 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4200, -1160, -7500 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gNeoArkPyramidCollision02D04Normals[13] = {
#include "assets/neo_ark_pyramid_collision_02D04_normals.inc"
};

static SVECTOR _gNeoArkPyramidCollision02D04Verts[66] = {
#include "assets/neo_ark_pyramid_collision_02D04_verts.inc"
};

static WorldCollisionGridFace _gNeoArkPyramidCollision02D04Faces[36] = {
#include "assets/neo_ark_pyramid_collision_02D04_faces.inc"
};

static s16 _gNeoArkPyramidCollision02D04Cells[194] = {
#include "assets/neo_ark_pyramid_collision_02D04_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkPyramidCollision02D04Cells[i])
static s16* _gNeoArkPyramidCollision02D04Table[15] = {
#include "assets/neo_ark_pyramid_collision_02D04_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_pyramid_801802C4[1] = {
    { NULL, _gNeoArkPyramidCollision02D04Normals, _gNeoArkPyramidCollision02D04Verts, _gNeoArkPyramidCollision02D04Faces, _gNeoArkPyramidCollision02D04Table, 4000, 0x2CEC, 5, 3, 4000, 36 },
};

ViewCamera D_neo_ark_pyramid_801802E8[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -5270, 0x7530, 7060 } }, 380 },
    { { { { 3888, 0, 1288 }, { -275, 4001, 830 }, { -1258, -875, 3798 } }, { 2039, 468, 5719 } }, 230 },
    { { { { 2546, 0, 3208 }, { 47, 4095, -37 }, { -3208, 60, 2545 } }, { -1654, 855, 7910 } }, 230 },
    { { { { -1401, 0, -3848 }, { 212, 4089, -77 }, { 3842, -226, -1399 } }, { 2273, 757, 5456 } }, 230 },
    { { { { -405, 0, -4075 }, { -324, 4083, 32 }, { 4062, 325, -404 } }, { -570, 1119, 8915 } }, 230 },
    { { { { -1401, 0, -3848 }, { 212, 4089, -77 }, { 3842, -226, -1399 } }, { 2273, 757, 5456 } }, 230 },
    { { { { -1554, 0, 3789 }, { 807, 4001, 331 }, { -3702, 872, -1518 } }, { 765, 870, 6068 } }, 257 },
    { { { { -2878, 0, 2914 }, { 16, 4095, 16 }, { -2914, 23, -2878 } }, { 1595, 667, 6136 } }, 312 },
};

SpriteBatch D_neo_ark_pyramid_80180408[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_pyramid_80180418[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_pyramid_80180428[12] = {
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -112, -48, 1050, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -96, -48, 1075, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -80, -40, 1100, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 8, 1606, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 0, 1611, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 0, 1621, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 0, 1588, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, -8, 1611, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 96, -8, 1611, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -8, 1611, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, -16, 1611, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 40, 1611, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_pyramid_80180518[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_pyramid_80180530[43] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 8, 1405, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 16, 1399, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 16, 1399, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 24, 1395, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 24, 1395, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 192 } }, -160, -120, 1625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 184 } }, -120, -120, 1625, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -104, -104, 1625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -96, -104, 1625, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, -120, 1625, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, -88, 1625, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -80, 1625, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, -80, 1625, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, 24, 1625, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 24, 1625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 1395, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -24, 24, 1437, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -32, 8, 1589, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -32, 16, 1503, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -40, 0, 1651, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -40, -8, 1745, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -40, -16, 1824, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -48, -24, 1963, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -48, -32, 2033, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -48, -40, 2182, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -56, -48, 2324, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -56, -56, 2624, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, -64, 2681, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 32, 1426, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 32, 1426, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 40, 1393, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 40, 1425, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 32, 1391, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 32, 1390, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, -16, 1562, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, -8, 1466, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 0, 1389, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 8, 1339, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 16, 1298, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 24, 1254, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 32, 1263, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 40, 1263, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 48, 1273, { .fields = { 40, 8 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_pyramid_8018088C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 34, 0, 0, { 1, 0 } },
    { 34, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_pyramid_801808AC[24] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 24, 723, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -160, -96, 1184, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -144, -112, 1268, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -128, -120, 1336, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -112, -120, 1845, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -96, -120, 1098, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 0, 772, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 24, 708, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 56, 639, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 606, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 0, 774, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 24, 713, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 56, 642, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 80, 589, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, 0, 756, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 24, 719, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, 56, 645, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 80, 593, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 0, 760, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 24, 711, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 56, 627, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 80, 585, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 0, 764, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, 24, 715, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_pyramid_80180A8C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_pyramid_80180AA4[41] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 8, 1405, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 192 } }, -160, -120, 1625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 184 } }, -120, -120, 1625, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -104, -104, 1625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -96, -104, 1625, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, -120, 1625, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, -88, 1625, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -80, 1625, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, -80, 1625, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, 24, 1625, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 24, 1625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 1395, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -24, 24, 1437, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -32, 8, 1589, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -32, 16, 1503, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -40, 0, 1651, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -40, -8, 1745, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -40, -16, 1824, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -48, -24, 1963, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -48, -32, 2033, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -48, -40, 2182, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -56, -48, 2324, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -56, -56, 2624, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, -64, 2681, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 32, 1426, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 40, 1393, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 32, 1426, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 40, 1426, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 32, 1391, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 32, 1390, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 16, 1399, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 24, 1395, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, -16, 1562, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, -8, 1466, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 0, 1389, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 8, 1339, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 16, 1298, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 24, 1254, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 32, 1263, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 40, 1263, { .fields = { 40, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 48, 1273, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_pyramid_80180DD8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 32, 0, 0, { 1, 0 } },
    { 32, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_pyramid_80180DF8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_pyramid_80180E08[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_pyramid_80180E18[8] = {
    { { .empty = D_neo_ark_pyramid_80180408 }, D_neo_ark_pyramid_80180408, NULL },
    { { .empty = D_neo_ark_pyramid_80180418 }, D_neo_ark_pyramid_80180418, NULL },
    { { .elements = D_neo_ark_pyramid_80180428 }, D_neo_ark_pyramid_80180518, NULL },
    { { .elements = D_neo_ark_pyramid_80180530 }, D_neo_ark_pyramid_8018088C, NULL },
    { { .elements = D_neo_ark_pyramid_801808AC }, D_neo_ark_pyramid_80180A8C, NULL },
    { { .elements = D_neo_ark_pyramid_80180AA4 }, D_neo_ark_pyramid_80180DD8, NULL },
    { { .empty = D_neo_ark_pyramid_80180DF8 }, D_neo_ark_pyramid_80180DF8, NULL },
    { { .empty = D_neo_ark_pyramid_80180E08 }, D_neo_ark_pyramid_80180E08, NULL },
};

WorldCoordPointLight D_neo_ark_pyramid_80180E78[11] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3490, -2000, -3010 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -80, -4000, -6040 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 2000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4120, -2000, -5600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2560, -2000, -9990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3940, -2000, -0x28E6 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5480, -2000, -0x2B16 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1900, -2000, -7420 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2760, -2000, -5140 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1360, -2000, -6580 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4890, -2000, -4660 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -230, -2000, -7890 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 4000 },
};

WorldCoordRoomLights D_neo_ark_pyramid_80181298[1] = {
    { 0, NULL, ARRAY_SIZE(D_neo_ark_pyramid_80180E78), D_neo_ark_pyramid_80180E78, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_pyramid_801812B0[6] = {
    { NULL, NULL, NULL, { -2792, -4000, -3605, 0 }, { { 2775, -4752, 1502, 0 }, { -2776, -4752, -1503, 0 }, { 2775, 4752, 1502, 0 }, { -2776, 4752, -1503, 0 } }, { -1953, 0, 3606, 0 }, { 0, 0, 4096, 0 }, 5701, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2914, -4033, -3426, 0 }, { { -2775, -4752, -1502, 0 }, { 2776, -4752, 1503, 0 }, { -2775, 4752, -1502, 0 }, { 2776, 4752, 1503, 0 } }, { 1952, 0, -3608, 0 }, { 0, 0, 4096, 0 }, 5701, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -449, -4064, -7490, 0 }, { { 1127, -4752, 3686, 0 }, { -1148, -4752, -3699, 0 }, { 1127, 4752, 3686, 0 }, { -1148, 4752, -3699, 0 } }, { -3931, 0, 1210, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -544, -3936, -7296, 0 }, { { -1147, -4752, -3698, 0 }, { 1128, -4752, 3687, 0 }, { -1147, 4752, -3698, 0 }, { 1128, 4752, 3687, 0 } }, { 3930, 0, -1211, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2766, -3968, -0x2BF1, 0 }, { { 32, -4752, 2884, 0 }, { -31, -4752, -2884, 0 }, { 32, 4752, 2884, 0 }, { -31, 4752, -2884, 0 } }, { -4106, 0, 44, 0 }, { 0, 0, 4096, 0 }, 5538, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2559, -4032, -0x2BC0, 0 }, { { -31, -4752, -2884, 0 }, { 32, -4752, 2884, 0 }, { -31, 4752, -2884, 0 }, { 32, 4752, 2884, 0 } }, { 4105, 0, -46, 0 }, { 0, 0, 4096, 0 }, 5538, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_pyramid_80181478[7] = {
    { NULL, NULL, NULL, { 3005, -3872, -7457, 0 }, { { -13, -5344, 1023, 0 }, { 14, -5344, -1022, 0 }, { -13, 5344, 1023, 0 }, { 14, 5344, -1022, 0 } }, { -4109, 0, -55, 0 }, { -4092, -201, -14, 0 }, 5418, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 20, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2272, -64, -7472, 0 }, { { 416, 14, -702, 0 }, { 416, -13, 703, 0 }, { -416, 14, -702, 0 }, { -416, -13, 703, 0 } }, { 0, 4103, 71, 0 }, { -4096, 0, 0, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_FACING, 71, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3104, -658, -7488, 0 }, { { 416, -2, -974, 0 }, { 416, 3, 975, 0 }, { -416, -2, -974, 0 }, { -416, 3, 975, 0 } }, { 0, 4099, -21, 0 }, { 4091, 0, -201, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100 | WORLD_COLLISION_TRIGGER_AUTOMATIC, 67, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2945, -48, -1888, 0 }, { { 975, -2, 416, 0 }, { -974, 3, 416, 0 }, { 975, -2, -416, 0 }, { -974, 3, -416, 0 } }, { 10, 4099, 0, 0 }, { -201, 0, -4091, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_WARP, 29, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1632, -64, -5985, 0 }, { { 963, -2, -443, 0 }, { -346, 3, 1002, 0 }, { 346, -2, -1001, 0 }, { -963, 3, 443, 0 } }, { 0, 4099, -11, 0 }, { 3405, 0, 2275, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6558, -64, -5760, 0 }, { { 416, 14, -894, 0 }, { 416, -13, 895, 0 }, { -416, 14, -894, 0 }, { -416, -13, 895, 0 } }, { 0, 4106, 56, 0 }, { -4096, 0, 0, 0 }, 985, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6528, -64, -9792, 0 }, { { 416, 14, -1278, 0 }, { 416, -13, 1279, 0 }, { -416, 14, -1278, 0 }, { -416, -13, 1279, 0 } }, { 0, 4119, 39, 0 }, { -4096, 0, 0, 0 }, 1342, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_neo_ark_pyramid_8018168C[2] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103800_80137D74 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_pyramid_801816A4[2] = {
    { 26, 26, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_pyramid_801816BC[3] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103800_80137D74 },
    { 26, 26, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_202600_801528D4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_pyramid_801816E0[3] = {
    { 13, 13, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401300_80158A18 },
    { 20, 20, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_302000_80177DF0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_pyramid_80181704[3] = {
    { 23, 23, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102300_80147AB8 },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205700_801611F8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_pyramid_80181728[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017C580, D_neo_ark_pyramid_8018168C },
    { D_map_neo_ark_8017C620, D_neo_ark_pyramid_801816A4 },
    { D_map_neo_ark_8017C690, D_neo_ark_pyramid_801816BC },
    { D_map_neo_ark_8017C720, D_neo_ark_pyramid_801816E0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017C760, D_neo_ark_pyramid_80181704 },
    { NULL, NULL },
};

WorldCollisionOccluder D_neo_ark_pyramid_80181790[3] = {
    { NULL, NULL, { 6416, -1104, -6464, 0 }, { { -2800, 1776, 0, 0 }, { 2800, 1776, 0, 0 }, { -2800, 208, 0, 0 }, { 2800, -3760, 0, 0 } }, { 0, 0, 4097, 0 }, 4664, 1, 0 },
    { NULL, NULL, { 6400, -1088, -8384, 0 }, { { -2800, 1776, 0, 0 }, { 2800, 1776, 0, 0 }, { -2800, 208, 0, 0 }, { 2800, -3760, 0, 0 } }, { 0, 0, 4097, 0 }, 4664, 1, 0 },
    { NULL, NULL, { 1536, -1984, -5744, 0 }, { { -5776, 0, 4448, 0 }, { 5776, 0, 4448, 0 }, { -5776, 0, -2464, 0 }, { 5776, 0, -6432, 0 } }, { 0, -4110, 0, 0 }, 8628, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_neo_ark_pyramid_80181844 = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

WorldCollisionFootstepSounds D_neo_ark_pyramid_80181850 = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionSurfaceProperties D_neo_ark_pyramid_8018185C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_pyramid_80181864[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_pyramid_8018186C[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_pyramid_80181874[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_pyramid_80181844 },
};

WorldCollisionSurfaceProperties D_neo_ark_pyramid_8018187C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_pyramid_80181850 },
};

WorldCollisionSurfaceProperties* D_neo_ark_pyramid_80181884[8] = {
    D_neo_ark_pyramid_8018185C,
    D_neo_ark_pyramid_80181864,
    D_neo_ark_pyramid_8018186C,
    D_neo_ark_pyramid_80181874,
    D_neo_ark_pyramid_8018187C,
    D_neo_ark_pyramid_8018185C,
    D_neo_ark_pyramid_8018185C,
    D_neo_ark_pyramid_8018185C,
};

s32 D_neo_ark_pyramid_801818A4 = 0;

static void func_neo_ark_pyramid_8017D7F4(s32 arg0);

/// Event task that turns the room's rotating quad one step. It hides the HUD
/// and runs capture command 1; unless that ends on event key 0xC it plays a
/// sound and sweeps the quad's angle over 0x156 in steps of 4, then bumps
/// game-flag nibble 0xEC. Below four turns it returns to the capture command;
/// on the fourth it plays the closing sound and capture command 2. Either exit
/// restores the HUD and the player's weapon before the task kills itself.
void func_neo_ark_pyramid_8017D600(Task* task)
{
    u16 count;

    switch (task->state) {
        case 0:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 8;
            gGameSession->hideHud                                      = 1;
            gGameSession->eventState                                   = 1;
            gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_HIDDEN;
            task->state++;
            break;
        case 1:
            Gp_RunCapCmd(1, 0);
            task->state++;
            break;
        case 2:
            D_80115690 = 1;
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 3:
            if (Gp_GetCapEventKey() == 0xC) {
                task->state = 0xA;
                break;
            }
            sndEvtRequestScriptStart(SOUND_NEO_ARK_PYRAMID_ROTATE, 0, 0);
            task->killCountdown = 0;
            task->state++;
            break;
        case 4:
            count               = task->killCountdown + 4;
            task->killCountdown = count;
            if ((s16)count >= 0x156) {
                gameFlagSetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT, gameFlagGetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT) + 1);
                func_neo_ark_pyramid_8017DAC0(0);
                if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT) >= 4) {
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_PYRAMID_ROTATE_DONE, 0, 0);
                    Gp_RunCapCmd(2, 0);
                    task->state++;
                } else {
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_PYRAMID_ROTATE_STOP, 0, 0);
                    task->state = 1;
                }
            } else {
                func_neo_ark_pyramid_8017DAC0((s16)count);
            }
            break;
        case 5:
            D_80115690 = 1;
            if (Gp_CapBusy() == 0) {
                task->state = 0xA;
            }
            break;
        case 10:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 3;
            gGameSession->hideHud                                      = 0;
            gGameSession->eventState                                   = 0;
            gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_RUNNING;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            taskKill(task);
            break;
    }
}

/// Queues the room's rotating quad: a 0xAE-pixel textured `POLY_FT4` centred
/// on the screen origin, rotated by `arg0` (0x1000 a full turn).
static void func_neo_ark_pyramid_8017D7F4(s32 arg0)
{
    POLY_FT4* prim;
    s16       src[4][2];
    s16       dst[4][2];
    s32       i;

    src[0][0] = -0x57;
    src[0][1] = -0x57;
    src[1][0] = 0x57;
    src[1][1] = -0x57;
    src[2][0] = -0x57;
    src[2][1] = 0x57;
    src[3][0] = 0x57;
    src[3][1] = 0x57;
    for (i = 0; i < 4; i++) {
        dst[i][0] = (src[i][0] * rcos(arg0) - src[i][1] * rsin(arg0)) >> 12;
        dst[i][1] = (src[i][0] * rsin(arg0) + src[i][1] * rcos(arg0)) >> 12;
    }
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2D);
    prim->x0    = dst[0][0];
    prim->y0    = dst[0][1];
    prim->x1    = dst[1][0];
    prim->y1    = dst[1][1];
    prim->x2    = dst[2][0];
    prim->y2    = dst[2][1];
    prim->x3    = dst[3][0];
    prim->y3    = dst[3][1];
    prim->u0    = 1;
    prim->v0    = 1;
    prim->u1    = 0xAF;
    prim->v1    = 1;
    prim->u2    = 1;
    prim->v2    = 0xAF;
    prim->u3    = 0xAF;
    prim->v3    = 0xAF;
    prim->clut  = 0x3FC0;
    prim->tpage = 0x8E;
    addPrim(gGpuCurrentOt + 0xC, prim);
}

/// Handler for message 0x13F1 in the room's message table; does nothing.
s32 func_neo_ark_pyramid_8017D9F0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table: copies the
/// incoming save-location record onto the outgoing one and forwards both to
/// `func_map_neo_ark_80179B14`. Always answers 1.
s32 func_neo_ark_pyramid_8017D9F8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

/// Handler for message 0x13F0 in the room's message table; does nothing.
s32 func_neo_ark_pyramid_8017DA3C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler for message 0x13EF in the room's message table. When the message's
/// `actionId` is 1 it resets the quad's angle; once the quad has turned four
/// times it spawns capture event 3, otherwise it has the player lower the
/// weapon and starts the task that turns the quad another step.
s32 func_neo_ark_pyramid_8017DA44(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if (request->actionId == 1) {
        func_neo_ark_pyramid_8017DAC0(0);
        if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT) == 4) {
            Gp_SpawnIfCapIdle(3, 1);
        } else {
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            taskSpawnFromTable(&D_neo_ark_pyramid_8017FC0C, 0, 0, 0);
        }
    }
    return 0;
}

/// Sets the quad's angle to twelfths of a turn counted by game-flag nibble
/// 0xEC, less four, plus the in-progress sweep `arg0`.
static void func_neo_ark_pyramid_8017DAC0(s32 arg0)
{
    D_neo_ark_pyramid_801818A4 = (((gameFlagGetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT) - 4) << 0xC) / 12) + arg0;
}

/// State 0 of the room's entry task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7 and advances.
static void func_neo_ark_pyramid_8017DB18(Task* task)
{
    task->msgTable = D_neo_ark_pyramid_8017FBE4;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's entry task: draws the rotating quad at its current
/// angle while the session's view is 8.
static void func_neo_ark_pyramid_8017DB5C(Task* task)
{
    if (gGameSession->location.loc.view == 8) {
        func_neo_ark_pyramid_8017D7F4(D_neo_ark_pyramid_801818A4);
    }
}

/// Task tick that dispatches on the task's state through the three-entry
/// handler table `D_neo_ark_pyramid_8017D5C4`, copied to the stack first.
void func_neo_ark_pyramid_8017DB98(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_pyramid_8017D5C4;
    sp.funcs[task->state](task);
}

/// One-shot task: on its first tick stores 0x601E2, 0x601FE and 0x6021A into
/// three gameplay globals and enables `gRoomEffectState->roomEffectMode`.
void func_neo_ark_pyramid_8017DBF0(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectFlashId               = EFFECT_NEO_ARK_PYRAMID_FLASH;
        gRoomEffectTwinTrailId           = EFFECT_NEO_ARK_PYRAMID_TWIN_TRAIL;
        gRoomEffectSparkBurstId          = EFFECT_NEO_ARK_PYRAMID_SPARK_BURST;
        gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
        arg0->state                      = 1;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_neo_ark_pyramid_8017DC50(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_neo_ark_pyramid_8017E6B4(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_neo_ark_pyramid_8017EF9C(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
