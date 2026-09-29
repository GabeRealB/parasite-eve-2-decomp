#include "rooms/dryfield_motel_loft.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

/// The room's message table, published at `Task::msgTable` by the room task.
extern GpMsgEntry D_dryfield_motel_loft_8017D6B4[];

s32 func_dryfield_motel_loft_8017D5D0(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_motel_loft_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_motel_loft_8017D600(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_motel_loft_8017D608(Task*, s32, GpMessageArg, GpMessageArg);

extern GpGridParams   D_dryfield_motel_loft_8017D9BC[1];
extern GpObj4C        D_dryfield_motel_loft_8017DE2C[12];
extern GpObj4C        D_dryfield_motel_loft_8017E1BC[2];
extern GpRoomCoordSet D_dryfield_motel_loft_8017E614[1];

GpMsgEntry D_dryfield_motel_loft_8017D6B4[5] = {
    { 5102, func_dryfield_motel_loft_8017D5D8 },
    { 5105, func_dryfield_motel_loft_8017D5D0 },
    { 5103, func_dryfield_motel_loft_8017D608 },
    { 5104, func_dryfield_motel_loft_8017D600 },
    { 0x7FFFFFFF, NULL },
};

GpRoomObjRec D_dryfield_motel_loft_8017D6DC[1] = {
    { D_dryfield_motel_loft_8017D9BC, D_dryfield_motel_loft_8017DE2C, D_dryfield_motel_loft_8017E1BC, NULL },
};

u8 * D_dryfield_motel_loft_8017D6EC[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_motel_loft_8017D6F0[1] = {
    { { .bytes = { 7, 0 } } },
};

GpRoomCoordRec D_dryfield_motel_loft_8017D6F4[1] = {
    { D_dryfield_motel_loft_8017E614, NULL },
};

GpWarpRec D_dryfield_motel_loft_8017D6FC[1] = {
    { { .words = { 0, 3953, 0, -2156 } }, { 0, 0, 0, 0 }, { .words = { 0, 3953, 0, -2156 } }, { 0, 0, 0, 0 }, 0x521F0002, 0x521F0001, 0, 2, 0, 466 },
};

SVECTOR D_dryfield_motel_loft_8017D734[8] = {
    { 4096, 0, 0, 0 },
    { 0, 0, 4096, 0 },
    { -4096, 0, 0, 0 },
    { 0, 0, -4096, 0 },
    { 0, -4096, 0, 0 },
    { 0, 4096, 0, 0 },
    { -4096, 0, 1, 0 },
    { 3311, 0, 2411, 0 },
};

SVECTOR D_dryfield_motel_loft_8017D774[30] = {
    { -6000, -2500, 2500, 0 },
    { -6000, 0, 2500, 0 },
    { -6000, 0, -2500, 0 },
    { -6000, -2500, -2500, 0 },
    { 5070, 0, -2500, 0 },
    { 5070, -2500, -2500, 0 },
    { 5070, 0, 2500, 0 },
    { 5070, -2500, 2500, 0 },
    { -4500, 0, 500, 0 },
    { -4500, -2000, 500, 0 },
    { -4500, -2000, -500, 0 },
    { -4500, 0, -500, 0 },
    { 965, -2000, -500, 0 },
    { 965, 0, -500, 0 },
    { 965, -2000, 500, 0 },
    { 965, 0, 500, 0 },
    { -6000, 0, 2500, 0 },
    { -6000, -2000, 2500, 0 },
    { -6000, -2000, 1856, 0 },
    { -6000, 0, 1856, 0 },
    { 5070, -2000, 1856, 0 },
    { 5070, 0, 1856, 0 },
    { 5070, -2000, 2500, 0 },
    { -415, 0, -1930, 0 },
    { -415, -2000, -1930, 0 },
    { -6000, -2000, -1930, 0 },
    { -6000, 0, -1930, 0 },
    { 0, -2000, -2500, 0 },
    { -6000, -2000, -2500, 0 },
    { 0, 0, -2500, 0 },
};

GpGridFace D_dryfield_motel_loft_8017D864[17] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 2, 4, 3, 5 }, 1, 0 },
    { { 4, 6, 5, 7 }, 2, 0 },
    { { 6, 1, 7, 0 }, 3, 0 },
    { { 6, 4, 1, 2 }, 4, 1 },
    { { 0, 3, 7, 5 }, 5, 0 },
    { { 9, 10, 8, 11 }, 2, 0 },
    { { 10, 12, 11, 13 }, 3, 0 },
    { { 12, 14, 13, 15 }, 0, 0 },
    { { 14, 9, 15, 8 }, 1, 0 },
    { { 14, 12, 9, 10 }, 4, 0 },
    { { 17, 18, 16, 19 }, 6, 0 },
    { { 18, 20, 19, 21 }, 3, 0 },
    { { 22, 20, 17, 18 }, 4, 0 },
    { { 24, 25, 23, 26 }, 1, 0 },
    { { 24, 27, 25, 28 }, 4, 0 },
    { { 27, 24, 29, 23 }, 7, 0 },
};

