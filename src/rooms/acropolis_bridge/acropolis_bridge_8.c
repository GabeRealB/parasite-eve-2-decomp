#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_bridge.h"
#include "rooms/room_common.h"
#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// `rtps` / `rtpt` / `mvmva 1, 0, 0, 3, 0`. The `inline_c.h` macros of those
/// names assemble to different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern s32  Gp_LcgState;
extern void func_acropolis_bridge_801827EC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_8017F868);

/// The wide variant of the bridge's falling dust streak: same one-pixel `DR_MOVE`
/// smear as `func_acropolis_bridge_80180FF0`, rolled over the whole drop height
/// instead of the upper band. The first frame rolls the streak out of
/// `Gp_LcgState`: `field_10.vy` is the row it starts on (0x60..0xEF),
/// `field_24` the lifetime in frames, `field_26` the width and `field_28` the
/// number of frames each row of fall takes. The column window widens with the
/// starting row - it runs from `0x40 - (vy - 0x60) / 3` to `0xD0 + spread`,
/// where `spread` is half the drop from 0x60 capped at 0x20 - so streaks that
/// begin higher up stay nearer the middle of the screen.
/// `Display_State.field_1f` picks the buffer half, and the OT slot is the row
/// scaled into the 0x800-deep range so a streak sorts against the room behind
/// it. The task releases itself once the camera turns away, the lifetime runs
/// out, or the streak falls off the bottom of the screen.
void func_acropolis_bridge_80180320(Task* task)
{
    RoomEffWork* work;
    RECT         rect;
    DR_MOVE*     mv;
    u16          rnd;
    s32          rndx;
    s32          range;
    s32          bufferY;
    s32          x;
    s32          y;
    s32          depth;

    work    = task->spawnArg2;
    bufferY = Display_State.field_1f * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if ((s16)work->field_22 == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rnd               = (u32)Gp_LcgState >> 16;
            work->field_10.vy = (u32)rnd % 144 + 0x60;
            /* x and y double as the drift and spread of the column window here */
            x                 = (work->field_10.vy - 0x60) / 3;
            y                 = work->field_10.vy < 0xA0 ? (work->field_10.vy - 0x60) / 2 : 0x20;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rndx              = (u32)Gp_LcgState >> 16;
            range             = y + 0x90;
            work->field_10.vx = rndx % (x + range) + (0x40 - x);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rnd               = (u32)Gp_LcgState >> 16;
            work->field_24    = (u32)rnd % 90 + 0x1E;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_26    = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->field_10.vy + (s16)work->field_22 / (s16)work->field_28;
        x     = work->field_10.vx;
        depth = 0x840 - (y - 0x60) * 8;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->field_26;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(Gpu_CurrentOt + (depth >> 4), mv);
        }
        work->field_22++;
        if ((s16)work->field_22 <= (s16)work->field_24 && y < 0xEF) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_8018063C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_8018099C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_80180CC0);

/// One falling dust streak on the bridge, drawn as a `DR_MOVE` that smears a
/// one-pixel-tall strip of the frame buffer down by a pixel. The first frame
/// rolls the whole streak out of `Gp_LcgState`: `field_10.vy` is the row it
/// starts on (0x68..0xE7), `field_10.vx` the column, `field_24` the lifetime in
/// frames, `field_26` the width and `field_28` the number of frames each row of
/// fall takes. The column is drawn from a range that widens with the starting
/// row - `(vy - 0x58) * 6`, capped at the full 240-pixel width once the streak
/// starts at 0x80 or below the horizon - so streaks that begin higher up stay
/// nearer the middle of the screen. `Display_State.field_1f` picks the buffer
/// half, and the OT slot is the row scaled into the 0x800-deep range so a
/// streak sorts against the room behind it. The task releases itself once the
/// camera turns away, the lifetime runs out, or the streak falls off the bottom
/// of the screen.
void func_acropolis_bridge_80180FF0(Task* task)
{
    RoomEffWork* work;
    RECT         rect;
    DR_MOVE*     mv;
    u16          rnd;
    s32          rndx;
    s32          bufferY;
    s32          x;
    s32          y;
    s32          depth;

    work    = task->spawnArg2;
    bufferY = Display_State.field_1f * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if ((s16)work->field_22 == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vy = (((u32)Gp_LcgState >> 16) & 0x7F) + 0x68;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rndx              = (u32)Gp_LcgState >> 16;
            work->field_10.vx = work->field_10.vy < 0x80 ? rndx % ((work->field_10.vy - 0x58) * 6) : rndx % 240;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rnd               = (u32)Gp_LcgState >> 16;
            work->field_24    = (u32)rnd % 90 + 0x1E;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_26    = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->field_10.vy + (s16)work->field_22 / (s16)work->field_28;
        x     = work->field_10.vx;
        depth = 0x800 - (y - 0x68) * 8;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->field_26;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(Gpu_CurrentOt + (depth >> 4), mv);
        }
        work->field_22++;
        if ((s16)work->field_22 <= (s16)work->field_24 && y < 0xEF) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_801812F4);

