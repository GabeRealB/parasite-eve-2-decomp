#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/mem.h"

/// `rtpt` / `mvmva 1,0,0,3,0` / `gpf 12`. The `inline_c.h` macros of those
/// names assemble to different words, so spell the instructions out.
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern s32 Gp_LcgState;

/// Scratch block one triangle is built in: the GTE depth and flag of its
/// projection, then its three corners in world space.
typedef struct _DryfieldNightMotelLoftTriScratch {
    s32     otz;
    s32     flag;
    SVECTOR v[3];
} _DryfieldNightMotelLoftTriScratch;

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_loft/dryfield_night_motel_loft_5", func_dryfield_night_motel_loft_8017E090);

/// Draws one flat grey `POLY_F3` of shade `shade`: an equilateral triangle of
/// radius `scale` in `coord`'s local YZ plane, its corners 0x555 apart,
/// rotated by `coord`'s `workm` and moved by its translation before projection
/// through `GsWSMATRIX`. A triangle the GTE flags as failed is dropped. The
/// triangle is made semi-transparent with a blend mode drawn from the LCG.
void func_dryfield_night_motel_loft_8017E540(GsCOORDINATE2* coord, s16 scale, u8 shade)
{
    _DryfieldNightMotelLoftTriScratch* blk;
    SVECTOR*                           p;
    SVECTOR*                           q;
    POLY_F3*                           prim;
    s32                                i;
    s32                                off;
    s32                                ang;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(_DryfieldNightMotelLoftTriScratch);
    blk                     = (_DryfieldNightMotelLoftTriScratch*)*(void**)G_SCRATCH_HEAD;
    gte_SetTransMatrix(&GsWSMATRIX);
    // `off` is the corner's byte offset in the block, initialised with the
    // other locals; indexing `v[]` instead lets loop strength reduction
    // materialise the offset later, at the end of the preheader.
    for (i = 0, off = 8, ang = 0, p = (SVECTOR*)blk; i < 3; i++) {
        p[1].vx = 0;
        p[1].vy = rsin(ang);
        p[1].vz = rcos(ang);
        gte_lddp(scale);
        gte_ldsv((SVECTOR*)((u8*)blk + off));
        gte_gpf12_real();
        gte_stsv((SVECTOR*)((u8*)blk + off));
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0((SVECTOR*)((u8*)blk + off));
        gte_mvmva_real();
        gte_stsv((SVECTOR*)((u8*)blk + off));
        off    += 8;
        p[1].vx = *(u16*)&p[1].vx + *(u16*)&coord->workm.t[0];
        q       = p + 1;
        q->vy   = *(u16*)&q->vy + *(u16*)&coord->workm.t[1];
        q->vz   = *(u16*)&q->vz + *(u16*)&coord->workm.t[2];
        p       = q;
        ang    += 0x555;
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv3(&blk->v[0], &blk->v[1], &blk->v[2]);
    gte_rtpt_real();
    prim           = (POLY_F3*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyF3(prim);
    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
    gte_stflg(&blk->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&blk->otz);
        setRGB0(prim, shade, shade, shade);
        addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_AddTpageShift((P_TAG*)prim, ((u32)Gp_LcgState >> 16) & 1, blk->otz);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(_DryfieldNightMotelLoftTriScratch);
}
