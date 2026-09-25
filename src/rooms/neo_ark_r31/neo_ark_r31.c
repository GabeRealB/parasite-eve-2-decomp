#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_neo_ark_r31_8017DC54;
extern u8  D_80071071[];

/// Room message handler table installed into `Task::msgTable`.
extern GpMsgEntry D_neo_ark_r31_8017D9F4[];
extern s32        D_80133F90;
extern s32        D_80134470;

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

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
            poly->v0 = poly->v1 = D_80071071[0] + (y + (buf << 4));
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

s32 func_neo_ark_r31_8017D8B0(void)
{
    return 0;
}

/// Message handler for the save location: copies the incoming `GpSaveLoc`
/// onto the outgoing one and passes both to `func_80179B14`. Returns 1.
s32 func_neo_ark_r31_8017D8B8(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}

s32 func_neo_ark_r31_8017D8FC(void)
{
    return 0;
}

s32 func_neo_ark_r31_8017D904(void)
{
    return 0;
}

/// Room task state 0: installs the message table, claims pointer slot 7,
/// sets `CdCmd_Queue.field_22A` to 2 and starts the room script with
/// `func_800E8634`. Advances to state 1.
void func_neo_ark_r31_8017D90C(Task* arg0)
{
    CdCmdQueue* queue;

    queue          = &CdCmd_Queue;
    arg0->msgTable = D_neo_ark_r31_8017D9F4;
    Game_SetPtrSlot(arg0, 7);
    queue->field_22A = 2;
    func_800E8634((s32)&D_80133F90, 0, (s32)&D_80134470);
    arg0->state = (s32)(arg0->state + 1);
}

/// Room task state 1: stores 2 into `D_800691CA` every tick.
void func_neo_ark_r31_8017D980(Task* task)
{
    CdCmd_Queue.field_22A = 2;
}

/// State handlers of the room task `func_neo_ark_r31_8017D990`, indexed by
/// `Task::state`: the set-up tick, the tick that stores 2 into `D_800691CA`,
/// and `taskKill`.
const TaskFuncTable3 D_neo_ark_r31_8017D5C4 = {
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
