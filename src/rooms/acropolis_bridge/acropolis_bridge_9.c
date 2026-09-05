#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_bridge.h"
#include "rooms/room_common.h"
#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// `rtps` / `rtpt` / `mvmva 1, 0, 0, 3, 0` / `gpf 1`. The `inline_c.h` macros
/// of those names assemble to different words, so spell the instructions out.
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern s32 Gp_LcgState;
extern s32 D_80115738;
extern s32 D_8011574C;

extern GpMsgEntry D_acropolis_bridge_801898FC[];
extern SVECTOR    D_acropolis_bridge_8018991C[7];
extern SVECTOR    D_acropolis_bridge_80189954[7];
extern SVECTOR    D_acropolis_bridge_8018998C[11];
extern SVECTOR    D_acropolis_bridge_801899E4;
extern u16        D_acropolis_bridge_801899EC[8];
extern u16        D_acropolis_bridge_801899FC[16];
extern u16        D_acropolis_bridge_80189A1C[11];
extern u16        D_acropolis_bridge_80189A32;
extern SVECTOR    D_acropolis_bridge_80189A34[2];
extern SVECTOR    D_acropolis_bridge_80189A44;
extern SVECTOR    D_acropolis_bridge_80189A4C;

extern void func_acropolis_bridge_801827EC(GsCOORDINATE2* arg0, s32 arg1, s16 arg2);
extern void func_acropolis_bridge_80182F8C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
extern void func_acropolis_bridge_801833A0(GsCOORDINATE2* arg0, u16 arg1, s16 arg2);

/// Per-frame driver for the bridge's ambient effect field, and the room's
/// message-table owner. On the first frame it publishes
/// `D_acropolis_bridge_801898FC` as slot 5's `Gp_DispatchMsg` table and seeds
/// `D_acropolis_bridge_80189A34` with the two tracked cable joints' world
/// positions.
///
/// Each frame it re-spawns the effects the current camera can see: the two
/// per-view bitmask tables (`D_acropolis_bridge_801899EC` /
/// `D_acropolis_bridge_80189A1C`) say which of the placed emitters are visible
/// from view `Gp_GetViewIndex()`, and each visible entry spawns its dust
/// (0x600B1 / 0x600B2) or spark (0x600B3) at the matching `SVECTOR`. View 9
/// lifts the dust 0x240 above the placed point.
///
/// On views 2, 5 and 6 (`bit & 0x62`) it also trails debris off the two moving
/// joints: `field_26` is the Manhattan distance the joint travelled since last
/// frame, biased by 0x20, and two `Gp_LcgState` rolls against that distance
/// decide whether this frame emits `D_8011574C` / `D_80115738`. The joint's
/// new position is written back for the next frame's delta.
///
/// `D_acropolis_bridge_801899FC` finally maps the view onto one of five
/// looping ambience effects (0x600B4..0x600B8): entering the view bursts 30
/// copies at once, staying in it emits one per frame, or one in two / one in
/// three while `Gp_State1C->field_16` says the scene is quiet.

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

/// Draws the flash the bridge collapse throws off as a screen-facing quad: the
/// unit quad `D_80111E38` scaled to `arg1` half-size, rotated by the task's own
/// `GsCOORDINATE2` (`workm`) and then projected through `GsWSMATRIX` into a
/// 0x28-byte `G_SCRATCH_HEAD` block. The first corner goes through `rtps` and
/// the other three through `rtpt`; a GTE error (`gte_stflg` sign bit) drops the
/// quad rather than drawing it. The `POLY_FT4` is the 0x38x0x38 cell at
/// `(0, 0x38)` of tpage 0x2B, modulated by the grey `arg2` and drawn
/// semi-transparent, and links into the OT at the projected depth.
void func_acropolis_bridge_801827EC(GsCOORDINATE2* arg0, s32 arg1, s16 arg2)
{
    register GsCOORDINATE2*        coord asm("t7");
    void**                         scratch;
    u8*                            head;
    AcropolisBridgeRotQuadScratch* blk;
    POLY_FT4*                      prim;
    GpQuadCorner*                  tbl;
    SVECTOR*                       sv;
    MATRIX*                        wm;
    s32                            i;

    coord = arg0;
    SOFT_TOUCH_REG(coord);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = (u8*)*scratch - sizeof(AcropolisBridgeRotQuadScratch);
    SOFT_TOUCH_REG(head);
    *scratch = head;
    blk      = (AcropolisBridgeRotQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    wm  = &coord->workm;
    tbl = D_80111E38;
    do {
        blk->v[i].vx = tbl[i].x * arg1;
        sv           = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(AcropolisBridgeRotQuadScratch, v));
        sv->vy       = 0;
        sv->vz       = tbl[i].y * arg1;
        gte_SetRotMatrix(wm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0_real();
        gte_stsv(&blk->v[i]);
        *(u16*)&blk->v[i].vx = *(u16*)&blk->v[i].vx + *(u16*)&coord->workm.t[0];
        *(u16*)&sv->vy       = *(u16*)&sv->vy + *(u16*)&coord->workm.t[1];
        i++;
        *(u16*)&sv->vz = *(u16*)&sv->vz + *(u16*)&coord->workm.t[2];
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps_real();
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt_real();
    setUV4(prim, 0, 0x38, 0x37, 0x38, 0, 0x6F, 0x37, 0x6F);
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stflg(&blk->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&blk->otz);
        blk->otz++;
        prim->tpage = 0x2B;
        setRGB0(prim, arg2, arg2, arg2);
        prim->clut = 0x43D1;
        setSemiTrans(prim, 1);
        addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(AcropolisBridgeRotQuadScratch);
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", D_acropolis_bridge_8017D6B0);
