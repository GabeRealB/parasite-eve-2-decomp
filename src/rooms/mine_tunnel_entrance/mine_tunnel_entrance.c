#include "common.h"
#include "main/stage.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// The room's message table, which the room task answers messages with.
extern GpMsgEntry D_mine_tunnel_entrance_8017DAF0[];

/// The tunnel's per-view quad positions, one `SVECTOR` per position, 8 bytes
/// apart. The runs overlap: `DB18[5]`, `DB30[2]` and `DB38[1]` are all the
/// same point, reached through whichever base the view's case names.
extern SVECTOR D_mine_tunnel_entrance_8017DB18[];
extern SVECTOR D_mine_tunnel_entrance_8017DB30[];
extern SVECTOR D_mine_tunnel_entrance_8017DB38[];
extern SVECTOR D_mine_tunnel_entrance_8017DB48[];

void func_mine_tunnel_entrance_8017D644(Task* arg0);
void func_mine_tunnel_entrance_8017D690(Task* task);
void func_mine_tunnel_entrance_8017D6B4(Task* task);
void func_mine_tunnel_entrance_8017D868(SVECTOR* arg0, s32 arg1, s32 arg2);

/// State handlers of the room task `func_mine_tunnel_entrance_8017D6BC` runs:
/// set-up, the scene-event state, an idle state and `taskKill`.
const TaskFuncTable4 D_mine_tunnel_entrance_8017D5C4 = {
    func_mine_tunnel_entrance_8017D644,
    func_mine_tunnel_entrance_8017D690,
    func_mine_tunnel_entrance_8017D6B4,
    taskKill,
};

s32 func_mine_tunnel_entrance_8017D5E8(void)
{
    return 0;
}

/// Message handler 0x13EE of the room's message table: copies the incoming
/// record onto the outgoing one, passes both to `func_80179A04`, and returns 1.
s32 func_mine_tunnel_entrance_8017D5F0(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

s32 func_mine_tunnel_entrance_8017D634(void)
{
    return 0;
}

s32 func_mine_tunnel_entrance_8017D63C(void)
{
    return 0;
}

/// State 0 of the room task: installs the room's message table, publishes the
/// task in pointer slot 7, advances to the next state and selects scene music entry 1.
void func_mine_tunnel_entrance_8017D644(Task* arg0)
{
    arg0->msgTable = D_mine_tunnel_entrance_8017DAF0;
    Game_SetPtrSlot(arg0, 7);
    arg0->state           = (s32)(arg0->state + 1);
    gStageSceneMusicEntry = 1;
}

/// State 1 of the room task: moves the saved scene event from 9 on to 10.
void func_mine_tunnel_entrance_8017D690(Task* task)
{
    if (Mc_SaveData.sceneEvent == 9) {
        Mc_SaveData.sceneEvent = 0xA;
    }
}

void func_mine_tunnel_entrance_8017D6B4(Task* task)
{
}

/// Per-frame entry of the room task: copies the state table onto the stack
/// and runs the handler for the task's current state.
void func_mine_tunnel_entrance_8017D6BC(Task* task)
{
    TaskFuncTable4 states;

    states = D_mine_tunnel_entrance_8017D5C4;
    states.funcs[task->state](task);
}

/// Sets `Gp_State1C->roomEffectMode` to 2, then draws the quads the current
/// camera view shows, one `func_mine_tunnel_entrance_8017D868` call per
/// position with UV column 0 or 1 and half-extent 0x300 (0x200 for view 6's
/// second quad). Other views draw nothing.
void func_mine_tunnel_entrance_8017D720(void)
{
    Gp_State1C->roomEffectMode = 2;
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB18;
            func_mine_tunnel_entrance_8017D868(&p[0], 0, 0x300);
            func_mine_tunnel_entrance_8017D868(&p[1], 0, 0x300);
            func_mine_tunnel_entrance_8017D868(&p[2], 1, 0x300);
            func_mine_tunnel_entrance_8017D868(&p[5], 1, 0x300);
            break;
        }
        case 3: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB30;
            func_mine_tunnel_entrance_8017D868(&p[0], 1, 0x300);
            func_mine_tunnel_entrance_8017D868(&p[1], 1, 0x300);
            func_mine_tunnel_entrance_8017D868(&p[2], 1, 0x300);
            break;
        }
        case 4: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB30;
            func_mine_tunnel_entrance_8017D868(&p[0], 1, 0x300);
            func_mine_tunnel_entrance_8017D868(&p[1], 1, 0x300);
            break;
        }
        case 5: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB38;
            func_mine_tunnel_entrance_8017D868(&p[0], 1, 0x300);
            break;
        }
        case 6: {
            SVECTOR* p = D_mine_tunnel_entrance_8017DB48;
            func_mine_tunnel_entrance_8017D868(&p[0], 1, 0x300);
            func_mine_tunnel_entrance_8017D868(&p[1], 1, 0x200);
            break;
        }
    }
}

/// Draws one screen-aligned textured quad at the world-space point `arg0`: projects
/// it through `gGfxViewCoord.workm` and, when the projection flag is non-negative,
/// queues a semi-transparent `POLY_FT4` (tpage 0x2B, clut `(arg1 & 0x3F) |
/// 0x4380`) into the ordering table at its depth. `arg1` also picks the
/// 40-texel UV column; `arg2` is the half-extent in world units, scaled to
/// `(s16)arg2 * 39 / otz` on screen. The colour flickers between 0x20 and 0x30
/// on alternate frames. A 0x10-byte scratch block holds the projection results.
void func_mine_tunnel_entrance_8017D868(SVECTOR* arg0, s32 arg1, s32 arg2)
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

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
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