s16 D_dryfield_motel_loft_8017D930[13] = {
    0,
    1,
    4,
    5,
    6,
    7,
    9,
    10,
    12,
    13,
    14,
    15,
    -1,
};

s16 D_dryfield_motel_loft_8017D94C[8] = {
    0,
    3,
    4,
    5,
    11,
    12,
    13,
    -1,
};

s16 D_dryfield_motel_loft_8017D95C[13] = {
    1,
    4,
    5,
    7,
    8,
    9,
    10,
    12,
    13,
    14,
    15,
    16,
    -1,
};

s16 D_dryfield_motel_loft_8017D978[6] = {
    3,
    4,
    5,
    12,
    13,
    -1,
};

s16 D_dryfield_motel_loft_8017D984[7] = {
    1,
    2,
    4,
    5,
    12,
    13,
    -1,
};

s16 D_dryfield_motel_loft_8017D994[7] = {
    2,
    3,
    4,
    5,
    12,
    13,
    -1,
};

s16 * D_dryfield_motel_loft_8017D9A4[6] = {
    D_dryfield_motel_loft_8017D930,
    D_dryfield_motel_loft_8017D94C,
    D_dryfield_motel_loft_8017D95C,
    D_dryfield_motel_loft_8017D978,
    D_dryfield_motel_loft_8017D984,
    D_dryfield_motel_loft_8017D994,
};

GpGridParams D_dryfield_motel_loft_8017D9BC[1] = {
    { NULL, D_dryfield_motel_loft_8017D734, D_dryfield_motel_loft_8017D774, D_dryfield_motel_loft_8017D864, D_dryfield_motel_loft_8017D9A4, 6000, 2500, 3, 2, 4000, 17 },
};

GpViewRec D_dryfield_motel_loft_8017D9E0[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x6978, 0 } }, 541 },
    { { { { 824, 0, -4012 }, { -771, 4019, -158 }, { 3937, 787, 809 } }, { 180, 1430, 1300 } }, 257 },
    { { { { 1131, 0, 3936 }, { 443, 4069, -127 }, { -3911, 461, 1124 } }, { -5880, 1430, 1620 } }, 257 },
    { { { { 388, 0, 4077 }, { -370, 4079, 35 }, { -4060, -372, 386 } }, { -600, 740, 1510 } }, 257 },
    { { { { -3971, 0, -1003 }, { -512, 3522, 2027 }, { 863, 2090, -3414 } }, { 5710, 2360, -2340 } }, 257 },
    { { { { -800, 0, 4016 }, { 475, 4067, 94 }, { -3988, 484, -795 } }, { -960, 1500, -1900 } }, 257 },
    { { { { -1105, 0, -3943 }, { -458, 4068, 128 }, { 3917, 476, -1098 } }, { 4980, 1500, -2080 } }, 257 },
};

