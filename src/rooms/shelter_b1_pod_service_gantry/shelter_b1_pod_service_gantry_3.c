#include "common.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "gameplay/gameplay.h"
#include "rooms/room_common.h"
#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern void D_shelter_b1_pod_service_gantry_8017FAF4;

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_3", func_shelter_b1_pod_service_gantry_8017D8F4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_3", func_shelter_b1_pod_service_gantry_8017DF70);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_3", func_shelter_b1_pod_service_gantry_8017E400);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_3", func_shelter_b1_pod_service_gantry_8017E880);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_3", func_shelter_b1_pod_service_gantry_8017ED3C);

/// Same body as `Room_Draw41` except that the 0x18-byte scratch block is
/// zeroed with `Mem_Set` before use: projects the coordinate's world position
/// through `GsWSMATRIX` and, when the GTE flag is non-negative, queues one
/// shade-tex `POLY_FT4` (tpage 0x2B, clut 0x4393) with a 56-texel UV tile
/// picked by `arg1` and an on-screen radius of `arg2 * 55 / otz`.
void func_shelter_b1_pod_service_gantry_8017F160(GsCOORDINATE2* arg0, u16 arg1, s16 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw14Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    DisplayState*      ds;
    s32                tex;
    u32                cell;
    s32                u1;
    s32                v0;
    s32                v1;
    s32                sarg;
    s32                t;
    s16                xy;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (RoomDraw14Scratch*)(head - 0x18);
    *scratch = block;
    Mem_Set(block, 0, 0x18);
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    block->vec.vz                               = *(u16*)&arg0->workm.t[2];
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        cell        = arg1;
        prim->clut  = 0x4393;
        tex         = (cell & 3) * 0x38;
        SOFT_BARRIER();
        v0            = ((cell & 7) >> 2) * 0x38;
        u1            = tex + 0x37;
        prim->v0      = v0;
        prim->v1      = v0;
        v1            = v0 + 0x37;
        prim->u1      = u1;
        prim->u3      = u1;
        sarg          = arg2;
        prim->v2      = v1;
        prim->v3      = v1;
        t             = sarg * 0x38;
        prim->u0      = tex;
        prim->u2      = tex;
        block->radius = (t - sarg) / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x0 = prim->x2 = xy;
        xy                  = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x1 = prim->x3 = xy;
        xy                  = (*(u16*)&block->sy - *(u16*)&block->radius) - (block->radius >> 1);
        ds                  = &gDisplayState;
        prim->y0 = prim->y1 = xy;
        xy                  = *(u16*)&block->sy + (block->radius >> 1);
        prim->y2 = prim->y3 = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_3", func_shelter_b1_pod_service_gantry_8017F450);
