#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/mem.h"
#include "rooms/room_common.h"

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")

/// Scratch block `func_dryfield_saloon_g_r_8017DEC4` takes from
/// `G_SCRATCH_HEAD` for one light shaft. The four vectors are the shaft's
/// corners in world space: the two roots, then the tip reached from each.
typedef struct {
    s32     otz;
    SVECTOR rootA;
    SVECTOR rootB;
    SVECTOR tipA;
    SVECTOR tipB;
} _DryfieldSaloonGRShaftScratch;

/// The room's effect positions in the model's local space; see
/// `func_dryfield_saloon_g_r_8017DA70`.
extern SVECTOR D_dryfield_saloon_g_r_8017ECE4[];

/// Entries 14 and 17 of `D_dryfield_saloon_g_r_8017ECE4`, the two shaft roots,
/// which the code also reaches under labels of their own.
extern SVECTOR D_dryfield_saloon_g_r_8017ED54;
extern SVECTOR D_dryfield_saloon_g_r_8017ED6C;

/// Draws the room's two light shafts as Gouraud quads. Both shafts share the
/// roots at positions 14 and 17 of `D_dryfield_saloon_g_r_8017ECE4`; each
/// root's tip lies at four times its offset to a later entry (15 and 18 for
/// the first shaft, 16 and 19 for the second). All four corners are moved to
/// world space through `coord->workm` and projected through `GsWSMATRIX`. The
/// roots take a grey of 0x20 or 0x30 on the parity of
/// `gDisplayState.animFrame` and the tips are black, so the shaft fades
/// outward. The quad is sorted by `tipB`'s `otz` and skipped when that is
/// below 0x11.
void func_dryfield_saloon_g_r_8017DEC4(GsCOORDINATE2* coord)
{
    u8*                            head;
    _DryfieldSaloonGRShaftScratch* block;
    POLY_G4*                       prim;
    SVECTOR*                       dirA;
    SVECTOR*                       dirB;
    s32                            i;
    s32                            j;
    s32                            rgb;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x24;
        *scratch = tmp;
        block    = (_DryfieldSaloonGRShaftScratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_saloon_g_r_8017ED54);
    gte_rtv0_real();
    gte_stsv(&((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->rootA);
    *(u16*)&block->rootA.vx = *(u16*)&block->rootA.vx + *(u16*)&coord->workm.t[0];
    *(u16*)&block->rootA.vy = *(u16*)&block->rootA.vy + *(u16*)&coord->workm.t[1];
    *(u16*)&block->rootA.vz = *(u16*)&block->rootA.vz + *(u16*)&coord->workm.t[2];

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_saloon_g_r_8017ED6C);
    gte_rtv0_real();
    gte_stsv(&((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->rootB);
    *(u16*)&block->rootB.vx = *(u16*)&block->rootB.vx + *(u16*)&coord->workm.t[0];
    *(u16*)&block->rootB.vy = *(u16*)&block->rootB.vy + *(u16*)&coord->workm.t[1];
    *(u16*)&block->rootB.vz = *(u16*)&block->rootB.vz + *(u16*)&coord->workm.t[2];

    for (i = 0; i < 2; i++) {
        j                      = i + 15;
        dirA                   = &D_dryfield_saloon_g_r_8017ECE4[j];
        *(u16*)&block->tipA.vx = *(u16*)&D_dryfield_saloon_g_r_8017ECE4[14].vx +
                                 (*(u16*)&dirA->vx - *(u16*)&D_dryfield_saloon_g_r_8017ECE4[14].vx) * 4;
        *(u16*)&block->tipA.vy = *(u16*)&D_dryfield_saloon_g_r_8017ECE4[14].vy +
                                 (*(u16*)&dirA->vy - *(u16*)&D_dryfield_saloon_g_r_8017ECE4[14].vy) * 4;
        *(u16*)&block->tipA.vz = *(u16*)&D_dryfield_saloon_g_r_8017ECE4[14].vz +
                                 (*(u16*)&dirA->vz - *(u16*)&D_dryfield_saloon_g_r_8017ECE4[14].vz) * 4;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->tipA);
        gte_rtv0_real();
        gte_stsv(&((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->tipA);
        *(u16*)&block->tipA.vx = *(u16*)&block->tipA.vx + *(u16*)&coord->workm.t[0];
        *(u16*)&block->tipA.vy = *(u16*)&block->tipA.vy + *(u16*)&coord->workm.t[1];
        *(u16*)&block->tipA.vz = *(u16*)&block->tipA.vz + *(u16*)&coord->workm.t[2];

        j                      = i + 18;
        dirB                   = &D_dryfield_saloon_g_r_8017ECE4[j];
        *(u16*)&block->tipB.vx = *(u16*)&D_dryfield_saloon_g_r_8017ECE4[17].vx +
                                 (*(u16*)&dirB->vx - *(u16*)&D_dryfield_saloon_g_r_8017ECE4[17].vx) * 4;
        *(u16*)&block->tipB.vy = *(u16*)&D_dryfield_saloon_g_r_8017ECE4[17].vy +
                                 (*(u16*)&dirB->vy - *(u16*)&D_dryfield_saloon_g_r_8017ECE4[17].vy) * 4;
        *(u16*)&block->tipB.vz = *(u16*)&D_dryfield_saloon_g_r_8017ECE4[17].vz +
                                 (*(u16*)&dirB->vz - *(u16*)&D_dryfield_saloon_g_r_8017ECE4[17].vz) * 4;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->tipB);
        gte_rtv0_real();
        gte_stsv(&((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->tipB);
        *(u16*)&block->tipB.vx = *(u16*)&block->tipB.vx + *(u16*)&coord->workm.t[0];
        *(u16*)&block->tipB.vy = *(u16*)&block->tipB.vy + *(u16*)&coord->workm.t[1];
        *(u16*)&block->tipB.vz = *(u16*)&block->tipB.vz + *(u16*)&coord->workm.t[2];

        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->rootA);
        gte_rtps_real();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&block->rootB, &((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->tipA,
                 &((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->tipB);
        gte_rtpt_real();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&block->otz);
        if (block->otz >= 0x11) {
            rgb = ((u8)gDisplayState.animFrame & 1) * 16 + 0x20;
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            setRGB0(prim, rgb, rgb, rgb);
            setRGB1(prim, rgb, rgb, rgb);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x24;
}
