#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern TaskDesc   D_dryfield_night_underpass_8017DCD8[];
extern GpMsgEntry D_dryfield_night_underpass_8017DCF0[];
extern SVECTOR    D_dryfield_night_underpass_8017DD20[8];
extern s16        D_dryfield_night_underpass_8017DD60[8];

/// Switch task the room's 0x13F0 handler spawns: plays cap command `spawnArg2`,
/// waits for it to finish, and once its event key reaches 0xA toggles game
/// nibble `spawnArg1`. When that nibble is 0x51 it also picks the room variant
/// to load next from nibbles 0xC9, 0x53 and 0x51 and writes it to both the
/// session and the save data. The last state flags the view dirty when the
/// chosen room is 5 or above, then kills the task.
void func_dryfield_night_underpass_8017D5D0(Task* task)
{
    GpSaveLoc    src;
    GpSaveLoc    dst;
    GpSaveLoc*   s;
    GpSaveLoc*   d;
    GameSession* session;
    s32          flag;
    s32          state;
    s32          arg;
    u8           room;

    flag  = task->spawnArg1;
    state = task->state;
    arg   = (s32)task->spawnArg2;
    switch (state) {
        case 0:
            Gp_RunCapCmd1(arg);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 2:
            if (Gp_GetCapEventKey() >= 0xA) {
                GameFlag_SetNibble(flag, GameFlag_GetNibble(flag) == 0);
                if (flag == 0x51) {
                    d           = &dst;
                    s           = &src;
                    *(u16*)&src = 0x26;
                    src.field_5 = 0;
                    if (s->field_5 == 0) {
                        if (GameFlag_GetNibble(0xC9) != 0) {
                            if (GameFlag_GetNibble(0x53) != 0) {
                                d->field_3 = 2;
                            } else {
                                d->field_3 = 1;
                            }
                            if (GameFlag_GetNibble(0x51) == 0) {
                                dst.field_3 = dst.field_3 + 2;
                            }
                        } else {
                            if (GameFlag_GetNibble(0x51) != 0) {
                                d->field_3 = 5;
                            } else {
                                d->field_3 = 6;
                            }
                        }
                    }
                    session                  = gGameSession;
                    room                     = dst.field_3;
                    session->at4.loc.room    = room;
                    Mc_SaveData.at4.loc.room = room;
                }
            }
            task->state = task->state + 1;
            return;
        case 3:
            if (gGameSession->at4.loc.room >= 5) {
                gGameSession->viewDirty = 1;
            }
            taskKill(task);
            return;
    }
}

/// Handler for message 0x13EE: copies the incoming record onto the outgoing one
/// and, unless the query is report-only (`field_5` set), answers record id 0x20
/// with 1 or 2 from nibble 0x51, raised by 2 while nibble 0x53 is set, and
/// record id 0x22 with 1 or 2 from nibble 0x52. Always returns 1.
s32 func_dryfield_night_underpass_8017D788(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 val;

    *out = *in;
    USE_REG(out);
    USE_REG(out);
    if (in->msgId == 0x20 && in->field_5 == 0) {
        val = GameFlag_GetNibble(0x51);
        if (val == 0) {
            val = 2;
        } else {
            val = 1;
        }
        out->field_3 = val;
        if (GameFlag_GetNibble(0x53) != 0) {
            out->field_3 = (u8)out->field_3 + 2;
        }
    }
    if (in->msgId == 0x22 && in->field_5 == 0) {
        val = GameFlag_GetNibble(0x52);
        if (val == 0) {
            val = 2;
        } else {
            val = 1;
        }
        out->field_3 = val;
    }
    return 1;
}

