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
extern GpMsgEntry D_dryfield_motel_room_3_8017D6B4[];

s32 func_dryfield_motel_room_3_8017D5D0(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_motel_room_3_8017D5D8(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_dryfield_motel_room_3_8017D600(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_motel_room_3_8017D608(Task *, s32, GpMessageArg, GpMessageArg);

extern GpGridParams D_dryfield_motel_room_3_8017DDA0[1];
extern GpObj4C D_dryfield_motel_room_3_8017DFC4[11];
extern GpObj4C D_dryfield_motel_room_3_8017E308[1];
extern GpRoomCoordSet D_dryfield_motel_room_3_8017E4D4[1];

GpMsgEntry D_dryfield_motel_room_3_8017D6B4[5] = {
    { 5102, func_dryfield_motel_room_3_8017D5D8 },
    { 5105, func_dryfield_motel_room_3_8017D5D0 },
    { 5103, func_dryfield_motel_room_3_8017D608 },
    { 5104, func_dryfield_motel_room_3_8017D600 },
    { 0x7FFFFFFF, NULL },
};

GpRoomObjRec D_dryfield_motel_room_3_8017D6DC[1] = {
    { D_dryfield_motel_room_3_8017DDA0, D_dryfield_motel_room_3_8017DFC4, D_dryfield_motel_room_3_8017E308, NULL },
};

u8 * D_dryfield_motel_room_3_8017D6EC[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_motel_room_3_8017D6F0[1] = {
    { { .bytes = { 6, 0 } } },
};

GpRoomCoordRec D_dryfield_motel_room_3_8017D6F4[1] = {
    { D_dryfield_motel_room_3_8017E4D4, NULL },
};

GpWarpRec D_dryfield_motel_room_3_8017D6FC[1] = {
    { { .words = { 3072, 4462, 0, 1067 } }, { 0, 0, 0, 0 }, { .words = { 3072, 4462, 0, 1067 } }, { 0, 0, 0, 0 }, 0x520D0002, 0x520D0001, 0, 2, 0, 484 },
};

SVECTOR D_dryfield_motel_room_3_8017D734[13] = {
    { 0, 0, 4096, 0 },
    { 0, 0, -4096, 0 },
    { 0, 4096, 0, 0 },
    { -4096, 0, 0, 0 },
    { 4096, 0, 0, 0 },
    { 0, -4096, 0, 0 },
    { -2721, 0, 3061, 0 },
    { -2721, 0, -3061, 0 },
    { 4053, 0, 591, 0 },
    { 4096, 0, 0, 0 },
    { -3926, 0, -1167, 0 },
    { -44, 0, -4096, 0 },
    { -4064, 0, -508, 0 },
};

SVECTOR D_dryfield_motel_room_3_8017D79C[103] = {
    { 3100, 100, 5000, 0 },
    { 3100, -2700, 5000, 0 },
    { 200, -2700, 5000, 0 },
    { 200, 100, 5000, 0 },
    { 200, 100, 4680, 0 },
    { 200, -2700, 4680, 0 },
    { 3450, -2700, 4680, 0 },
    { 3450, 100, 4680, 0 },
    { 4800, 100, 200, 0 },
    { 4800, -2700, 200, 0 },
    { 200, -2700, 200, 0 },
    { 200, 100, 200, 0 },
    { 4800, -2600, 6800, 0 },
    { 200, -2600, 6800, 0 },
    { 200, -2600, 200, 0 },
    { 4800, -2600, 200, 0 },
    { 200, 100, 6800, 0 },
    { 200, -2700, 6800, 0 },
    { 4800, -2700, 6800, 0 },
    { 4800, 100, 6800, 0 },
    { 4800, -2700, 200, 0 },
    { 4800, 100, 200, 0 },
    { 200, 100, 200, 0 },
    { 200, -2700, 200, 0 },
    { 200, -2700, 6800, 0 },
    { 200, 100, 6800, 0 },
    { 4800, -2000, 1600, 0 },
    { 4350, -2000, 2000, 0 },
    { 4350, -2000, 4230, 0 },
    { 4800, -2000, 4630, 0 },
    { 4800, 0, 4630, 0 },
    { 4350, 0, 4230, 0 },
    { 4350, 0, 2000, 0 },
    { 4800, 0, 1600, 0 },
    { 1100, 0, 100, 0 },
    { 1100, -500, 100, 0 },
    { 890, -500, 1540, 0 },
    { 890, 0, 1540, 0 },
    { 200, -500, 1540, 0 },
    { 200, 0, 1540, 0 },
    { 200, -500, 100, 0 },
    { 3450, 100, 6050, 0 },
    { 3450, -2000, 6050, 0 },
    { 3450, -2000, 6800, 0 },
    { 3450, 100, 6800, 0 },
    { 3100, 100, 6050, 0 },
    { 3100, -2000, 6050, 0 },
    { 3100, 100, 6800, 0 },
    { 3100, -2000, 6800, 0 },
    { 3450, -2000, 4680, 0 },
    { 3450, -2000, 5050, 0 },
    { 3450, 100, 5050, 0 },
    { 2140, 0, 6800, 0 },
    { 2140, -720, 6800, 0 },
    { 2360, -720, 6060, 0 },
    { 2360, 0, 6060, 0 },
    { 3300, -720, 6050, 0 },
    { 3300, 0, 6050, 0 },
    { 3300, -720, 6800, 0 },
    { 200, -400, 3450, 0 },
    { 820, -400, 3450, 0 },
    { 820, -400, 2650, 0 },
    { 200, -400, 2650, 0 },
    { 200, 0, 2650, 0 },
    { 820, 0, 2650, 0 },
    { 820, 0, 3450, 0 },
    { 200, 0, 3450, 0 },
    { 200, -450, 4680, 0 },
    { 2490, -450, 4680, 0 },
    { 2490, -450, 3400, 0 },
    { 200, -450, 3400, 0 },
    { 200, 0, 3400, 0 },
    { 2490, 0, 3400, 0 },
    { 2490, 0, 4680, 0 },
    { 200, -450, 2650, 0 },
    { 2480, -450, 2650, 0 },
    { 2480, -450, 1490, 0 },
    { 200, -450, 1490, 0 },
    { 200, 0, 1490, 0 },
    { 2480, 0, 1490, 0 },
    { 2480, 0, 2650, 0 },
    { 3100, -2700, 6800, 0 },
    { 3100, -2000, 5000, 0 },
    { 3450, -2700, 6800, 0 },
    { 3100, 100, 5050, 0 },
    { 3100, -2000, 5050, 0 },
    { 4800, 0, 5000, 0 },
    { 4800, 0, 200, 0 },
    { 3100, 0, 200, 0 },
    { 3100, 0, 5000, 0 },
    { 3100, 0, 6800, 0 },
    { 4800, 0, 6800, 0 },
    { 200, 0, 200, 0 },
    { 200, 0, 5000, 0 },
    { 200, 0, 6800, 0 },
    { 1580, 50, 6800, 0 },
    { 1580, -1060, 6800, 0 },
    { 1620, -1060, 6480, 0 },
    { 1620, 50, 6480, 0 },
    { 2270, -1060, 6480, 0 },
    { 2270, 50, 6480, 0 },
    { 2270, -1060, 6800, 0 },
    { 2270, 50, 6800, 0 },
};

GpGridFace D_dryfield_motel_room_3_8017DAD4[45] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 5, 6, 4, 7 }, 1, 0 },
    { { 9, 10, 8, 11 }, 0, 0 },
    { { 13, 14, 12, 15 }, 2, 0 },
    { { 17, 18, 16, 19 }, 1, 0 },
    { { 18, 20, 19, 21 }, 3, 0 },
    { { 23, 24, 22, 25 }, 4, 0 },
    { { 27, 28, 26, 29 }, 5, 0 },
    { { 29, 28, 30, 31 }, 6, 0 },
    { { 28, 27, 31, 32 }, 3, 0 },
    { { 27, 26, 32, 33 }, 7, 0 },
    { { 35, 36, 34, 37 }, 8, 0 },
    { { 36, 38, 37, 39 }, 0, 0 },
    { { 40, 38, 35, 36 }, 5, 0 },
    { { 42, 43, 41, 44 }, 9, 0 },
    { { 46, 42, 45, 41 }, 1, 0 },
    { { 48, 46, 47, 45 }, 3, 0 },
    { { 49, 50, 7, 51 }, 9, 0 },
    { { 53, 54, 52, 55 }, 10, 0 },
    { { 54, 56, 55, 57 }, 11, 0 },
    { { 58, 56, 53, 54 }, 5, 0 },
    { { 60, 61, 59, 62 }, 5, 0 },
    { { 62, 61, 63, 64 }, 1, 0 },
    { { 61, 60, 64, 65 }, 9, 0 },
    { { 60, 59, 65, 66 }, 0, 0 },
    { { 68, 69, 67, 70 }, 5, 0 },
    { { 70, 69, 71, 72 }, 1, 0 },
    { { 69, 68, 72, 73 }, 9, 0 },
    { { 75, 76, 74, 77 }, 5, 0 },
    { { 77, 76, 78, 79 }, 1, 0 },
    { { 76, 75, 79, 80 }, 9, 0 },
    { { 75, 74, 80, 63 }, 0, 0 },
    { { 81, 1, 48, 82 }, 3, 0 },
    { { 48, 82, 43, 49 }, 2, 0 },
    { { 6, 83, 49, 43 }, 9, 0 },
    { { 85, 82, 84, 0 }, 3, 0 },
    { { 50, 85, 51, 84 }, 0, 0 },
    { { 87, 88, 86, 89 }, 5, 1 },
    { { 90, 91, 89, 86 }, 5, 1 },
    { { 88, 92, 89, 93 }, 5, 1 },
    { { 94, 90, 93, 89 }, 5, 2 },
    { { 96, 97, 95, 98 }, 12, 0 },
    { { 97, 99, 98, 100 }, 1, 0 },
    { { 101, 99, 96, 97 }, 5, 0 },
    { { 101, 96, 102, 95 }, 0, 0 },
};

s16 D_dryfield_motel_room_3_8017DCF0[25] = {
    1,
    2,
    3,
    5,
    6,
    7,
    9,
    10,
    11,
    12,
    13,
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
    37,
    39,
    -1,
};

s16 D_dryfield_motel_room_3_8017DD24[30] = {
    0,
    1,
    3,
    4,
    5,
    6,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    25,
    26,
    27,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    -1,
};

s16 D_dryfield_motel_room_3_8017DD60[9] = {
    2,
    3,
    5,
    7,
    8,
    9,
    10,
    37,
    -1,
};

s16 D_dryfield_motel_room_3_8017DD74[13] = {
    3,
    4,
    5,
    7,
    8,
    9,
    14,
    15,
    33,
    34,
    37,
    38,
    -1,
};

s16 * D_dryfield_motel_room_3_8017DD90[4] = {
    D_dryfield_motel_room_3_8017DCF0,
    D_dryfield_motel_room_3_8017DD24,
    D_dryfield_motel_room_3_8017DD60,
    D_dryfield_motel_room_3_8017DD74,
};

GpGridParams D_dryfield_motel_room_3_8017DDA0[1] = {
    { NULL, D_dryfield_motel_room_3_8017D734, D_dryfield_motel_room_3_8017D79C, D_dryfield_motel_room_3_8017DAD4, D_dryfield_motel_room_3_8017DD90, -200, -100, 2, 2, 4000, 45 },
};

GpViewRec D_dryfield_motel_room_3_8017DDC4[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -2500, 0x34BC, -3500 } }, 316 },
    { { { { 654, 0, -4043 }, { 0, 4096, 0 }, { 4043, 0, 654 } }, { -1650, 1050, -700 } }, 275 },
    { { { { 1425, 0, -3840 }, { -296, 4083, -109 }, { 3828, 315, 1420 } }, { -200, 1250, -900 } }, 235 },
    { { { { 3930, 0, -1152 }, { -94, 4082, -324 }, { 1148, 337, 3917 } }, { -1100, 1450, -200 } }, 230 },
    { { { { -3619, 0, 1916 }, { 173, 4079, 327 }, { -1908, 371, -3605 } }, { -3150, 1200, -3450 } }, 275 },
    { { { { 4066, 0, -488 }, { 0, 4096, 0 }, { 488, 0, 4066 } }, { -3800, 950, -2590 } }, 246 },
    { { { { -768, 0, -4023 }, { 313, 4083, -59 }, { 4011, -319, -765 } }, { -200, 750, -6400 } }, 235 },
    { { { { -419, 0, 4074 }, { 2571, 3176, 264 }, { -3160, 2585, -325 } }, { -3200, 2550, -6240 } }, 235 },
};