GpSprtCmd D_dryfield_motel_loft_8017DADC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_loft_8017DAEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_loft_8017DAFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_motel_loft_8017DB0C[9] = {
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 56, -96, 875, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 56, 0, 875, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 72, -120, 750, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 112 } }, 72, 0, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 40, -80, 1050, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 40, 0, 1050, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -56, 1150, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 32, 0, 1150, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, 24, -48, 1200, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_motel_loft_8017DBC0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_motel_loft_8017DBD8[8] = {
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -64, -120, 750, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, -64, -24, 750, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -96, -120, 650, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, -96, -24, 650, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -128, -120, 600, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, -128, -24, 600, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -160, -120, 575, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -160, -24, 575, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_motel_loft_8017DC78[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_motel_loft_8017DC90[7] = {
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -16, -56, 1325, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -64, 8, 1125, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -64, -72, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -40, -64, 1250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -40, 0, 1250, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, -88, 16, 1000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -88, -80, 1000, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_motel_loft_8017DD1C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_motel_loft_8017DD34[7] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 8, 1500, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 0, -64, 1425, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 0, 0, 1425, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 32, -72, 1250, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, 32, 0, 1250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 64, 16, 1000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 64, -80, 1000, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_motel_loft_8017DDC0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_motel_loft_8017DDD8[7] = {
    { { .empty = D_dryfield_motel_loft_8017DADC }, D_dryfield_motel_loft_8017DADC, NULL },
    { { .empty = D_dryfield_motel_loft_8017DAEC }, D_dryfield_motel_loft_8017DAEC, NULL },
    { { .empty = D_dryfield_motel_loft_8017DAFC }, D_dryfield_motel_loft_8017DAFC, NULL },
    { { .elements = D_dryfield_motel_loft_8017DB0C }, D_dryfield_motel_loft_8017DBC0, NULL },
    { { .elements = D_dryfield_motel_loft_8017DBD8 }, D_dryfield_motel_loft_8017DC78, NULL },
    { { .elements = D_dryfield_motel_loft_8017DC90 }, D_dryfield_motel_loft_8017DD1C, NULL },
    { { .elements = D_dryfield_motel_loft_8017DD34 }, D_dryfield_motel_loft_8017DDC0, NULL },
};

GpObj4C D_dryfield_motel_loft_8017DE2C[12] = {
    { NULL, NULL, NULL, { 3007, -1040, -417, 0 }, { { -4, -1264, -2500, 0 }, { -4, 1264, -2500, 0 }, { -3, -1264, 2489, 0 }, { -3, 1264, 2489, 0 } }, { -4106, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 2782, -1088, -354, 0 }, { { -3, -1264, 2492, 0 }, { -3, 1264, 2492, 0 }, { -3, -1264, -2498, 0 }, { -3, 1264, -2498, 0 } }, { 4104, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 928, -1024, 1199, 0 }, { { 687, -1264, 1236, 0 }, { 687, 1264, 1236, 0 }, { -687, -1264, -1235, 0 }, { -687, 1264, -1235, 0 } }, { 3587, 0, -1998, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 3, 7, 1, 0 },
    { NULL, NULL, NULL, { 991, -1056, 959, 0 }, { { -685, -1264, -1237, 0 }, { -685, 1264, -1237, 0 }, { 686, -1264, 1237, 0 }, { 686, 1264, 1237, 0 } }, { -3582, 0, 1984, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 7, 3, 1, 0 },
    { NULL, NULL, NULL, { -1217, -1120, -1473, 0 }, { { 37, -1264, -1420, 0 }, { 37, 1264, -1420, 0 }, { -59, -1264, 1406, 0 }, { -59, 1264, 1406, 0 } }, { -4094, 0, -141, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1506, -1088, -1665, 0 }, { { 16, -1264, 1410, 0 }, { 16, 1264, 1410, 0 }, { -28, -1264, -1417, 0 }, { -28, 1264, -1417, 0 } }, { 4102, 0, -66, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -4929, -1056, -1248, 0 }, { { 275, -1264, 1373, 0 }, { 275, 1264, 1373, 0 }, { -311, -1264, -1393, 0 }, { -311, 1264, -1393, 0 } }, { 4015, 0, -852, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -4577, -1056, -1312, 0 }, { { -243, -1264, -1408, 0 }, { -243, 1264, -1408, 0 }, { 213, -1264, 1383, 0 }, { 213, 1264, 1383, 0 } }, { -4042, 0, 659, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { -5187, -1056, 703, 0 }, { { 1368, -1264, -360, 0 }, { 1368, 1264, -360, 0 }, { -1367, -1264, 360, 0 }, { -1367, 1264, 360, 0 } }, { -1044, 0, -3962, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { -5282, -1024, 512, 0 }, { { -1367, -1264, 360, 0 }, { -1367, 1264, 360, 0 }, { 1368, -1264, -360, 0 }, { 1368, 1264, -360, 0 } }, { 1044, 0, 3971, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { -1761, -1088, 1472, 0 }, { { -17, -1264, -1414, 0 }, { -17, 1264, -1414, 0 }, { 17, -1264, 1414, 0 }, { 17, 1264, 1414, 0 } }, { -4096, 0, 46, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { -2177, -1056, 1471, 0 }, { { 15, -1264, 1415, 0 }, { 15, 1264, 1415, 0 }, { -14, -1264, -1414, 0 }, { -14, 1264, -1414, 0 } }, { 4095, 0, -43, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 7, 6, 129, 0 },
};

GpObj4C D_dryfield_motel_loft_8017E1BC[2] = {
    { NULL, NULL, NULL, { 4480, -48, -512, 0 }, { { -448, 0, -1024, 0 }, { 448, 0, -1024, 0 }, { -448, 0, 1024, 0 }, { 448, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1115, 0, 29, 20, 2, 0 },
    { NULL, NULL, NULL, { -1264, -64, -80, 0 }, { { -5040, 0, -2672, 0 }, { 5040, 0, -2672, 0 }, { -5040, 0, 2672, 0 }, { 5040, 0, 2672, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 5701, 2, 2, 0, 131, 0 },
};

GpPointLight D_dryfield_motel_loft_8017E254[10] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5800, -1810, 1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2703, 2703, 2703, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4496, -1799, 2067 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1884, 1884, 1884, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5758, -1799, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2703, 2703, 2703, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1944, -1799, -2150 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2703, 2703, 2703, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -66, -1799, -2150 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2703, 2703, 2703, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 508, -1799, 2058 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1884, 1884, 1884, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4505, -1799, -2154 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2703, 2703, 2703, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3201, -1799, -10 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2703, 2703, 2703, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2805, -1799, -10 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2703, 2703, 2703, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2716, -1799, 2056 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1884, 1884, 1884, { 0, 0 } }, 2000, 3000 },
};

GpRoomCoordSet D_dryfield_motel_loft_8017E614[1] = {
    { 0, NULL, 10, D_dryfield_motel_loft_8017E254, 0, NULL },
};

s32 D_dryfield_motel_loft_8017E62C[3] = {
    0x10000035,
    0x10000037,
    0x10000035,
};

GpRoomParamRec D_dryfield_motel_loft_8017E638[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_motel_loft_8017E640[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec * D_dryfield_motel_loft_8017E648[8] = {
    D_dryfield_motel_loft_8017E638,
    D_dryfield_motel_loft_8017E640,
    D_dryfield_motel_loft_8017E638,
    D_dryfield_motel_loft_8017E638,
    D_dryfield_motel_loft_8017E638,
    D_dryfield_motel_loft_8017E638,
    D_dryfield_motel_loft_8017E638,
    D_dryfield_motel_loft_8017E638,
};

static void func_dryfield_motel_loft_8017D610(Task* task);
static void func_dryfield_motel_loft_8017D654(Task* task);

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_motel_loft_8017D5D0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply unchanged and returns 1.
s32 func_dryfield_motel_loft_8017D5D8(Task* task, s32 msgId, RoomEventMsg * in, RoomEventMsg * out)
{
    *out = *in;
    return 1;
}

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_dryfield_motel_loft_8017D600(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_motel_loft_8017D608(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and advances to the next state.
static void func_dryfield_motel_loft_8017D610(Task* task)
{
    task->msgTable = D_dryfield_motel_loft_8017D6B4;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: the room has nothing to do each frame.
static void func_dryfield_motel_loft_8017D654(Task* task)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_motel_loft_8017D5C4 = {
    { func_dryfield_motel_loft_8017D610, func_dryfield_motel_loft_8017D654, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_motel_loft_8017D5C4`.
void func_dryfield_motel_loft_8017D65C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_loft_8017D5C4;
    sp.funcs[task->state](task);
}
