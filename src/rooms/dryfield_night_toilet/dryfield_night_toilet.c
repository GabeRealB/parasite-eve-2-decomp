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
void func_dryfield_night_toilet_8017D690(Task* task)
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
void func_dryfield_night_toilet_8017D71C(Task* task)
{
}

/// The room task's three states.
const TaskFuncTable3 D_dryfield_night_toilet_8017D5C4 = {
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
void func_dryfield_night_toilet_8017D77C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw25Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                tex;
    s32                idx;
    s32                u0;
    s32                u1;
    register s32       sarg asm("v1");
    s32                t;
    s32                field8;
    s32                blend;
    s32                v;
    u8                 code;
    s16                xy;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    tmp     = head - 0xC;
    block   = (RoomDraw25Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomDraw25Scratch*)(head - 0xC))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - 0xC))->otz >= 0x11) {
        ds          = &gDisplayState;
        idx         = (s16)tex;
        field8      = (u8)ds->animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        prim->u0    = u0;
        prim->u2    = u0;
        SOFT_USE_REG(u0);
        v        = 0x27;
        prim->u1 = u1;
        prim->u3 = u1;
        SOFT_USE_REG(u1);
        sarg     = arg2 << 16;
        prim->v2 = v;
        prim->v3 = v;
        SCHED_BARRIER();
        code     = prim->code;
        sarg     = sarg >> 16;
        prim->v0 = 0;
        prim->v1 = 0;
        blend    = ((field8 & 1) * 16) + 0x20;
        COMPILER_BARRIER();
        prim->code = code | 2;
        t          = sarg * 40;
        setRGB0(prim, blend, blend, blend);
        ((RoomDraw25Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw25Scratch*)(head - 0xC))->otz;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sx - *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sx + *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sy - *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sy + *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw25Scratch*)(head - 0xC))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0xC;
}

/// Draws the room's glow sprite (texture cell 1, half-extent 0x200) at the
/// point the current camera view (`gGameSession->at4.loc.view`) shows: view 4
/// uses the second point, views 5 and 9 the first, and every other view draws
/// nothing.
void func_dryfield_night_toilet_8017D9F8(void)
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
