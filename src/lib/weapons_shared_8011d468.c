#include "common.h"
#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "weapons/weapons_shared_8011d468.h"
#include "gameplay/3CD8.h"
#include "weapons/weapons_shared_8011d864.h"

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/* `otzp` is a second name for the same block on purpose: `gte_stszotz` takes
   its address in a register of its own, so the ROM keeps a `move` the single
   pointer would have coalesced away. The `gte_ldv0` / `gte_stsxy` addresses
   and every `otz` reload are spelled out from `head` for the same reason -
   off `blk` they would reuse the block register instead. */

/// `rtps` / `rtpt` / `mvmva 1, 0, 0, 3, 0`. The `inline_c.h` macros of those
/// names assemble to different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/* Every scratch vector address is computed off `head`, not off `blk`, so the
   loads and stores keep spelling the block out from `head` rather than reusing
   the `blk` register the way CSE off `blk` would. */

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
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
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
        addPrim((u_long*)(((((u32)((WeaponFlashCoreScratch*)(head - 0x18))->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + sizeof(WeaponFlashCoreScratch);
}

void WeaponsShared8011d864(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**             scratch;
    u8*                head;
    WeaponQuadScratch* blk;
    POLY_G4*           prim;
    MATRIX*            wm;
    s32                ang;
    s32                back;
    s32                len;
    s32                depth;

    /* `len` and `depth` are locals rather than literals on purpose: as
       constants GCC turns the `* 0x600` into a shift-and-add and drops the
       `mult` the ROM keeps. */
    depth    = -0x200;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (WeaponQuadScratch*)(head - sizeof(WeaponQuadScratch));
    *scratch = blk;
    gte_SetTransMatrix(&GsWSMATRIX);
    ang = arg1;

    /* Corner 0: on the small circle, 0xC0 behind the flash direction. */
    back         = ang - 0xC0;
    blk->v[0].vx = (u32)rsin(back) >> 4;
    blk->v[0].vy = (u32)rcos(back) >> 4;
    blk->v[0].vz = 0;
    wm           = &arg0->workm;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((WeaponQuadScratch*)(head - 0x24))->v[0]);
    gte_rtv0_real();
    gte_stsv(&((WeaponQuadScratch*)(head - 0x24))->v[0]);
    *(u16*)&blk->v[0].vx = *(u16*)&blk->v[0].vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&blk->v[0].vy = *(u16*)&blk->v[0].vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&blk->v[0].vz = *(u16*)&blk->v[0].vz + *(u16*)&arg0->workm.t[2];

    /* Corner 1: the far tip, a full 0x600 out and 0x200 towards the camera. */
    len          = 0x600;
    blk->v[1].vx = (rsin(ang) * len) >> 12;
    blk->v[1].vy = (rcos(ang) * len) >> 12;
    blk->v[1].vz = depth;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((WeaponQuadScratch*)(head - 0x24))->v[1]);
    gte_rtv0_real();
    gte_stsv(&((WeaponQuadScratch*)(head - 0x24))->v[1]);
    *(u16*)&blk->v[1].vx = *(u16*)&blk->v[1].vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&blk->v[1].vy = *(u16*)&blk->v[1].vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&blk->v[1].vz = *(u16*)&blk->v[1].vz + *(u16*)&arg0->workm.t[2];

    /* Corner 2: on the small circle, straight along the flash direction. This
       is the only lit corner. */
    blk->v[2].vx = (u32)rsin(ang) >> 4;
    blk->v[2].vy = (u32)rcos(ang) >> 4;
    blk->v[2].vz = 0;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((WeaponQuadScratch*)(head - 0x24))->v[2]);
    gte_rtv0_real();
    gte_stsv(&((WeaponQuadScratch*)(head - 0x24))->v[2]);
    *(u16*)&blk->v[2].vx = *(u16*)&blk->v[2].vx + *(u16*)&arg0->workm.t[0];
    ang                  = ang + 0xC0;
    *(u16*)&blk->v[2].vy = *(u16*)&blk->v[2].vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&blk->v[2].vz = *(u16*)&blk->v[2].vz + *(u16*)&arg0->workm.t[2];

    /* Corner 3: on the small circle, 0xC0 ahead of the flash direction. */
    blk->v[3].vx = (u32)rsin(ang) >> 4;
    blk->v[3].vy = (u32)rcos(ang) >> 4;
    blk->v[3].vz = 0;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((WeaponQuadScratch*)(head - 0x24))->v[3]);
    gte_rtv0_real();
    gte_stsv(&((WeaponQuadScratch*)(head - 0x24))->v[3]);
    *(u16*)&blk->v[3].vx = *(u16*)&blk->v[3].vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&blk->v[3].vy = *(u16*)&blk->v[3].vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&blk->v[3].vz = *(u16*)&blk->v[3].vz + *(u16*)&arg0->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((WeaponQuadScratch*)(head - 0x24))->v[0]);
    gte_rtps_real();
    prim           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&((WeaponQuadScratch*)(head - 0x24))->v[1], &((WeaponQuadScratch*)(head - 0x24))->v[2],
             &((WeaponQuadScratch*)(head - 0x24))->v[3]);
    gte_rtpt_real();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    if (((WeaponQuadScratch*)(head - 0x24))->otz >= 0x11) {
        setRGB0(prim, 0, 0, 0);
        setRGB1(prim, 0, 0, 0);
        setRGB2(prim, arg2 >> 1, arg2 >> 1, arg2);
        setRGB3(prim, 0, 0, 0);
        addPrim((u_long*)(((((u32)((WeaponQuadScratch*)(head - 0x24))->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, ((WeaponQuadScratch*)(head - 0x24))->otz);
    }
    *scratch = (u8*)*scratch + sizeof(WeaponQuadScratch);
}
