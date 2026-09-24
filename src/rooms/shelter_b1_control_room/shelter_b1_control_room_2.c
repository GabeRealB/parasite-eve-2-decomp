#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"
#include "rooms/room_common.h"

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s8       D_8007106B;
extern s32      D_80115730;
extern s32      D_80115734;
extern s32      D_80115754;
extern TaskDesc D_shelter_b1_control_room_80181BBC;
extern SVECTOR  D_shelter_b1_control_room_80181BD4[];
extern SVECTOR  D_shelter_b1_control_room_80181C3C[];

void func_shelter_b1_control_room_8017F39C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b1_control_room_8017FBE0(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Streamed-scene task. It blanks the display and queues CD command 0x61 on
/// the stream slot of the current location with its view replaced by 0x64,
/// then shows the display once the command queue signals it. The scene runs
/// until the CD is idle or the pad check aborts it, which is recorded in
/// `spawnArg1`. After the stream state is restored an aborted scene kills the
/// task at once, and a finished one after 0x3D more ticks; either way the
/// display heap is reset.
void func_shelter_b1_control_room_8017EF24(Task* arg0)
{
    u8          slotParam[4];
    s32         state;
    GameLoc     key;
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
        case 6:
            goto L_case6;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = gGameSession->at4;
    key.loc.view = 0x64;
    slotParam[0] = Stream_FindSlot(key.raw.data, 0, 0);
    CdCmd_Enqueue(0x61, 0, slotParam);
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
        state           = task->state;
        task->spawnArg1 = 0;
        task->state     = state + 1;
        return;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    task->spawnArg1 = 1;
    task->state     = task->state + 1;
    return;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
    goto advance;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    if (task->spawnArg1 != 0) {
        goto kill;
    }
advance:
    task->state = task->state + 1;
    return;

L_case6:
    task->killCountdown = task->killCountdown + 1;
    if (task->killCountdown < 0x3D) {
        return;
    }
kill:
    taskKill(task);
    Display_ResetHeapWrapper();
}

void func_shelter_b1_control_room_8017F100(Task* arg0)
{
    Display_SpawnWithOt(&D_shelter_b1_control_room_80181BBC, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

void func_shelter_b1_control_room_8017F150(Task* task)
{
    u8       view;
    SVECTOR* p;

    if (task->state == 0) {
        D_80115734  = 0x60276;
        D_80115730  = 0x60277;
        D_80115754  = 0x60278;
        task->state = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            p = D_shelter_b1_control_room_80181BD4;
            SOFT_TOUCH_REG(p);
            func_shelter_b1_control_room_8017F39C(&p[0], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[2], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[4], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[6], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[8], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[10], 0x100, 0x243);
            func_shelter_b1_control_room_8017FBE0(&p[12], 0x180, 0x421);
            break;
        case 3:
            p = D_shelter_b1_control_room_80181BD4;
            SOFT_TOUCH_REG(p);
            func_shelter_b1_control_room_8017F39C(&p[0], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[2], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[4], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[6], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[8], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[10], 0x100, 0x243);
            func_shelter_b1_control_room_8017FBE0(&p[12], 0x180, 0x421);
            func_shelter_b1_control_room_8017FBE0(&p[14], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[15], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[16], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[17], 0x200, 0x23);
            break;
        case 4:
            p = D_shelter_b1_control_room_80181C3C;
            SOFT_TOUCH_REG(p);
            func_shelter_b1_control_room_8017FBE0(&p[0], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[1], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[2], 0x200, 0x23);
            break;
        case 6:
            p = D_shelter_b1_control_room_80181C3C;
            SOFT_TOUCH_REG(p);
            func_shelter_b1_control_room_8017FBE0(&p[0], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[1], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[2], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[3], 0x200, 0x23);
            break;
    }
}

/// Draws a glowing bar between the world points `arg0[0]` and `arg0[1]`,
/// projected through `Gfx_ViewWorldMtx`; nothing is drawn when either
/// projection flags an error. Each end gets a half-disc of gouraud wedges of
/// radius `(s16)arg1 * 64` over its depth, joined by quads across the bar. The
/// lit vertices take the colour packed in `arg2`, one nibble per channel in
/// the high nibble, with bit 3 following the animation frame.
void func_shelter_b1_control_room_8017F39C(SVECTOR* arg0, s32 arg1, s32 arg2)
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
    gte_rtps_real();
    gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx0);
    gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps_real();
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

/// Draws a glowing disc at the world point `arg0`, projected through
/// `Gfx_ViewWorldMtx`, unless the projection flags an error: four gouraud
/// wedges of radius `(s16)arg1 * 64` over the depth, black at the rim. The
/// centre takes the colour packed in `arg2`, one nibble per channel in the
/// high nibble, with bit 3 following the animation frame.
void func_shelter_b1_control_room_8017FBE0(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    register u8*       tmp asm("v0");
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (RoomDraw13Scratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1 = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        ang  = 0;
        tmp  = (u8*)&gDisplayState;
        SOFT_TOUCH_REG(tmp);
        ds            = (DisplayState*)tmp;
        blend         = (*(u8*)&ds->animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}