/// The bridge's dust cloud: one semi-transparent `POLY_FT4` billboard placed at
/// the task's world position. The four corners are taken from the unit quad in
/// `D_acropolis_bridge_8018990C`, scaled by 0x300 and rotated by the
/// coordinate's `workm` - and then each rotated corner is overwritten with that
/// same `workm` translation, so all four collapse onto the object origin. The
/// quad is projected with one `RTPS` plus one `RTPT` directly into the
/// primitive, tinted a random grey, and linked into the OT at the `RTPS` depth
/// biased by 0x20; depths under 0x11 are dropped rather than drawn. The task
/// releases its work block on every tick, so the puff lasts one frame.
void func_acropolis_bridge_801819C8(Task* task)
{
    void**                      scratch;
    u8*                         head;
    AcropolisBridgeQuadScratch* block;
    AcropolisBridgeQuadCorner*  tbl;
    POLY_FT4*                   prim;
    GsCOORDINATE2*              coord;
    RoomEffWork*                work;
    MATRIX*                     m;
    SVECTOR*                    v;
    s32                         i;
    u8                          col;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = task->spawnArg2;
    Gp_UpdateCoord(coord);

    scratch        = (void**)G_SCRATCH_HEAD;
    i              = 0;
    m              = &coord->workm;
    tbl            = D_acropolis_bridge_8018990C;
    head           = (u8*)*scratch - sizeof(AcropolisBridgeQuadScratch);
    work->field_22 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0;
    *scratch       = head;
    block          = (AcropolisBridgeQuadScratch*)*scratch;
    do {
        v                = ((AcropolisBridgeQuadScratch*)((SVECTOR*)block + i))->vec;
        block->vec[i].vx = tbl[i].x * 0x300;
        v->vy            = 0;
        v->vz            = tbl[i].y * 0x300;
        gte_SetRotMatrix(m);
        gte_ldv0(&block->vec[i]);
        gte_rtv0_real();
        gte_stsv(&block->vec[i]);
        *(u16*)&block->vec[i].vx = *(u16*)&coord->workm.t[0];
        i++;
        *(u16*)&v->vy = *(u16*)&coord->workm.t[1];
        *(u16*)&v->vz = *(u16*)&coord->workm.t[2];
    } while (i < 4);

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps_real();
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt_real();
    setUV4(prim, 0, 0x10, 0x27, 0x10, 0, 0x37, 0x27, 0x37);
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&block->otz);
    block->otz += 0x20;
    if (block->otz >= 0x11) {
        prim->tpage = 0x2B;
        prim->clut  = 0x4381;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        col         = ((u32)Gp_LcgState >> 16) & 0xF;
        setRGB0(prim, col, col, col);
        setSemiTrans(prim, 1);
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(AcropolisBridgeQuadScratch);
    Gp_ReleaseState1CMem(work, task);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_80181D28);

