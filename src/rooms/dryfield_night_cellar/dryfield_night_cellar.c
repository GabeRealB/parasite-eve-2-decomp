#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

#include "gameplay/captions.h"
#include "gameplay/display.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "main/fs.h"

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
static void func_dryfield_night_cellar_8017D6FC(Task* task)
{
    task->msgTable = D_dryfield_night_cellar_8017DAA8;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// The room entry task's idle state.
static void func_dryfield_night_cellar_8017D740(Task* task)
{
}

/// The room entry task's three states: set the room up, idle, end.
static const TaskFuncTable3 D_dryfield_night_cellar_8017D5C4 = {
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
/// projected through `gGfxViewCoord.workm`; when the GTE flag word is
/// non-negative, one semi-transparent `POLY_FT4` is queued at its OTZ (tpage
/// 0x2B, clut `(arg1 & 0x3F) | 0x4380`). `(s16)arg1` also picks the 40-texel
/// wide texture column, `(s16)arg2` is the half-extent scaled by 39 / OTZ,
/// and the grey level flickers between 0x20 and 0x30 with bit 0 of the
/// display's animation frame. Works in a 0x10-byte scratchpad block.
static void func_dryfield_night_cellar_8017D7A0(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    s32                idx;
    s32                blend;
    s16                xy;

    block = SCRATCH_PUSH(RoomDraw13Scratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)arg1;
        blend       = (((u8)gDisplayState.animFrame & 1) * 16) + 0x20;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        setUVWH(prim, idx * 40, 0, 0x27, 0x27);
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        xy            = block->sx - (u16)block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = block->sx + (u16)block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = block->sy - (u16)block->radius;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = block->sy + (u16)block->radius;
        prim->y3      = xy;
        prim->y2      = xy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}

/// Per-frame effect: once event nibble 0x52 is 1, draws a glow sprite on each
/// of the two points belonging to the current camera view
/// (`gGameSession->at4.loc.view`), 2 or 3. Every other view draws nothing.
static void func_dryfield_night_cellar_8017DA28(void)
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
