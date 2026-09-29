#include "rooms/neo_ark_r31.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

/// Room message handler table installed into `Task::msgTable`.
extern GpMsgEntry D_neo_ark_r31_8017D9F4[];
extern s32        D_80133F90;
extern s32        D_80134470;

s32  func_neo_ark_r31_8017D8B0(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_neo_ark_r31_8017D8B8(Task*, s32, GpSaveLoc*, GpSaveLoc*);
s32  func_neo_ark_r31_8017D8FC(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_neo_ark_r31_8017D904(Task*, s32, GpMessageArg, GpMessageArg);
void func_neo_ark_r31_8017D5D0(Task*);

TaskDesc D_neo_ark_r31_8017D9E8 = { 0, 192, func_neo_ark_r31_8017D5D0, { .model = NULL } };

GpMsgEntry D_neo_ark_r31_8017D9F4[5] = {
    { 5102, func_neo_ark_r31_8017D8B8 },
    { 5105, func_neo_ark_r31_8017D8B0 },
    { 5103, func_neo_ark_r31_8017D904 },
    { 5104, func_neo_ark_r31_8017D8FC },
    { 0x7FFFFFFF, NULL },
};

u8* D_neo_ark_r31_8017DA1C[1] = {
    D_8010CAF8,
};

GpViewCountRec D_neo_ark_r31_8017DA20[1] = {
    { { .bytes = { 3, 0 } } },
};

GpWarpRec D_neo_ark_r31_8017DA24[1] = {
    { { .words = { 2048, 0, 0, 0 } }, { 0, 0, 0, 0 }, { .words = { 2048, 0, 0, 0 } }, { 0, 0, 0, 0 }, 0, 0, 0, 1, 0, 0 },
};

GpViewRec D_neo_ark_r31_8017DA5C[3] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7510, 0x61A8, -6980 } }, 329 },
    { { { { 2889, 0, -2903 }, { 2898, 236, 2884 }, { 167, -4089, 167 } }, { -8000, -1000, -7000 } }, 289 },
    { { { { -3243, 0, 2501 }, { 1904, 2655, 2469 }, { -1621, 3118, -2102 } }, { -8340, 1050, -7830 } }, 289 },
};

GpSprtCmd D_neo_ark_r31_8017DAC8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_neo_ark_r31_8017DAD8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_neo_ark_r31_8017DAE8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_neo_ark_r31_8017DAF8[3] = {
    { { .empty = D_neo_ark_r31_8017DAC8 }, D_neo_ark_r31_8017DAC8, NULL },
    { { .empty = D_neo_ark_r31_8017DAD8 }, D_neo_ark_r31_8017DAD8, NULL },
    { { .empty = D_neo_ark_r31_8017DAE8 }, D_neo_ark_r31_8017DAE8, NULL },
};

GpPointLight D_neo_ark_r31_8017DB1C[1] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -0x2710, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 0x186A0, 0x186A0 },
};

GpRoomCoordSet D_neo_ark_r31_8017DB7C = { 0, NULL, 1, D_neo_ark_r31_8017DB1C, 0, NULL };

