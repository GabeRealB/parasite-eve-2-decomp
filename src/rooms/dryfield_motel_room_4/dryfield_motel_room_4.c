#include "rooms/dryfield_motel_room_4.h"

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
extern GpMsgEntry D_dryfield_motel_room_4_8017D6B4[];

s32 func_dryfield_motel_room_4_8017D5D0(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_dryfield_motel_room_4_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_motel_room_4_8017D600(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_dryfield_motel_room_4_8017D608(Task*, s32, TaskMessageArg, TaskMessageArg);

extern GpGridParams   D_dryfield_motel_room_4_8017DDF0[1];
extern GpObj4C        D_dryfield_motel_room_4_8017DF94[8];
extern GpObj4C        D_dryfield_motel_room_4_8017E1F4[1];
extern GpRoomCoordSet D_dryfield_motel_room_4_8017E420[1];

GpMsgEntry D_dryfield_motel_room_4_8017D6B4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_motel_room_4_8017D5D8 },
    { 5105, func_dryfield_motel_room_4_8017D5D0 },
    { 5103, func_dryfield_motel_room_4_8017D608 },
    { 5104, func_dryfield_motel_room_4_8017D600 },
    { 0x7FFFFFFF, NULL },
};

GpRoomObjRec D_dryfield_motel_room_4_8017D6DC[1] = {
    { D_dryfield_motel_room_4_8017DDF0, D_dryfield_motel_room_4_8017DF94, D_dryfield_motel_room_4_8017E1F4, NULL },
};

u8* D_dryfield_motel_room_4_8017D6EC[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_motel_room_4_8017D6F0[1] = {
    { { .bytes = { 6, 0 } } },
};

GpRoomCoordRec D_dryfield_motel_room_4_8017D6F4[1] = {
    { D_dryfield_motel_room_4_8017E420, NULL },
};

GpWarpRec D_dryfield_motel_room_4_8017D6FC[1] = {
    { { .words = { 0, 3400, 0, 675 } }, { 0, 0, 0, 0 }, { .words = { 0, 3400, 0, 675 } }, { 0, 0, 0, 0 }, 0x520E0002, 0x520E0001, 0, 2, 0, 483 },
};

SVECTOR D_dryfield_motel_room_4_8017D734[14] = {
#include "assets/dryfield_motel_room_4_collision_00830_normals.inc"
};

SVECTOR D_dryfield_motel_room_4_8017D7A4[103] = {
#include "assets/dryfield_motel_room_4_collision_00830_verts.inc"
};

GpGridFace D_dryfield_motel_room_4_8017DADC[47] = {
#include "assets/dryfield_motel_room_4_collision_00830_faces.inc"
};

s16 D_dryfield_motel_room_4_8017DD10[104] = {
#include "assets/dryfield_motel_room_4_collision_00830_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_motel_room_4_8017DD10[i])
s16* D_dryfield_motel_room_4_8017DDE0[4] = {
#include "assets/dryfield_motel_room_4_collision_00830_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_motel_room_4_8017DDF0[1] = {
    { NULL, D_dryfield_motel_room_4_8017D734, D_dryfield_motel_room_4_8017D7A4, D_dryfield_motel_room_4_8017DADC, D_dryfield_motel_room_4_8017DDE0, -200, -200, 2, 2, 4000, 47 },
};

GpViewRec D_dryfield_motel_room_4_8017DE14[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3500, 0x2904, -2500 } }, 289 },
    { { { { 652, 0, 4043 }, { 208, 4090, -33 }, { -4038, 211, 651 } }, { -6800, 1250, -950 } }, 275 },
    { { { { 3984, 0, -951 }, { -39, 4092, -166 }, { 950, 171, 3980 } }, { -1200, 1450, -200 } }, 240 },
    { { { { 660, 0, -4042 }, { -57, 4095, -9 }, { 4042, 58, 660 } }, { -1400, 1250, -1050 } }, 275 },
    { { { { 209, 0, 4090 }, { 1972, 3588, -100 }, { -3583, 1975, 183 } }, { -6800, 2170, -3550 } }, 263 },
    { { { { 866, 0, -4003 }, { -172, 4092, -37 }, { 3999, 176, 865 } }, { -2200, 1160, -3120 } }, 269 },
};

SpriteBatch D_dryfield_motel_room_4_8017DEEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_4_8017DEFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_4_8017DF0C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_4_8017DF1C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_4_8017DF2C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_4_8017DF3C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_motel_room_4_8017DF4C[6] = {
    { { .empty = D_dryfield_motel_room_4_8017DEEC }, D_dryfield_motel_room_4_8017DEEC, NULL },
    { { .empty = D_dryfield_motel_room_4_8017DEFC }, D_dryfield_motel_room_4_8017DEFC, NULL },
    { { .empty = D_dryfield_motel_room_4_8017DF0C }, D_dryfield_motel_room_4_8017DF0C, NULL },
    { { .empty = D_dryfield_motel_room_4_8017DF1C }, D_dryfield_motel_room_4_8017DF1C, NULL },
    { { .empty = D_dryfield_motel_room_4_8017DF2C }, D_dryfield_motel_room_4_8017DF2C, NULL },
    { { .empty = D_dryfield_motel_room_4_8017DF3C }, D_dryfield_motel_room_4_8017DF3C, NULL },
};

