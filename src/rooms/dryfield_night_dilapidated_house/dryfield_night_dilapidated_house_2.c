#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "rooms/dryfield_night_dilapidated_house.h"

extern s8 D_8007106B;

/// `rtps` / `rtpt`. The `inline_c.h` macros of those names assemble to
/// different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
/// `mvmva` rotating V0 by the rotation matrix with no translation.
#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Scratch block one quad is built in: the GTE depth of its last three
/// corners, then the four corners after they are placed in world space.
typedef struct _DryfieldNightDilapidatedHouseBandScratch {
    s32     otz;
    SVECTOR v[4];
} _DryfieldNightDilapidatedHouseBandScratch;

/// The prism corners, eight per prism: a lit ring of four, then the far ring.
extern SVECTOR D_dryfield_night_dilapidated_house_801872CC[];

/// Entry 1 of the room's two-entry descriptor table, the task that plays a
/// stream. It blanks the display and allocates the auxiliary buffers, looks
/// up the stream slot for the current location with view 0x65 or 0x64
/// (chosen by `Wip_SysFlags.field_0`) and queues CD command 0x61 for it,
/// shows the display once the queue's `field_1FA` is set, and blanks it again
/// when the CD goes idle - or, on the pad's 0x800 flag, early, activating CD
/// phase 1. Once the CD is idle it restores the stream state, clears the
/// image buffers, shows the display again, kills itself and resets the heap.
void func_dryfield_night_dilapidated_house_8017DB20(Task* arg0)
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
        key.loc.view = 0x65;
    } else {
        key.loc.view = 0x64;
    }
    slot = Stream_FindSlot(key.raw.data, 0, 0);
    {
        register s32 cmd asm("a0");
        register s32 zero asm("a1");
        register u8* p asm("a2");
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
    if ((Stream_RestoreAfterLoad(0, 0) & 0xFFFF) == 0) {
        return;
    }
    Mem_Set(Fs_ImgBuffers, 0, 0x25800);
    SetDispMask(1);
    taskKill(task);
    Display_ResetHeapWrapper();
}

/// Entry 0 of the room's two-entry descriptor table: spawns entry 1, the
/// stream-playing task, with an ordering table, sets `D_8007106B`, spawns the
/// view tasks and kills itself.
void func_dryfield_night_dilapidated_house_8017DCE0(Task* arg0)
{
    Display_SpawnWithOt(&D_dryfield_night_dilapidated_house_801872B4, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

/// Draws a four-sided prism from the eight corners at
/// `D_dryfield_night_dilapidated_house_801872CC[arg1..]` as five gouraud
/// `POLY_G4`: four sides joining the lit ring `[0..3]` to the far ring
/// `[4..7]`, then a cap over the lit ring. Each corner is rotated by `coord`'s
/// `workm` and moved by its translation before projection through
/// `GsWSMATRIX`. The lit corners share a pulsing colour whose red is three
/// quarters of its green and blue; the far corners are black.
void func_dryfield_night_dilapidated_house_8017DD30(GsCOORDINATE2* coord, s16 arg1)
{
    _DryfieldNightDilapidatedHouseBandScratch* blk;
    POLY_G4*                                   prim;
    s32                                        i;
    s32                                        next;
    s32                                        far;
    s32                                        farNext;
    s16                                        pulse;
    s16                                        red;
    s16                                        blue;
    s16                                        green;

    pulse                  = (rsin(gDisplayState.animFrame << 10) >> 12) + 0x10;
    *(u8**)G_SCRATCH_HEAD -= sizeof(_DryfieldNightDilapidatedHouseBandScratch);
    blk                    = *(_DryfieldNightDilapidatedHouseBandScratch**)G_SCRATCH_HEAD;
    gte_SetTransMatrix(&GsWSMATRIX);
    red   = pulse * 3 / 4;
    green = pulse;
    blue  = pulse;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + i]);
        gte_mvmva_real();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx += coord->workm.t[0];
        blk->v[0].vy += coord->workm.t[1];
        blk->v[0].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        next = (i + 1) & 3;
        gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + next]);
        gte_mvmva_real();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx += coord->workm.t[0];
        blk->v[1].vy += coord->workm.t[1];
        blk->v[1].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        far = i + 4;
        gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + far]);
        gte_mvmva_real();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx += coord->workm.t[0];
        blk->v[2].vy += coord->workm.t[1];
        blk->v[2].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        farNext = next + 4;
        gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + farNext]);
        gte_mvmva_real();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx += coord->workm.t[0];
        blk->v[3].vy += coord->workm.t[1];
        blk->v[3].vz += coord->workm.t[2];
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps_real();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt_real();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        setRGB0(prim, red, green, blue);
        setRGB1(prim, red, green, blue);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1]);
    gte_mvmva_real();
    gte_stsv(&blk->v[0]);
    blk->v[0].vx += coord->workm.t[0];
    blk->v[0].vy += coord->workm.t[1];
    blk->v[0].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + 1]);
    gte_mvmva_real();
    gte_stsv(&blk->v[1]);
    blk->v[1].vx += coord->workm.t[0];
    blk->v[1].vy += coord->workm.t[1];
    blk->v[1].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + 3]);
    gte_mvmva_real();
    gte_stsv(&blk->v[2]);
    blk->v[2].vx += coord->workm.t[0];
    blk->v[2].vy += coord->workm.t[1];
    blk->v[2].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + 2]);
    gte_mvmva_real();
    gte_stsv(&blk->v[3]);
    blk->v[3].vx += coord->workm.t[0];
    blk->v[3].vy += coord->workm.t[1];
    blk->v[3].vz += coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps_real();
    prim           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt_real();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    setRGB0(prim, red, green, blue);
    setRGB1(prim, red, green, blue);
    setRGB2(prim, red, green, blue);
    setRGB3(prim, red, green, blue);
    addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    *(u8**)G_SCRATCH_HEAD += sizeof(_DryfieldNightDilapidatedHouseBandScratch);
}

/// Per-frame draw of the room's model task: recomputes the model's world
/// coordinate, then draws up to three prisms, from corner sets 0, 8 and 0x10.
/// Each is gated on `gGameSession->at4.loc.view` taken as a bit index into a
/// fixed mask; the second mask is contained in the other two, so a view in it
/// draws all three.
void func_dryfield_night_dilapidated_house_8017E670(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            mask;

    coord = ((TmdObject*)arg0->extra)->coords;
    mask  = 1 << gGameSession->at4.loc.view;
    Gp_UpdateCoord(coord);
    if (mask & 0x99C) {
        func_dryfield_night_dilapidated_house_8017DD30(coord, 0);
    }
    if (mask & 0x998) {
        func_dryfield_night_dilapidated_house_8017DD30(coord, 8);
    }
    if (mask & 0x9F8) {
        func_dryfield_night_dilapidated_house_8017DD30(coord, 0x10);
    }
}