GpSprtCmd D_dryfield_motel_room_3_8017DEE4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_room_3_8017DEF4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_room_3_8017DF04[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_room_3_8017DF14[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_room_3_8017DF24[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_room_3_8017DF34[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_room_3_8017DF44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_room_3_8017DF54[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_motel_room_3_8017DF64[8] = {
    { { .empty = D_dryfield_motel_room_3_8017DEE4 }, D_dryfield_motel_room_3_8017DEE4, NULL },
    { { .empty = D_dryfield_motel_room_3_8017DEF4 }, D_dryfield_motel_room_3_8017DEF4, NULL },
    { { .empty = D_dryfield_motel_room_3_8017DF04 }, D_dryfield_motel_room_3_8017DF04, NULL },
    { { .empty = D_dryfield_motel_room_3_8017DF14 }, D_dryfield_motel_room_3_8017DF14, NULL },
    { { .empty = D_dryfield_motel_room_3_8017DF24 }, D_dryfield_motel_room_3_8017DF24, NULL },
    { { .empty = D_dryfield_motel_room_3_8017DF34 }, D_dryfield_motel_room_3_8017DF34, NULL },
    { { .empty = D_dryfield_motel_room_3_8017DF44 }, D_dryfield_motel_room_3_8017DF44, NULL },
    { { .empty = D_dryfield_motel_room_3_8017DF54 }, D_dryfield_motel_room_3_8017DF54, NULL },
};

GpObj4C D_dryfield_motel_room_3_8017DFC4[11] = {
    { NULL, NULL, NULL, { 3519, -1136, 2943, 0 }, { { -989, -2160, -373, 0 }, { 990, -2160, 374, 0 }, { -989, 2160, -373, 0 }, { 990, 2160, 374, 0 } }, { 1453, 0, -3857, 0 }, { 0, 0, 4096, 0 }, 2401, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 3455, -1104, 2815, 0 }, { { 990, -2128, 374, 0 }, { -989, -2128, -373, 0 }, { 990, 2128, 374, 0 }, { -989, 2128, -373, 0 } }, { -1455, 0, 3849, 0 }, { 0, 0, 4096, 0 }, 2374, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 2591, -1104, 862, 0 }, { { 246, -2128, -669, 0 }, { -245, -2128, 670, 0 }, { 246, 2128, -669, 0 }, { -245, 2128, 670, 0 } }, { 3859, 0, 1414, 0 }, { 0, 0, 4096, 0 }, 2231, 0, 3, 5, 1, 0 },
    { NULL, NULL, NULL, { 2719, -1120, 831, 0 }, { { -278, -2144, 657, 0 }, { 278, -2144, -657, 0 }, { -278, 2144, 657, 0 }, { 278, 2144, -657, 0 } }, { -3775, 0, -1600, 0 }, { 0, 0, 4096, 0 }, 2246, 0, 5, 3, 1, 0 },
    { NULL, NULL, NULL, { 4094, -1136, 5086, 0 }, { { -829, -2160, -245, 0 }, { 830, -2160, 246, 0 }, { -829, 2160, -245, 0 }, { 830, 2160, 246, 0 } }, { 1160, 0, -3929, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 4, 6, 1, 0 },
    { NULL, NULL, NULL, { 4063, -1136, 4991, 0 }, { { 830, -2160, 246, 0 }, { -829, -2160, -245, 0 }, { 830, 2160, 246, 0 }, { -829, 2160, -245, 0 } }, { -1163, 0, 3926, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 6, 4, 1, 0 },
    { NULL, NULL, NULL, { 3199, -1104, 5630, 0 }, { { -6, -2128, -865, 0 }, { 6, -2128, 865, 0 }, { -6, 2128, -865, 0 }, { 6, 2128, 865, 0 } }, { 4095, 0, -30, 0 }, { 0, 0, 4096, 0 }, 2289, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { 3328, -1152, 5631, 0 }, { { 6, -2176, 865, 0 }, { -6, -2176, -865, 0 }, { 6, 2176, 865, 0 }, { -6, 2176, -865, 0 } }, { -4101, 0, 26, 0 }, { 0, 0, 4096, 0 }, 2332, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { 2112, -1120, 5840, 0 }, { { -6, -2144, -977, 0 }, { 6, -2144, 977, 0 }, { -6, 2144, -977, 0 }, { 6, 2144, 977, 0 } }, { 4097, 0, -27, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 2272, -1152, 5888, 0 }, { { 6, -2176, 977, 0 }, { -6, -2176, -977, 0 }, { 6, 2176, 977, 0 }, { -6, 2176, -977, 0 } }, { -4122, 0, 23, 0 }, { 0, 0, 4096, 0 }, 2374, 0, 8, 7, 1, 0 },
    { NULL, NULL, NULL, { 4383, -1120, 1056, 0 }, { { 3, -2160, -885, 0 }, { -2, -2160, 886, 0 }, { 3, 2160, -885, 0 }, { -2, 2160, 886, 0 } }, { 4102, 0, 10, 0 }, { 0, 0, 4096, 0 }, 2332, 0, 2, 3, 129, 0 },
};

GpObj4C D_dryfield_motel_room_3_8017E308[1] = {
    { NULL, NULL, NULL, { 4528, -48, 1040, 0 }, { { -368, 0, -816, 0 }, { 368, 0, -816, 0 }, { -368, 0, 816, 0 }, { 368, 0, 816, 0 } }, { 0, 4115, 0, 0 }, { -4096, 0, 0, 0 }, 893, 0, 2, 21, 130, 0 },
};

GpPointLight D_dryfield_motel_room_3_8017E354[4] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 934, -1735, 6756 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2025, 2025, 2025, { 0, 0 } }, 1140, 5440 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 425, -1236, 2352 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2800, 2800, 2800, { 0, 0 } }, 2444, 7346 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4499, -1500, 5898 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2600, 2600, 2600, { 0, 0 } }, 880, 4011 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 450, -1278, 6899 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2900, 2900, 2900, { 0, 0 } }, 987, 2342 },
};