s32 func_acropolis_bridge_801820A0(Task* task)
{
    GsCOORDINATE2* coord;
    SVECTOR        pos;
    s32            i;

    coord = ((TmdObject*)task->extra)->field_8;

    i = 0;
    do {
        pos.vx      = -0x3E58;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vy      = ((u32)Gp_LcgState >> 16) % 1536 + 0xF830;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vz      = (((u32)Gp_LcgState >> 16) & 0xF) + 0xF63C;
        Gp_SpawnEff(0x600BC, coord, 0, &pos);

        pos.vx      = -0x3E58;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vy      = ((u32)Gp_LcgState >> 16) % 1536 + 0xF830;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vz      = 0xFA06 - (((u32)Gp_LcgState >> 16) & 0xF);
        Gp_SpawnEff(0x600BC, coord, 0, &pos);

        pos.vx      = -0x3E58;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vy      = (((u32)Gp_LcgState >> 16) & 0xF) + 0xF830;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vz      = (u16)((u32)Gp_LcgState >> 16) % 970 + 0xF63C;
        Gp_SpawnEff(0x600BC, coord, 0, &pos);
        i++;
    } while (i < 0x20);

    i = 0;
    do {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vx      = -0x3E58;
        pos.vy      = ((u32)Gp_LcgState >> 16) % 1536 - 0x7D0;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vz      = (u16)((u32)Gp_LcgState >> 16) % 970 - 0x9C4;
        Gp_SpawnEff(0x600BC, coord, 0, &pos);
        i++;
    } while (i < 8);

    return 0;
}

/// One falling mote of the bridge's ambient dust: drifts the task's coordinate
/// frame by the per-mote velocity in `RoomEffWork::field_10`, projects the
/// result through `GsWSMATRIX` with a single `RTPS`, and links a 1x1 tile into
/// the OT at the resulting depth. The velocity and the grey level are rolled
/// once, on the first tick (`field_22 == 0`); the mote is released after 0x1F
/// ticks or once it has fallen past y = -0x1D.
void func_acropolis_bridge_80182394(Task* task)
{
    void**                      scratch;
    u8*                         head;
    AcropolisBridgeMoteScratch* block;
    AcropolisBridgeMoteScratch* depth;
    TILE_1*                     prim;
    GsCOORDINATE2*              coord;
    RoomEffWork*                work;

    scratch  = (void**)G_SCRATCH_HEAD;
    coord    = ((TmdObject*)task->extra)->field_8;
    head     = *scratch;
    block    = (AcropolisBridgeMoteScratch*)(head - 0xC);
    *scratch = block;
    depth    = block;
    work     = task->spawnArg2;
    Gp_UpdateCoord(coord);

    if ((s16)work->field_22 == 0) {
        work->field_10.vz = 0;
        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
        work->field_10.vx = ((u32)Gp_LcgState >> 16) & 0xF;
        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
        work->field_10.vy = (((u32)Gp_LcgState >> 16) & 3) - 1;
        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
        work->field_24    = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x30;
    }

    coord->coord.t[0] += work->field_10.vx;
    coord->coord.t[1] += work->field_10.vy;
    coord->coord.t[2] += work->field_10.vz;
    coord->flg         = 0;
    block->vec.vx      = *(u16*)&coord->workm.t[0];
    block->vec.vy      = *(u16*)&coord->workm.t[1];
    block->vec.vz      = *(u16*)&coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AcropolisBridgeMoteScratch*)(head - 0xC))->vec);
    gte_rtps_real();
    prim           = (TILE_1*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setTile1(prim);
    gte_stsxy(&prim->x0);
    gte_stszotz(&depth->otz);
    if (((AcropolisBridgeMoteScratch*)(head - 0xC))->otz >= 0x11) {
        setRGB0(prim, work->field_24 >> 1, work->field_24, work->field_24);
        addPrim((u_long*)(((((u32)((AcropolisBridgeMoteScratch*)(head - 0xC))->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 0, ((AcropolisBridgeMoteScratch*)(head - 0xC))->otz);
        work->field_10.vy += 6;
    }
    *scratch = (u8*)*scratch + 0xC;
    work->field_22++;
    if ((s16)work->field_22 >= 0x1F || coord->coord.t[1] >= -0x1D) {
        Gp_ReleaseState1CMem(work, task);
    }
}

void func_acropolis_bridge_80182694(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    if (Gp_State1C->field_4 != 0) {
        func_acropolis_bridge_801827EC(coord, (s16)work->field_26, (s16)work->field_24);
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->field_22++;
        switch (task->state) {
            case 0:
                work->field_24 = 0x40;
                work->field_26 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
                coord->flg  = 0;
                task->state = 1;
                /* fallthrough */
            case 1:
                work->field_26 += 0x20;
                func_acropolis_bridge_801827EC(coord, (s16)work->field_26, (s16)work->field_24);
                if ((s16)work->field_24 >= 3) {
                    work->field_24 -= 2;
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_801827EC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_80182AF8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_80182F8C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_801833A0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", D_acropolis_bridge_8017D6CC);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", D_acropolis_bridge_8017D6E8);
