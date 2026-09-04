#include "common.h"

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s32 D_80070F70;

/// Draws the two light shafts falling through an elevator hall's ceiling grate.
/// The task's `GsCOORDINATE2` origin is projected once through `GsWSMATRIX`
/// (`RTPS`) into a 0x14-byte `G_SCRATCH_HEAD` block; anything closer than
/// `otz` 0x11 is dropped. `spawnArg1`'s low byte pulses the shafts' red
/// channel off the frame counter `D_80070F70`, folding the counter's 0..0xFF
/// ramp into a 0..0x80 triangle so the shafts brighten and dim; its high byte
/// is the shaft length, divided by `otz` so the two `POLY_G4` halves narrow
/// with distance.
void RoomsShared8017f77c(Task* arg0)
{
    u8*               head;
    u8*               raw;
    RoomShaftScratch* block;
    POLY_G4*          prim;
    GsCOORDINATE2*    coord;
    void*             mem;
    u16               vz;
    s32               i;
    s32               red;
    s32               pulse;
    s32               level;

    coord = ((TmdObject*)arg0->extra)->field_8;
    mem   = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    head = *(void**)G_SCRATCH_HEAD;
    raw  = head - 0x14;
    /* `raw` and `block` have to stay separate registers: the ROM computes the
       block address into a scratch register and copies it into the callee-saved
       one the rest of the function uses. */
    SOFT_TOUCH_REG(raw);
    block                   = (RoomShaftScratch*)raw;
    block->vec.vx           = *(u16*)&coord->workm.t[0];
    block->vec.vy           = *(u16*)&coord->workm.t[1];
    vz                      = *(u16*)&coord->workm.t[2];
    *(void**)G_SCRATCH_HEAD = block;
    block->vec.vz           = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomShaftScratch*)(head - 0x14))->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomShaftScratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomShaftScratch*)(head - 0x14))->otz >= 0x11) {
        pulse = D_80070F70 * ((RoomShaftArg*)&arg0->spawnArg1)->phase;
        if (pulse & 0x80) {
            level = 0x80 - (pulse & 0x7F);
        } else {
            level = pulse & 0x7F;
        }
        /* Same split for the ramp: the ROM keeps the triangle result in a
           scratch register and copies it into the callee-saved `red`. */
        red              = level;
        block->halfWidth = (((RoomShaftArg*)&arg0->spawnArg1)->height << 9) / block->otz;
        for (i = 0; i < 2; i++) {
            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, red, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - block->halfWidth;
            prim->x1 = prim->x2 = block->sx;
            prim->x3            = block->sx + block->halfWidth;
            prim->y0 = prim->y2 = prim->y3 = block->sy;
            prim->y1                       = (block->sy - block->halfWidth) + block->halfWidth * (i + i);
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    Gp_ReleaseState1CMem(mem, arg0);
}
