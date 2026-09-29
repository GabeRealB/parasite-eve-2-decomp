#include "rooms/shelter_1f_heliport_s4.h"

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

#include "mapui/map_shelter.h"

/// The room's message table, handed to its event task in state 0.
extern GpMsgEntry D_shelter_1f_heliport_s4_8017D6D0[];

s32 func_shelter_1f_heliport_s4_8017D5D0(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_1f_heliport_s4_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_1f_heliport_s4_8017D61C(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_1f_heliport_s4_8017D624(Task*, s32, GpMessageArg, GpMessageArg);

GpMsgEntry D_shelter_1f_heliport_s4_8017D6D0[5] = {
    { 5102, func_shelter_1f_heliport_s4_8017D5D8 },
    { 5105, func_shelter_1f_heliport_s4_8017D5D0 },
    { 5103, func_shelter_1f_heliport_s4_8017D624 },
    { 5104, func_shelter_1f_heliport_s4_8017D61C },
    { 0x7FFFFFFF, NULL },
};

u8 * D_shelter_1f_heliport_s4_8017D6F8[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_1f_heliport_s4_8017D6FC[1] = {
    { { .bytes = { 5, 0 } } },
};

GpWarpRec D_shelter_1f_heliport_s4_8017D700[1] = {
    { { .words = { 0, 8000, 0, 1632 } }, { 0, 0, 0, 0 }, { .words = { 0, 8000, 0, 1632 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
};

SVECTOR D_shelter_1f_heliport_s4_8017D738[7] = {
    { 0, 0, -4096, 0 },
    { -4096, 0, 0, 0 },
    { -2896, 0, 2896, 0 },
    { 0, 0, 4096, 0 },
    { 4096, 0, 0, 0 },
    { 0, -4096, 0, 0 },
    { 4096, 0, 0, 0 },
};

SVECTOR D_shelter_1f_heliport_s4_8017D770[34] = {
    { 3000, 0, 6000, 0 },
    { 3000, -3000, 6000, 0 },
    { 9000, -3000, 6000, 0 },
    { 9000, 0, 6000, 0 },
    { 3000, 0, 8000, 0 },
    { 3000, -3000, 8000, 0 },
    { 9500, -3000, 6000, 0 },
    { 9500, 0, 6000, 0 },
    { 9000, -3000, 0, 0 },
    { 9000, 0, 0, 0 },
    { 9500, 0, 500, 0 },
    { 9500, -3000, 500, 0 },
    { 9000, -3000, -2000, 0 },
    { 9000, 0, -2000, 0 },
    { 9000, 0, 0, 0 },
    { 9000, -3000, 0, 0 },
    { 7000, -3000, -2000, 0 },
    { 7000, 0, -2000, 0 },
    { 7000, -3000, 1000, 0 },
    { 7000, 0, 1000, 0 },
    { 0, -3000, 1000, 0 },
    { 0, 0, 1000, 0 },
    { 0, 0, 0, 0 },
    { 9000, 0, 0, 0 },
    { 0, 0, -2000, 0 },
    { 9000, 0, 7000, 0 },
    { 0x2710, 0, 7000, 0 },
    { 0x2710, 0, 0, 0 },
    { 0, 0, 7000, 0 },
    { 0, 0, 8000, 0 },
    { 9000, 0, 8000, 0 },
    { 0, -3000, 8000, 0 },
    { 0, 0, 8000, 0 },
    { 0, -3000, 8000, 0 },
};

GpGridFace D_shelter_1f_heliport_s4_8017D880[15] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 5, 1, 4, 0 }, 1, 0 },
    { { 7, 3, 6, 2 }, 0, 0 },
    { { 9, 10, 8, 11 }, 2, 0 },
    { { 10, 7, 11, 6 }, 1, 0 },
    { { 13, 14, 12, 15 }, 1, 0 },
    { { 17, 13, 16, 12 }, 3, 0 },
    { { 16, 18, 17, 19 }, 4, 0 },
    { { 18, 20, 19, 21 }, 3, 0 },
    { { 23, 13, 22, 24 }, 5, 0 },
    { { 25, 26, 23, 27 }, 5, 0 },
    { { 22, 28, 23, 25 }, 5, 0 },
    { { 29, 30, 28, 25 }, 5, 0 },
    { { 20, 31, 21, 29 }, 6, 0 },
    { { 33, 5, 32, 4 }, 0, 0 },
};

s16 D_shelter_1f_heliport_s4_8017D934[5] = {
    8,
    9,
    11,
    13,
    -1,
};

s16 D_shelter_1f_heliport_s4_8017D940[5] = {
    0,
    1,
    11,
    13,
    -1,
};

s16 D_shelter_1f_heliport_s4_8017D94C[7] = {
    0,
    1,
    11,
    12,
    13,
    14,
    -1,
};

s16 D_shelter_1f_heliport_s4_8017D95C[6] = {
    6,
    7,
    8,
    9,
    11,
    -1,
};

s16 D_shelter_1f_heliport_s4_8017D968[3] = {
    0,
    11,
    -1,
};

s16 D_shelter_1f_heliport_s4_8017D970[4] = {
    0,
    11,
    12,
    -1,
};

s16 D_shelter_1f_heliport_s4_8017D978[8] = {
    3,
    4,
    5,
    6,
    9,
    10,
    11,
    -1,
};

s16 D_shelter_1f_heliport_s4_8017D988[6] = {
    0,
    2,
    4,
    10,
    11,
    -1,
};

s16 D_shelter_1f_heliport_s4_8017D994[7] = {
    0,
    2,
    4,
    10,
    11,
    12,
    -1,
};

s16 * D_shelter_1f_heliport_s4_8017D9A4[9] = {
    D_shelter_1f_heliport_s4_8017D934,
    D_shelter_1f_heliport_s4_8017D940,
    D_shelter_1f_heliport_s4_8017D94C,
    D_shelter_1f_heliport_s4_8017D95C,
    D_shelter_1f_heliport_s4_8017D968,
    D_shelter_1f_heliport_s4_8017D970,
    D_shelter_1f_heliport_s4_8017D978,
    D_shelter_1f_heliport_s4_8017D988,
    D_shelter_1f_heliport_s4_8017D994,
};

GpGridParams D_shelter_1f_heliport_s4_8017D9C8 = { NULL, D_shelter_1f_heliport_s4_8017D738, D_shelter_1f_heliport_s4_8017D770, D_shelter_1f_heliport_s4_8017D880, D_shelter_1f_heliport_s4_8017D9A4, 0, 2000, 3, 3, 4000, 15 };

GpViewRec D_shelter_1f_heliport_s4_8017D9EC[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4500, 0x3CE8, -3400 } }, 235 },
    { { { { 600, 0, -4051 }, { -345, 4081, -51 }, { 4037, 348, 598 } }, { -1395, 1503, -3025 } }, 225 },
    { { { { 577, 0, 4055 }, { 345, 4081, -49 }, { -4040, 349, 575 } }, { -8585, 1503, -2595 } }, 225 },
    { { { { 3910, 0, 1218 }, { -32, 4094, 105 }, { -1217, -110, 3909 } }, { -2572, 1124, -1708 } }, 269 },
    { { { { -4049, 0, 618 }, { -54, 4079, -359 }, { -615, -363, -4033 } }, { -8235, 928, -4313 } }, 289 },
};

