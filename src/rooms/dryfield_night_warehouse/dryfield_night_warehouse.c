#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

void func_dryfield_night_warehouse_8017D610(Task* task);
void func_dryfield_night_warehouse_8017D654(Task* task);
void func_dryfield_night_warehouse_8017D6B4(GsCOORDINATE2* coord, s16 arg1);
void func_dryfield_night_warehouse_8017DFF4(GsCOORDINATE2* coord, s16 arg1, s16 arg2);

/// The room's message table: handlers for messages 0x13EE, 0x13F1, 0x13EF and
/// 0x13F0, closed by a 0x7FFFFFFF entry.
extern GpMsgEntry D_dryfield_night_warehouse_8017E830[];

/// Ring centres in the space of the coordinate drawn under, one per circle.
extern SVECTOR D_dryfield_night_warehouse_8017E858[];
/// Ring radii, parallel to the centres.
extern s16 D_dryfield_night_warehouse_8017E8D8[];

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and answers 0.
s32 func_dryfield_night_warehouse_8017D5D0(void)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table: copies the location
/// record the sender passes onto the reply record and answers 1.
s32 func_dryfield_night_warehouse_8017D5D8(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}

/// Handler for message 0x13F0 in the room's message table: does nothing and
/// answers 0.
s32 func_dryfield_night_warehouse_8017D600(void)
{
    return 0;
}

/// Handler for message 0x13EF in the room's message table: does nothing and
/// answers 0.
s32 func_dryfield_night_warehouse_8017D608(void)
{
    return 0;
}

/// State 0 of the room task: publishes the room's message table in
/// `Task::msgTable`, claims pointer slot 7 and advances to the next state.
void func_dryfield_night_warehouse_8017D610(Task* task)
{
    task->msgTable = D_dryfield_night_warehouse_8017E830;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room task: the room idles here and does nothing.
void func_dryfield_night_warehouse_8017D654(Task* task)
{
}

/// The room task's three states: set up, idle, then `taskKill`.
const TaskFuncTable3 D_dryfield_night_warehouse_8017D5C4 = {
    { func_dryfield_night_warehouse_8017D610, func_dryfield_night_warehouse_8017D654, taskKill },
};

/// The room task: copies its three-state table onto the stack and runs the
/// entry for the task's current state.
void func_dryfield_night_warehouse_8017D65C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_warehouse_8017D5C4;
    sp.funcs[task->state](task);
}

/// Draws a four-sided prism from `D_dryfield_night_warehouse_8017E858[arg1..]`
/// as five gouraud `POLY_G4`: four sides joining the lit ring `[0..3]` to the
/// far ring `[4..7]`, then a cap over the lit ring. Each corner is rotated by
/// `coord`'s `workm` and moved by its translation before projection through
/// `GsWSMATRIX`. The lit corners share a pulsing colour whose red is three
/// quarters of its green and blue; the far corners are black.
void func_dryfield_night_warehouse_8017D6B4(GsCOORDINATE2* coord, s16 arg1)
{
    RoomQuadScratch* blk;
    POLY_G4*         prim;
    s32              i;
    s32              next;
    s32              far;
    s32              farNext;
    s16              pulse;
    s16              red;
    s16              blue;
    s16              green;

    pulse = (rsin(gDisplayState.animFrame << 10) >> 12) + 0x10;
    SCRATCH_PUSH(RoomQuadScratch);
    blk = SCRATCH_HEAD(RoomQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    red   = pulse * 3 / 4;
    green = pulse;
    blue  = pulse;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1 + i]);
        gte_rtv0();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx += coord->workm.t[0];
        blk->v[0].vy += coord->workm.t[1];
        blk->v[0].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        next = (i + 1) & 3;
        gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1 + next]);
        gte_rtv0();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx += coord->workm.t[0];
        blk->v[1].vy += coord->workm.t[1];
        blk->v[1].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        far = i + 4;
        gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1 + far]);
        gte_rtv0();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx += coord->workm.t[0];
        blk->v[2].vy += coord->workm.t[1];
        blk->v[2].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        farNext = next + 4;
        gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1 + farNext]);
        gte_rtv0();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx += coord->workm.t[0];
        blk->v[3].vy += coord->workm.t[1];
        blk->v[3].vz += coord->workm.t[2];
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
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
    gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1]);
    gte_rtv0();
    gte_stsv(&blk->v[0]);
    blk->v[0].vx += coord->workm.t[0];
    blk->v[0].vy += coord->workm.t[1];
    blk->v[0].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1 + 1]);
    gte_rtv0();
    gte_stsv(&blk->v[1]);
    blk->v[1].vx += coord->workm.t[0];
    blk->v[1].vy += coord->workm.t[1];
    blk->v[1].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1 + 3]);
    gte_rtv0();
    gte_stsv(&blk->v[2]);
    blk->v[2].vx += coord->workm.t[0];
    blk->v[2].vy += coord->workm.t[1];
    blk->v[2].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_warehouse_8017E858[arg1 + 2]);
    gte_rtv0();
    gte_stsv(&blk->v[3]);
    blk->v[3].vx += coord->workm.t[0];
    blk->v[3].vy += coord->workm.t[1];
    blk->v[3].vz += coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();
    prim           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
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
    SCRATCH_POP(RoomQuadScratch);
}

