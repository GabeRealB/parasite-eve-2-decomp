#include "rooms/neo_ark_eve_elevator.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

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
#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

/// The room's message table, which state 0 of its event task installs.
extern GpMsgEntry D_neo_ark_eve_elevator_8017D724[];

static void func_neo_ark_eve_elevator_8017D678(Task* task);
static void func_neo_ark_eve_elevator_8017D6BC(Task* task);

/// The event task's three states: install the message table, idle, and kill.
static const TaskFuncTable3 D_neo_ark_eve_elevator_8017D5C4 = {
    {
        func_neo_ark_eve_elevator_8017D678,
        func_neo_ark_eve_elevator_8017D6BC,
        taskKill,
    },
};

s32 func_neo_ark_eve_elevator_8017D5D0(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_eve_elevator_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_neo_ark_eve_elevator_8017D668(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_eve_elevator_8017D670(Task*, s32, GpMessageArg, GpMessageArg);

extern GpGridParams   D_neo_ark_eve_elevator_8017DA2C[1];
extern GpObj4C        D_neo_ark_eve_elevator_8017DBC8[1];
extern GpRoomCoordSet D_neo_ark_eve_elevator_8017DBB0[1];

GpMsgEntry D_neo_ark_eve_elevator_8017D724[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_eve_elevator_8017D5D8 },
    { 5105, func_neo_ark_eve_elevator_8017D5D0 },
    { 5103, func_neo_ark_eve_elevator_8017D670 },
    { 5104, func_neo_ark_eve_elevator_8017D668 },
    { 0x7FFFFFFF, NULL },
};

GpRoomObjRec D_neo_ark_eve_elevator_8017D74C[1] = {
    { D_neo_ark_eve_elevator_8017DA2C, NULL, D_neo_ark_eve_elevator_8017DBC8, NULL },
};

GpRoomCoordRec D_neo_ark_eve_elevator_8017D75C[1] = {
    { D_neo_ark_eve_elevator_8017DBB0, NULL },
};

u8* D_neo_ark_eve_elevator_8017D764[1] = {
    D_8010CAF8,
};

GpViewCountRec D_neo_ark_eve_elevator_8017D768[1] = {
    { { .bytes = { 4, 0 } } },
};

GpWarpRec D_neo_ark_eve_elevator_8017D76C[1] = {
    { { .words = { 3072, -960, 0, 0 } }, { 0, 0, 0, 0 }, { .words = { 3072, -960, 0, 0 } }, { 0, 0, 0, 0 }, 0x55090002, 0x55090001, 0, 2, 0, 444 },
};

SVECTOR D_neo_ark_eve_elevator_8017D7A4[14] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_normals.inc"
};

SVECTOR D_neo_ark_eve_elevator_8017D814[24] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_verts.inc"
};

GpGridFace D_neo_ark_eve_elevator_8017D8D4[24] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_faces.inc"
};

s16 D_neo_ark_eve_elevator_8017D9F4[26] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_cells.inc"
};

#define GRID_CELL(i) (&D_neo_ark_eve_elevator_8017D9F4[i])
s16* D_neo_ark_eve_elevator_8017DA28[1] = {
#include "assets/neo_ark_eve_elevator_collision_0046C_table.inc"
};
#undef GRID_CELL

GpGridParams D_neo_ark_eve_elevator_8017DA2C[1] = {
    { NULL, D_neo_ark_eve_elevator_8017D7A4, D_neo_ark_eve_elevator_8017D814, D_neo_ark_eve_elevator_8017D8D4, D_neo_ark_eve_elevator_8017DA28, 2050, 1050, 1, 1, 4000, 24 },
};

GpViewRec D_neo_ark_eve_elevator_8017DA50[4] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 1000, 0x4E20, 0 } }, 1371 },
    { { { { 943, 0, -3985 }, { -3917, 755, -927 }, { 735, 4025, 174 } }, { 1340, 3520, 90 } }, 207 },
    { { { { 943, 0, -3985 }, { -3917, 755, -927 }, { 735, 4025, 174 } }, { 1340, 3520, 90 } }, 207 },
    { { { { 943, 0, -3985 }, { -3917, 755, -927 }, { 735, 4025, 174 } }, { 1340, 3520, 90 } }, 207 },
};

