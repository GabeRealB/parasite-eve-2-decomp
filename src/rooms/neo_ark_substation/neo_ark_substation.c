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
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// The room's own `GpMsgEntry[]` - the message table the message task publishes.
extern GpMsgEntry D_neo_ark_substation_8017E294[];
/// Spawn table for the ambience task the message task starts.
extern TaskDesc D_neo_ark_substation_8017E2BC[];
/// The room's ambience table, one `(pan, vol)` entry per area.
extern RoomAmbienceEntry D_neo_ark_substation_8017E2C8[];

extern SVECTOR D_neo_ark_substation_8017E310[];
extern SVECTOR D_neo_ark_substation_8017E330[];
extern SVECTOR D_neo_ark_substation_8017E350[];
extern SVECTOR D_neo_ark_substation_8017E360[];
extern SVECTOR D_neo_ark_substation_8017E380[];

void func_neo_ark_substation_8017D7AC(Task* task);
void func_neo_ark_substation_8017D814(Task* task);
void func_neo_ark_substation_8017DA50(SVECTOR* arg0, s32 arg1, s32 arg2);

/// State table of the room's message task: set-up
/// (`func_neo_ark_substation_8017D7AC`), an empty per-frame state and
/// `taskKill`. Its bytes open the room's rodata, ahead of the ambience task's
/// jump table.
const TaskFuncTable3 D_neo_ark_substation_8017D5C4 = {
    func_neo_ark_substation_8017D7AC,
    func_neo_ark_substation_8017D814,
    taskKill,
};

/// Keeps the substation's looping ambience in step with the area the session is
/// in: `gGameSession->at4.loc.view` selects one of the room's nine `(pan, vol)`
/// entries, and state 0 starts that loop with `SndEvt_EnqueueType6`. States 1
/// through 4 then watch for the session's index to stop matching the area
/// `Mc_SaveData.at4.loc.view` publishes - state 1 tests the pair and 2, 3 and 4 walk the task
/// along - and state 5 retunes the playing loop to the new entry with
/// `SndEvt_EnqueueTypeA` and returns to state 1 to keep watching.
void func_neo_ark_substation_8017D608(Task* task)
{
    s32 pan;
    s32 vol;
    u8  idx;

    idx = gGameSession->at4.loc.view;
    if (idx < 9) {
        pan = D_neo_ark_substation_8017E2C8[idx].pan;
        vol = D_neo_ark_substation_8017E2C8[idx].vol;
    } else {
        pan = 0;
        vol = 0;
    }

    switch (task->state) {
        case 0:
            SndEvt_EnqueueType6(0x55210003, (s8)pan, (s8)vol);
            task->state = task->state + 1;
            break;
        case 1:
            if (Mc_SaveData.at4.loc.view != gGameSession->at4.loc.view) {
                task->state = task->state + 1;
            }
            break;
        case 2:
        case 3:
        case 4:
            task->state = task->state + 1;
            break;
        case 5:
            SndEvt_EnqueueTypeA(0x55210003, (s8)pan, (s8)vol);
            task->state = 1;
            break;
    }
}

s32 func_neo_ark_substation_8017D71C(void)
{
    return 0;
}

/// Handler the room's message table gives message 0x13EE: copies the incoming
/// `GpSaveLoc` onto the outgoing one and passes both on to `func_80179B14`.
/// Always returns 1.
s32 func_neo_ark_substation_8017D724(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}

s32 func_neo_ark_substation_8017D768(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0xDF) != 0 ? 3 : 5);
    }
    return 0;
}

s32 func_neo_ark_substation_8017D7A4(void)
{
    return 0;
}

/// State 0 of the room's message task: park the room's message table in
/// `Task::msgTable`, publish the task in pointer slot 7, start the ambience
/// task (`func_neo_ark_substation_8017D608`) only while game flag 0xDF is
/// clear, and advance to state 1.
void func_neo_ark_substation_8017D7AC(Task* task)
{
    task->msgTable = D_neo_ark_substation_8017E294;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0xDF) == 0) {
        Task_SpawnFromTable(D_neo_ark_substation_8017E2BC, 0, 0, 0);
    }
    task->state = (s32)(task->state + 1);
}

/// Per-frame state of the room's message task: nothing to do, the task only
/// holds the message table.
void func_neo_ark_substation_8017D814(Task* task)
{
}

