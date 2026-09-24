#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/wipsys.h"
#include "rooms/dryfield_night_garage.h"
#include "rooms/room_common.h"

extern s8 D_8007106B;

extern s32            D_dryfield_night_garage_80182DE0;
extern s32            D_dryfield_night_garage_80182DE4;
extern GpAreaApplyRec D_dryfield_night_garage_801875D8[];
extern GpAreaApplyRec D_dryfield_night_garage_80187620[];

/// Descriptor of the task `func_dryfield_night_garage_80180D4C` spawns.
extern TaskDesc D_dryfield_night_garage_80183380;

/// The garage's two point-pair runs, 8-byte `SVECTOR`s laid back to back from
/// `801833A4`: four pairs the visit-3/15 case sweeps (`A4[0]`, `A4[2]`, `A4[4]`,
/// `A4[6]`), of which the last two are also what visit 11 sweeps from
/// `801833D4`. `801833D4` is named separately because visit 11 reaches it by
/// name where the visit-3/15 case reaches the same address as `A4[6]` -
/// indexing emits the base plus 0x30, naming it emits its own `lui`.
extern SVECTOR D_dryfield_night_garage_801833A4[];
extern SVECTOR D_dryfield_night_garage_801833D4;

void func_dryfield_night_garage_801809A4(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_RunCapCmd(arg0->spawnArg1, 0);
            Gp_DispatchMsg(func_dryfield_night_garage_80180A64(0), 0x7DB, (s32)&D_dryfield_night_garage_80182DE0, 0);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_MsgPlayerWeapon(1);
                Gp_DispatchMsg(func_dryfield_night_garage_80180A64(0), 0x7DB, (s32)&D_dryfield_night_garage_80182DE4, 0);
                break;
            }
            return;
    }
    taskKill(arg0);
}

Task* func_dryfield_night_garage_80180A64(s32 arg0)
{
    GpWorkObj* work;
    Task*      task;

    work = Gp_FindWorkById(gGameSession->at4.loc.area | ((arg0 << 12) | (gGameSession->at4.loc.stage << 8)));
    task = NULL;
    if (work != NULL) {
        task = (Task*)work->field_0;
    }
    return task;
}

void func_dryfield_night_garage_80180AB0(void)
{
    Gp_ApplyAreaRecs(D_dryfield_night_garage_801875D8);
    GameFlag_SetNibble(0x59, 0);
    GameFlag_SetNibble(0x5A, 0);
    GameFlag_SetNibble(0x4B, 5);
    GameFlag_SetNibble(0x35, 1);
    if (GameFlag_GetNibble(0xCE) != 0) {
        Gp_ApplyAreaRecs(D_dryfield_night_garage_80187620);
    }
}

void func_dryfield_night_garage_80180B20(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    s16         slot;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key = gGameSession->at4;
    if (Wip_SysFlags.field_0 == 2) {
        if (task->spawnArg1 != 0) {
            key.loc.view = 0x67;
        } else {
            key.loc.view = 0x65;
        }
    } else {
        if (task->spawnArg1 != 0) {
            key.loc.view = 0x66;
        } else {
            key.loc.view = 0x64;
        }
    }
    slot = Stream_FindSlot(key.raw.data, 0, 0);
    {
        s32 cmd;
        s32 zero;
        u8* p;
        cmd  = 0x61;
        zero = 0;
        p    = slotParam;
        SOFT_TOUCH_REG4(cmd, zero, p, slot);
        slotParam[0] = slot;
        CdCmd_Enqueue(cmd, zero, p);
    }
    goto advance;

L_case2:
    if (queue->field_1FA == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    taskKill(task);
    Display_ResetHeapWrapper();
}

/// Fades the screen to white: draws a white overlay whose level, kept in
/// `killCountdown`, rises by 4 each frame, and kills the task once it reaches
/// 0x100.
void func_dryfield_night_garage_80180CEC(Task* arg0)
{
    u16 temp_v0;

    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
    temp_v0             = arg0->killCountdown + 4;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}

/// Spawns `D_dryfield_night_garage_80183380` with an ordering table, passing
/// on the task's `spawnArg1`, sets `D_8007106B`, spawns the view tasks and
/// kills itself.
void func_dryfield_night_garage_80180D4C(Task* arg0)
{
    Display_SpawnWithOt(&D_dryfield_night_garage_80183380, 1, arg0->spawnArg1, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

/// Draws a glowing strip between `arg0[0]` and `arg0[1]`. Both points are
/// projected; when the second lies past the near limit, gouraud quads are
/// fanned around each projected point with a radius of `arg1 * 64` over its
/// depth, starting at angle `arg2`, and joined by quads between the two. The
/// inner colour alternates between 0x20 and 0x30 with the frame counter.
void func_dryfield_night_garage_80180D9C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg1 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        ang       = 0;
        base      = (s16)arg2;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 16) | 0x20;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = rgb;
            p->g2    = rgb;
            prim->b2 = rgb;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(base + t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            SCHED_BARRIER();
            t3             = ang - 0x1000;
            prim           = (POLY_G4*)gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->r1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Garage room draw: sweeps the glowing strip the current visit
/// (`gGameSession->at4.loc.view`) selects. Visits 3 and 15 sweep all four of the
/// room's run, 7 and 14 only its first pair, and 11 the last pair of the run
/// with its own blend (`arg2` 0x800 instead of 0). Each case names its own last
/// draw, which `jump.c` cross-jumps into one tail block after the last case.
void func_dryfield_night_garage_80181518(void)
{
    switch (gGameSession->at4.loc.view) {
        case 3:
        case 15: {
            SVECTOR* p = D_dryfield_night_garage_801833A4;
            func_dryfield_night_garage_80180D9C(&p[0], 0x200, 0);
            func_dryfield_night_garage_80180D9C(&p[2], 0x200, 0);
            func_dryfield_night_garage_80180D9C(&p[4], 0x200, 0);
            func_dryfield_night_garage_80180D9C(&p[6], 0x200, 0);
            break;
        }
        case 7:
        case 14:
            func_dryfield_night_garage_80180D9C(&D_dryfield_night_garage_801833A4[0], 0x200, 0);
            break;
        case 11: {
            SVECTOR* p = &D_dryfield_night_garage_801833D4;
            func_dryfield_night_garage_80180D9C(&p[0], 0x200, 0x800);
            func_dryfield_night_garage_80180D9C(&p[2], 0x200, 0x800);
            break;
        }
    }
}
