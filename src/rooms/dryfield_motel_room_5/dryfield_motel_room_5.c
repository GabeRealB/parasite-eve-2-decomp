#include "common.h"

#include <psyq/libgte.h>
#include "rooms/room.h"
#include "rooms/room_common.h"

#include "gameplay/display.h"
#include "gameplay/message.h"

#include "main/session.h"
#include "main/task.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "rooms/stage_tables.h"

/// The room's message table, published at `Task::msgTable` by the room task.
extern GpMsgEntry D_dryfield_motel_room_5_8017D6B4[];

s32 func_dryfield_motel_room_5_8017D5D0(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_motel_room_5_8017D5D8(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_dryfield_motel_room_5_8017D600(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_motel_room_5_8017D608(Task *, s32, GpMessageArg, GpMessageArg);

extern GpGridParams D_dryfield_motel_room_5_8017DC9C[1];
extern GpObj4C D_dryfield_motel_room_5_8017DE40[8];
extern GpObj4C D_dryfield_motel_room_5_8017E0A0[1];
extern GpRoomCoordSet D_dryfield_motel_room_5_8017E56C[1];

GpMsgEntry D_dryfield_motel_room_5_8017D6B4[5] = {
    { 5102, func_dryfield_motel_room_5_8017D5D8 },
    { 5105, func_dryfield_motel_room_5_8017D5D0 },
    { 5103, func_dryfield_motel_room_5_8017D608 },
    { 5104, func_dryfield_motel_room_5_8017D600 },
    { 0x7FFFFFFF, NULL },
};

GpRoomObjRec D_dryfield_motel_room_5_8017D6DC[1] = {
    { D_dryfield_motel_room_5_8017DC9C, D_dryfield_motel_room_5_8017DE40, D_dryfield_motel_room_5_8017E0A0, NULL },
};

u8 * D_dryfield_motel_room_5_8017D6EC[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_motel_room_5_8017D6F0[1] = {
    { { .bytes = { 6, 0 } } },
};

GpRoomCoordRec D_dryfield_motel_room_5_8017D6F4[1] = {
    { D_dryfield_motel_room_5_8017E56C, NULL },
};

GpWarpRec D_dryfield_motel_room_5_8017D6FC[1] = {
    { { .words = { 3072, 4350, 0, 2426 } }, { 0, 0, 0, 0 }, { .words = { 3072, 4350, 0, 2426 } }, { 0, 0, 0, 0 }, 0x521C0002, 0x521C0001, 0, 2, 0, 464 },
};

SVECTOR D_dryfield_motel_room_5_8017D734[12] = {
    { 0, -4096, 0, 0 },
    { 4096, 0, 0, 0 },
    { 0, 0, 4096, 0 },
    { 0, 0, -4096, 0 },
    { 0, 4096, 0, 0 },
    { -4096, 0, 0, 0 },
    { 3285, 0, -2446, 0 },
    { 4096, 0, 0, 0 },
    { 4094, 0, -136, 0 },
    { 3849, 0, -1402, 0 },
    { 819, 0, -4013, 0 },
    { 0, 0, 4096, 0 },
};

SVECTOR D_dryfield_motel_room_5_8017D794[85] = {
    { 150, -450, 0, 0 },
    { 150, -450, 1250, 0 },
    { 2450, -450, 1250, 0 },
    { 2450, -450, 0, 0 },
    { 2450, 0, 0, 0 },
    { 2450, 0, 1250, 0 },
    { 150, 0, 1250, 0 },
    { 760, 0, 2230, 0 },
    { 760, -2000, 2230, 0 },
    { 1950, -2000, 2230, 0 },
    { 1950, 0, 2230, 0 },
    { 4800, -2600, 5800, 0 },
    { 200, -2600, 5800, 0 },
    { 200, -2600, 0, 0 },
    { 4800, -2600, 0, 0 },
    { 3510, 0, 820, 0 },
    { 3510, -700, 820, 0 },
    { 3510, -700, 0, 0 },
    { 3510, 0, 0, 0 },
    { 4800, -700, 820, 0 },
    { 4800, -700, 0, 0 },
    { 4800, 0, 820, 0 },
    { 2300, -2000, 2700, 0 },
    { 2300, 0, 2700, 0 },
    { 760, -2000, 2700, 0 },
    { 760, 0, 2700, 0 },
    { 200, 0, 0, 0 },
    { 200, -2600, 0, 0 },
    { 200, -2600, 5800, 0 },
    { 200, 0, 5800, 0 },
    { 4800, 0, 5800, 0 },
    { 4800, 0, 0, 0 },
    { 760, 0, 1190, 0 },
    { 760, -1300, 1190, 0 },
    { 810, -1300, 2700, 0 },
    { 810, 0, 2700, 0 },
    { 200, 0, 1190, 0 },
    { 200, -1300, 1190, 0 },
    { 200, -1300, 2700, 0 },
    { 2200, 0, 2900, 0 },
    { 2200, -2600, 2900, 0 },
    { 200, -2600, 2900, 0 },
    { 200, 0, 2900, 0 },
    { 200, 0, 2700, 0 },
    { 200, -2600, 2700, 0 },
    { 2400, -2600, 2700, 0 },
    { 2400, 0, 2700, 0 },
    { 2200, -2000, 5800, 0 },
    { 2200, -2600, 5800, 0 },
    { 2200, -2000, 2900, 0 },
    { 2400, -2000, 5800, 0 },
    { 2400, -2000, 2700, 0 },
    { 2400, -2600, 5800, 0 },
    { 2200, 0, 5800, 0 },
    { 2200, -2000, 5250, 0 },
    { 2200, 0, 5250, 0 },
    { 2400, -2000, 5250, 0 },
    { 2400, 0, 5250, 0 },
    { 2400, 0, 5800, 0 },
    { 2200, 0, 4400, 0 },
    { 2200, -2000, 4400, 0 },
    { 2400, 0, 4400, 0 },
    { 2400, -2000, 4400, 0 },
    { 690, 0, 4450, 0 },
    { 690, -700, 4450, 0 },
    { 1200, -700, 5850, 0 },
    { 1200, 0, 5850, 0 },
    { 200, 0, 4350, 0 },
    { 200, -700, 4350, 0 },
    { 200, -700, 5850, 0 },
    { 4800, 0, 0, 0 },
    { 4800, -2600, 0, 0 },
    { 200, -2600, 0, 0 },
    { 200, 0, 0, 0 },
    { 200, -900, 5800, 0 },
    { 4850, -900, 5800, 0 },
    { 4850, -900, 5300, 0 },
    { 200, -900, 5300, 0 },
    { 200, 0, 5300, 0 },
    { 4850, 0, 5300, 0 },
    { 4800, 0, 2900, 0 },
    { 4800, 0, 0, 0 },
    { 2200, 0, 0, 0 },
    { 200, 0, 0, 0 },
    { 200, 0, 2900, 0 },
};

GpGridFace D_dryfield_motel_room_5_8017DA3C[38] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 3, 2, 4, 5 }, 1, 0 },
    { { 2, 1, 5, 6 }, 2, 0 },
    { { 8, 9, 7, 10 }, 3, 0 },
    { { 12, 13, 11, 14 }, 4, 0 },
    { { 16, 17, 15, 18 }, 5, 0 },
    { { 19, 20, 16, 17 }, 0, 0 },
    { { 19, 16, 21, 15 }, 2, 0 },
    { { 9, 22, 10, 23 }, 6, 0 },
    { { 8, 24, 9, 22 }, 0, 0 },
    { { 24, 8, 25, 7 }, 5, 0 },
    { { 27, 28, 26, 29 }, 7, 0 },
    { { 11, 14, 30, 31 }, 5, 0 },
    { { 33, 34, 32, 35 }, 8, 0 },
    { { 37, 33, 36, 32 }, 3, 0 },
    { { 34, 33, 38, 37 }, 0, 0 },
    { { 40, 41, 39, 42 }, 2, 0 },
    { { 44, 45, 43, 46 }, 3, 0 },
    { { 48, 40, 47, 49 }, 5, 0 },
    { { 47, 49, 50, 51 }, 4, 0 },
    { { 45, 52, 51, 50 }, 1, 0 },
    { { 47, 54, 53, 55 }, 5, 0 },
    { { 54, 56, 55, 57 }, 3, 0 },
    { { 56, 50, 57, 58 }, 1, 0 },
    { { 60, 49, 59, 39 }, 5, 0 },
    { { 62, 60, 61, 59 }, 2, 0 },
    { { 51, 62, 46, 61 }, 1, 0 },
    { { 64, 65, 63, 66 }, 9, 0 },
    { { 68, 64, 67, 63 }, 10, 0 },
    { { 65, 64, 69, 68 }, 0, 0 },
    { { 28, 11, 29, 30 }, 3, 0 },
    { { 71, 72, 70, 73 }, 11, 0 },
    { { 75, 76, 74, 77 }, 0, 0 },
    { { 77, 76, 78, 79 }, 3, 0 },
    { { 81, 82, 80, 39 }, 0, 1 },
    { { 53, 30, 39, 80 }, 0, 1 },
    { { 82, 83, 39, 84 }, 0, 1 },
    { { 29, 53, 84, 39 }, 0, 2 },
};

s16 D_dryfield_motel_room_5_8017DC04[30] = {
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
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    24,
    25,
    26,
    31,
    34,
    35,
    36,
    37,
    -1,
};

s16 D_dryfield_motel_room_5_8017DC40[21] = {
    4,
    11,
    12,
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
    32,
    33,
    35,
    37,
    -1,
};

s16 D_dryfield_motel_room_5_8017DC6C[8] = {
    4,
    6,
    7,
    12,
    31,
    34,
    35,
    -1,
};

s16 D_dryfield_motel_room_5_8017DC7C[7] = {
    4,
    12,
    30,
    32,
    33,
    35,
    -1,
};

s16 * D_dryfield_motel_room_5_8017DC8C[4] = {
    D_dryfield_motel_room_5_8017DC04,
    D_dryfield_motel_room_5_8017DC40,
    D_dryfield_motel_room_5_8017DC6C,
    D_dryfield_motel_room_5_8017DC7C,
};

GpGridParams D_dryfield_motel_room_5_8017DC9C[1] = {
    { NULL, D_dryfield_motel_room_5_8017D734, D_dryfield_motel_room_5_8017D794, D_dryfield_motel_room_5_8017DA3C, D_dryfield_motel_room_5_8017DC8C, -150, 0, 2, 2, 4000, 38 },
};

GpViewRec D_dryfield_motel_room_5_8017DCC0[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -2500, 0x2C10, -3000 } }, 289 },
    { { { { 828, 0, -4011 }, { -323, 4082, -66 }, { 3998, 330, 825 } }, { -300, 1280, -900 } }, 235 },
    { { { { 725, 0, 4031 }, { 300, 4084, -54 }, { -4019, 305, 723 } }, { -4800, 1280, -900 } }, 235 },
    { { { { 4029, 0, 732 }, { 75, 4074, -415 }, { -728, 422, 4008 } }, { -4100, 1380, -500 } }, 246 },
    { { { { 4048, 0, -622 }, { -374, 3272, -2434 }, { 497, 2463, 3234 } }, { -800, 2580, -2900 } }, 246 },
    { { { { -4048, 0, 622 }, { 335, 3452, 2178 }, { -524, 2204, -3411 } }, { -1600, 2280, -5800 } }, 246 },
};

