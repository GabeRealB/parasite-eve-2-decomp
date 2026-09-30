#include "rooms/dryfield_night_underpass.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"

extern TaskDesc   D_dryfield_night_underpass_8017DCD8[];
extern GpMsgEntry D_dryfield_night_underpass_8017DCF0[];
extern SVECTOR    D_dryfield_night_underpass_8017DD20[8];
extern s16        D_dryfield_night_underpass_8017DD60[8];

void func_dryfield_night_underpass_8017D5D0(Task*);
s32  func_dryfield_night_underpass_8017D788(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_dryfield_night_underpass_8017D868(Task*, s32, s32, GpMessageArg);
s32  func_dryfield_night_underpass_8017D8CC(Task*, s32, s32, s32);
s32  func_dryfield_night_underpass_8017D900(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_dryfield_night_underpass_8017D908(Task*, s32, GpMessageArg, GpMessageArg);

extern GpGridParams   D_dryfield_night_underpass_8017E6D4[1];
extern GpObj3A        D_dryfield_night_underpass_8017FBE0[3];
extern GpObj4C        D_dryfield_night_underpass_8017F558[16];
extern GpObj4C        D_dryfield_night_underpass_8017FA18[6];
extern GpRoomCoordSet D_dryfield_night_underpass_8017FFF4[1];
extern GpRoomCoordSet D_dryfield_night_underpass_8018024C[1];

TaskDesc D_dryfield_night_underpass_8017DCD8[2] = {
    { 0, 32, func_dryfield_night_underpass_8017D5D0, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpMsgEntry D_dryfield_night_underpass_8017DCF0[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_underpass_8017D788 },
    { 5105, func_dryfield_night_underpass_8017D900 },
    { 5103, func_dryfield_night_underpass_8017D908 },
    { 5104, func_dryfield_night_underpass_8017D868 },
    { 5106, func_dryfield_night_underpass_8017D8CC },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_dryfield_night_underpass_8017DD20[8] = {
    { 0x4588, -3480, -1500, 0 },
    { 0x3B60, -3480, -6000, 0 },
    { 0x4588, -3480, -8700, 0 },
    { 0x34BC, -3480, -7200, 0 },
    { 9300, -3480, -9800, 0 },
    { 6200, -3480, -0x29CC, 0 },
    { 4850, -3420, -7200, 0 },
    { 500, -3480, -9700, 0 },
};

s16 D_dryfield_night_underpass_8017DD60[8] = {
    2052,
    8,
    536,
    528,
    16,
    64,
    32,
    160,
};

GpRoomObjRec D_dryfield_night_underpass_8017DD70[6] = {
    { D_dryfield_night_underpass_8017E6D4, D_dryfield_night_underpass_8017F558, D_dryfield_night_underpass_8017FA18, D_dryfield_night_underpass_8017FBE0 },
    { D_dryfield_night_underpass_8017E6D4, D_dryfield_night_underpass_8017F558, D_dryfield_night_underpass_8017FA18, D_dryfield_night_underpass_8017FBE0 },
    { D_dryfield_night_underpass_8017E6D4, D_dryfield_night_underpass_8017F558, D_dryfield_night_underpass_8017FA18, D_dryfield_night_underpass_8017FBE0 },
    { D_dryfield_night_underpass_8017E6D4, D_dryfield_night_underpass_8017F558, D_dryfield_night_underpass_8017FA18, D_dryfield_night_underpass_8017FBE0 },
    { D_dryfield_night_underpass_8017E6D4, D_dryfield_night_underpass_8017F558, D_dryfield_night_underpass_8017FA18, D_dryfield_night_underpass_8017FBE0 },
    { D_dryfield_night_underpass_8017E6D4, D_dryfield_night_underpass_8017F558, D_dryfield_night_underpass_8017FA18, D_dryfield_night_underpass_8017FBE0 },
};

GpRoomCoordRec D_dryfield_night_underpass_8017DDD0[6] = {
    { D_dryfield_night_underpass_8017FFF4, NULL },
    { D_dryfield_night_underpass_8018024C, NULL },
    { D_dryfield_night_underpass_8017FFF4, NULL },
    { D_dryfield_night_underpass_8018024C, NULL },
    { D_dryfield_night_underpass_8017FFF4, NULL },
    { D_dryfield_night_underpass_8017FFF4, NULL },
};

u8 D_dryfield_night_underpass_8017DE00[12] = {
    1,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    26,
    0,
};

u8 D_dryfield_night_underpass_8017DE0C[12] = {
    1,
    11,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

u8 D_dryfield_night_underpass_8017DE18[12] = {
    1,
    21,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    26,
    0,
};

u8 D_dryfield_night_underpass_8017DE24[12] = {
    1,
    22,
    23,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

u8 D_dryfield_night_underpass_8017DE30[12] = {
    1,
    24,
    25,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

u8* D_dryfield_night_underpass_8017DE3C[6] = {
    D_8010CAF8,
    D_dryfield_night_underpass_8017DE00,
    D_dryfield_night_underpass_8017DE0C,
    D_dryfield_night_underpass_8017DE18,
    D_dryfield_night_underpass_8017DE24,
    D_dryfield_night_underpass_8017DE30,
};

GpViewCountRec D_dryfield_night_underpass_8017DE54[6] = {
    { { .bytes = { 26, 0 } } },
    { { .bytes = { 11, 0 } } },
    { { .bytes = { 11, 0 } } },
    { { .bytes = { 11, 0 } } },
    { { .bytes = { 11, 0 } } },
    { { .bytes = { 11, 0 } } },
};

GpWarpRec D_dryfield_night_underpass_8017DE60[3] = {
    { { .words = { 0, 5303, -997, -0x2C94 } }, { 0, 0, 0, 0 }, { .words = { 0, 5303, -997, -0x2C94 } }, { 0, 0, 0, 0 }, 0x53260008, 0x53260007, 0, 6, 0, 463 },
    { { .words = { 1024, 0x3D31, -1000, -4216 } }, { 0, 0, 0, 0 }, { .words = { 1024, 0x3D31, -1000, -4216 } }, { 0, 0, 0, 0 }, 0x53260006, 0x53260005, 0, 2, 0, 462 },
    { { .words = { 2048, 1212, -1000, -7543 } }, { 0, 0, 0, 0 }, { .words = { 2048, 1212, -1000, -7543 } }, { 0, 0, 0, 0 }, 0x53260001, 0x53260001, 0, 7, 2, 0 },
};

SVECTOR D_dryfield_night_underpass_8017DF08[12] = {
#include "assets/dryfield_night_underpass_collision_01114_normals.inc"
};

SVECTOR D_dryfield_night_underpass_8017DF68[70] = {
#include "assets/dryfield_night_underpass_collision_01114_verts.inc"
};

GpGridFace D_dryfield_night_underpass_8017E198[46] = {
#include "assets/dryfield_night_underpass_collision_01114_faces.inc"
};

s16 D_dryfield_night_underpass_8017E3C0[346] = {
#include "assets/dryfield_night_underpass_collision_01114_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_underpass_8017E3C0[i])
s16* D_dryfield_night_underpass_8017E674[24] = {
#include "assets/dryfield_night_underpass_collision_01114_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_underpass_8017E6D4[1] = {
    { NULL, D_dryfield_night_underpass_8017DF08, D_dryfield_night_underpass_8017DF68, D_dryfield_night_underpass_8017E198, D_dryfield_night_underpass_8017E674, 3000, 0x32C8, 6, 4, 4000, 46 },
};

GpViewRec D_dryfield_night_underpass_8017E6F8[26] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7864, 0x6671, 5100 } }, 329 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { -4007, 0, 848 }, { -132, 4045, -626 }, { -837, -639, -3958 } }, { -0x42CB, 1690, 3166 } }, 230 },
    { { { { 998, 0, -3972 }, { 996, 3964, 250 }, { 3845, -1027, 966 } }, { -6656, 1455, 9182 } }, 230 },
    { { { { 994, 0, 3973 }, { -803, 4011, 201 }, { -3891, -828, 974 } }, { -0x2AAE, 1498, 9252 } }, 230 },
    { { { { -3946, 0, 1096 }, { -240, 3996, -865 }, { -1069, -898, -3850 } }, { -6279, 1492, 7705 } }, 230 },
    { { { { 951, 0, 3983 }, { -763, 4020, 182 }, { -3910, -784, 934 } }, { -5505, 1532, 9129 } }, 230 },
    { { { { 1678, 0, 3736 }, { 2611, 2928, -1173 }, { -2671, 2863, 1200 } }, { -444, 3936, 9337 } }, 230 },
    { { { { 1312, 0, -3879 }, { 1085, 3932, 367 }, { 3724, -1146, 1260 } }, { -0x293A, 1292, 9364 } }, 230 },
    { { { { 3773, 0, 1592 }, { 1116, 2920, -2645 }, { -1135, 2871, 2690 } }, { -0x43B0, 3973, 2327 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { -4007, 0, 848 }, { -132, 4045, -626 }, { -837, -639, -3958 } }, { -0x42CB, 1690, 3166 } }, 230 },
    { { { { 998, 0, -3972 }, { 996, 3964, 250 }, { 3845, -1027, 966 } }, { -6656, 1455, 9182 } }, 230 },
    { { { { 994, 0, 3973 }, { -803, 4011, 201 }, { -3891, -828, 974 } }, { -0x2AAE, 1498, 9252 } }, 230 },
    { { { { -3946, 0, 1096 }, { -240, 3996, -865 }, { -1069, -898, -3850 } }, { -6279, 1492, 7705 } }, 230 },
    { { { { 951, 0, 3983 }, { -763, 4020, 182 }, { -3910, -784, 934 } }, { -5505, 1532, 9129 } }, 230 },
    { { { { 1678, 0, 3736 }, { 2611, 2928, -1173 }, { -2671, 2863, 1200 } }, { -444, 3936, 9337 } }, 230 },
    { { { { 1312, 0, -3879 }, { 1085, 3932, 367 }, { 3724, -1146, 1260 } }, { -0x293A, 1292, 9364 } }, 230 },
    { { { { 3773, 0, 1592 }, { 1116, 2920, -2645 }, { -1135, 2871, 2690 } }, { -0x43B0, 3973, 2327 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { 4042, 0, 659 }, { 359, 3434, -2202 }, { -553, 2231, 3389 } }, { -0x421F, 3970, 8993 } }, 230 },
    { { { { 4075, 0, 405 }, { 186, 3640, -1868 }, { -360, 1877, 3622 } }, { -0x4162, 3823, 5117 } }, 230 },
    { { { { 4042, 0, 659 }, { 359, 3434, -2202 }, { -553, 2231, 3389 } }, { -0x421F, 3970, 8993 } }, 230 },
    { { { { 4075, 0, 405 }, { 186, 3640, -1868 }, { -360, 1877, 3622 } }, { -0x4162, 3823, 5117 } }, 230 },
    { { { { -4047, 0, 631 }, { -451, 2858, -2898 }, { -440, -2933, -2824 } }, { -0x4229, 2000, 6853 } }, 348 },
};

SpriteBatch D_dryfield_night_underpass_8017EAA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017EAB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_underpass_8017EAC0[16] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 48, -120, 1016, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 80, 964, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, -120, 1000, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, -120, 1000, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, 24, 975, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, 24, 975, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 88, -24, 975, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -24, 975, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -64, 975, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, -64, 975, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 80, 975, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 80, 975, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, 32, 993, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, -64, 1005, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -24, 1032, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 8, 1014, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017EC00[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_underpass_8017EC18[11] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 96, 2125, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -24, 2129, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, 56, 1860, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, 48, 2000, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -16, 16, 2058, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 48 } }, -96, -72, 2125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -96, -120, 2125, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 120 } }, -96, -24, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, 40, 2125, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 80 } }, -160, -120, 2125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, -40, 2125, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017ECF4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_underpass_8017ED0C[10] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -120, 910, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -48, 913, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -96, 995, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, 8, 901, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 56, 896, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -160, -120, 897, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, -48, 897, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, 8, 897, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 56, 897, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 897, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017EDD4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017EDF4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017EE04[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017EE14[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_underpass_8017EE24[11] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -72, -112, 1325, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -120, -120, 1325, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, -120, 1325, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -56, -16, 1197, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, -64, 1250, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 24, 1162, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 56, 1133, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 80 } }, -112, -64, 1175, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 80 } }, -160, -64, 1175, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 88 } }, -112, 16, 1125, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -160, 16, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017EF00[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017EF20[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017EF30[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017EF40[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_underpass_8017EF50[16] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 48, -120, 1016, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 80, 964, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, -120, 1000, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, -120, 1000, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, 24, 975, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, 24, 975, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 88, -24, 975, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -24, 975, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -64, 975, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, -64, 975, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 80, 975, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 80, 975, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, 32, 993, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, -64, 1005, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -24, 1032, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 8, 1014, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017F090[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_underpass_8017F0A8[11] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 96, 2125, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -24, 2129, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, 56, 1860, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, 48, 2000, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -16, 16, 2058, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 48 } }, -96, -72, 2125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -96, -120, 2125, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 120 } }, -96, -24, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, 40, 2125, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 80 } }, -160, -120, 2125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, -40, 2125, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017F184[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_underpass_8017F19C[10] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -120, 910, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -48, 913, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -96, 995, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, 8, 901, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 56, 896, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -160, -120, 897, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, -48, 897, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, 8, 897, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 56, 897, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 897, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017F264[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F284[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F294[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F2A4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_underpass_8017F2B4[11] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -72, -112, 1325, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -120, -120, 1325, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, -120, 1325, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -56, -16, 1197, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, -64, 1250, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 24, 1162, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 56, 1133, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 80 } }, -112, -64, 1175, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 80 } }, -160, -64, 1175, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 88 } }, -112, 16, 1125, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -160, 16, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_underpass_8017F390[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F3B0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F3C0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F3D0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F3E0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F3F0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F400[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_underpass_8017F410[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_underpass_8017F420[26] = {
    { { .empty = D_dryfield_night_underpass_8017EAA0 }, D_dryfield_night_underpass_8017EAA0, NULL },
    { { .empty = D_dryfield_night_underpass_8017EAB0 }, D_dryfield_night_underpass_8017EAB0, NULL },
    { { .elements = D_dryfield_night_underpass_8017EAC0 }, D_dryfield_night_underpass_8017EC00, NULL },
    { { .elements = D_dryfield_night_underpass_8017EC18 }, D_dryfield_night_underpass_8017ECF4, NULL },
    { { .elements = D_dryfield_night_underpass_8017ED0C }, D_dryfield_night_underpass_8017EDD4, NULL },
    { { .empty = D_dryfield_night_underpass_8017EDF4 }, D_dryfield_night_underpass_8017EDF4, NULL },
    { { .empty = D_dryfield_night_underpass_8017EE04 }, D_dryfield_night_underpass_8017EE04, NULL },
    { { .empty = D_dryfield_night_underpass_8017EE14 }, D_dryfield_night_underpass_8017EE14, NULL },
    { { .elements = D_dryfield_night_underpass_8017EE24 }, D_dryfield_night_underpass_8017EF00, NULL },
    { { .empty = D_dryfield_night_underpass_8017EF20 }, D_dryfield_night_underpass_8017EF20, NULL },
    { { .empty = D_dryfield_night_underpass_8017EF30 }, D_dryfield_night_underpass_8017EF30, NULL },
    { { .empty = D_dryfield_night_underpass_8017EF40 }, D_dryfield_night_underpass_8017EF40, NULL },
    { { .elements = D_dryfield_night_underpass_8017EF50 }, D_dryfield_night_underpass_8017F090, NULL },
    { { .elements = D_dryfield_night_underpass_8017F0A8 }, D_dryfield_night_underpass_8017F184, NULL },
    { { .elements = D_dryfield_night_underpass_8017F19C }, D_dryfield_night_underpass_8017F264, NULL },
    { { .empty = D_dryfield_night_underpass_8017F284 }, D_dryfield_night_underpass_8017F284, NULL },
    { { .empty = D_dryfield_night_underpass_8017F294 }, D_dryfield_night_underpass_8017F294, NULL },
    { { .empty = D_dryfield_night_underpass_8017F2A4 }, D_dryfield_night_underpass_8017F2A4, NULL },
    { { .elements = D_dryfield_night_underpass_8017F2B4 }, D_dryfield_night_underpass_8017F390, NULL },
    { { .empty = D_dryfield_night_underpass_8017F3B0 }, D_dryfield_night_underpass_8017F3B0, NULL },
    { { .empty = D_dryfield_night_underpass_8017F3C0 }, D_dryfield_night_underpass_8017F3C0, NULL },
    { { .empty = D_dryfield_night_underpass_8017F3D0 }, D_dryfield_night_underpass_8017F3D0, NULL },
    { { .empty = D_dryfield_night_underpass_8017F3E0 }, D_dryfield_night_underpass_8017F3E0, NULL },
    { { .empty = D_dryfield_night_underpass_8017F3F0 }, D_dryfield_night_underpass_8017F3F0, NULL },
    { { .empty = D_dryfield_night_underpass_8017F400 }, D_dryfield_night_underpass_8017F400, NULL },
    { { .empty = D_dryfield_night_underpass_8017F410 }, D_dryfield_night_underpass_8017F410, NULL },
};

GpObj4C D_dryfield_night_underpass_8017F558[16] = {
    { NULL, NULL, NULL, { 0x4070, -2560, -5280, 0 }, { { -1616, -1888, 0, 0 }, { 1616, -1888, 0, 0 }, { -1616, 1888, 0, 0 }, { 1616, 1888, 0, 0 } }, { 0, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x40C0, -2544, -5408, 0 }, { { 1616, -1840, 0, 0 }, { -1616, -1840, 0, 0 }, { 1616, 1840, 0, 0 }, { -1616, 1840, 0, 0 } }, { 0, 0, 4105, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x3A3F, -2720, -8402, 0 }, { { 65, -1824, -1727, 0 }, { -64, -1824, 1728, 0 }, { 65, 1824, -1727, 0 }, { -64, 1824, 1728, 0 } }, { 4101, 0, 151, 0 }, { 0, 0, 4096, 0 }, 2508, 0, 3, 9, 1, 0 },
    { NULL, NULL, NULL, { 0x3ABE, -2736, -8307, 0 }, { { -81, -1808, 1758, 0 }, { 81, -1808, -1757, 0 }, { -81, 1808, 1758, 0 }, { 81, 1808, -1757, 0 } }, { -4096, 0, -190, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 9, 3, 1, 0 },
    { NULL, NULL, NULL, { 9023, -2688, -8449, 0 }, { { 0, -1824, 1615, 0 }, { 1, -1824, -1616, 0 }, { 0, 1824, 1615, 0 }, { 1, 1824, -1616, 0 } }, { -4102, 0, -2, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 8799, -2736, -8577, 0 }, { { -79, -1808, -1614, 0 }, { 78, -1808, 1613, 0 }, { -79, 1808, -1614, 0 }, { 78, 1808, 1613, 0 } }, { 4091, 0, -200, 0 }, { 0, 0, 4096, 0 }, 2415, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 5214, -2656, -0x27A2, 0 }, { { 1616, -1824, 1, 0 }, { -1615, -1824, 0, 0 }, { 1616, 1824, 1, 0 }, { -1615, 1824, 0, 0 } }, { -2, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 5150, -2624, -9954, 0 }, { { -1615, -1888, 0, 0 }, { 1616, -1888, 1, 0 }, { -1615, 1888, 0, 0 }, { 1616, 1888, 1, 0 } }, { 0, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 3230, -2624, -8482, 0 }, { { -207, -1792, -2412, 0 }, { 207, -1792, 2413, 0 }, { -207, 1792, -2412, 0 }, { 207, 1792, 2413, 0 } }, { 4094, 0, -353, 0 }, { 0, 0, 4096, 0 }, 3007, 0, 5, 7, 1, 0 },
    { NULL, NULL, NULL, { 3454, -2624, -8513, 0 }, { { 159, -1856, 2416, 0 }, { -159, -1856, -2415, 0 }, { 159, 1856, 2416, 0 }, { -159, 1856, -2415, 0 } }, { -4100, 0, 269, 0 }, { 0, 0, 4096, 0 }, 3050, 0, 7, 5, 1, 0 },
    { NULL, NULL, NULL, { -960, -2592, -8577, 0 }, { { 306, -1856, 1579, 0 }, { -322, -1856, -1589, 0 }, { 306, 1856, 1579, 0 }, { -322, 1856, -1589, 0 } }, { -4028, 0, 798, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 8, 7, 1, 0 },
    { NULL, NULL, NULL, { -1056, -2528, -8385, 0 }, { { -322, -1856, -1592, 0 }, { 308, -1856, 1578, 0 }, { -322, 1856, -1592, 0 }, { 308, 1856, 1578, 0 } }, { 4028, 0, -802, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 0x3130, -2592, -8513, 0 }, { { 17, -1824, 1905, 0 }, { -16, -1824, -1904, 0 }, { 17, 1824, 1905, 0 }, { -16, 1824, -1904, 0 } }, { -4104, 0, 35, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 4, 9, 1, 0 },
    { NULL, NULL, NULL, { 0x3061, -2528, -8577, 0 }, { { 1, -1824, -1615, 0 }, { 0, -1824, 1616, 0 }, { 1, 1824, -1615, 0 }, { 0, 1824, 1616, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 9, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x40C0, -2624, -1029, 0 }, { { 1577, -1840, 182, 0 }, { -1630, -1840, -237, 0 }, { 1577, 1840, 182, 0 }, { -1630, 1840, -237, 0 } }, { -534, 0, 4073, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 10, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x40F1, -2624, -962, 0 }, { { -1854, -1840, -209, 0 }, { 1834, -1840, 184, 0 }, { -1854, 1840, -209, 0 }, { 1834, 1840, 184, 0 } }, { 434, 0, -4078, 0 }, { 0, 0, 4096, 0 }, 2610, 0, 2, 10, 129, 0 },
};

GpObj4C D_dryfield_night_underpass_8017FA18[6] = {
    { NULL, NULL, NULL, { 5088, -1088, -0x2C70, 0 }, { { -1024, 0, -432, 0 }, { 1024, 0, -432, 0 }, { -1024, 0, 432, 0 }, { 1024, 0, 432, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1108, 0, 34, 17, 2, 0 },
    { NULL, NULL, NULL, { 1600, -1056, -7424, 0 }, { { -832, 0, -592, 0 }, { 768, 0, -592, 0 }, { -832, 0, 432, 0 }, { 768, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1019, 0, 3, 51, 2, 0 },
    { NULL, NULL, NULL, { 0x3B40, -1088, -3904, 0 }, { { 432, 0, -1024, 0 }, { 432, 0, 1024, 0 }, { -432, 0, -1024, 0 }, { -432, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1108, 0, 32, 34, 2, 0 },
    { NULL, NULL, NULL, { -1616, -1056, -8464, 0 }, { { -432, 0, -960, 0 }, { 432, 0, -960, 0 }, { -432, 0, 960, 0 }, { 432, 0, 960, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1047, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { 0x40E0, -1056, -576, 0 }, { { -1024, 0, -304, 0 }, { 1024, 0, -304, 0 }, { -1024, 0, 304, 0 }, { 1024, 0, 304, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1063, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 1648, -1056, -7184, 0 }, { { -1008, 0, -544, 0 }, { 1008, 0, -544, 0 }, { -1008, 0, 544, 0 }, { 1008, 0, 544, 0 } }, { 0, 4121, 0, 0 }, { 0, 0, -4096, 0 }, 1144, 0, 3, 51, 132, 0 },
};

GpObj3A D_dryfield_night_underpass_8017FBE0[3] = {
    { NULL, NULL, { 1184, -2304, -0x2840, 0 }, { { -2912, -3328, 0, 0 }, { 2912, -3328, 0, 0 }, { -2912, 3328, 0, 0 }, { 2912, 3328, 0, 0 } }, { 0, 0, -4106, 0 }, { 52, 17 }, 1, 0 },
    { NULL, NULL, { 0x2FB0, -2240, -0x2820, 0 }, { { -5840, -3264, 0, 0 }, { 5840, -3264, 0, 0 }, { -5840, 3264, 0, 0 }, { 5840, 3264, 0, 0 } }, { 0, 0, -4111, 0 }, { 19, 26 }, 1, 0 },
    { NULL, NULL, { 8992, -3328, -4416, 0 }, { { -5840, -2848, 2496, 0 }, { 5840, -2848, -2496, 0 }, { -5840, 2848, 2496, 0 }, { 5840, 2848, -2496, 0 } }, { -1614, 0, -3777, 0 }, { 33, 27 }, 129, 0 },
};

GpPointLight D_dryfield_night_underpass_8017FC94[9] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3214, -1973, -3920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1966, 2621, 3276, { 0, 0 } }, 2500, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x44C0, -3483, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3C28, -3483, -6000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x44C0, -3483, -8500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9300, -3483, -9600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x34BC, -3483, -7400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6200, -3483, -0x2904 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 500, -3483, -9500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4850, -3423, -7400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
};

GpRoomCoordSet D_dryfield_night_underpass_8017FFF4[1] = {
    { 0, NULL, 9, D_dryfield_night_underpass_8017FC94, 0, NULL },
};

GpPointLight D_dryfield_night_underpass_8018000C[6] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4268, -2483, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1720, 2088, 2457, { 0, 0 } }, 0, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 500, -2483, -8500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1720, 2088, 2457, { 0, 0 } }, 0, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3F48, -2483, -8000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1720, 2088, 2457, { 0, 0 } }, 0, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2AF8, -2483, -8300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1720, 2088, 2457, { 0, 0 } }, 0, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5850, -2423, -9100 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1720, 2088, 2457, { 0, 0 } }, 0, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x314C, -1973, -3920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 4096, 4096, { 0, 0 } }, 0, 8000 },
};

GpRoomCoordSet D_dryfield_night_underpass_8018024C[1] = {
    { 0, NULL, 6, D_dryfield_night_underpass_8018000C, 0, NULL },
};

GpAreaTmdRec D_dryfield_night_underpass_80180264[2] = {
    { 5, 5, 3, 0, { 0, 0 }, D_80153D60 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_underpass_8018027C[2] = {
    { 16, 16, 0, 0, { 0, 0 }, D_801445DC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_underpass_80180294[2] = {
    { 11, 11, 0, 0, { 0, 0 }, D_80147400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_underpass_801802AC[2] = {
    { 22, 22, 3, 0, { 0, 0 }, D_80154188 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_underpass_801802C4[2] = {
    { 15, 15, 0, 0, { 0, 0 }, D_8013BE28 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_underpass_801802DC[12] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017CF58, D_dryfield_night_underpass_80180264 },
    { D_map_dryfield_full_8017CF78, D_dryfield_night_underpass_8018027C },
    { D_map_dryfield_full_8017CFD8, D_dryfield_night_underpass_80180294 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017D018, D_dryfield_night_underpass_801802AC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017D038, D_dryfield_night_underpass_801802C4 },
};

s32 D_dryfield_night_underpass_8018033C[3] = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

s32 D_dryfield_night_underpass_80180348[3] = {
    0x10000035,
    0x10000037,
    0x10000035,
};

GpRoomParamRec D_dryfield_night_underpass_80180354[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_underpass_8018035C[1] = {
    { 0, 0, 1, 0, D_dryfield_night_underpass_8018033C },
};

GpRoomParamRec D_dryfield_night_underpass_80180364[1] = {
    { 0, 0, 1, 0, D_dryfield_night_underpass_8018033C },
};

GpRoomParamRec D_dryfield_night_underpass_8018036C[1] = {
    { 0, 0, 1, 0, D_dryfield_night_underpass_80180348 },
};

GpRoomParamRec* D_dryfield_night_underpass_80180374[8] = {
    D_dryfield_night_underpass_80180354,
    D_dryfield_night_underpass_80180354,
    D_dryfield_night_underpass_80180364,
    D_dryfield_night_underpass_8018036C,
    D_dryfield_night_underpass_8018035C,
    D_dryfield_night_underpass_80180354,
    D_dryfield_night_underpass_80180354,
    D_dryfield_night_underpass_80180354,
};

static void func_dryfield_night_underpass_8017D910(Task* task);
static void func_dryfield_night_underpass_8017D954(Task* task);
static void func_dryfield_night_underpass_8017D9B4(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Switch task the room's 0x13F0 handler spawns: plays cap command `spawnArg2`,
/// waits for it to finish, and once its event key reaches 0xA toggles game
/// nibble `spawnArg1`. When that nibble is 0x51 it also picks the room variant
/// to load next from nibbles 0xC9, 0x53 and 0x51 and writes it to both the
/// session and the save data. The last state flags the view dirty when the
/// chosen room is 5 or above, then kills the task.
void func_dryfield_night_underpass_8017D5D0(Task* task)
{
    RoomEventMsg  src;
    RoomEventMsg  dst;
    RoomEventMsg* s;
    RoomEventMsg* d;
    GameSession*  session;
    s32           flag;
    s32           state;
    s32           arg;
    u8            room;

    flag  = task->spawnArg1.value;
    state = task->state;
    arg   = task->spawnArg2.value;
    switch (state) {
        case 0:
            Gp_RunCapCmd1(arg);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 2:
            if (Gp_GetCapEventKey() >= 0xA) {
                GameFlag_SetNibble(flag, GameFlag_GetNibble(flag) == 0);
                if (flag == 0x51) {
                    d             = &dst;
                    s             = &src;
                    src.areaId    = 0x26;
                    src.queryOnly = ROOM_EVENT_EXECUTE;
                    if (s->queryOnly == ROOM_EVENT_EXECUTE) {
                        if (GameFlag_GetNibble(0xC9) != 0) {
                            if (GameFlag_GetNibble(0x53) != 0) {
                                d->room = 2;
                            } else {
                                d->room = 1;
                            }
                            if (GameFlag_GetNibble(0x51) == 0) {
                                dst.room = dst.room + 2;
                            }
                        } else {
                            if (GameFlag_GetNibble(0x51) != 0) {
                                d->room = 5;
                            } else {
                                d->room = 6;
                            }
                        }
                    }
                    session                           = gGameSession;
                    room                              = dst.room;
                    session->at4.loc.room             = room;
                    Mc_SaveData[0].state.at4.loc.room = room;
                }
            }
            task->state = task->state + 1;
            return;
        case 3:
            if (gGameSession->at4.loc.room >= 5) {
                gGameSession->viewDirty = 1;
            }
            taskKill(task);
            return;
    }
}

/// Handler for message 0x13EE: copies the incoming record onto the outgoing one
/// and, unless the query is report-only (`queryOnly` set), answers record id 0x20
/// with 1 or 2 from nibble 0x51, raised by 2 while nibble 0x53 is set, and
/// record id 0x22 with 1 or 2 from nibble 0x52. Always returns 1.
s32 func_dryfield_night_underpass_8017D788(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->areaId == 0x20 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (GameFlag_GetNibble(0x51) == 0) {
            out->room = 2;
        } else {
            out->room = 1;
        }
        if (GameFlag_GetNibble(0x53) != 0) {
            out->room += 2;
        }
    }
    if (in->areaId == 0x22 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (GameFlag_GetNibble(0x52) == 0) {
            out->room = 2;
        } else {
            out->room = 1;
        }
    }
    return 1;
}

/// Handler for message 0x13F0: for `arg2` 1 or 2, spawns the room's switch
/// task `func_dryfield_night_underpass_8017D5D0` from the task table, toggling
/// nibble 0x51 with cap command 1 or nibble 0x52 with cap command 2. Any other
/// value spawns nothing. Always returns 0.
s32 func_dryfield_night_underpass_8017D868(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    switch (arg2) {
        case 1:
            Task_SpawnFromTable(D_dryfield_night_underpass_8017DCD8, 0, 0x51, 1);
            break;
        case 2:
            Task_SpawnFromTable(D_dryfield_night_underpass_8017DCD8, 0, 0x52, 2);
            break;
    }
    return 0;
}

/// Handler for message 0x13F2: when `arg2` is 2, queues stage sound 0x52260002.
/// Always returns 0.
s32 func_dryfield_night_underpass_8017D8CC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        Gp_EnqueueStageSnd6(0x52260000 | 2, 0, 0);
    }
    return 0;
}

/// Handler for message 0x13F1: does nothing and returns 0.
s32 func_dryfield_night_underpass_8017D900(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EF: does nothing and returns 0.
s32 func_dryfield_night_underpass_8017D908(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// First state of the room task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7, and advances.
static void func_dryfield_night_underpass_8017D910(Task* task)
{
    task->msgTable = D_dryfield_night_underpass_8017DCF0;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: idles.
static void func_dryfield_night_underpass_8017D954(Task* task)
{
}

/// State handlers of the room task `func_dryfield_night_underpass_8017D95C`,
/// indexed by `Task::state`: the set-up tick, the idle tick, and `taskKill`.
static const TaskFuncTable3 D_dryfield_night_underpass_8017D5C4 = {
    { func_dryfield_night_underpass_8017D910, func_dryfield_night_underpass_8017D954, taskKill },
};

/// Room task: runs the state handler `D_dryfield_night_underpass_8017D5C4`
/// names for `Task::state`, through a copy of the table taken onto the stack.
void func_dryfield_night_underpass_8017D95C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_underpass_8017D5C4;
    sp.funcs[task->state](task);
}

/// Draws one glow sprite at the world-space point `arg0`: projects it through
/// `gGfxViewCoord.workm` and, when the GTE flag is non-negative, queues a
/// semi-transparent `POLY_FT4` centred on the projection (tpage 0x2B, clut
/// `(arg1 & 0x3F) | 0x4380`, UV column `(s16)arg1 * 40`). `arg2` is a signed
/// half-extent; the on-screen radius is `(s16)arg2 * 39 / otz`. The grey level
/// alternates between 0x20 and 0x30 with `animFrame`. Works in 0x10 bytes of
/// scratch, released on exit.
static void func_dryfield_night_underpass_8017D9B4(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    s32                idx;
    s32                blend;
    s16                xy;

    block = SCRATCH_PUSH(RoomDraw13Scratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)arg1;
        blend       = (((u8)gDisplayState.animFrame & 1) * 16) + 0x20;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        setUVWH(prim, idx * 40, 0, 0x27, 0x27);
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        xy            = block->sx - (u16)block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = block->sx + (u16)block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = block->sy - (u16)block->radius;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = block->sy + (u16)block->radius;
        prim->y3      = xy;
        prim->y2      = xy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}

/// Per-frame effect: draws the glow anchors the current visit lights, one per
/// offset in `D_...DD20` whose `D_...DD60` bitmask contains the visit's bit
/// (`gGameSession->at4.loc.view`). The whole effect is skipped unless the room flag
/// (`GameFlag_GetNibble(0x53)`) is clear.
void func_dryfield_night_underpass_8017DC3C(Task* unused)
{
    s32      mask;
    s32      i;
    SVECTOR* vec;
    s16*     flags;

    mask = 1 << gGameSession->at4.loc.view;
    if (GameFlag_GetNibble(0x53) == 0) {
        i     = 0;
        vec   = D_dryfield_night_underpass_8017DD20;
        flags = D_dryfield_night_underpass_8017DD60;
        do {
            if (mask & *flags) {
                func_dryfield_night_underpass_8017D9B4(vec, 0, 0x280);
            }
            vec++;
            i++;
            flags++;
        } while (i < 8);
    }
}