/// Handler for message 0x13F0: for `arg2` 1 or 2, spawns the room's switch
/// task `func_dryfield_night_underpass_8017D5D0` from the task table, toggling
/// nibble 0x51 with cap command 1 or nibble 0x52 with cap command 2. Any other
/// value spawns nothing. Always returns 0.
s32 func_dryfield_night_underpass_8017D868(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 1:
            Task_SpawnFromTable(D_dryfield_night_underpass_8017DCD8, 0, 0x51, 1);
            break;
        case 2:
            Task_SpawnFromTable(D_dryfield_night_underpass_8017DCD8, 0, 0x52, 2);
            break;
    }
    return 0;
}

/// Handler for message 0x13F2: when `arg2` is 2, queues stage sound 0x52260002.
/// Always returns 0.
s32 func_dryfield_night_underpass_8017D8CC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        Gp_EnqueueStageSnd6(0x52260000 | 2, 0, 0);
    }
    return 0;
}

/// Handler for message 0x13F1: does nothing and returns 0.
s32 func_dryfield_night_underpass_8017D900(void)
{
    return 0;
}

/// Handler for message 0x13EF: does nothing and returns 0.
s32 func_dryfield_night_underpass_8017D908(void)
{
    return 0;
}

/// First state of the room task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7, and advances.
void func_dryfield_night_underpass_8017D910(Task* task)
{
    task->msgTable = D_dryfield_night_underpass_8017DCF0;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: idles.
void func_dryfield_night_underpass_8017D954(Task* task)
{
}

/// State handlers of the room task `func_dryfield_night_underpass_8017D95C`,
/// indexed by `Task::state`: the set-up tick, the idle tick, and `taskKill`.
const TaskFuncTable3 D_dryfield_night_underpass_8017D5C4 = {
    { func_dryfield_night_underpass_8017D910, func_dryfield_night_underpass_8017D954, taskKill },
};

/// Room task: runs the state handler `D_dryfield_night_underpass_8017D5C4`
/// names for `Task::state`, through a copy of the table taken onto the stack.
void func_dryfield_night_underpass_8017D95C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_underpass_8017D5C4;
    sp.funcs[task->state](task);
}

/// Draws one glow sprite at the world-space point `arg0`: projects it through
/// `Gfx_ViewWorldMtx` and, when the GTE flag is non-negative, queues a
/// semi-transparent `POLY_FT4` centred on the projection (tpage 0x2B, clut
/// `(arg1 & 0x3F) | 0x4380`, UV column `(s16)arg1 * 40`). `arg2` is a signed
/// half-extent; the on-screen radius is `(s16)arg2 * 39 / otz`. The grey level
/// alternates between 0x20 and 0x30 with `animFrame`. Works in 0x10 bytes of
/// scratch, released on exit.
void func_dryfield_night_underpass_8017D9B4(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw13Scratch* block;
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
    tmp     = head - 0x10;
    block   = (RoomDraw13Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (((RoomDraw13Scratch*)tmp)->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ds             = &gDisplayState;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
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
        ((RoomDraw13Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        xy       = ((RoomDraw13Scratch*)tmp)->sx - (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sx + (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sy - (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sy + (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw13Scratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Per-frame effect: draws the glow anchors the current visit lights, one per
/// offset in `D_...DD20` whose `D_...DD60` bitmask contains the visit's bit
/// (`gGameSession->at4.loc.view`). The whole effect is skipped unless the room flag
/// (`GameFlag_GetNibble(0x53)`) is clear.
void func_dryfield_night_underpass_8017DC3C(void)
{
    s32      mask;
    s32      i;
    SVECTOR* vec;
    s16*     flags;

    mask = 1 << gGameSession->at4.loc.view;
    if (GameFlag_GetNibble(0x53) == 0) {
        i     = 0;
        vec   = D_dryfield_night_underpass_8017DD20;
        flags = D_dryfield_night_underpass_8017DD60;
        do {
            if (mask & *flags) {
                func_dryfield_night_underpass_8017D9B4(vec, 0, 0x280);
            }
            vec++;
            i++;
            flags++;
        } while (i < 8);
    }
}
