#include "rooms/dryfield_night_motel_room_1.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "dryfield_night_motel_room_1_private.h"

#include "gameplay/message.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/room_common.h"

/// The room's message table, published at `Task::msgTable` by the room task.
extern GpMsgEntry D_dryfield_night_motel_room_1_8017DA2C[];

s32 func_dryfield_night_motel_room_1_8017D5F0(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_motel_room_1_8017D5F8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_night_motel_room_1_8017D680(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_motel_room_1_8017D688(Task*, s32, GpMessageArg, GpMessageArg);

GpMsgEntry D_dryfield_night_motel_room_1_8017DA2C[5] = {
    { 5102, func_dryfield_night_motel_room_1_8017D5F8 },
    { 5105, func_dryfield_night_motel_room_1_8017D5F0 },
    { 5103, func_dryfield_night_motel_room_1_8017D688 },
    { 5104, func_dryfield_night_motel_room_1_8017D680 },
    { 0x7FFFFFFF, NULL },
};

static void func_dryfield_night_motel_room_1_8017D690(Task* task);
static void func_dryfield_night_motel_room_1_8017D6D4(Task* task);

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_1_8017D5F0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 2 that is not report-only (`field_5 == 0`),
/// answers game nibble 0x61 plus one while game nibble 0x7A is below 4, and 3
/// once it has reached 4. Returns 1.
s32 func_dryfield_night_motel_room_1_8017D5F8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 val;
    s32 n;

    *out = *in;
    if (in->prefix.packed == 2 && in->field_5 == 0) {
        n = GameFlag_GetNibble(0x7A);
        if (n >= 4) {
            val = 3;
        } else {
            val = GameFlag_GetNibble(0x61) + 1;
        }
        out->field_3 = val;
    }
    return 1;
}

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_1_8017D680(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_1_8017D688(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and advances to the next state.
static void func_dryfield_night_motel_room_1_8017D690(Task* task)
{
    task->msgTable = D_dryfield_night_motel_room_1_8017DA2C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: the room has nothing to do each frame.
static void func_dryfield_night_motel_room_1_8017D6D4(Task* task)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_night_motel_room_1_8017D5C4 = {
    { func_dryfield_night_motel_room_1_8017D690, func_dryfield_night_motel_room_1_8017D6D4, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_night_motel_room_1_8017D5C4`.
void func_dryfield_night_motel_room_1_8017D6DC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_room_1_8017D5C4;
    sp.funcs[task->state](task);
}

/// Queues a flickering sprite at the world point `arg0`: a semi-transparent
/// `POLY_FT4` square centred on the point's projection, with half-width
/// `arg2 * 39 / otz`, textured from the 40-texel cell `arg1` of tpage 0x2B and
/// shaded 0x20 or 0x30 on alternate frames. Points closer than OTZ 0x11 are
/// skipped.
void func_dryfield_night_motel_room_1_8017D734(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw25Scratch* block;
    POLY_FT4*          prim;
    s32                u;
    s32                blend;
    s32                idx;
    u8                 frame;

    block = SCRATCH_PUSH(RoomDraw25Scratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    gte_stsxy(&block->sx);
    gte_stszotz(&block->otz);
    if (block->otz > 0x10) {
        idx         = (s16)arg1;
        frame       = gDisplayState.animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u           = idx * 40;
        setUV4(prim, u, 0, u + 39, 0, u, 39, u + 39, 39);
        blend = ((frame & 1) << 4) + 0x20;
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->radius;
        prim->x1 = prim->x3 = block->sx + block->radius;
        prim->y0 = prim->y1 = block->sy - block->radius;
        prim->y2 = prim->y3 = block->sy + block->radius;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    }
    SCRATCH_POP(RoomDraw25Scratch);
}
