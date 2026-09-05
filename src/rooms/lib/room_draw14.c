#include "common.h"

#include "main/display.h"
#include "main/mem.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent `POLY_FT4`
/// (tpage 0x2A, clut 0x42CB). `arg1` selects one of four 24-texel UV columns
/// `(arg1 & 3) * 24 + 0x60` at v=0..0x17. `arg2` is a signed half-extent; the
/// on-screen radius is `(s16)arg2 * 23 / (otz + 1)`. `arg3` is the RGB on all
/// three channels. Shared body, linked into every room overlay that uses it.
void Room_Draw14(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw14Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    DisplayState*      ds;
    s32                tex;
    s32                u0;
    s32                u1;
    s32                sarg;
    s32                t;
    s16                xy;
    u16                vz;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (RoomDraw14Scratch*)(head - 0x18);
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    vz                                          = *(u16*)&arg0->workm.t[2];
    *scratch                                    = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42CB;
        t           = (tex & 3) * 24;
        u0          = t + 0x60;
        u1          = t + 0x77;
        SOFT_USE_REG(u1);
        prim->u0 = u0;
        prim->u2 = u0;
        prim->v2 = 0x17;
        prim->v3 = 0x17;
        sarg     = (s16)arg2;
        prim->u1 = u1;
        prim->u3 = u1;
        t        = sarg * 24;
        setRGB0(prim, arg3, arg3, arg3);
        prim->v0      = 0;
        prim->v1      = 0;
        block->radius = (t - sarg) / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = *(u16*)&block->sy - *(u16*)&block->radius;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = *(u16*)&block->sy + *(u16*)&block->radius;
        prim->y3      = xy;
        prim->y2      = xy;
        ds            = &Display_State;
        addPrim((u_long*)(((((u32)block->otz << ds->field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}
