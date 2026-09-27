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

#include "rooms/room.h"
#include "rooms/room_common.h"

/// The room's message table, published at `Task::msgTable` by the room task.
extern GpMsgEntry D_dryfield_night_motel_room_4_8017DA48[];

/// The room's sprite points for views 2, 3 and 5: three `SVECTOR`s.
extern SVECTOR D_dryfield_night_motel_room_4_8017DA70[];

/// The sprite point view 4 draws. Only its first `SVECTOR` is read here; the
/// words after it are other room data.
extern SVECTOR D_dryfield_night_motel_room_4_8017DA88[];

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_4_8017D5D0(void)
{
    return 0;
}

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 2 that is not report-only (`field_5 == 0`),
/// answers game nibble 0x61 plus one while game nibble 0x7A is below 4, and 3
/// once it has reached 4. Returns 1.
s32 func_dryfield_night_motel_room_4_8017D5D8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 val;
    s32 n;

    *out = *in;
    if (in->msgId == 2 && in->field_5 == 0) {
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
s32 func_dryfield_night_motel_room_4_8017D660(void)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_night_motel_room_4_8017D668(void)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and advances to the next state.
static void func_dryfield_night_motel_room_4_8017D670(Task* task)
{
    task->msgTable = D_dryfield_night_motel_room_4_8017DA48;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: nothing left to do but idle.
static void func_dryfield_night_motel_room_4_8017D6B4(Task* task)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_night_motel_room_4_8017D5C4 = {
    { func_dryfield_night_motel_room_4_8017D670, func_dryfield_night_motel_room_4_8017D6B4, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_night_motel_room_4_8017D5C4`.
void func_dryfield_night_motel_room_4_8017D6BC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_room_4_8017D5C4;
    sp.funcs[task->state](task);
}

/// Queues a flickering sprite at the world point `arg0`: a semi-transparent
/// `POLY_FT4` square centred on the point's projection, with half-width
/// `(s16)arg2 * 39 / otz`, textured from the 40-texel cell `(s16)arg1` of
/// tpage 0x2B (clut `(arg1 & 0x3F) | 0x4380`) and shaded 0x20 or 0x30 on
/// alternate frames. Points closer than OTZ 0x11 are skipped.
static void func_dryfield_night_motel_room_4_8017D714(SVECTOR* arg0, s32 arg1, s32 arg2)
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

/// Per-frame effect: queues the room's flickering sprites for the view
/// `gGameSession->at4.loc.view` selects - views 2 and 3 the first two points,
/// view 4 the point at `D_dryfield_night_motel_room_4_8017DA88`, view 5 the
/// first point plus the third, drawn from texture cell 2 with a smaller
/// half-width. Other views draw nothing.
///
/// `jump.c` cross-jumps the trailing sprite calls into one tail here rather
/// than two, because the view-5 group shares its first call with the view-2/3
/// group.
static void func_dryfield_night_motel_room_4_8017D990(void)
{
    switch (gGameSession->at4.loc.view) {
        case 2:
        case 3: {
            SVECTOR* p = D_dryfield_night_motel_room_4_8017DA70;
            func_dryfield_night_motel_room_4_8017D714(&p[0], 1, 0x200);
            func_dryfield_night_motel_room_4_8017D714(&p[1], 1, 0x240);
            break;
        }
        case 4: {
            SVECTOR* p = D_dryfield_night_motel_room_4_8017DA88;
            func_dryfield_night_motel_room_4_8017D714(&p[0], 1, 0x200);
            break;
        }
        case 5: {
            SVECTOR* p = D_dryfield_night_motel_room_4_8017DA70;
            func_dryfield_night_motel_room_4_8017D714(&p[0], 1, 0x200);
            func_dryfield_night_motel_room_4_8017D714(&p[2], 2, 0x180);
            break;
        }
    }
}
