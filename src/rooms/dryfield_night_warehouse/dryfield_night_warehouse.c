#include "common.h"
#include "rooms/dryfield_night_warehouse.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

#include "gameplay/actor_render.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/room_effects.h"

#include "main/display.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"

#include "gameplay/collision.h"
#include "gameplay/direction_input.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "rooms/stage_tables.h"

#include "actors/task_tables.h"
#include "gameplay/area.h"
#include "mapui/stage_tables.h"

static void func_dryfield_night_warehouse_8017D610(Task* task);
static void func_dryfield_night_warehouse_8017D654(Task* task);
static void func_dryfield_night_warehouse_8017D6B4(GpCoord* coord, s16 arg1);
static void func_dryfield_night_warehouse_8017DFF4(GpCoord* coord, s16 arg1, s16 arg2);

/// The room's message table: handlers for messages 0x13EE, 0x13F1, 0x13EF and
/// 0x13F0, closed by a 0x7FFFFFFF entry.
extern GpMsgEntry D_dryfield_night_warehouse_8017E830[];

/// Ring centres in the space of the coordinate drawn under, one per circle.
extern SVECTOR D_dryfield_night_warehouse_8017E858[];
/// Ring radii, parallel to the centres.
extern s16 D_dryfield_night_warehouse_8017E8D8[];