GpSprtCmd D_shelter_1f_heliport_s4_8017DAA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_1f_heliport_s4_8017DAB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_1f_heliport_s4_8017DAC0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_1f_heliport_s4_8017DAD0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_1f_heliport_s4_8017DAE0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_1f_heliport_s4_8017DAF0[5] = {
    { { .empty = D_shelter_1f_heliport_s4_8017DAA0 }, D_shelter_1f_heliport_s4_8017DAA0, NULL },
    { { .empty = D_shelter_1f_heliport_s4_8017DAB0 }, D_shelter_1f_heliport_s4_8017DAB0, NULL },
    { { .empty = D_shelter_1f_heliport_s4_8017DAC0 }, D_shelter_1f_heliport_s4_8017DAC0, NULL },
    { { .empty = D_shelter_1f_heliport_s4_8017DAD0 }, D_shelter_1f_heliport_s4_8017DAD0, NULL },
    { { .empty = D_shelter_1f_heliport_s4_8017DAE0 }, D_shelter_1f_heliport_s4_8017DAE0, NULL },
};

GpLight D_shelter_1f_heliport_s4_8017DB2C[4] = {
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1028, 1028, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3076, 3076, 3076, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1028, 1028, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 409, 409, 409, { 0, 0 } },
};

GpPointLight D_shelter_1f_heliport_s4_8017DC8C[5] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4500, -2620, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2048, 2048, 2048, { 0, 0 } }, 1339, 2360 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3200, -2159, -922 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 2220, 3442 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -766, -2000, -4618 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1319, 2059 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1240, -5082, 2801 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 7000, 7001 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3966, -2581, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1800, 3001 },
};

