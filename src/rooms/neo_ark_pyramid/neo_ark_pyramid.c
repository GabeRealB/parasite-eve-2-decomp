#include "rooms/neo_ark_pyramid.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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

/// Angle of the room's rotating quad, set by `func_neo_ark_pyramid_8017DAC0`.
extern s32      D_neo_ark_pyramid_801818A4;
extern TaskDesc D_neo_ark_pyramid_8017FC0C;

/// The room's message table: handlers for messages 0x13EE, 0x13F1, 0x13EF
/// and 0x13F0, closed by a 0x7FFFFFFF entry.
extern GpMsgEntry D_neo_ark_pyramid_8017FBE4[];

/// The two points on the spawner's parent coordinate that the ribbon task
/// trails: the first entry places the effect, the second (reached here both
/// as `[1]` and under its own label) is the ribbon's other edge.

static void func_neo_ark_pyramid_8017DAC0(s32 arg0);
static void func_neo_ark_pyramid_8017DB18(Task* task);
static void func_neo_ark_pyramid_8017DB5C(Task* task);
static void func_neo_ark_pyramid_8017DEF4(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_neo_ark_pyramid_8017E320(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_neo_ark_pyramid_8017EBA4(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_neo_ark_pyramid_8017F224(GfxCoord* arg0, s16 arg1, u8* rgb);

/// State handlers of the room's entry task, indexed by its state through
/// `func_neo_ark_pyramid_8017DB98`: set-up, per-frame draw, then kill.
static const TaskFuncTable3 D_neo_ark_pyramid_8017D5C4 = {
    { func_neo_ark_pyramid_8017DB18, func_neo_ark_pyramid_8017DB5C, taskKill }
};

void func_neo_ark_pyramid_8017D600(Task*);
s32  func_neo_ark_pyramid_8017D9F0(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_neo_ark_pyramid_8017D9F8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_neo_ark_pyramid_8017DA3C(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_neo_ark_pyramid_8017DA44(Task*, s32, DirectionActionRequest* request, GpMessageArg);

extern GpGridParams   D_neo_ark_pyramid_801802C4[1];
extern GpObj3A        D_neo_ark_pyramid_80181790[3];
extern GpObj4C        D_neo_ark_pyramid_801812B0[6];
extern GpObj4C        D_neo_ark_pyramid_80181478[7];
extern GpRoomCoordSet D_neo_ark_pyramid_80181298[1];

GpMsgEntry D_neo_ark_pyramid_8017FBE4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_pyramid_8017D9F8 },
    { 5105, func_neo_ark_pyramid_8017D9F0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_pyramid_8017DA44 },
    { 5104, func_neo_ark_pyramid_8017DA3C },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_neo_ark_pyramid_8017FC0C = { 0, 32, func_neo_ark_pyramid_8017D600, { .model = NULL } };

SVECTOR D_neo_ark_pyramid_8017FC18[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

GpRoomObjRec D_neo_ark_pyramid_8017FC28[2] = {
    { D_neo_ark_pyramid_801802C4, D_neo_ark_pyramid_801812B0, D_neo_ark_pyramid_80181478, D_neo_ark_pyramid_80181790 },
    { D_neo_ark_pyramid_801802C4, D_neo_ark_pyramid_801812B0, D_neo_ark_pyramid_80181478, D_neo_ark_pyramid_80181790 },
};

GpRoomCoordRec D_neo_ark_pyramid_8017FC48[2] = {
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
    D_8010CAF8,
    D_neo_ark_pyramid_8017FC58,
};

GpViewCountRec D_neo_ark_pyramid_8017FC68[2] = {
    { { .bytes = { 8, 0 } } },
    { { .bytes = { 8, 0 } } },
};

GpWarpRec D_neo_ark_pyramid_8017FC6C[2] = {
    { { .words = { 2048, -2944, 0, -2035 } }, { 0, 0, 0, 0 }, { .words = { 2048, -2944, 0, -2035 } }, { 0, 0, 0, 0 }, 0x55200002, 0x55200001, 0, 2, 0, 0 },
    { { .words = { 3072, 3200, -650, -7490 } }, { 0, 0, 0, 0 }, { .words = { 3072, 4200, -1160, -7500 } }, { 0, 0, 0, 0 }, 0, 0, 0, 4, 1, 0 },
};

SVECTOR D_neo_ark_pyramid_8017FCDC[13] = {
#include "assets/neo_ark_pyramid_collision_02D04_normals.inc"
};

SVECTOR D_neo_ark_pyramid_8017FD44[66] = {
#include "assets/neo_ark_pyramid_collision_02D04_verts.inc"
};

GpGridFace D_neo_ark_pyramid_8017FF54[36] = {
#include "assets/neo_ark_pyramid_collision_02D04_faces.inc"
};

s16 D_neo_ark_pyramid_80180104[194] = {
#include "assets/neo_ark_pyramid_collision_02D04_cells.inc"
};

#define GRID_CELL(i) (&D_neo_ark_pyramid_80180104[i])
s16* D_neo_ark_pyramid_80180288[15] = {
#include "assets/neo_ark_pyramid_collision_02D04_table.inc"
};
#undef GRID_CELL

GpGridParams D_neo_ark_pyramid_801802C4[1] = {
    { NULL, D_neo_ark_pyramid_8017FCDC, D_neo_ark_pyramid_8017FD44, D_neo_ark_pyramid_8017FF54, D_neo_ark_pyramid_80180288, 4000, 0x2CEC, 5, 3, 4000, 36 },
};

GpViewRec D_neo_ark_pyramid_801802E8[8] = {
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

GpSprtElem D_neo_ark_pyramid_80180428[12] = {
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

GpSprtElem D_neo_ark_pyramid_80180530[43] = {
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

GpSprtElem D_neo_ark_pyramid_801808AC[24] = {
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

GpSprtElem D_neo_ark_pyramid_80180AA4[41] = {
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

GpSprtRec D_neo_ark_pyramid_80180E18[8] = {
    { { .empty = D_neo_ark_pyramid_80180408 }, D_neo_ark_pyramid_80180408, NULL },
    { { .empty = D_neo_ark_pyramid_80180418 }, D_neo_ark_pyramid_80180418, NULL },
    { { .elements = D_neo_ark_pyramid_80180428 }, D_neo_ark_pyramid_80180518, NULL },
    { { .elements = D_neo_ark_pyramid_80180530 }, D_neo_ark_pyramid_8018088C, NULL },
    { { .elements = D_neo_ark_pyramid_801808AC }, D_neo_ark_pyramid_80180A8C, NULL },
    { { .elements = D_neo_ark_pyramid_80180AA4 }, D_neo_ark_pyramid_80180DD8, NULL },
    { { .empty = D_neo_ark_pyramid_80180DF8 }, D_neo_ark_pyramid_80180DF8, NULL },
    { { .empty = D_neo_ark_pyramid_80180E08 }, D_neo_ark_pyramid_80180E08, NULL },
};

GpPointLight D_neo_ark_pyramid_80180E78[11] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3490, -2000, -3010 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -80, -4000, -6040 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 2000, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4120, -2000, -5600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2560, -2000, -9990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3940, -2000, -0x28E6 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5480, -2000, -0x2B16 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1900, -2000, -7420 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2760, -2000, -5140 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1360, -2000, -6580 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4890, -2000, -4660 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -230, -2000, -7890 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
};

GpRoomCoordSet D_neo_ark_pyramid_80181298[1] = {
    { 0, NULL, 11, D_neo_ark_pyramid_80180E78, 0, NULL },
};

GpObj4C D_neo_ark_pyramid_801812B0[6] = {
    { NULL, NULL, NULL, { -2792, -4000, -3605, 0 }, { { 2775, -4752, 1502, 0 }, { -2776, -4752, -1503, 0 }, { 2775, 4752, 1502, 0 }, { -2776, 4752, -1503, 0 } }, { -1953, 0, 3606, 0 }, { 0, 0, 4096, 0 }, 5701, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -2914, -4033, -3426, 0 }, { { -2775, -4752, -1502, 0 }, { 2776, -4752, 1503, 0 }, { -2775, 4752, -1502, 0 }, { 2776, 4752, 1503, 0 } }, { 1952, 0, -3608, 0 }, { 0, 0, 4096, 0 }, 5701, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -449, -4064, -7490, 0 }, { { 1127, -4752, 3686, 0 }, { -1148, -4752, -3699, 0 }, { 1127, 4752, 3686, 0 }, { -1148, 4752, -3699, 0 } }, { -3931, 0, 1210, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -544, -3936, -7296, 0 }, { { -1147, -4752, -3698, 0 }, { 1128, -4752, 3687, 0 }, { -1147, 4752, -3698, 0 }, { 1128, 4752, 3687, 0 } }, { 3930, 0, -1211, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 2766, -3968, -0x2BF1, 0 }, { { 32, -4752, 2884, 0 }, { -31, -4752, -2884, 0 }, { 32, 4752, 2884, 0 }, { -31, 4752, -2884, 0 } }, { -4106, 0, 44, 0 }, { 0, 0, 4096, 0 }, 5538, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 2559, -4032, -0x2BC0, 0 }, { { -31, -4752, -2884, 0 }, { 32, -4752, 2884, 0 }, { -31, 4752, -2884, 0 }, { 32, 4752, 2884, 0 } }, { 4105, 0, -46, 0 }, { 0, 0, 4096, 0 }, 5538, 0, 5, 4, 129, 0 },
};

GpObj4C D_neo_ark_pyramid_80181478[7] = {
    { NULL, NULL, NULL, { 3005, -3872, -7457, 0 }, { { -13, -5344, 1023, 0 }, { 14, -5344, -1022, 0 }, { -13, 5344, 1023, 0 }, { 14, 5344, -1022, 0 } }, { -4109, 0, -55, 0 }, { -4092, -201, -14, 0 }, 5418, 0x8000, 20, 33, 2, 0 },
    { NULL, NULL, NULL, { 2272, -64, -7472, 0 }, { { 416, 14, -702, 0 }, { 416, -13, 703, 0 }, { -416, 14, -702, 0 }, { -416, -13, 703, 0 } }, { 0, 4103, 71, 0 }, { -4096, 0, 0, 0 }, 814, 1, 71, 64, 2, 0 },
    { NULL, NULL, NULL, { 3104, -658, -7488, 0 }, { { 416, -2, -974, 0 }, { 416, 3, 975, 0 }, { -416, -2, -974, 0 }, { -416, 3, 975, 0 } }, { 0, 4099, -21, 0 }, { 4091, 0, -201, 0 }, 1055, 0x8101, 67, 192, 2, 0 },
    { NULL, NULL, NULL, { -2945, -48, -1888, 0 }, { { 975, -2, 416, 0 }, { -974, 3, 416, 0 }, { 975, -2, -416, 0 }, { -974, 3, -416, 0 } }, { 10, 4099, 0, 0 }, { -201, 0, -4091, 0 }, 1055, 0, 29, 18, 2, 0 },
    { NULL, NULL, NULL, { -1632, -64, -5985, 0 }, { { 963, -2, -443, 0 }, { -346, 3, 1002, 0 }, { 346, -2, -1001, 0 }, { -963, 3, 443, 0 } }, { 0, 4099, -11, 0 }, { 3405, 0, 2275, 0 }, 1055, 0x4005, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 6558, -64, -5760, 0 }, { { 416, 14, -894, 0 }, { 416, -13, 895, 0 }, { -416, 14, -894, 0 }, { -416, -13, 895, 0 } }, { 0, 4106, 56, 0 }, { -4096, 0, 0, 0 }, 985, 2, 4, 0, 2, 0 },
    { NULL, NULL, NULL, { 6528, -64, -9792, 0 }, { { 416, 14, -1278, 0 }, { 416, -13, 1279, 0 }, { -416, 14, -1278, 0 }, { -416, -13, 1279, 0 } }, { 0, 4119, 39, 0 }, { -4096, 0, 0, 0 }, 1342, 2, 5, 0, 130, 0 },
};

GpAreaTmdRec D_neo_ark_pyramid_8018168C[2] = {
    { 38, 38, 0, 0, { 0, 0 }, D_80137D74 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_pyramid_801816A4[2] = {
    { 26, 26, 0, 0, { 0, 0 }, D_8013A8D4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_pyramid_801816BC[3] = {
    { 38, 38, 0, 0, { 0, 0 }, D_80137D74 },
    { 26, 26, 1, 0, { 0, 0 }, D_801528D4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_pyramid_801816E0[3] = {
    { 13, 13, 3, 0, { 0, 0 }, D_80158A18 },
    { 20, 20, 2, 0, { 0, 0 }, D_80177DF0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_pyramid_80181704[3] = {
    { 23, 23, 0, 0, { 0, 0 }, D_80147AB8 },
    { 57, 57, 1, 0, { 0, 0 }, D_801611F8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_neo_ark_pyramid_80181728[13] = {
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

GpObj3A D_neo_ark_pyramid_80181790[3] = {
    { NULL, NULL, { 6416, -1104, -6464, 0 }, { { -2800, 1776, 0, 0 }, { 2800, 1776, 0, 0 }, { -2800, 208, 0, 0 }, { 2800, -3760, 0, 0 } }, { 0, 0, 4097, 0 }, { 56, 18 }, 1, 0 },
    { NULL, NULL, { 6400, -1088, -8384, 0 }, { { -2800, 1776, 0, 0 }, { 2800, 1776, 0, 0 }, { -2800, 208, 0, 0 }, { 2800, -3760, 0, 0 } }, { 0, 0, 4097, 0 }, { 56, 18 }, 1, 0 },
    { NULL, NULL, { 1536, -1984, -5744, 0 }, { { -5776, 0, 4448, 0 }, { 5776, 0, 4448, 0 }, { -5776, 0, -2464, 0 }, { 5776, 0, -6432, 0 } }, { 0, -4110, 0, 0 }, { -76, 33 }, 129, 0 },
};

s32 D_neo_ark_pyramid_80181844[3] = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

s32 D_neo_ark_pyramid_80181850[3] = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

GpRoomParamRec D_neo_ark_pyramid_8018185C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_neo_ark_pyramid_80181864[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_neo_ark_pyramid_8018186C[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_neo_ark_pyramid_80181874[1] = {
    { 0, 0, 1, 0, D_neo_ark_pyramid_80181844 },
};

GpRoomParamRec D_neo_ark_pyramid_8018187C[1] = {
    { 0, 0, 1, 0, D_neo_ark_pyramid_80181850 },
};

GpRoomParamRec* D_neo_ark_pyramid_80181884[8] = {
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
            Mc_SaveData[0].state.at4.loc.view = 8;
            gGameSession->hideHud             = 1;
            gGameSession->eventState          = 1;
            Gp_StateF0.field_4                = 2;
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
            SndEvt_EnqueueType6(0x55200003, 0, 0);
            task->killCountdown = 0;
            task->state++;
            break;
        case 4:
            count               = task->killCountdown + 4;
            task->killCountdown = count;
            if ((s16)count >= 0x156) {
                GameFlag_SetNibble(0xEC, GameFlag_GetNibble(0xEC) + 1);
                func_neo_ark_pyramid_8017DAC0(0);
                if (GameFlag_GetNibble(0xEC) >= 4) {
                    SndEvt_EnqueueType6(0x55200005, 0, 0);
                    Gp_RunCapCmd(2, 0);
                    task->state++;
                } else {
                    SndEvt_EnqueueType6(0x55200004, 0, 0);
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
            Mc_SaveData[0].state.at4.loc.view = 3;
            gGameSession->hideHud             = 0;
            gGameSession->eventState          = 0;
            Gp_StateF0.field_4                = 0;
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
s32 func_neo_ark_pyramid_8017D9F0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
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
s32 func_neo_ark_pyramid_8017DA3C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EF in the room's message table. When the message's
/// `actionId` is 1 it resets the quad's angle; once the quad has turned four
/// times it spawns capture event 3, otherwise it has the player lower the
/// weapon and starts the task that turns the quad another step.
s32 func_neo_ark_pyramid_8017DA44(Task* task, s32 msgId, DirectionActionRequest* request, GpMessageArg arg3)
{
    if (request->actionId == 1) {
        func_neo_ark_pyramid_8017DAC0(0);
        if (GameFlag_GetNibble(0xEC) == 4) {
            Gp_SpawnIfCapIdle(3, 1);
        } else {
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            Task_SpawnFromTable(&D_neo_ark_pyramid_8017FC0C, 0, 0, 0);
        }
    }
    return 0;
}

/// Sets the quad's angle to twelfths of a turn counted by game-flag nibble
/// 0xEC, less four, plus the in-progress sweep `arg0`.
static void func_neo_ark_pyramid_8017DAC0(s32 arg0)
{
    D_neo_ark_pyramid_801818A4 = (((GameFlag_GetNibble(0xEC) - 4) << 0xC) / 12) + arg0;
}

/// State 0 of the room's entry task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7 and advances.
static void func_neo_ark_pyramid_8017DB18(Task* task)
{
    task->msgTable = D_neo_ark_pyramid_8017FBE4;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's entry task: draws the rotating quad at its current
/// angle while the session's view is 8.
static void func_neo_ark_pyramid_8017DB5C(Task* task)
{
    if (gGameSession->at4.loc.view == 8) {
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
/// three gameplay globals and sets `Gp_State1C` room effect mode 2.
void func_neo_ark_pyramid_8017DBF0(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758                 = 0x601E2;
        D_8011572C                 = 0x601FE;
        D_80115750                 = 0x6021A;
        Gp_State1C->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
        arg0->state                = 1;
    }
}

/// `Gp_State1C` effect task for a flash that builds up and bursts. Over
/// `spawnArg1` frames it ramps a brightness from zero to 0x100, drawing two
/// wedge discs and a ring in the colour `(b, b/4, b/2)`; on the last frame it
/// flashes the screen with that colour, then draws the star-shaped glow
/// while dimming by 0x10 a frame, and releases the effect once the brightness
/// is spent. Once the room's event state leaves zero it only waits for state 4
/// to release.
void func_neo_ark_pyramid_8017DC50(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1.value;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_neo_ark_pyramid_8017E320(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_neo_ark_pyramid_8017E320(coord, (s16)((u16)work->angle * 2), rgb);
                func_neo_ark_pyramid_8017DEF4(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_neo_ark_pyramid_8017F224(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Draws a ring of sixteen gouraud quads around the coordinate's projected
/// position, when it projects. The ring runs from radius
/// `(s16)arg1 * 64 / (otz + 1)`, which is black, to
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`, which takes the colour `rgb`.
static void func_neo_ark_pyramid_8017DEF4(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s16                blackRadius = arg1;
    s16                tintRadius  = arg1 + arg2;

    block         = SCRATCH_PUSH(RoomDraw02Scratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (blackRadius * 64) / block->otz;
        block->rInner = (tintRadius * 64) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomDraw02Scratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues eight gouraud `POLY_G4` wedges around
/// the projected centre. `arg1` is a signed half-extent; the on-screen radius
/// is `arg1 * 64 / (otz + 1)`. Only the centre vertex takes the colour
/// `rgb`, so each wedge fades to black at the rim.
static void func_neo_ark_pyramid_8017E320(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomFanScratch);
}

/// `Gp_State1C` effect task drawing a ribbon between the trails of two points
/// on the spawner's parent coordinate. The first frame allocates sixteen
/// coordinates, places the effect at the first offset, and fills both
/// eight-slot trails with the two points' current view positions. Each later
/// frame records the two points into the next slot, rebuilds all sixteen,
/// draws the ribbon through `func_neo_ark_pyramid_8017EBA4` and releases the
/// effect once its age reaches `spawnArg1`. It stops advancing once the room's
/// event state reaches 2.
void func_neo_ark_pyramid_8017E6B4(Task* task)
{
    GfxCoord   coord;
    GfxCoord*  coords;
    GfxCoord*  objCoord;
    GfxCoord*  dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = task->work;
    work     = (GpEffWork*)task->spawnArg2.pointer;
    objCoord = task->extra.coordBody->coord;

    if (Gp_State1C->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = memCalloc(sizeof(GfxCoord[16]), 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work             = coords;
                objCoord->parent       = work->parent;
                objCoord->coord.t[0]   = D_neo_ark_pyramid_8017FC18[0].vx;
                objCoord->coord.t[1]   = D_neo_ark_pyramid_8017FC18[0].vy;
                objCoord->coord.t[2]   = D_neo_ark_pyramid_8017FC18[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_neo_ark_pyramid_8017FC18[1];
                coord.coord.t[0]   = vec->vx;
                coord.coord.t[1]   = vec->vy;
                coord.coord.t[2]   = vec->vz;
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst         = &coords[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst         = &coords[i + 8];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                coord.parent = work->parent;
                {
                    SVECTOR* edge    = &D_neo_ark_pyramid_8017FC18[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                dst         = &coords[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst         = &coords[(work->age & 7) + 8];
                dst->parent = &gGfxViewCoord;
                dst->workm  = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst               = &coords[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                    dst               = &coords[i + 8];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                }
                func_neo_ark_pyramid_8017EBA4(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws a ribbon between two eight-slot coordinate trails as seven gouraud
/// `POLY_G4` quads, walking backwards from slot `arg2`. Each quad spans
/// `workm.t` of two adjacent slots on `arg0` and `arg1`; its newer edge is
/// weighted `0x40 - 9 * i` and its older edge nine less, so the ribbon fades
/// along its length. `arg3` holds the colour as per-channel multipliers of that
/// weight: red from bits 8 up, green from bits 4-5, blue from bits 0-1. A quad
/// the GTE flags as bad is skipped.
static void func_neo_ark_pyramid_8017EBA4(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GfxCoord*          a;
    GfxCoord*          b;
    POLY_G4*           prim;
    s32                i;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    blk = SCRATCH_PUSH(RoomDraw03Scratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        i0           = (arg2 - i) & 7;
        i1           = (arg2 - i - 1) & 7;
        a            = &arg0[i0];
        blk->v[0].vx = a->workm.t[0];
        blk->v[0].vy = a->workm.t[1];
        blk->v[0].vz = a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = b->workm.t[0];
        blk->v[1].vy = b->workm.t[1];
        blk->v[1].vz = b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = a->workm.t[0];
        blk->v[2].vy = a->workm.t[1];
        blk->v[2].vz = a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = b->workm.t[0];
        blk->v[3].vy = b->workm.t[1];
        blk->v[3].vz = b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            blk->otz       = blk->otz + 1;
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            lo             = (fade - 9) & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            b2             = lo * (arg3 & 3);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_STACK_RELEASE_BLOCK(RoomDraw03Scratch);
}

/// `Gp_State1C` effect task for a burst. The first frame spawns effect
/// `0x60076` at the coordinate and then, depending on `spawnArg1`, either
/// effect `0x60070` followed by seven frames of randomly aimed `0x60070`
/// sparks, or two `0x6007C` effects followed by seven frames of two expanding
/// rings in a fading `(a, a/2, a/4)` colour. The effect is then released.
/// Once the room's event state leaves zero it only waits for state 4 to
/// release.
void func_neo_ark_pyramid_8017EF9C(Task* task)
{
    GfxCoord*  objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.coordBody->coord;
    work     = (GpEffWork*)task->spawnArg2.pointer;

    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1.value != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_neo_ark_pyramid_8017DEF4(objCoord, 0x100, 0x100, rgb);
            func_neo_ark_pyramid_8017DEF4(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, draws a star-shaped glow from gouraud
/// `POLY_G4` wedges that are coloured only at the centre and fade to black.
/// `arg1` is a signed half-extent giving an outer radius
/// `arg1 * 64 / (otz + 1)` and an inner one `arg1 * 8 / (otz + 1)`. Eight
/// wedges span the outer radius in half the colour `arg2`, eight more span
/// half of it at full colour, and four long spikes reach twice the outer
/// radius between points on the inner one.
static void func_neo_ark_pyramid_8017F224(GfxCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomBillboardScratch);
}