s32 func_dryfield_night_warehouse_8017D5D0(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_warehouse_8017D5D8(Task *, s32, GpSaveLoc *, GpSaveLoc *);
s32 func_dryfield_night_warehouse_8017D600(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_warehouse_8017D608(Task *, s32, GpMessageArg, GpMessageArg);

extern GpGridParams D_dryfield_night_warehouse_8017EF08[1];
extern GpObj4C D_dryfield_night_warehouse_8017F6F4[4];
extern GpObj4C D_dryfield_night_warehouse_8017F84C[10];
extern GpRoomBoundVec D_dryfield_night_warehouse_8017F824[5];
extern GpRoomCoordSet D_dryfield_night_warehouse_8017F6DC[1];

GpMsgEntry D_dryfield_night_warehouse_8017E830[5] = {
    { 5102, func_dryfield_night_warehouse_8017D5D8 },
    { 5105, func_dryfield_night_warehouse_8017D5D0 },
    { 5103, func_dryfield_night_warehouse_8017D608 },
    { 5104, func_dryfield_night_warehouse_8017D600 },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_dryfield_night_warehouse_8017E858[16] = {
    { 1190, -2950, -1540, 0 },
    { 310, -510, -180, 0 },
    { 2130, -2950, -3235, 0 },
    { 1200, -400, -1770, 0 },
    { 4340, -2950, -2940, 0 },
    { 3270, 0, -1290, 0 },
    { 3990, -2950, -1990, 0 },
    { 3030, -290, -510, 0 },
    { 5140, -2100, -3920, 0 },
    { 3860, -2100, -3920, 0 },
    { 3860, -660, -3920, 0 },
    { 5140, -660, -3920, 0 },
    { 4340, 0, -1930, 0 },
    { 3060, 0, -1930, 0 },
    { 3500, 0, -3030, 0 },
    { 4780, 0, -3030, 0 },
};

s16 D_dryfield_night_warehouse_8017E8D8[8] = {
    100,
    150,
    125,
    175,
    135,
    185,
    100,
    150,
};

GpRoomCoordRec D_dryfield_night_warehouse_8017E8E8[3] = {
    { D_dryfield_night_warehouse_8017F6DC, D_dryfield_night_warehouse_8017F824 },
    { D_dryfield_night_warehouse_8017F6DC, D_dryfield_night_warehouse_8017F824 },
    { D_dryfield_night_warehouse_8017F6DC, D_dryfield_night_warehouse_8017F824 },
};

GpRoomObjRec D_dryfield_night_warehouse_8017E900[3] = {
    { D_dryfield_night_warehouse_8017EF08, D_dryfield_night_warehouse_8017F6F4, D_dryfield_night_warehouse_8017F84C, NULL },
    { D_dryfield_night_warehouse_8017EF08, D_dryfield_night_warehouse_8017F6F4, D_dryfield_night_warehouse_8017F84C, NULL },
    { D_dryfield_night_warehouse_8017EF08, D_dryfield_night_warehouse_8017F6F4, D_dryfield_night_warehouse_8017F84C, NULL },
};

u8 * D_dryfield_night_warehouse_8017E930[3] = {
    D_8010CAF8,
    D_8010CAF8,
    D_8010CAF8,
};

GpViewCountRec D_dryfield_night_warehouse_8017E93C[3] = {
    { { .bytes = { 4, 0 } } },
    { { .bytes = { 4, 0 } } },
    { { .bytes = { 4, 0 } } },
};

GpWarpRec D_dryfield_night_warehouse_8017E944[2] = {
    { { .words = { 0, 2631, 0, -3535 } }, { 0, 0, 0, 0 }, { .words = { 1024, 1721, 0, -2640 } }, { 0, 0, 0, 0 }, 0x53070002, 0x53070001, 0, 2, 0, 468 },
    { { .words = { 3072, 5437, 2, -1084 } }, { 0, 0, 0, 0 }, { .words = { 3072, 4856, 2, -2146 } }, { 0, 0, 0, 0 }, 0, 0, 0, 4, 0, 0 },
};

SVECTOR D_dryfield_night_warehouse_8017E9B4[10] = {
    { -4096, 0, 0, 0 },
    { -3862, 0, 1363, 0 },
    { 0, -4096, 0, 0 },
    { 0, 0, 4096, 0 },
    { 3832, 0, 1446, 0 },
    { 4096, 0, 0, 0 },
    { 0, 0, -4096, 0 },
    { 0, 4096, 0, 0 },
    { -3871, 0, 1340, 0 },
    { 3832, 0, 1446, 0 },
};

SVECTOR D_dryfield_night_warehouse_8017EA04[74] = {
    { 5900, 300, -4100, 0 },
    { 5900, 300, -1287, 0 },
    { 5900, -1530, -1287, 0 },
    { 5900, -3200, -4100, 0 },
    { 5900, -1530, -713, 0 },
    { 5900, 300, -713, 0 },
    { 5900, -3200, 200, 0 },
    { 2800, 100, -3925, 0 },
    { 3250, 100, -2650, 0 },
    { 3250, -500, -2650, 0 },
    { 2800, -500, -3925, 0 },
    { 3800, -600, 0, 0 },
    { 3800, -600, -750, 0 },
    { 2200, -600, -750, 0 },
    { 2200, -600, 0, 0 },
    { 5350, 100, -1450, 0 },
    { 6000, 100, -1450, 0 },
    { 6000, -2050, -1450, 0 },
    { 5350, -2050, -1450, 0 },
    { 4050, -500, -3975, 0 },
    { 3550, -500, -2650, 0 },
    { 3550, 100, -2650, 0 },
    { 4050, 100, -3975, 0 },
    { 5900, 300, 200, 0 },
    { 1350, 0, -4000, 0 },
    { 1350, -2450, -4000, 0 },
    { 1350, -2450, -3400, 0 },
    { 1350, 0, -3400, 0 },
    { 3800, 100, -750, 0 },
    { 2200, 100, -750, 0 },
    { 0, -2450, -3400, 0 },
    { 0, 0, -3400, 0 },
    { 100, -3000, -100, 0 },
    { 100, 0, -100, 0 },
    { 100, 0, -3900, 0 },
    { 100, -3000, -3900, 0 },
    { 7650, 0, -3900, 0 },
    { 7650, -3000, -3900, 0 },
    { 7650, 0, -100, 0 },
    { 7650, -3000, -100, 0 },
    { 0, -2450, -4000, 0 },
    { 5350, 100, -3100, 0 },
    { 5350, -2050, -3100, 0 },
    { 6000, -2050, -3100, 0 },
    { 6000, 100, -3100, 0 },
    { 5350, -500, -4000, 0 },
    { 5350, 100, -4000, 0 },
    { 5350, 100, -3000, 0 },
    { 5350, -500, -3000, 0 },
    { 6000, -500, -3000, 0 },
    { 6000, -500, -4000, 0 },
    { 0, -600, 0, 0 },
    { 1250, -600, 0, 0 },
    { 1250, -600, -2400, 0 },
    { 0, -600, -2400, 0 },
    { 0, 100, -2400, 0 },
    { 1250, 100, -2400, 0 },
    { 1250, 100, 0, 0 },
    { 2200, 100, 0, 0 },
    { 3800, 100, 0, 0 },
    { 1250, -1200, -2400, 0 },
    { 1250, -1200, 0, 0 },
    { 0, -1200, -2400, 0 },
    { 2200, -1200, -750, 0 },
    { 3800, -1200, -750, 0 },
    { 2200, -1200, 0, 0 },
    { 3800, -1200, 0, 0 },
    { 3250, -1200, -2650, 0 },
    { 2800, -1200, -3950, 0 },
    { 2800, -500, -3950, 0 },
    { 3550, -1200, -2650, 0 },
    { 4050, -1200, -3975, 0 },
    { 5350, -1200, -3000, 0 },
    { 5350, -1200, -4000, 0 },
};

GpGridFace D_dryfield_night_warehouse_8017EC54[39] = {
    { { 1, 2, 0, 3 }, 0, 4 },
    { { 2, 1, 4, 5 }, 0, 5 },
    { { 2, 4, 3, 6 }, 0, 4 },
    { { 8, 9, 7, 10 }, 1, 4 },
    { { 12, 13, 11, 14 }, 2, 4 },
    { { 16, 17, 15, 18 }, 3, 4 },
    { { 20, 21, 19, 22 }, 4, 4 },
    { { 20, 9, 21, 8 }, 3, 4 },
    { { 4, 5, 6, 23 }, 0, 4 },
    { { 9, 20, 10, 19 }, 2, 4 },
    { { 25, 26, 24, 27 }, 5, 4 },
    { { 12, 28, 13, 29 }, 6, 4 },
    { { 26, 30, 27, 31 }, 3, 4 },
    { { 33, 34, 32, 35 }, 5, 4 },
    { { 34, 36, 35, 37 }, 3, 4 },
    { { 36, 38, 37, 39 }, 0, 4 },
    { { 38, 33, 39, 32 }, 6, 4 },
    { { 38, 36, 33, 34 }, 2, 6 },
    { { 32, 35, 39, 37 }, 7, 4 },
    { { 40, 30, 25, 26 }, 2, 4 },
    { { 42, 43, 41, 44 }, 6, 4 },
    { { 18, 42, 15, 41 }, 0, 4 },
    { { 46, 47, 45, 48 }, 0, 4 },
    { { 48, 49, 45, 50 }, 2, 4 },
    { { 17, 43, 18, 42 }, 2, 4 },
    { { 52, 53, 51, 54 }, 2, 4 },
    { { 54, 53, 55, 56 }, 6, 4 },
    { { 53, 52, 56, 57 }, 5, 4 },
    { { 13, 29, 14, 58 }, 0, 4 },
    { { 11, 59, 12, 28 }, 5, 4 },
    { { 61, 52, 60, 53 }, 5, 5 },
    { { 54, 62, 53, 60 }, 6, 5 },
    { { 64, 12, 63, 13 }, 6, 5 },
    { { 14, 65, 13, 63 }, 0, 5 },
    { { 66, 11, 64, 12 }, 5, 5 },
    { { 68, 69, 67, 9 }, 8, 5 },
    { { 67, 9, 70, 20 }, 3, 5 },
    { { 70, 20, 71, 19 }, 9, 5 },
    { { 73, 45, 72, 48 }, 0, 5 },
};

s16 D_dryfield_night_warehouse_8017EE28[38] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    9,
    10,
    11,
    12,
    13,
    14,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    -1,
};

s16 D_dryfield_night_warehouse_8017EE74[15] = {
    4,
    11,
    13,
    16,
    17,
    18,
    25,
    27,
    28,
    29,
    30,
    32,
    33,
    34,
    -1,
};

s16 D_dryfield_night_warehouse_8017EE94[31] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    11,
    14,
    15,
    16,
    17,
    18,
    20,
    21,
    22,
    23,
    24,
    28,
    29,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    -1,
};

s16 D_dryfield_night_warehouse_8017EED4[17] = {
    0,
    1,
    2,
    4,
    5,
    8,
    11,
    15,
    16,
    17,
    18,
    21,
    24,
    29,
    32,
    34,
    -1,
};

s16 * D_dryfield_night_warehouse_8017EEF8[4] = {
    D_dryfield_night_warehouse_8017EE28,
    D_dryfield_night_warehouse_8017EE74,
    D_dryfield_night_warehouse_8017EE94,
    D_dryfield_night_warehouse_8017EED4,
};

GpGridParams D_dryfield_night_warehouse_8017EF08[1] = {
    { NULL, D_dryfield_night_warehouse_8017E9B4, D_dryfield_night_warehouse_8017EA04, D_dryfield_night_warehouse_8017EC54, D_dryfield_night_warehouse_8017EEF8, 0, 4100, 2, 2, 4000, 39 },
};

GpViewRec D_dryfield_night_warehouse_8017EF2C[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3000, 7700, 2000 } }, 230 },
    { { { { -981, 0, 3976 }, { -168, 4092, -41 }, { -3972, -173, -981 } }, { -5338, 1066, 1266 } }, 207 },
    { { { { -832, 0, -4010 }, { -256, 4087, 53 }, { 4002, 262, -831 } }, { -288, 1366, 916 } }, 207 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
    { { { { 2311, 0, -3381 }, { -2032, 3273, -1389 }, { 2702, 2461, 1847 } }, { -5038, 1666, 2016 } }, 207 },
    { { { { -832, 0, -4010 }, { -256, 4087, 53 }, { 4002, 262, -831 } }, { -288, 1366, 916 } }, 207 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
};

GpSprtCmd D_dryfield_night_warehouse_8017F04C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_warehouse_8017F05C[23] = {
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -8, 16, 1075, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 24, 1025, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -112, 64, 475, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, 64, 500, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 80, 375, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 64, 450, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 64, 425, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 375, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 64, 675, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, 64, 675, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 72, 625, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, 32, 500, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 112, 72, 625, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 96, 575, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 80, 350, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 80, 625, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 80, 40, 675, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 64, 625, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, 32, 650, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, 32, 600, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -160, 0, 700, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -160, 32, 700, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, -160, 56, 700, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_warehouse_8017F228[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 6, 0, 0, { 0, 0 } },
    { 8, 12, 0, 0, { 2, 0 } },
    { 20, 3, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_warehouse_8017F258[22] = {
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, 32, 975, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, 32, 925, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 32, 875, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 40, 825, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, 40, 837, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 24, 775, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 24, 575, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 72, 600, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 72, 595, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -96, 72, 475, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -128, 72, 475, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 40, 475, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, 24, 787, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 40, 825, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 32, 812, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 64, 625, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 64, 625, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 56, 825, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -88, 56, 750, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 32, 1125, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, 16, 1050, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, 120, 0, 1125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_warehouse_8017F410[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 3, 0, 0, { 0, 0 } },
    { 5, 14, 0, 0, { 2, 0 } },
    { 19, 3, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_warehouse_8017F440[1] = {
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -160, 80, 500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_warehouse_8017F454[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_warehouse_8017F46C[4] = {
    { { .empty = D_dryfield_night_warehouse_8017F04C }, D_dryfield_night_warehouse_8017F04C, NULL },
    { { .elements = D_dryfield_night_warehouse_8017F05C }, D_dryfield_night_warehouse_8017F228, NULL },
    { { .elements = D_dryfield_night_warehouse_8017F258 }, D_dryfield_night_warehouse_8017F410, NULL },
    { { .elements = D_dryfield_night_warehouse_8017F440 }, D_dryfield_night_warehouse_8017F454, NULL },
};

GpPointLight D_dryfield_night_warehouse_8017F49C[6] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2731, -1953, 17 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2051, 2460, 2460, { 0, 0 } }, 2039, 2301 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 839, -710, -1699 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1640, 2052, 2052, { 0, 0 } }, 951, 1382 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3317, -270, -901 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1436, 1642, 1642, { 0, 0 } }, 561, 755 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3416, -158, -3078 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1130, 1230, 1230, { 0, 0 } }, 1163, 1602 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4169, -1528, 161 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2050, 2460, 2460, { 0, 0 } }, 1319, 1642 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3902, -1641, -3609 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2458, 2870, 2871, { 0, 0 } }, 1420, 4454 },
};