GpSprtCmd D_dryfield_motel_room_5_8017DD98[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_room_5_8017DDA8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_room_5_8017DDB8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_room_5_8017DDC8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_room_5_8017DDD8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_room_5_8017DDE8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_motel_room_5_8017DDF8[6] = {
    { { .empty = D_dryfield_motel_room_5_8017DD98 }, D_dryfield_motel_room_5_8017DD98, NULL },
    { { .empty = D_dryfield_motel_room_5_8017DDA8 }, D_dryfield_motel_room_5_8017DDA8, NULL },
    { { .empty = D_dryfield_motel_room_5_8017DDB8 }, D_dryfield_motel_room_5_8017DDB8, NULL },
    { { .empty = D_dryfield_motel_room_5_8017DDC8 }, D_dryfield_motel_room_5_8017DDC8, NULL },
    { { .empty = D_dryfield_motel_room_5_8017DDD8 }, D_dryfield_motel_room_5_8017DDD8, NULL },
    { { .empty = D_dryfield_motel_room_5_8017DDE8 }, D_dryfield_motel_room_5_8017DDE8, NULL },
};

GpObj4C D_dryfield_motel_room_5_8017DE40[8] = {
    { NULL, NULL, NULL, { 2384, -1296, 1392, 0 }, { { 112, -1936, -1360, 0 }, { -112, -1936, 1360, 0 }, { 112, 1936, -1360, 0 }, { -112, 1936, 1360, 0 } }, { 4092, 0, 335, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 2496, -1120, 1408, 0 }, { { -112, -2144, 1360, 0 }, { 112, -2144, -1360, 0 }, { -112, 2144, 1360, 0 }, { 112, 2144, -1360, 0 } }, { -4092, 0, -338, 0 }, { 0, 0, 4096, 0 }, 2534, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 3663, -1120, 3199, 0 }, { { 1449, -2144, 275, 0 }, { -1451, -2144, -277, 0 }, { 1449, 2144, 275, 0 }, { -1451, 2144, -277, 0 } }, { -768, 0, 4032, 0 }, { 0, 0, 4096, 0 }, 2598, 0, 4, 2, 1, 0 },
    { NULL, NULL, NULL, { 3711, -1104, 3295, 0 }, { { -1568, -2128, -307, 0 }, { 1566, -2128, 305, 0 }, { -1568, 2128, -307, 0 }, { 1566, 2128, 305, 0 } }, { 783, 0, -4021, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 2, 4, 1, 0 },
    { NULL, NULL, NULL, { 2206, -1104, 4927, 0 }, { { -43, -2128, -1006, 0 }, { 43, -2128, 1006, 0 }, { -43, 2128, -1006, 0 }, { 43, 2128, 1006, 0 } }, { 4115, 0, -178, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 2302, -1072, 5023, 0 }, { { 43, -2096, 1006, 0 }, { -43, -2096, -1006, 0 }, { 43, 2096, 1006, 0 }, { -43, 2096, -1006, 0 } }, { -4120, 0, 176, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 1183, -1072, 4446, 0 }, { { -1093, -2096, 109, 0 }, { 1093, -2096, -109, 0 }, { -1093, 2096, 109, 0 }, { 1093, 2096, -109, 0 } }, { -409, 0, -4081, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 1184, -1072, 4320, 0 }, { { 1093, -2096, -109, 0 }, { -1093, -2096, 109, 0 }, { 1093, 2096, -109, 0 }, { -1093, 2096, 109, 0 } }, { 406, 0, 4078, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 5, 6, 129, 0 },
};

GpObj4C D_dryfield_motel_room_5_8017E0A0[1] = {
    { NULL, NULL, NULL, { 4368, 0, 2272, 0 }, { { -432, 0, -800, 0 }, { 432, 0, -800, 0 }, { -432, 0, 800, 0 }, { 432, 0, 800, 0 } }, { 0, 4112, 0, 0 }, { -4096, 0, 0, 0 }, 907, 0, 29, 18, 130, 0 },
};

GpPointLight D_dryfield_motel_room_5_8017E0EC[12] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2807, -936, -1738 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3280, 3076, 2870, { 0, 0 } }, 1587, 2185 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8192, -2555, -1604 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2460, 2460, 2460, { 0, 0 } }, 1120, 1672 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6380, -2555, -1625 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2460, 2460, 2460, { 0, 0 } }, 1057, 1718 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2944, -1003, -1164 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3278, 3278, 3278, { 0, 0 } }, 1839, 2505 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 490, -1975, -479 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 3072, { 0, 0 } }, 1107, 1294 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 169, -2006, -1625 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 3076, { 0, 0 } }, 1112, 1374 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -911, -2035, -2681 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 3072, { 0, 0 } }, 1288, 1572 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1618, -1020, -130 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2870, 2870, 2870, { 0, 0 } }, 2591, 3272 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3750, -1440, -3 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1229, 1229, 1229, { 0, 0 } }, 2029, 2673 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6865, -1440, 10 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1229, 1229, 1229, { 0, 0 } }, 1935, 2601 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6885, -1440, -2420 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2666, 2666, 2663, { 0, 0 } }, 2646, 3283 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3665, -1440, -2318 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3280, 3280, 3280, { 0, 0 } }, 2164, 2832 },
};

