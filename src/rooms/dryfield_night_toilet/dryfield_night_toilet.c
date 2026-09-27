#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3CD8.h"
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
extern GpMsgEntry D_dryfield_night_toilet_8017DA70[];

/// The room's two glow-sprite points, one per camera view group that shows
/// one.
extern SVECTOR D_dryfield_night_toilet_8017DAA0[];
extern SVECTOR D_dryfield_night_toilet_8017DAA8[];

/// Gameplay's task descriptor table; the room task spawns its entry 0.
extern TaskDesc D_8013E51C[];

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 0xF that is not report-only (`field_5 == 0`),
/// answers game nibble 0x61 plus one. Returns 1.
s32 func_dryfield_night_toilet_8017D5D0(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 0xF && in->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    return 1;
}

/// Message-table handler for id 0x13F2: on event 5 queues stage sound
/// 0x52100005. Returns 0.
s32 func_dryfield_night_toilet_8017D644(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 5) {
        Gp_EnqueueStageSnd6(0x52100000 | 5, 0, 0);
    }
    return 0;
}

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_night_toilet_8017D678(void)
{
    return 0;
}

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_dryfield_night_toilet_8017D680(void)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_night_toilet_8017D688(void)
{
    return 0;
}

/// First state of the room task: publishes the room's message table and claims
/// pointer slot 7. When game nibble 0xAF is still clear and the session's place
/// (`gGameSession->at4.loc.place`) is 1, it sets the nibble to 1 and spawns
/// entry 0 of `D_8013E51C`. Advances to the next state either way.
static void func_dryfield_night_toilet_8017D690(Task* task)
{
    task->msgTable = D_dryfield_night_toilet_8017DA70;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0xAF) == 0 && gGameSession->at4.loc.place == 1) {
        GameFlag_SetNibble(0xAF, 1);
        Task_SpawnFromTable(D_8013E51C, 0, 0, 0);
    }
    task->state = task->state + 1;
}

/// Second state of the room task: the room has nothing to do each frame.
static void func_dryfield_night_toilet_8017D71C(Task* task)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_night_toilet_8017D5C4 = {
    { func_dryfield_night_toilet_8017D690, func_dryfield_night_toilet_8017D71C, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_night_toilet_8017D5C4`.
void func_dryfield_night_toilet_8017D724(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_toilet_8017D5C4;
    sp.funcs[task->state](task);
}

/// Queues a flickering sprite at the world point `arg0`: a semi-transparent
/// `POLY_FT4` square centred on the point's projection, with half-width
/// `arg2 * 39 / otz`, textured from the 40-texel cell `arg1` of tpage 0x2B and
/// shaded 0x20 or 0x30 on alternate frames. Points closer than OTZ 0x11 are
/// skipped.
static void func_dryfield_night_toilet_8017D77C(SVECTOR* arg0, s32 arg1, s32 arg2)
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
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)), prim);
    }
    SCRATCH_POP(RoomDraw25Scratch);
}

/// Draws the room's glow sprite (texture cell 1, half-extent 0x200) at the
/// point the current camera view (`gGameSession->at4.loc.view`) shows: view 4
/// uses the second point, views 5 and 9 the first, and every other view draws
/// nothing.
static void func_dryfield_night_toilet_8017D9F8(void)
{
    switch (gGameSession->at4.loc.view) {
        case 4:
            func_dryfield_night_toilet_8017D77C(&D_dryfield_night_toilet_8017DAA8[0], 1, 0x200);
            break;
        case 5:
        case 9:
            func_dryfield_night_toilet_8017D77C(&D_dryfield_night_toilet_8017DAA0[0], 1, 0x200);
            break;
    }
}