GpAreaTmdRec D_neo_ark_r31_8017DB94[3] = {
    { 101, 618, 3, 0, { 0, 0 }, D_80139F8C },
    { 132, 618, 5, 0, { 0, 0 }, D_801437EC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_neo_ark_r31_8017DBB8[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017C550, D_neo_ark_r31_8017DB94 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

s32 D_neo_ark_r31_8017DC20[3] = {
    0x10000011,
    0x10000013,
    0x10000011,
};

GpRoomParamRec D_neo_ark_r31_8017DC2C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec* D_neo_ark_r31_8017DC34[8] = {
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
};

s32 D_neo_ark_r31_8017DC54 = 0;

static void func_neo_ark_r31_8017D90C(Task* arg0);
static void func_neo_ark_r31_8017D980(Task* task);

void func_neo_ark_r31_8017D5D0(Task* task)
{
    POLY_FT4* poly;
    DR_STP*   stp;
    s32       buf;
    s32       otz;
    s32       x;
    s32       y;
    s32       sx;
    s32       sy;
    s32       px;

    otz = 6;
    buf = gDisplayState.otBuffer;
    if (task->state == 0) {
        D_neo_ark_r31_8017DC54 = 3;
        task->state++;
    }
    if (D_neo_ark_r31_8017DC54 < 0) {
        Task_CallExit(task);
        return;
    }
    for (x = 0; x < 0x140; x += 0xA0) {
        sx = x - 0xA0;
        for (y = 0; y < 0xF0; y += 0xF0) {
            sy              = y - 0x78;
            poly            = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor += sizeof(POLY_FT4);
            poly->tpage     = getTPage(2, 0, x & ~0x3F, buf << 8);
            poly->y0 = poly->y1 = sy;
            poly->v0 = poly->v1 = (y + (buf << 4)) + gDisplayState.vramYOffset;
            if (poly->v0 < 0x10) {
                poly->y2 = poly->y3 = y + 0x78;
                poly->v2 = poly->v3 = poly->v0 + 0xF0;
            } else {
                s32 d    = 0xFF - poly->v0;
                poly->y2 = poly->y3 = sy + d;
                poly->v2 = poly->v3 = poly->v0 + d;
            }
            px       = sx - D_neo_ark_r31_8017DC54;
            poly->x0 = poly->x2 = px;
            poly->u0 = poly->u2 = x & 0x3F;
            if (poly->u0 < 0x60) {
                poly->x1 = poly->x3 = poly->x0 + 0xA0;
                poly->u1 = poly->u3 = poly->u0 + 0xA0;
            } else {
                s32 d    = 0xFF - poly->u0;
                poly->x1 = poly->x3 = poly->x0 + d;
                poly->u1 = poly->u3 = poly->u0 + d;
            }
            setlen(poly, 9);
            setcode(poly, 0x2F);
            addPrim(gGpuCurrentOt + otz, poly);
        }
    }
    stp             = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor += sizeof(DR_STP);
    SetDrawStp(stp, 0);
    addPrim(gGpuCurrentOt + otz, stp);
    stp             = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor += sizeof(DR_STP);
    SetDrawStp(stp, 1);
    addPrim(gGpuCurrentOt + 0x3FF, stp);
}

s32 func_neo_ark_r31_8017D8B0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message handler for the save location: copies the incoming `GpSaveLoc`
/// onto the outgoing one and passes both to `func_map_neo_ark_80179B14`. Returns 1.
s32 func_neo_ark_r31_8017D8B8(Task* arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

s32 func_neo_ark_r31_8017D8FC(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_neo_ark_r31_8017D904(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Room task state 0: installs the message table, claims pointer slot 7,
/// sets `CdCmd_Queue.field_22A` to 2 and starts the room script with
/// `func_800E8634`. Advances to state 1.
static void func_neo_ark_r31_8017D90C(Task* arg0)
{
    CdCmdQueue* queue;

    queue          = &CdCmd_Queue;
    arg0->msgTable = D_neo_ark_r31_8017D9F4;
    Game_SetPtrSlot(arg0, 7);
    queue->field_22A = 2;
    func_800E8634(&D_80133F90, 0, &D_80134470);
    arg0->state = (s32)(arg0->state + 1);
}

/// Room task state 1: stores 2 into `CdCmd_Queue.field_22A` every tick.
static void func_neo_ark_r31_8017D980(Task* task)
{
    CdCmd_Queue.field_22A = 2;
}

/// State handlers of the room task `func_neo_ark_r31_8017D990`, indexed by
/// `Task::state`: the set-up tick, the tick that stores 2 into `CdCmd_Queue.field_22A`,
/// and `taskKill`.
static const TaskFuncTable3 D_neo_ark_r31_8017D5C4 = {
    {
        func_neo_ark_r31_8017D90C,
        func_neo_ark_r31_8017D980,
        taskKill,
    },
};

/// Room task: dispatches through a stack copy of its state table.
void func_neo_ark_r31_8017D990(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_r31_8017D5C4;
    sp.funcs[task->state](task);
}