GpRoomCoordSet D_dryfield_night_warehouse_8017F6DC[1] = {
    { 0, NULL, 6, D_dryfield_night_warehouse_8017F49C, 0, NULL },
};

GpObj4C D_dryfield_night_warehouse_8017F6F4[4] = {
    { NULL, NULL, NULL, { 2927, -1584, -2065, 0 }, { { -487, 1744, 1952, 0 }, { 487, 1744, -1951, 0 }, { -487, -1744, 1952, 0 }, { 487, -1744, -1951, 0 } }, { 3985, 0, 994, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 3072, -960, -2114, 0 }, { { 487, 1984, -1951, 0 }, { -487, 1984, 1952, 0 }, { 487, -1984, -1951, 0 }, { -487, -1984, 1952, 0 } }, { -3980, 0, -994, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 4703, -1104, -1858, 0 }, { { 1097, 2128, -1688, 0 }, { -1133, 2128, 1658, 0 }, { 1097, -2128, -1688, 0 }, { -1133, -2128, 1658, 0 } }, { -3424, 0, -2283, 0 }, { 0, 0, 4096, 0 }, 2918, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 4607, -1168, -1922, 0 }, { { -1133, 2192, 1655, 0 }, { 1097, 2192, -1692, 0 }, { -1133, -2192, 1655, 0 }, { 1097, -2192, -1692, 0 } }, { 3424, 0, 2281, 0 }, { 0, 0, 4096, 0 }, 2974, 0, 4, 3, 129, 0 },
};