GpObj4C D_dryfield_motel_room_4_8017DF94[8] = {
    { NULL, NULL, NULL, { 1872, -1136, 2736, 0 }, { { -1808, -2160, 16, 0 }, { 1808, -2160, -16, 0 }, { -1808, 2160, 16, 0 }, { 1808, 2160, -16, 0 } }, { -37, 0, -4105, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 1888, -1088, 2656, 0 }, { { 1808, -2112, -16, 0 }, { -1808, -2112, 16, 0 }, { 1808, 2112, -16, 0 }, { -1808, 2112, 16, 0 } }, { 36, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 2769, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 4511, -1152, 1519, 0 }, { { -17, -2176, -1316, 0 }, { 14, -2176, 1313, 0 }, { -17, 2176, -1316, 0 }, { 14, 2176, 1313, 0 } }, { 4095, 0, -49, 0 }, { 0, 0, 4096, 0 }, 2534, 0, 4, 2, 1, 0 },
    { NULL, NULL, NULL, { 4639, -1120, 1471, 0 }, { { 15, -2144, 1314, 0 }, { -16, -2144, -1315, 0 }, { 15, 2144, 1314, 0 }, { -16, 2144, -1315, 0 } }, { -4107, 0, 47, 0 }, { 0, 0, 4096, 0 }, 2508, 0, 2, 4, 1, 0 },
    { NULL, NULL, NULL, { 3647, -1120, 3502, 0 }, { { 0, -2144, -738, 0 }, { 0, -2144, 738, 0 }, { 0, 2144, -738, 0 }, { 0, 2144, 738, 0 } }, { 4105, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2260, 0, 5, 3, 1, 0 },
    { NULL, NULL, NULL, { 3776, -1136, 3488, 0 }, { { 0, -2160, 738, 0 }, { 0, -2160, -738, 0 }, { 0, 2160, 738, 0 }, { 0, 2160, -738, 0 } }, { -4109, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2275, 0, 3, 5, 1, 0 },
    { NULL, NULL, NULL, { 5456, -1088, 3840, 0 }, { { -112, -2160, 1026, 0 }, { 112, -2160, -1026, 0 }, { -112, 2160, 1026, 0 }, { 112, 2160, -1026, 0 } }, { -4082, 0, -447, 0 }, { 0, 0, 4096, 0 }, 2387, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 5344, -1088, 3808, 0 }, { { 112, -2160, -1026, 0 }, { -112, -2160, 1026, 0 }, { 112, 2160, -1026, 0 }, { -112, 2160, 1026, 0 } }, { 4079, 0, 444, 0 }, { 0, 0, 4096, 0 }, 2387, 0, 6, 5, 129, 0 },
};

GpObj4C D_dryfield_motel_room_4_8017E1F4[1] = {
    { NULL, NULL, NULL, { 3584, -48, 512, 0 }, { { -832, 0, -320, 0 }, { 832, 0, -320, 0 }, { -832, 0, 320, 0 }, { 832, 0, 320, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 891, 0, 2, 22, 130, 0 },
};

GpPointLight D_dryfield_motel_room_4_8017E240[5] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5369, -1319, 5201 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2700, 2700, 2700, { 0, 0 } }, 2100, 4442 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4280, -1466, 1405 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4500, 4500, 4500, { 0, 0 } }, 1020, 2601 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1051, -1619, 2995 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2600, 2600, 2600, { 0, 0 } }, 2485, 3743 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1025, -1772, -171 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 5440, 5440, 5440, { 0, 0 } }, 2140, 3071 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5111, -1523, 416 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3600, 3600, 3600, { 0, 0 } }, 788, 1130 },
};

GpRoomCoordSet D_dryfield_motel_room_4_8017E420[1] = {
    { 0, NULL, 5, D_dryfield_motel_room_4_8017E240, 0, NULL },
};

s32 D_dryfield_motel_room_4_8017E438[3] = {
    0x10000035,
    0x10000037,
    0x10000035,
};

s32 D_dryfield_motel_room_4_8017E444[3] = {
    0x10000001,
    0x10000003,
    0x10000001,
};

GpRoomParamRec D_dryfield_motel_room_4_8017E450[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_motel_room_4_8017E458[1] = {
    { 0, 0, 1, 0, D_dryfield_motel_room_4_8017E438 },
};

GpRoomParamRec D_dryfield_motel_room_4_8017E460[1] = {
    { 0, 0, 1, 0, D_dryfield_motel_room_4_8017E444 },
};

GpRoomParamRec D_dryfield_motel_room_4_8017E468[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec* D_dryfield_motel_room_4_8017E470[8] = {
    D_dryfield_motel_room_4_8017E450,
    D_dryfield_motel_room_4_8017E458,
    D_dryfield_motel_room_4_8017E460,
    D_dryfield_motel_room_4_8017E468,
    D_dryfield_motel_room_4_8017E450,
    D_dryfield_motel_room_4_8017E450,
    D_dryfield_motel_room_4_8017E450,
    D_dryfield_motel_room_4_8017E450,
};

static void func_dryfield_motel_room_4_8017D610(Task* task);
static void func_dryfield_motel_room_4_8017D654(Task* task);

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_motel_room_4_8017D5D0(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply unchanged and returns 1.
s32 func_dryfield_motel_room_4_8017D5D8(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    return 1;
}

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_dryfield_motel_room_4_8017D600(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_motel_room_4_8017D608(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and advances to the next state.
static void func_dryfield_motel_room_4_8017D610(Task* task)
{
    task->msgTable = D_dryfield_motel_room_4_8017D6B4;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: the room has nothing to do each frame.
static void func_dryfield_motel_room_4_8017D654(Task* task)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_motel_room_4_8017D5C4 = {
    { func_dryfield_motel_room_4_8017D610, func_dryfield_motel_room_4_8017D654, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_motel_room_4_8017D5C4`.
void func_dryfield_motel_room_4_8017D65C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_room_4_8017D5C4;
    sp.funcs[task->state](task);
}
