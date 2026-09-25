#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

/// The room's message table, installed on the room entry task.
extern GpMsgEntry D_dryfield_cellar_8017DB8C[];

/// The cellar's glow points, one pair per camera view that shows them: view 2
/// draws the pair at the first address and view 3 the pair at the second.
extern SVECTOR D_dryfield_cellar_8017DBBC[2];
extern SVECTOR D_dryfield_cellar_8017DBCC[2];

/// Message-table handler for message 0x13F0. On event 0xD it runs a CAP
/// command: 0xD while event nibble 0x11B is below 2, otherwise 4 or 0xE
/// depending on whether `func_800B7420(0x83)` reports non-zero. Every other
/// event does nothing. Always answers 0.
s32 func_dryfield_cellar_8017D5D0(s32 arg0, s32 arg1, s32 arg2)
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
s32 func_dryfield_cellar_8017D62C(void)
{
    return 0;
}

/// Message-table handler for message 0x13EE. Copies the incoming record onto
/// the outgoing one; for a query 0x26 without `field_5` set it answers in
/// `field_3` from event nibbles 0xC9, 0x53 and 0x51 (1 to 4 while 0xC9 is set,
/// 5 or 6 otherwise). Always answers 1.
s32 func_dryfield_cellar_8017D634(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
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
s32 func_dryfield_cellar_8017D6F4(void)
{
    return 0;
}

/// Message-table handler for message 0x13F2: on event 3 queues sound event
/// 0x52220003. Always answers 0.
s32 func_dryfield_cellar_8017D6FC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        SndEvt_EnqueueType6(0x52220000 | 3, 0, 0);
    }
    return 0;
}

/// The room entry task's first state: installs the room's message table, hands
/// the task to pointer slot 7, moves on to the next state and sets the
/// gameplay byte `D_80115598`.
void func_dryfield_cellar_8017D730(Task* task)
{
    task->msgTable = D_dryfield_cellar_8017DB8C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
    D_80115598  = 1;
}

/// The room entry task's idle state.
void func_dryfield_cellar_8017D77C(Task* task)
{
}

/// The room entry task's three states: set the room up, idle, end.
const TaskFuncTable3 D_dryfield_cellar_8017D5C4 = {
    { func_dryfield_cellar_8017D730, func_dryfield_cellar_8017D77C, taskKill },
};

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_cellar_8017D784(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_cellar_8017D5C4;
    sp.funcs[task->state](task);
}

/// Draws one glow sprite at the point `arg1`, given in the local space of
/// `arg0`: the point is rotated by the coordinate's `workm` and offset by its
/// translation, then projected through `GsWSMATRIX`. Anything nearer than OTZ
/// 0x11 is dropped. Otherwise one semi-transparent `POLY_FT4` on tpage 0x2B is
/// queued at its OTZ; `(s16)arg2` picks the 40-texel wide texture column and
/// the clut `(arg2 & 0x3F) | 0x4380`, `(s16)arg3` is the half-extent scaled by
/// 39 / OTZ, and the grey level flickers between 0x20 and 0x30 with bit 0 of
/// the display's animation frame. Works in a 0x14-byte scratchpad block.
void func_dryfield_cellar_8017D7DC(GpCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw35Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                su;
    s32                sv;
    s32                u0;
    s32                u1;
    s32                flip;
    s32                rgb;
    s16                xy;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x14;
    block    = (RoomDraw35Scratch*)(head - 0x14);

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&((RoomDraw35Scratch*)(head - 0x14))->vec);
    block->vec.vx = *(u16*)&block->vec.vx + *(u16*)&arg0->workm.t[0];
    block->vec.vy = *(u16*)&block->vec.vy + *(u16*)&arg0->workm.t[1];
    block->vec.vz = *(u16*)&block->vec.vz + *(u16*)&arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw35Scratch*)(head - 0x14))->vec);
    gte_rtps();

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomDraw35Scratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw35Scratch*)(head - 0x14))->otz >= 0x11) {
        ds          = &gDisplayState;
        flip        = (u8)ds->animFrame;
        su          = (s16)arg2;
        sv          = (s16)arg3;
        prim->tpage = 0x2B;
        prim->clut  = (su & 0x3F) | 0x4380;
        u0          = su * 0x28;
        u1          = u0 + 0x27;
        prim->u1    = u1;
        prim->u3    = u1;
        prim->u0    = u0;
        prim->u2    = u0;
        prim->v0    = 0;
        prim->v1    = 0;
        prim->v2    = 0x27;
        prim->v3    = 0x27;
        rgb         = (flip & 1) << 4;
        rgb        += 0x20;
        setSemiTrans(prim, 1);
        prim->r0         = rgb;
        prim->g0         = rgb;
        prim->b0         = rgb;
        block->halfWidth = (sv * 0x27) / block->otz;
        xy               = *(u16*)&block->sx - *(u16*)&block->halfWidth;
        prim->x2         = xy;
        prim->x0         = xy;
        xy               = *(u16*)&block->sx + *(u16*)&block->halfWidth;
        prim->x3         = xy;
        prim->x1         = xy;
        xy               = *(u16*)&block->sy - *(u16*)&block->halfWidth;
        prim->y1         = xy;
        prim->y0         = xy;
        xy               = *(u16*)&block->sy + *(u16*)&block->halfWidth;
        prim->y3         = xy;
        prim->y2         = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x14);
}

/// Per-frame effect on a model task: once event nibble 0x52 is 1, draws a glow
/// sprite on each of the two points belonging to the current camera view
/// (`gGameSession->at4.loc.view`), 2 or 3, placed in the model's coordinate
/// space. Every other view draws nothing.
void func_dryfield_cellar_8017DAEC(Task* arg0)
{
    GpCoord* coord;

    coord = arg0->extra.tmd->coords;
    if (GameFlag_GetNibble(0x52) == 1) {
        if (gGameSession->at4.loc.view == 2) {
            func_dryfield_cellar_8017D7DC(coord, D_dryfield_cellar_8017DBBC, 1, 0x280);
            func_dryfield_cellar_8017D7DC(coord, D_dryfield_cellar_8017DBBC + 1, 1, 0x280);
        } else if (gGameSession->at4.loc.view == 3) {
            func_dryfield_cellar_8017D7DC(coord, D_dryfield_cellar_8017DBCC, 1, 0x280);
            func_dryfield_cellar_8017D7DC(coord, D_dryfield_cellar_8017DBCC + 1, 1, 0x280);
        }
    }
}
