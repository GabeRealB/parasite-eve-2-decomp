#include "common.h"

#include "main/display.h"
#include "main/mem.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// `Room_Draw23` with `s16` arguments: projects the coordinate's world position
/// through `GsWSMATRIX` and, when the GTE flag is non-negative, queues one
/// semi-transparent shade-tex `POLY_FT4` (tpage 0x2B, clut 0x43D2). `arg1`
/// selects a 56-texel UV tile with signed `% 4` / `% 8` instead of the masks,
/// then the quad is biased to v+0x70..v-0x59. The on-screen radius is
/// `arg2 * 55 / otz`; the quad is 2*radius on a side, shifted up so the
/// projected point sits at three-quarters height. Shared body, linked into
/// every room overlay that uses it.
void Room_Draw28(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw23Scratch* block;
    RoomDraw23Scratch* p;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s16                cell;
    s16                cell2;
    s32                u0;
    s32                vbase;
    s32                v0u;
    s32                x;
    s16                xy;
    u16                vy;
    u16                vz;

    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw23Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (RoomDraw23Scratch*)(head - 0x18);
    vy                                          = *(u16*)&arg0->workm.t[1];
    SOFT_TOUCH_REG_USE(block, vy);
    p = block;
    SOFT_TOUCH_REG(p);
    p->vec.vy = vy;
    vz        = *(u16*)&arg0->workm.t[2];
    *scratch  = block;
    p->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&p->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw23Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw23Scratch*)(head - 0x18))->flag);
    if (p->flag >= 0) {
        gte_stszotz(&((RoomDraw23Scratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        cell        = arg1 % 4;
        u0          = cell * 0x38;
        prim->u0    = u0;
        cell2       = arg1 % 8;
        x           = cell2;
        vbase       = (x / 4) * 0x38;
        SOFT_USE_REG(x);
        v0u = vbase + 0x70;
        SOFT_BARRIER();
        head     = (u8*)(u0 + 0x37);
        prim->v0 = v0u;
        prim->u1 = (s32)head;
        prim->v1 = v0u;
        prim->u2 = u0;
        SOFT_BARRIER();
        prim->v2 = vbase - 0x59;
        prim->u3 = (s32)head;
        SOFT_BARRIER();
        prim->v3      = vbase - 0x59;
        block->radius = (arg2 * 0x37) / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = (*(u16*)&block->sy - *(u16*)&block->radius) - (block->radius >> 1);
        ds            = &Display_State;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = *(u16*)&block->sy + (block->radius >> 1);
        prim->y3      = xy;
        prim->y2      = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}
