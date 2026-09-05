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
/// the GTE flag is non-negative and `otz` is at least 0x41, queues one
/// semi-transparent shade-tex `POLY_FT4` (tpage 0x2B, clut 0x4383) rotated
/// about the projected centre. `arg1` selects a 48-texel UV tile in a 5-wide
/// grid: u = `(arg1 % 5) * 48`, v = `(arg1 / 5) * 48 - 0x80`. `arg2` is a
/// signed half-extent; the on-screen radius is `(s16)arg2 * 47 / otz`.
/// `arg3` is the spin angle, applied at `arg3` and `arg3 + 0x400` through
/// `rsin`/`rcos`. Shared body, linked into every room overlay that uses it.
void Room_Draw39(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw39Scratch* block;
    s32*               otzp;
    POLY_FT4*          prim;
    s32                ang;
    s32                ang2;
    s32                sine;
    s32                span;
    s32                u0;
    s32                v0;
    s32                u1;
    s32                v1;
    u16                vz;
    u16                tex;

    scratch = (void**)G_SCRATCH_HEAD;
    SOFT_TOUCH_REG_USE(arg2, scratch);
    head          = *scratch;
    block         = (RoomDraw39Scratch*)(head - 0x1C);
    block->vec.vx = *(u16*)&arg0->workm.t[0];
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    otzp          = &block->otz;
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw39Scratch*)(head - 0x1C))->vec);
    gte_rtps_real();
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomDraw39Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw39Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(otzp);
        if (((RoomDraw39Scratch*)(head - 0x1C))->otz >= 0x41) {
            ang         = (s16)arg3;
            prim->tpage = 0x2B;
            prim->clut  = 0x4383;
            prim->code |= 3;
            tex         = arg1;
            u0          = (tex % 5) * 0x30;
            v0          = (tex / 5) * 0x30;
            u1          = u0 + 0x2F;
            v1          = v0 - 0x51;
            v0          = v0 - 0x80;
            setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
            sine = rsin(ang);
            span = (s16)arg2 * 0x2F;
            block->dx =
                ((span / ((RoomDraw39Scratch*)(head - 0x1C))->otz) * sine) >> 12;
            block->dy =
                ((span / ((RoomDraw39Scratch*)(head - 0x1C))->otz) * rcos(ang)) >> 12;
            prim->x0 = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x3 = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->y0 = *(u16*)&block->sy - *(u16*)&block->dy;
            prim->y3 = *(u16*)&block->sy + *(u16*)&block->dy;
            ang2     = ang + 0x400;
            block->dx =
                ((span / ((RoomDraw39Scratch*)(head - 0x1C))->otz) * rsin(ang2)) >> 12;
            block->dy =
                ((span / ((RoomDraw39Scratch*)(head - 0x1C))->otz) * rcos(ang2)) >> 12;
            prim->x1 = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x2 = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->y1 = *(u16*)&block->sy - *(u16*)&block->dy;
            prim->y2 = *(u16*)&block->sy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)((RoomDraw39Scratch*)(head - 0x1C))->otz
                                 << Display_State.field_128) >>
                                2) &
                               0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
