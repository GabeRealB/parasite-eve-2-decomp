#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/room_effects.h"
#include "gameplay/loading.h"

#include "gameplay/scene.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"

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

static void func_mine_tunnel_entrance_8017D644(Task* arg0);
static void func_mine_tunnel_entrance_8017D690(Task* task);
static void func_mine_tunnel_entrance_8017D6B4(Task* task);
static void func_mine_tunnel_entrance_8017D868(SVECTOR* arg0, s32 arg1, s32 arg2);

/// State handlers of the room task `func_mine_tunnel_entrance_8017D6BC` runs:
/// set-up, the scene-event state, an idle state and `taskKill`.
static const TaskFuncTable4 D_mine_tunnel_entrance_8017D5C4 = {
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
static void func_mine_tunnel_entrance_8017D644(Task* arg0)
{
    arg0->msgTable = D_mine_tunnel_entrance_8017DAF0;
    Game_SetPtrSlot(arg0, 7);
    arg0->state           = (s32)(arg0->state + 1);
    gStageSceneMusicEntry = 1;
}

/// State 1 of the room task: moves the saved scene event from 9 on to 10.
static void func_mine_tunnel_entrance_8017D690(Task* task)
{
    if (Mc_SaveData[0].state.sceneEvent == 9) {
        Mc_SaveData[0].state.sceneEvent = 0xA;
    }
}

static void func_mine_tunnel_entrance_8017D6B4(Task* task)
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
static void func_mine_tunnel_entrance_8017D720(void)
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
static void func_mine_tunnel_entrance_8017D868(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    s32                u;
    s32                blend;
    s32                idx;
    u8                 frame;

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
        setPolyFT4(prim);
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
    SCRATCH_POP(RoomDraw13Scratch);
}
