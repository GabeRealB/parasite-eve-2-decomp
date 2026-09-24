#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/mem.h"
#include "rooms/room_common.h"
#include "rooms/dryfield_motel_balcony.h"

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// Draws one mote: projects the coordinate's world position through
/// `GsWSMATRIX` and, unless the GTE flags the projection, queues one
/// semi-transparent textured square centred on it. `arg1`'s low two bits and
/// `arg2`'s top nibble pick the 24-texel texture cell, `arg2`'s low twelve
/// bits are the half-extent (scaled by 23 / (otz + 1)), `arg3`'s low byte is
/// the grey level and its top nibble picks the palette.
void func_dryfield_motel_balcony_8017DF84(GsCOORDINATE2* arg0, u16 arg1, u16 arg2, u16 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw14Scratch* block;
    RoomDraw14Scratch* next;
    POLY_FT4*          prim;
    DisplayState*      ds;
    u16                row;
    u16                pal;
    s32                u0;
    s32                u1;
    s16                xy;
    u16                vz;

    row                                         = arg2 >> 12;
    arg2                                       &= 0xFFF;
    scratch                                     = (void**)G_SCRATCH_HEAD;
    pal                                         = arg3 >> 12;
    arg3                                       &= 0xFF;
    head                                        = *scratch;
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    next                                        = (RoomDraw14Scratch*)(head - 0x18);
    __asm__("move %0,%1" : "=r"(block) : "r"(next));
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        setRGB0(prim, arg3, arg3, arg3);
        if (pal != 0) {
            prim->clut = getClut(pal * 16 + 0xF0, 0x10B);
        } else {
            prim->clut = getClut(0xB0, 0x10B);
        }
        u0 = row * 0x60 + (arg1 & 3) * 24;
        u1 = u0 + 0x17;
        setUV4(prim, u0, 0, u1, 0, u0, 0x17, u1, 0x17);
        block->radius = arg2 * 23 / block->otz;
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
        ds            = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(u8**)G_SCRATCH_HEAD += 0x18;
}
