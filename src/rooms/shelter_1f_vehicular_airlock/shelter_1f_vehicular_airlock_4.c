#include "common.h"

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, if
/// the resulting OTZ is at least 0x11, queues two gouraud `POLY_G4` diamonds
/// and two gouraud `LINE_G3` diagonals around the projected centre. `arg2` is a
/// signed half-extent; the on-screen radius is `(s16)arg2 * 32 / otz`. `arg1`
/// scales the display frame counter into `rsin`, giving a pulse of
/// `rsin(...) / 68 + 0x3C`; `arg3` packs the lit vertex's colour as per-channel
/// multipliers of that pulse, red in bits 8 and up, green in bits 4-5 and blue
/// in bits 0-1.
void func_shelter_1f_vehicular_airlock_8017E80C(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u8*               head;
    RoomShaftScratch* block;
    POLY_G4*          prim;
    LINE_G3*          line;
    s32               sine;
    u8                pulse;
    s32               r;
    s32               g;
    s32               b;
    s32               radius;
    s32               i;
    s32               t1;
    s32               t2;
    s32               twice;
    u16               sx;
    u16               sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x14);
        SOFT_TOUCH_REG(tmp);
        block = (RoomShaftScratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps_real();
    gte_stsxy(&((RoomShaftScratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomShaftScratch*)(head - 0x14))->otz >= 0x11) {
        sine             = rsin(gDisplayState.animFrame * (s16)arg1);
        radius           = ((s16)arg2 * 32) / ((RoomShaftScratch*)(head - 0x14))->otz;
        pulse            = sine / 68 + 0x3C;
        r                = pulse * ((s16)arg3 >> 8);
        g                = pulse * (((s16)arg3 >> 4) & 3);
        b                = pulse * (arg3 & 3);
        i                = 0;
        block->halfWidth = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->halfWidth;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->halfWidth;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->halfWidth) + (block->halfWidth * twice);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = (LINE_G3*)gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, r, g, b);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->halfWidth * t1);
            line->y0 = block->sy - (block->halfWidth * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->halfWidth * t1);
            line->y2 = block->sy + (block->halfWidth * t2);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}
