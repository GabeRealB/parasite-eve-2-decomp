#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "main/display.h"
#include "main/mem.h"
#include "rooms/room_common.h"

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// Draws one frame of a textured semi-transparent puff at the coordinate's
/// world position, skipped when it fails to project or sits too close
/// (OTZ below 0x41). `arg1` is the animation frame, a 48-texel cell of a
/// five-wide grid; `arg2` is the half-extent, scaled by 47 over the OTZ on
/// screen; `arg3` is the angle the quad is rotated by.
void func_dryfield_night_main_street_8017F608(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
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
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
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
                                 << gDisplayState.otDepthShift) >>
                                2) &
                               0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