GpRoomCoordSet D_shelter_1f_heliport_s4_8017DE6C = { 4, D_shelter_1f_heliport_s4_8017DB2C, 5, D_shelter_1f_heliport_s4_8017DC8C, 0, NULL };

GpObj4C D_shelter_1f_heliport_s4_8017DE84[6] = {
    { NULL, NULL, NULL, { 5008, -1296, 3536, 0 }, { { 588, -2112, -2769, 0 }, { -611, -2112, 2745, 0 }, { 588, 2112, -2769, 0 }, { -611, 2112, 2745, 0 } }, { 4004, 0, 870, 0 }, { 0, 0, 4096, 0 }, 3519, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 5153, -1360, 3536, 0 }, { { -624, -2112, 2765, 0 }, { 610, -2112, -2781, 0 }, { -624, 2112, 2765, 0 }, { 610, 2112, -2781, 0 } }, { -4013, 0, -893, 0 }, { 0, 0, 4096, 0 }, 3537, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 7968, -1312, 848, 0 }, { { 1504, -2112, 64, 0 }, { -1504, -2112, -64, 0 }, { 1504, 2112, 64, 0 }, { -1504, 2112, -64, 0 } }, { -175, 0, 4092, 0 }, { 0, 0, 4096, 0 }, 2585, 0, 2, 5, 1, 0 },
    { NULL, NULL, NULL, { 8080, -1280, 960, 0 }, { { -1632, -2112, -64, 0 }, { 1632, -2112, 64, 0 }, { -1632, 2112, -64, 0 }, { 1632, 2112, 64, 0 } }, { 160, 0, -4095, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 5, 2, 1, 0 },
    { NULL, NULL, NULL, { 1455, -1377, 5314, 0 }, { { -2320, -2112, -880, 0 }, { 2320, -2112, 880, 0 }, { -2320, 2112, -880, 0 }, { 2320, 2112, 880, 0 } }, { 1459, 0, -3847, 0 }, { 0, 0, 4096, 0 }, 3258, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 1537, -1440, 5217, 0 }, { { 2352, -2112, 928, 0 }, { -2352, -2112, -928, 0 }, { 2352, 2112, 928, 0 }, { -2352, 2112, -928, 0 } }, { -1509, 0, 3823, 0 }, { 0, 0, 4096, 0 }, 3288, 0, 4, 3, 129, 0 },
};

s32 D_shelter_1f_heliport_s4_8017E04C[3] = {
    0x10000011,
    0x10000013,
    0x10000011,
};

GpRoomParamRec D_shelter_1f_heliport_s4_8017E058[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec * D_shelter_1f_heliport_s4_8017E060[8] = {
    D_shelter_1f_heliport_s4_8017E058,
    D_shelter_1f_heliport_s4_8017E058,
    D_shelter_1f_heliport_s4_8017E058,
    D_shelter_1f_heliport_s4_8017E058,
    D_shelter_1f_heliport_s4_8017E058,
    D_shelter_1f_heliport_s4_8017E058,
    D_shelter_1f_heliport_s4_8017E058,
    D_shelter_1f_heliport_s4_8017E058,
};

static void func_shelter_1f_heliport_s4_8017D62C(Task* task);
static void func_shelter_1f_heliport_s4_8017D670(Task* task);

s32 func_shelter_1f_heliport_s4_8017D5D0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming record onto the
/// outgoing one, hands both to `func_map_shelter_80179A04` and returns 1.
s32 func_shelter_1f_heliport_s4_8017D5D8(Task* arg0, s32 arg1, RoomEventMsg * in, RoomEventMsg * out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

s32 func_shelter_1f_heliport_s4_8017D61C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_1f_heliport_s4_8017D624(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances to state 1.
static void func_shelter_1f_heliport_s4_8017D62C(Task* task)
{
    task->msgTable = D_shelter_1f_heliport_s4_8017D6D0;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's event task: does nothing, so the task idles here.
static void func_shelter_1f_heliport_s4_8017D670(Task* task)
{
}

/// The event task's three states: install the message table, idle, and kill.
static const TaskFuncTable3 D_shelter_1f_heliport_s4_8017D5C4 = {
    {
        func_shelter_1f_heliport_s4_8017D62C,
        func_shelter_1f_heliport_s4_8017D670,
        taskKill,
    },
};

/// The room's event task: runs the handler for its current state, through a
/// stack copy of the state table.
void func_shelter_1f_heliport_s4_8017D678(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_heliport_s4_8017D5C4;
    sp.funcs[task->state](task);
}