GpRoomCoordSet D_dryfield_motel_room_5_8017E56C[1] = {
    { 0, NULL, 12, D_dryfield_motel_room_5_8017E0EC, 0, NULL },
};

s32 D_dryfield_motel_room_5_8017E584[3] = {
    0x10000035,
    0x10000037,
    0x10000035,
};

s32 D_dryfield_motel_room_5_8017E590[3] = {
    0x10000001,
    0x10000003,
    0x10000001,
};

GpRoomParamRec D_dryfield_motel_room_5_8017E59C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_motel_room_5_8017E5A4[1] = {
    { 0, 0, 1, 0, D_dryfield_motel_room_5_8017E584 },
};

GpRoomParamRec D_dryfield_motel_room_5_8017E5AC[1] = {
    { 0, 0, 1, 0, D_dryfield_motel_room_5_8017E590 },
};

GpRoomParamRec D_dryfield_motel_room_5_8017E5B4[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec * D_dryfield_motel_room_5_8017E5BC[8] = {
    D_dryfield_motel_room_5_8017E59C,
    D_dryfield_motel_room_5_8017E5A4,
    D_dryfield_motel_room_5_8017E5AC,
    D_dryfield_motel_room_5_8017E5B4,
    D_dryfield_motel_room_5_8017E59C,
    D_dryfield_motel_room_5_8017E59C,
    D_dryfield_motel_room_5_8017E59C,
    D_dryfield_motel_room_5_8017E59C,
};

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_motel_room_5_8017D5D0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply unchanged and returns 1.
s32 func_dryfield_motel_room_5_8017D5D8(Task* task, s32 msgId, RoomEventMsg * in, RoomEventMsg * out)
{
    *out = *in;
    return 1;
}

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_dryfield_motel_room_5_8017D600(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_motel_room_5_8017D608(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and advances to the next state.
static void func_dryfield_motel_room_5_8017D610(Task* task)
{
    task->msgTable = D_dryfield_motel_room_5_8017D6B4;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: the room has nothing to do each frame.
static void func_dryfield_motel_room_5_8017D654(Task* task)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_motel_room_5_8017D5C4 = {
    { func_dryfield_motel_room_5_8017D610, func_dryfield_motel_room_5_8017D654, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_motel_room_5_8017D5C4`.
void func_dryfield_motel_room_5_8017D65C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_room_5_8017D5C4;
    sp.funcs[task->state](task);
}
