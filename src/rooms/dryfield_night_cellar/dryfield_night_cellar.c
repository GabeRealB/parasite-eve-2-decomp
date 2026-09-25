#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// The room's message table, installed on the room entry task.
extern GpMsgEntry D_dryfield_night_cellar_8017DAA8[];

/// The cellar's glow points, one pair per camera view that shows them: view 2
/// draws the pair at the first address and view 3 the pair at the second.
extern SVECTOR D_dryfield_night_cellar_8017DAD0[];
extern SVECTOR D_dryfield_night_cellar_8017DAE0[];

/// Message-table handler for message 0x13F0. On event 0xD it runs a CAP
/// command: 0xD while event nibble 0x11B is below 2, otherwise 4 or 0xE
/// depending on whether `func_800B7420(0x83)` reports non-zero. Every other
/// event does nothing. Always answers 0.
s32 func_dryfield_night_cellar_8017D5D0(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 != 8) {
        if (arg2 == 0xD) {
            if (GameFlag_GetNibble(0x11B) >= 2) {
                if (func_800B7420(0x83) == 0) {
                    Gp_RunCapCmd1(0xE);
                } else {
                    Gp_RunCapCmd1(4);
                }
            } else {
                Gp_RunCapCmd1(0xD);
            }
        }
    }
    return 0;
}

/// Message-table handler for message 0x13F1: does nothing and answers 0.
s32 func_dryfield_night_cellar_8017D62C(void)
{
    return 0;
}

/// Message-table handler for message 0x13EE. Copies the incoming record onto
/// the outgoing one; for a query 0x26 without `field_5` set it answers in
/// `field_3` from event nibbles 0xC9, 0x53 and 0x51 (1 to 4 while 0xC9 is set,
/// 5 or 6 otherwise). Always answers 1.
s32 func_dryfield_night_cellar_8017D634(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 0x26 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) != 0) {
                out->field_3 = 2;
            } else {
                out->field_3 = 1;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                out->field_3 = (u8)out->field_3 + 2;
            }
        } else {
            if (GameFlag_GetNibble(0x51) != 0) {
                out->field_3 = 5;
            } else {
                out->field_3 = 6;
            }
        }
    }
    return 1;
}

/// Message-table handler for message 0x13EF: does nothing and answers 0.
s32 func_dryfield_night_cellar_8017D6F4(void)
{
    return 0;
}

/// The room entry task's first state: installs the room's message table, hands
/// the task to pointer slot 7 and moves on to the next state.
void func_dryfield_night_cellar_8017D6FC(Task* task)
{
    task->msgTable = D_dryfield_night_cellar_8017DAA8;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// The room entry task's idle state.
void func_dryfield_night_cellar_8017D740(Task* task)
{
}

/// The room entry task's three states: set the room up, idle, end.
const TaskFuncTable3 D_dryfield_night_cellar_8017D5C4 = {
    { func_dryfield_night_cellar_8017D6FC, func_dryfield_night_cellar_8017D740, taskKill },
};

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_night_cellar_8017D748(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_cellar_8017D5C4;
    sp.funcs[task->state](task);
}

/// Draws one glow sprite at the world-space point `arg0`. The point is
/// projected through `Gfx_ViewWorldMtx`; when the GTE flag word is
/// non-negative, one semi-transparent `POLY_FT4` is queued at its OTZ (tpage
/// 0x2B, clut `(arg1 & 0x3F) | 0x4380`). `(s16)arg1` also picks the 40-texel
/// wide texture column, `(s16)arg2` is the half-extent scaled by 39 / OTZ,
/// and the grey level flickers between 0x20 and 0x30 with bit 0 of the
/// display's animation frame. Works in a 0x10-byte scratchpad block.
void func_dryfield_night_cellar_8017D7A0(SVECTOR* arg0, s32 arg1, s32 arg2)
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
        xy       = ((RoomDraw13Scratch*)tmp)->sx - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sx + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sy - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sy + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw13Scratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Per-frame effect: once event nibble 0x52 is 1, draws a glow sprite on each
/// of the two points belonging to the current camera view
/// (`gGameSession->at4.loc.view`), 2 or 3. Every other view draws nothing.
void func_dryfield_night_cellar_8017DA28(void)
{
    u8 visit;

    if (GameFlag_GetNibble(0x52) == 1) {
        visit = gGameSession->at4.loc.view;
        if (visit == 2) {
            func_dryfield_night_cellar_8017D7A0(&D_dryfield_night_cellar_8017DAD0[0], 1, 0x280);
            func_dryfield_night_cellar_8017D7A0(&D_dryfield_night_cellar_8017DAD0[1], 1, 0x280);
        } else if (visit == 3) {
            func_dryfield_night_cellar_8017D7A0(&D_dryfield_night_cellar_8017DAE0[0], 1, 0x280);
            func_dryfield_night_cellar_8017D7A0(&D_dryfield_night_cellar_8017DAE0[1], 1, 0x280);
        }
    }
}
