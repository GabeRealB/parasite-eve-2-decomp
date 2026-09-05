#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "weapons/weapons_shared_8011d468.h"

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/* `otzp` is a second name for the same block on purpose: `gte_stszotz` takes
   its address in a register of its own, so the ROM keeps a `move` the single
   pointer would have coalesced away. The `gte_ldv0` / `gte_stsxy` addresses
   and every `otz` reload are spelled out from `head` for the same reason -
   off `blk` they would reuse the block register instead. */
void WeaponsShared8011d468(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**                  scratch;
    u8*                     head;
    WeaponFlashCoreScratch* blk;
    WeaponFlashCoreScratch* otzp;
    POLY_FT4*               prim;
    s32                     ang;
    u16                     vz;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    blk         = (WeaponFlashCoreScratch*)(head - sizeof(WeaponFlashCoreScratch));
    blk->vec.vx = *(u16*)&arg0->workm.t[0];
    blk->vec.vy = *(u16*)&arg0->workm.t[1];
    vz          = *(u16*)&arg0->workm.t[2];
    otzp        = blk;
    *scratch    = blk;
    blk->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((WeaponFlashCoreScratch*)(head - 0x18))->vec);
    gte_rtps_real();
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((WeaponFlashCoreScratch*)(head - 0x18))->sxy);
    gte_stszotz(&otzp->otz);
    if (((WeaponFlashCoreScratch*)(head - 0x18))->otz >= 0x11) {
        ang         = arg2;
        prim->tpage = 0x29;
        prim->clut  = 0x428B;
        setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        setcode(prim, getcode(prim) | 3);
        blk->dx  = (((arg1 * 55) / ((WeaponFlashCoreScratch*)(head - 0x18))->otz) * rsin(ang)) >> 12;
        blk->dy  = (((arg1 * 55) / ((WeaponFlashCoreScratch*)(head - 0x18))->otz) * rcos(ang)) >> 12;
        prim->x0 = *(u16*)&blk->sxy.vx + *(u16*)&blk->dx;
        prim->x3 = *(u16*)&blk->sxy.vx - *(u16*)&blk->dx;
        prim->y0 = *(u16*)&blk->sxy.vy - *(u16*)&blk->dy;
        ang      = ang + 0x400;
        prim->y3 = *(u16*)&blk->sxy.vy + *(u16*)&blk->dy;
        blk->dx  = (((arg1 * 55) / ((WeaponFlashCoreScratch*)(head - 0x18))->otz) * rsin(ang)) >> 12;
        blk->dy  = (((arg1 * 55) / ((WeaponFlashCoreScratch*)(head - 0x18))->otz) * rcos(ang)) >> 12;
        prim->x1 = *(u16*)&blk->sxy.vx + *(u16*)&blk->dx;
        prim->x2 = *(u16*)&blk->sxy.vx - *(u16*)&blk->dx;
        prim->y1 = *(u16*)&blk->sxy.vy - *(u16*)&blk->dy;
        prim->y2 = *(u16*)&blk->sxy.vy + *(u16*)&blk->dy;
        addPrim((u_long*)(((((u32)((WeaponFlashCoreScratch*)(head - 0x18))->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + sizeof(WeaponFlashCoreScratch);
}
