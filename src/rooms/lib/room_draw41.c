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
/// the GTE flag is non-negative, queues one semi-transparent shade-tex
/// `POLY_FT4` (tpage 0x2B, clut 0x4393). `arg1` selects a 56-texel UV tile in
/// a 4-wide 2-row grid: u = `(arg1 & 3) * 56`, v = `((arg1 & 7) >> 2) * 56`.
/// `arg2` is a signed half-extent; the on-screen radius is
/// `(s16)arg2 * 55 / otz`. The quad is axis-aligned and 2*radius on a side,
/// shifted up so the projected point sits at three-quarters height
/// (`y0 = sy - r - r/2`, `y2 = sy + r/2`). Shared body, linked into every
/// room overlay that uses it.
void Room_Draw41(GsCOORDINATE2* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw41Scratch* block;
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
    u16                vz;

    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw41Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (RoomDraw41Scratch*)(head - 0x18);
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    vz                                          = *(u16*)&arg0->workm.t[2];
    tex                                         = arg1;
    SOFT_TOUCH_REG(tex);
    *scratch      = block;
    block->vec.vz = vz;
    vec           = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw41Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw41Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw41Scratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        cell        = (u16)tex;
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
        sarg          = (s16)arg2;
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
        ds                  = &Display_State;
        prim->y0 = prim->y1 = xy;
        xy                  = *(u16*)&block->sy + (block->radius >> 1);
        prim->y2 = prim->y3 = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}
