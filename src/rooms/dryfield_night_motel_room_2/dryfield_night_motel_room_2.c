#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// The room's message table, which the event task installs in state 0.
extern GpMsgEntry D_dryfield_night_motel_room_2_8017DA1C[];

/// The night motel room's drawable points, one 8-byte `SVECTOR` per disc. The
/// visit the pair belongs to is `gGameSession->at4.loc.view`.
extern SVECTOR D_dryfield_night_motel_room_2_8017DA44[];
extern SVECTOR D_dryfield_night_motel_room_2_8017DA54[];

/// The room's handler for message 0x13F1: does nothing and returns 0.
s32 func_dryfield_night_motel_room_2_8017D5D0(void)
{
    return 0;
}

/// The room's handler for message 0x13EE. It passes the message on unchanged,
/// except that message 2 with `field_5` clear gets its `field_3` rewritten:
/// 3 once game-flag nibble 0x7A has reached 4, otherwise one more than nibble
/// 0x61. Always reports the message handled.
s32 func_dryfield_night_motel_room_2_8017D5D8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 val;
    s32 n;

    *out = *in;
    if (in->msgId == 2 && in->field_5 == 0) {
        n = GameFlag_GetNibble(0x7A);
        if (n < 4) {
            val = 3;
            TOUCH_REG(val);
            val = GameFlag_GetNibble(0x61) + 1;
        } else {
            val = 3;
        }
        out->field_3 = val;
    }
    return 1;
}

/// The room's handler for message 0x13F0: does nothing and returns 0.
s32 func_dryfield_night_motel_room_2_8017D660(void)
{
    return 0;
}

/// The room's handler for message 0x13EF: does nothing and returns 0.
s32 func_dryfield_night_motel_room_2_8017D668(void)
{
    return 0;
}

/// State 0 of the room's event task: installs the room's message table, puts
/// the task in pointer slot 7 and moves on to state 1.
static void func_dryfield_night_motel_room_2_8017D670(Task* task)
{
    task->msgTable = D_dryfield_night_motel_room_2_8017DA1C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// The event task's per-frame state: nothing to do.
static void func_dryfield_night_motel_room_2_8017D6B4(Task* task)
{
}

/// The event task's three states: set-up, the idle per-frame state, and the
/// kill.
static const TaskFuncTable3 D_dryfield_night_motel_room_2_8017D5C4 = {
    {
        func_dryfield_night_motel_room_2_8017D670,
        func_dryfield_night_motel_room_2_8017D6B4,
        taskKill,
    },
};

/// Runs the room's event task through its three-state table, which it copies
/// onto the stack before the call.
static void func_dryfield_night_motel_room_2_8017D6BC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_room_2_8017D5C4;
    sp.funcs[task->state](task);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, unless
/// its OTZ is below 0x11, queues one semi-transparent `POLY_FT4` centred on it
/// at that depth: tpage 0x2B, clut `(arg1 & 0x3F) | 0x4380`, and the 40-texel
/// texture column `(s16)arg1 * 40`, rows 0 to 0x27. `(s16)arg2` is the
/// half-extent, scaled on screen by 39 / OTZ. The grey level alternates between
/// 0x20 and 0x30 with bit 0 of the display's animation frame. Works in a
/// 0xC-byte scratchpad block.
static void func_dryfield_night_motel_room_2_8017D714(SVECTOR* arg0, s32 arg1, s32 arg2)
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
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    }
    SCRATCH_POP(RoomDraw25Scratch);
}

/// Night motel room 2 draw: queues the room's glowing discs for the visit
/// `gGameSession->at4.loc.view` selects - visits 2 and 3 a pair at one point,
/// 5 and 6 a single one at another. Visits outside those ranges draw nothing.
/// `jump.c` cross-jumps the two trailing disc-draw calls into one tail.
static void func_dryfield_night_motel_room_2_8017D990(void)
{
    switch (gGameSession->at4.loc.view) {
        case 2:
        case 3: {
            SVECTOR* p = D_dryfield_night_motel_room_2_8017DA44;
            func_dryfield_night_motel_room_2_8017D714(&p[0], 1, 0x200);
            func_dryfield_night_motel_room_2_8017D714(&p[1], 1, 0x240);
            break;
        }
        case 5:
        case 6: {
            SVECTOR* p = D_dryfield_night_motel_room_2_8017DA54;
            func_dryfield_night_motel_room_2_8017D714(&p[0], 2, 0x180);
            break;
        }
    }
}