/// Runs the room's message task's current state through a stack copy of
/// `D_neo_ark_substation_8017D5C4`.
void func_neo_ark_substation_8017D81C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_substation_8017D5C4;
    sp.funcs[task->state](task);
}

void func_neo_ark_substation_8017D874(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_neo_ark_substation_8017E310;
            func_neo_ark_substation_8017DA50(&p[0], 0x200, 0x444);
            break;
        }
        case 3: {
            SVECTOR* p = D_neo_ark_substation_8017E310;
            func_neo_ark_substation_8017DA50(&p[0], 0x200, 0x444);
            func_neo_ark_substation_8017DA50(&p[2], 0x200, 0x444);
            func_neo_ark_substation_8017DA50(&p[4], 0x200, 0x444);
            func_neo_ark_substation_8017DA50(&p[6], 0x200, 0x444);
            break;
        }
        case 4: {
            SVECTOR* p = D_neo_ark_substation_8017E310;
            func_neo_ark_substation_8017DA50(&p[0], 0x200, 0x222);
            func_neo_ark_substation_8017DA50(&p[2], 0x200, 0x333);
            func_neo_ark_substation_8017DA50(&p[4], 0x200, 0x444);
            func_neo_ark_substation_8017DA50(&p[6], 0x200, 0x444);
            func_neo_ark_substation_8017DA50(&p[16], 0x200, 0x222);
            func_neo_ark_substation_8017DA50(&p[18], 0x200, 0x333);
            break;
        }
        case 5: {
            SVECTOR* p = D_neo_ark_substation_8017E330;
            func_neo_ark_substation_8017DA50(&p[0], 0x200, 0x444);
            func_neo_ark_substation_8017DA50(&p[2], 0x200, 0x333);
            break;
        }
        case 6: {
            SVECTOR* p = D_neo_ark_substation_8017E350;
            func_neo_ark_substation_8017DA50(&p[0], 0x200, 0x333);
            func_neo_ark_substation_8017DA50(&p[2], 0x200, 0x444);
            func_neo_ark_substation_8017DA50(&p[12], 0x200, 0x444);
            break;
        }
        case 7: {
            SVECTOR* p = D_neo_ark_substation_8017E360;
            func_neo_ark_substation_8017DA50(&p[0], 0x200, 0x444);
            func_neo_ark_substation_8017DA50(&p[2], 0x200, 0x333);
            func_neo_ark_substation_8017DA50(&p[4], 0x200, 0x222);
            func_neo_ark_substation_8017DA50(&p[12], 0x200, 0x444);
            func_neo_ark_substation_8017DA50(&p[14], 0x200, 0x333);
            func_neo_ark_substation_8017DA50(&p[16], 0x200, 0x222);
            break;
        }
        case 8: {
            SVECTOR* p = D_neo_ark_substation_8017E380;
            func_neo_ark_substation_8017DA50(&p[0], 0x200, 0x444);
            break;
        }
    }
}

/// Draws a glow between the two world points `arg0[0]` and `arg0[1]`: both are
/// projected through `Gfx_ViewWorldMtx`, and unless the GTE flags either
/// projection, gouraud `POLY_G4` wedges are queued around each end and a band
/// joins them, each tinted at the centre and black at the rim. `arg1` is the
/// radius in world units, scaled by each point's depth; `arg2` is the tint as
/// three 4-bit channels (red at bit 8, green at bit 4, blue at bit 0), with 8
/// added to each on odd display frames so the glow flickers.
void func_neo_ark_substation_8017DA50(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw08Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t3;
    s32                t2;
    s32                limit;
    s32                angStart;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    s32                scaled;
    s32                sum;
    u8                 r;
    u8                 g;
    u8                 b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (RoomDraw08Scratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx0);
    gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx1);
        gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((RoomDraw08Scratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((RoomDraw08Scratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            SCHED_BARRIER();
            ang    = (s16)ang;
            blend  = (*(u8*)&ds->animFrame & 1) * 8;
            packed = arg2 << 16;
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend | tr;
            g      = blend | tg;
            b      = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim = (POLY_G4*)gGpuPrimCursor;
                    USE_REG(prim);
                    t   = ang - angStart;
                    t <<= 1;
                    TOUCH_REG(t);
                    sum            = angStart + t;
                    t              = sum;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();
                    t3   = ang + 0x800;
                    prim = (POLY_G4*)gGpuPrimCursor;
                    SOFT_BARRIER();
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