GpRoomBoundVec D_dryfield_night_warehouse_8017F824[5] = {
    { 4, 0, 0, 0 },
    { 16, 0, 0, 6 },
    { 16, 0, 0, 6 },
    { 400, 400, 400, 400 },
    { 16, 0, 0, 6 },
};

GpObj4C D_dryfield_night_warehouse_8017F84C[10] = {
    { NULL, NULL, NULL, { 2303, -62, -3664, 0 }, { { -543, 0, -208, 0 }, { 544, 0, -208, 0 }, { -543, 0, 208, 0 }, { 544, 0, 208, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 579, 0, 5, 19, 2, 0 },
    { NULL, NULL, NULL, { 5600, -65, -864, 0 }, { { 352, 0, -671, 0 }, { 352, 0, 672, 0 }, { -352, 0, -671, 0 }, { -352, 0, 672, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 757, 0, 9, 34, 2, 0 },
    { NULL, NULL, NULL, { 5040, -64, -2224, 0 }, { { 272, 0, -783, 0 }, { 272, 0, 784, 0 }, { -272, 0, -783, 0 }, { -272, 0, 784, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 829, 2, 9, 0, 2, 0 },
    { NULL, NULL, NULL, { 784, -64, -2592, 0 }, { { -751, 0, -208, 0 }, { 752, 0, -208, 0 }, { -751, 0, 208, 0 }, { 752, 0, 208, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 778, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 1504, -64, -1408, 0 }, { { -271, 0, -1296, 0 }, { 272, 0, -1296, 0 }, { -271, 0, 1296, 0 }, { 272, 0, 1296, 0 } }, { 0, 4111, 0, 0 }, { 4091, 0, 201, 0 }, 1324, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 1744, -64, -368, 0 }, { { -543, 0, -256, 0 }, { 544, 0, -256, 0 }, { -543, 0, 256, 0 }, { 544, 0, 256, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 600, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 4512, -64, -416, 0 }, { { -543, 0, -256, 0 }, { 544, 0, -256, 0 }, { -543, 0, 256, 0 }, { 544, 0, 256, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 600, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 3040, -64, -2880, 0 }, { { -351, 0, -416, 0 }, { 352, 0, -416, 0 }, { -351, 0, 416, 0 }, { 352, 0, 416, 0 } }, { 0, 4115, 0, 0 }, { -601, 0, 4052, 0 }, 543, 2, 7, 0, 2, 0 },
    { NULL, NULL, NULL, { 3008, -64, -416, 0 }, { { -1327, 0, -848, 0 }, { 1328, 0, -848, 0 }, { -1327, 0, 240, 0 }, { 1328, 0, 240, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1572, 2, 2, 0, 4, 0 },
    { NULL, NULL, NULL, { 672, -64, -3200, 0 }, { { -703, 0, -208, 0 }, { 704, 0, -208, 0 }, { -703, 0, 208, 0 }, { 704, 0, 208, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, 4096, 0 }, 732, 2, 8, 0, 130, 0 },
};

GpAreaTmdRec D_dryfield_night_warehouse_8017FB44[2] = {
    { 25, 25, 0, 0, { 0, 0 }, D_801379A8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_warehouse_8017FB5C[2] = {
    { 40, 40, 0, 0, { 0, 0 }, D_8013E500 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_warehouse_8017FB74[3] = {
    { 16, 16, 0, 0, { 0, 0 }, D_801445DC },
    { 25, 25, 1, 0, { 0, 0 }, D_8014F9A8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_warehouse_8017FB98[12] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017B408, D_dryfield_night_warehouse_8017FB44 },
    { NULL, NULL },
    { D_map_dryfield_full_8017B438, D_dryfield_night_warehouse_8017FB5C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017B4A8, D_dryfield_night_warehouse_8017FB74 },
};

s32 D_dryfield_night_warehouse_8017FBF8[3] = {
    0x10000045,
    0x10000047,
    0x10000045,
};

GpRoomParamRec D_dryfield_night_warehouse_8017FC04[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_warehouse_8017FC0C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_warehouse_8017FC14[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_dryfield_night_warehouse_8017FC1C[1] = {
    { 0, 0, 1, 0, D_dryfield_night_warehouse_8017FBF8 },
};

GpRoomParamRec * D_dryfield_night_warehouse_8017FC24[8] = {
    D_dryfield_night_warehouse_8017FC04,
    D_dryfield_night_warehouse_8017FC04,
    D_dryfield_night_warehouse_8017FC04,
    D_dryfield_night_warehouse_8017FC04,
    D_dryfield_night_warehouse_8017FC0C,
    D_dryfield_night_warehouse_8017FC14,
    D_dryfield_night_warehouse_8017FC1C,
    D_dryfield_night_warehouse_8017FC04,
};

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and answers 0.
s32 func_dryfield_night_warehouse_8017D5D0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table: copies the location
/// record the sender passes onto the reply record and answers 1.
s32 func_dryfield_night_warehouse_8017D5D8(Task* task, s32 msgId, GpSaveLoc * src, GpSaveLoc * dst)
{
    *dst = *src;
    return 1;
}

/// Handler for message 0x13F0 in the room's message table: does nothing and
/// answers 0.
s32 func_dryfield_night_warehouse_8017D600(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EF in the room's message table: does nothing and
/// answers 0.
s32 func_dryfield_night_warehouse_8017D608(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// State 0 of the room task: publishes the room's message table in
/// `Task::msgTable`, claims pointer slot 7 and advances to the next state.
static void func_dryfield_night_warehouse_8017D610(Task* task)
{
    task->msgTable = D_dryfield_night_warehouse_8017E830;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room task: the room idles here and does nothing.
static void func_dryfield_night_warehouse_8017D654(Task* task)
{
}

/// The room task's three states: set up, idle, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_warehouse_8017D5C4 = {
    { func_dryfield_night_warehouse_8017D610, func_dryfield_night_warehouse_8017D654, taskKill },
};

/// The room task: copies its three-state table onto the stack and runs the
/// entry for the task's current state.
void func_dryfield_night_warehouse_8017D65C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_warehouse_8017D5C4;
    sp.funcs[task->state](task);
}

/// Draws a four-sided prism from `D_dryfield_night_warehouse_8017E858[arg1..]`
/// as five gouraud `POLY_G4`: four sides joining the lit ring `[0..3]` to the
/// far ring `[4..7]`, then a cap over the lit ring. Each corner is rotated by
/// `coord`'s `workm` and moved by its translation before projection through
/// `GsWSMATRIX`. The lit corners share a pulsing colour whose red is three
/// quarters of its green and blue; the far corners are black.
static void func_dryfield_night_warehouse_8017D6B4(GpCoord* coord, s16 arg1)
{
    RoomQuadScratch* blk;
    POLY_G4*         prim;
    s32              i;
    s32              next;
    s32              far;
    s32              farNext;
    s16              pulse;
    s16              red;
    s16              blue;
    s16              green;

    pulse = (rsin(gDisplayState.animFrame << 10) >> 12) + 0x10;
    SCRATCH_PUSH(RoomQuadScratch);
    blk = SCRATCH_HEAD(RoomQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    red   = pulse * 3 / 4;
    green = pulse;
    blue  = pulse;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1 + i]);
        gte_rtv0();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx += coord->workm.t[0];
        blk->v[0].vy += coord->workm.t[1];
        blk->v[0].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        next = (i + 1) & 3;
        gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1 + next]);
        gte_rtv0();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx += coord->workm.t[0];
        blk->v[1].vy += coord->workm.t[1];
        blk->v[1].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        far = i + 4;
        gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1 + far]);
        gte_rtv0();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx += coord->workm.t[0];
        blk->v[2].vy += coord->workm.t[1];
        blk->v[2].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        farNext = next + 4;
        gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1 + farNext]);
        gte_rtv0();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx += coord->workm.t[0];
        blk->v[3].vy += coord->workm.t[1];
        blk->v[3].vz += coord->workm.t[2];
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        setRGB0(prim, red, green, blue);
        setRGB1(prim, red, green, blue);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim(Gpu_OtEntryAtByteOffset((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1]);
    gte_rtv0();
    gte_stsv(&blk->v[0]);
    blk->v[0].vx += coord->workm.t[0];
    blk->v[0].vy += coord->workm.t[1];
    blk->v[0].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1 + 1]);
    gte_rtv0();
    gte_stsv(&blk->v[1]);
    blk->v[1].vx += coord->workm.t[0];
    blk->v[1].vy += coord->workm.t[1];
    blk->v[1].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1 + 3]);
    gte_rtv0();
    gte_stsv(&blk->v[2]);
    blk->v[2].vx += coord->workm.t[0];
    blk->v[2].vy += coord->workm.t[1];
    blk->v[2].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1 + 2]);
    gte_rtv0();
    gte_stsv(&blk->v[3]);
    blk->v[3].vx += coord->workm.t[0];
    blk->v[3].vy += coord->workm.t[1];
    blk->v[3].vz += coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();
    prim           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    setRGB0(prim, red, green, blue);
    setRGB1(prim, red, green, blue);
    setRGB2(prim, red, green, blue);
    setRGB3(prim, red, green, blue);
    addPrim(Gpu_OtEntryAtByteOffset((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
            prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    SCRATCH_POP(RoomQuadScratch);
}

/// Draws the band joining ring `arg1` to ring `arg1 + 1` as `arg2` gouraud
/// `POLY_G4` segments, starting at an angle that turns with
/// `gDisplayState.animFrame`. Each corner is rotated by `coord`'s `workm` and
/// moved by its translation before projection through `GsWSMATRIX`. The corners
/// on ring `arg1` share a pulsing colour whose red is three quarters of its
/// green and blue; the corners on ring `arg1 + 1` are black.
static void func_dryfield_night_warehouse_8017DFF4(GpCoord* coord, s16 arg1, s16 arg2)
{
    RoomQuadScratch* blk;
    POLY_G4*         prim;
    s16              red;
    s16              blue;
    s16              green;
    s16              step;
    s16              start;
    s16              pulse;
    s32              angle;
    s32              next;

    pulse = (rsin(gDisplayState.animFrame << 10) >> 12) + 0x10;
    SCRATCH_PUSH(RoomQuadScratch);
    blk   = SCRATCH_HEAD(RoomQuadScratch);
    start = gDisplayState.animFrame & 0xFFF;
    step  = 0x1000 / arg2;
    gte_SetTransMatrix(&GsWSMATRIX);
    red   = pulse * 3 / 4;
    green = pulse;
    blue  = pulse;
    for (angle = start; angle < start + step * arg2; angle = next) {
        blk->v[0].vx = D_dryfield_night_warehouse_8017E858[arg1].vx +
                       ((rsin(angle) * D_dryfield_night_warehouse_8017E8D8[arg1]) >> 12);
        blk->v[0].vy = D_dryfield_night_warehouse_8017E858[arg1].vy;
        blk->v[0].vz = D_dryfield_night_warehouse_8017E858[arg1].vz +
                       ((rcos(angle) * D_dryfield_night_warehouse_8017E8D8[arg1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[0]);
        gte_rtv0();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx += coord->workm.t[0];
        blk->v[0].vy += coord->workm.t[1];
        next          = angle + step;
        blk->v[0].vz += coord->workm.t[2];

        blk->v[1].vx = D_dryfield_night_warehouse_8017E858[arg1].vx +
                       ((rsin(next) * D_dryfield_night_warehouse_8017E8D8[arg1]) >> 12);
        blk->v[1].vy = D_dryfield_night_warehouse_8017E858[arg1].vy;
        blk->v[1].vz = D_dryfield_night_warehouse_8017E858[arg1].vz +
                       ((rcos(next) * D_dryfield_night_warehouse_8017E8D8[arg1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[1]);
        gte_rtv0();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx += coord->workm.t[0];
        blk->v[1].vy += coord->workm.t[1];
        blk->v[1].vz += coord->workm.t[2];

        blk->v[2].vx = D_dryfield_night_warehouse_8017E858[arg1 + 1].vx +
                       ((rsin(angle) * D_dryfield_night_warehouse_8017E8D8[arg1 + 1]) >> 12);
        blk->v[2].vy = D_dryfield_night_warehouse_8017E858[arg1 + 1].vy;
        blk->v[2].vz = D_dryfield_night_warehouse_8017E858[arg1 + 1].vz +
                       ((rcos(angle) * D_dryfield_night_warehouse_8017E8D8[arg1 + 1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[2]);
        gte_rtv0();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx += coord->workm.t[0];
        blk->v[2].vy += coord->workm.t[1];
        blk->v[2].vz += coord->workm.t[2];

        blk->v[3].vx = D_dryfield_night_warehouse_8017E858[arg1 + 1].vx +
                       ((rsin(next) * D_dryfield_night_warehouse_8017E8D8[arg1 + 1]) >> 12);
        blk->v[3].vy = D_dryfield_night_warehouse_8017E858[arg1 + 1].vy;
        blk->v[3].vz = D_dryfield_night_warehouse_8017E858[arg1 + 1].vz +
                       ((rcos(next) * D_dryfield_night_warehouse_8017E8D8[arg1 + 1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[3]);
        gte_rtv0();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx += coord->workm.t[0];
        blk->v[3].vy += coord->workm.t[1];
        blk->v[3].vz += coord->workm.t[2];

        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        setRGB0(prim, red, green, blue);
        setRGB1(prim, red, green, blue);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim(Gpu_OtEntryAtByteOffset((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    SCRATCH_POP(RoomQuadScratch);
}

/// Per-frame effect on the room's model task: recomputes the model's world
/// matrix and then re-poses it. The current visit is the stage-visit byte
/// `gGameSession->at4.loc.view` taken as a bit index, and each pose is gated on that
/// bit being one of a fixed set of visits.
void func_dryfield_night_warehouse_8017E778(Task* arg0)
{
    GpCoord* coord;
    s32      mask;

    coord = arg0->extra.tmd->coords;
    mask  = 1 << gGameSession->at4.loc.view;
    Gp_UpdateCoord(coord);
    if (mask & 0x24C) {
        func_dryfield_night_warehouse_8017D6B4(coord, 8);
    }
    if (mask & 4) {
        func_dryfield_night_warehouse_8017DFF4(coord, 0, 8);
    }
    if (mask & 0x24C) {
        func_dryfield_night_warehouse_8017DFF4(coord, 2, 8);
    }
    if (mask & 0x3DC) {
        func_dryfield_night_warehouse_8017DFF4(coord, 4, 8);
        func_dryfield_night_warehouse_8017DFF4(coord, 6, 8);
    }
}