GpRoomCoordSet D_dryfield_motel_room_3_8017E4D4[1] = {
    { 0, NULL, 4, D_dryfield_motel_room_3_8017E354, 0, NULL },
};

s32 D_dryfield_motel_room_3_8017E4EC[3] = {
    0x10000035,
    0x10000037,
    0x10000035,
};

s32 D_dryfield_motel_room_3_8017E4F8[3] = {
    0x10000001,
    0x10000003,
    0x10000001,
};

GpRoomParamRec D_dryfield_motel_room_3_8017E504[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_motel_room_3_8017E50C[1] = {
    { 0, 0, 1, 0, D_dryfield_motel_room_3_8017E4EC },
};

GpRoomParamRec D_dryfield_motel_room_3_8017E514[1] = {
    { 0, 0, 1, 0, D_dryfield_motel_room_3_8017E4F8 },
};

GpRoomParamRec D_dryfield_motel_room_3_8017E51C[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec * D_dryfield_motel_room_3_8017E524[8] = {
    D_dryfield_motel_room_3_8017E504,
    D_dryfield_motel_room_3_8017E50C,
    D_dryfield_motel_room_3_8017E514,
    D_dryfield_motel_room_3_8017E51C,
    D_dryfield_motel_room_3_8017E504,
    D_dryfield_motel_room_3_8017E504,
    D_dryfield_motel_room_3_8017E504,
    D_dryfield_motel_room_3_8017E504,
};

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_motel_room_3_8017D5D0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply unchanged and returns 1.
s32 func_dryfield_motel_room_3_8017D5D8(Task* task, s32 msgId, RoomEventMsg * in, RoomEventMsg * out)
{
    *out = *in;
    return 1;
}

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_dryfield_motel_room_3_8017D600(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_motel_room_3_8017D608(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and advances to the next state.
static void func_dryfield_motel_room_3_8017D610(Task* task)
{
    task->msgTable = D_dryfield_motel_room_3_8017D6B4;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: the room has nothing to do each frame.
static void func_dryfield_motel_room_3_8017D654(Task* task)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_motel_room_3_8017D5C4 = {
    { func_dryfield_motel_room_3_8017D610, func_dryfield_motel_room_3_8017D654, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_motel_room_3_8017D5C4`.
void func_dryfield_motel_room_3_8017D65C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_room_3_8017D5C4;
    sp.funcs[task->state](task);
}