SpriteBatch D_neo_ark_eve_elevator_8017DAE0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_eve_elevator_8017DAF0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_eve_elevator_8017DB00[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_eve_elevator_8017DB10[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_neo_ark_eve_elevator_8017DB20[4] = {
    { { .empty = D_neo_ark_eve_elevator_8017DAE0 }, D_neo_ark_eve_elevator_8017DAE0, NULL },
    { { .empty = D_neo_ark_eve_elevator_8017DAF0 }, D_neo_ark_eve_elevator_8017DAF0, NULL },
    { { .empty = D_neo_ark_eve_elevator_8017DB00 }, D_neo_ark_eve_elevator_8017DB00, NULL },
    { { .empty = D_neo_ark_eve_elevator_8017DB10 }, D_neo_ark_eve_elevator_8017DB10, NULL },
};

GpPointLight D_neo_ark_eve_elevator_8017DB50[1] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -1742, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1228, 2867, 2621, { 0, 0 } }, 1500, 2500 },
};

GpRoomCoordSet D_neo_ark_eve_elevator_8017DBB0[1] = {
    { 0, NULL, 1, D_neo_ark_eve_elevator_8017DB50, 0, NULL },
};

GpObj4C D_neo_ark_eve_elevator_8017DBC8[1] = {
    { NULL, NULL, NULL, { -240, -48, 48, 0 }, { { -304, 0, -976, 0 }, { 304, 0, -976, 0 }, { -304, 0, 976, 0 }, { 304, 0, 976, 0 } }, { 0, 4109, 0, 0 }, { -4096, 0, 0, 0 }, 1021, 0, 24, 17, 130, 0 },
};

s32 D_neo_ark_eve_elevator_8017DC14[3] = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

GpRoomParamRec D_neo_ark_eve_elevator_8017DC20[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_neo_ark_eve_elevator_8017DC28[1] = {
    { 0, 0, 1, 0, D_neo_ark_eve_elevator_8017DC14 },
};

GpRoomParamRec* D_neo_ark_eve_elevator_8017DC30[8] = {
    D_neo_ark_eve_elevator_8017DC20,
    D_neo_ark_eve_elevator_8017DC28,
    D_neo_ark_eve_elevator_8017DC20,
    D_neo_ark_eve_elevator_8017DC20,
    D_neo_ark_eve_elevator_8017DC20,
    D_neo_ark_eve_elevator_8017DC20,
    D_neo_ark_eve_elevator_8017DC20,
    D_neo_ark_eve_elevator_8017DC20,
};

/// The room's handler for message 0x13F1: does nothing and returns 0.
s32 func_neo_ark_eve_elevator_8017D5D0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming record onto the
/// outgoing one and passes both to `func_map_neo_ark_80179B14`. It returns 1 unless the
/// record's `msgId` is 0x18 and `CdCmd_IsIdle` returns 0; in that case it
/// returns 0, first starting cap event 1 through `Gp_SpawnIfCapIdle` when the
/// record's `queryOnly` is 0.
s32 func_neo_ark_eve_elevator_8017D5D8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    if (in->areaId != 0x18) {
        return 1;
    }
    if (CdCmd_IsIdle() != 0) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 0;
    }
    Gp_SpawnIfCapIdle(1, 1);
    return 0;
}

/// The room's handler for message 0x13F0: does nothing and returns 0.
s32 func_neo_ark_eve_elevator_8017D668(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// The room's handler for message 0x13EF: does nothing and returns 0.
s32 func_neo_ark_eve_elevator_8017D670(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances to state 1.
static void func_neo_ark_eve_elevator_8017D678(Task* task)
{
    task->msgTable = D_neo_ark_eve_elevator_8017D724;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's event task: does nothing, so the task idles here.
static void func_neo_ark_eve_elevator_8017D6BC(Task* task)
{
}

/// The room's event task: runs the handler for its current state, through a
/// stack copy of the state table.
void func_neo_ark_eve_elevator_8017D6C4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_eve_elevator_8017D5C4;
    sp.funcs[task->state](task);
}

/// An empty function nothing in the room's tables names.
void func_neo_ark_eve_elevator_8017D71C(Task* unused)
{
}