/// Draws the band joining ring `arg1` to ring `arg1 + 1` as `arg2` gouraud
/// `POLY_G4` segments, starting at an angle that turns with
/// `gDisplayState.animFrame`. Each corner is rotated by `coord`'s `workm` and
/// moved by its translation before projection through `GsWSMATRIX`. The corners
/// on ring `arg1` share a pulsing colour whose red is three quarters of its
/// green and blue; the corners on ring `arg1 + 1` are black.
void func_dryfield_night_warehouse_8017DFF4(GsCOORDINATE2* coord, s16 arg1, s16 arg2)
{
    RoomQuadScratch* blk;
    POLY_G4*         prim;
    s16              red;
    s16              blue;
    s16              green;
    s16              step;
    s16              start;
    s16              pulse;
    s32              angle;
    s32              next;

    pulse = (rsin(gDisplayState.animFrame << 10) >> 12) + 0x10;
    SCRATCH_PUSH(RoomQuadScratch);
    blk   = SCRATCH_HEAD(RoomQuadScratch);
    start = gDisplayState.animFrame & 0xFFF;
    step  = 0x1000 / arg2;
    gte_SetTransMatrix(&GsWSMATRIX);
    red   = pulse * 3 / 4;
    green = pulse;
    blue  = pulse;
    for (angle = start; angle < start + step * arg2; angle = next) {
        blk->v[0].vx = D_dryfield_night_warehouse_8017E858[arg1].vx +
                       ((rsin(angle) * D_dryfield_night_warehouse_8017E8D8[arg1]) >> 12);
        blk->v[0].vy = D_dryfield_night_warehouse_8017E858[arg1].vy;
        blk->v[0].vz = D_dryfield_night_warehouse_8017E858[arg1].vz +
                       ((rcos(angle) * D_dryfield_night_warehouse_8017E8D8[arg1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[0]);
        gte_rtv0();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx += coord->workm.t[0];
        blk->v[0].vy += coord->workm.t[1];
        next          = angle + step;
        blk->v[0].vz += coord->workm.t[2];

        blk->v[1].vx = D_dryfield_night_warehouse_8017E858[arg1].vx +
                       ((rsin(next) * D_dryfield_night_warehouse_8017E8D8[arg1]) >> 12);
        blk->v[1].vy = D_dryfield_night_warehouse_8017E858[arg1].vy;
        blk->v[1].vz = D_dryfield_night_warehouse_8017E858[arg1].vz +
                       ((rcos(next) * D_dryfield_night_warehouse_8017E8D8[arg1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[1]);
        gte_rtv0();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx += coord->workm.t[0];
        blk->v[1].vy += coord->workm.t[1];
        blk->v[1].vz += coord->workm.t[2];

        blk->v[2].vx = D_dryfield_night_warehouse_8017E858[arg1 + 1].vx +
                       ((rsin(angle) * D_dryfield_night_warehouse_8017E8D8[arg1 + 1]) >> 12);
        blk->v[2].vy = D_dryfield_night_warehouse_8017E858[arg1 + 1].vy;
        blk->v[2].vz = D_dryfield_night_warehouse_8017E858[arg1 + 1].vz +
                       ((rcos(angle) * D_dryfield_night_warehouse_8017E8D8[arg1 + 1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[2]);
        gte_rtv0();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx += coord->workm.t[0];
        blk->v[2].vy += coord->workm.t[1];
        blk->v[2].vz += coord->workm.t[2];

        blk->v[3].vx = D_dryfield_night_warehouse_8017E858[arg1 + 1].vx +
                       ((rsin(next) * D_dryfield_night_warehouse_8017E8D8[arg1 + 1]) >> 12);
        blk->v[3].vy = D_dryfield_night_warehouse_8017E858[arg1 + 1].vy;
        blk->v[3].vz = D_dryfield_night_warehouse_8017E858[arg1 + 1].vz +
                       ((rcos(next) * D_dryfield_night_warehouse_8017E8D8[arg1 + 1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[3]);
        gte_rtv0();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx += coord->workm.t[0];
        blk->v[3].vy += coord->workm.t[1];
        blk->v[3].vz += coord->workm.t[2];

        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
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
    SCRATCH_POP(RoomQuadScratch);
}

/// Per-frame effect on the room's model task: recomputes the model's world
/// matrix and then re-poses it. The current visit is the stage-visit byte
/// `gGameSession->at4.loc.view` taken as a bit index, and each pose is gated on that
/// bit being one of a fixed set of visits.
void func_dryfield_night_warehouse_8017E778(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            mask;

    coord = arg0->extra.tmd->coords;
    mask  = 1 << gGameSession->at4.loc.view;
    Gp_UpdateCoord(coord);
    if (mask & 0x24C) {
        func_dryfield_night_warehouse_8017D6B4(coord, 8);
    }
    if (mask & 4) {
        func_dryfield_night_warehouse_8017DFF4(coord, 0, 8);
    }
    if (mask & 0x24C) {
        func_dryfield_night_warehouse_8017DFF4(coord, 2, 8);
    }
    if (mask & 0x3DC) {
        func_dryfield_night_warehouse_8017DFF4(coord, 4, 8);
        func_dryfield_night_warehouse_8017DFF4(coord, 6, 8);
    }
}
