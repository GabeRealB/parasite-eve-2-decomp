#include "common.h"

#include "main/display.h"
#include "main/mem.h"
#include "rooms/dryfield_night_saloon_g_r.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")

/// Scratch block `func_dryfield_night_saloon_g_r_8017EB38` takes from
/// `G_SCRATCH_HEAD` for one light shaft. The four vectors are the shaft's
/// corners in world space: the two roots, then the tip reached from each.
typedef struct {
    s32     otz;
    SVECTOR rootA;
    SVECTOR rootB;
    SVECTOR tipA;
    SVECTOR tipB;
} _DryfieldNightSaloonGRShaftScratch;

/// Entries 14 and 17 of `D_dryfield_night_saloon_g_r_80185074`, the two
/// shaft roots, which the code also reaches under labels of their own.
extern SVECTOR D_dryfield_night_saloon_g_r_801850E4;
extern SVECTOR D_dryfield_night_saloon_g_r_801850FC;

/// Draws the room's two light shafts as Gouraud quads. Both shafts share the
/// roots at positions 14 and 17 of `D_dryfield_night_saloon_g_r_80185074`;
/// each root's tip lies at four times its offset to a later entry (15 and 18
/// for the first shaft, 16 and 19 for the second). All four corners are
/// moved to world space through `coord->workm` and projected through
/// `GsWSMATRIX`. The roots take a grey of 0x20 or 0x30 depending on the
/// parity of `gDisplayState.animFrame` and the tips are black, so the shaft
/// fades outward. The quad is sorted by `tipB`'s `otz` and skipped when that
/// is below 0x11.
void func_dryfield_night_saloon_g_r_8017EB38(GsCOORDINATE2* coord)
{
    u8*                                 head;
    _DryfieldNightSaloonGRShaftScratch* block;
    POLY_G4*                            prim;
    SVECTOR*                            dirA;
    SVECTOR*                            dirB;
    s32                                 i;
    s32                                 j;
    s32                                 rgb;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x24;
        *scratch = tmp;
        block    = (_DryfieldNightSaloonGRShaftScratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_saloon_g_r_801850E4);
    gte_rtv0_real();
    gte_stsv(&((_DryfieldNightSaloonGRShaftScratch*)(head - 0x24))->rootA);
    *(u16*)&block->rootA.vx = *(u16*)&block->rootA.vx + *(u16*)&coord->workm.t[0];
    *(u16*)&block->rootA.vy = *(u16*)&block->rootA.vy + *(u16*)&coord->workm.t[1];
    *(u16*)&block->rootA.vz = *(u16*)&block->rootA.vz + *(u16*)&coord->workm.t[2];

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_saloon_g_r_801850FC);
    gte_rtv0_real();
    gte_stsv(&((_DryfieldNightSaloonGRShaftScratch*)(head - 0x24))->rootB);
    *(u16*)&block->rootB.vx = *(u16*)&block->rootB.vx + *(u16*)&coord->workm.t[0];
    *(u16*)&block->rootB.vy = *(u16*)&block->rootB.vy + *(u16*)&coord->workm.t[1];
    *(u16*)&block->rootB.vz = *(u16*)&block->rootB.vz + *(u16*)&coord->workm.t[2];

    for (i = 0; i < 2; i++) {
        j                      = i + 15;
        dirA                   = &D_dryfield_night_saloon_g_r_80185074[j];
        *(u16*)&block->tipA.vx = *(u16*)&D_dryfield_night_saloon_g_r_80185074[14].vx +
                                 (*(u16*)&dirA->vx - *(u16*)&D_dryfield_night_saloon_g_r_80185074[14].vx) * 4;
        *(u16*)&block->tipA.vy = *(u16*)&D_dryfield_night_saloon_g_r_80185074[14].vy +
                                 (*(u16*)&dirA->vy - *(u16*)&D_dryfield_night_saloon_g_r_80185074[14].vy) * 4;
        *(u16*)&block->tipA.vz = *(u16*)&D_dryfield_night_saloon_g_r_80185074[14].vz +
                                 (*(u16*)&dirA->vz - *(u16*)&D_dryfield_night_saloon_g_r_80185074[14].vz) * 4;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((_DryfieldNightSaloonGRShaftScratch*)(head - 0x24))->tipA);
        gte_rtv0_real();
        gte_stsv(&((_DryfieldNightSaloonGRShaftScratch*)(head - 0x24))->tipA);
        *(u16*)&block->tipA.vx = *(u16*)&block->tipA.vx + *(u16*)&coord->workm.t[0];
        *(u16*)&block->tipA.vy = *(u16*)&block->tipA.vy + *(u16*)&coord->workm.t[1];
        *(u16*)&block->tipA.vz = *(u16*)&block->tipA.vz + *(u16*)&coord->workm.t[2];

        j                      = i + 18;
        dirB                   = &D_dryfield_night_saloon_g_r_80185074[j];
        *(u16*)&block->tipB.vx = *(u16*)&D_dryfield_night_saloon_g_r_80185074[17].vx +
                                 (*(u16*)&dirB->vx - *(u16*)&D_dryfield_night_saloon_g_r_80185074[17].vx) * 4;
        *(u16*)&block->tipB.vy = *(u16*)&D_dryfield_night_saloon_g_r_80185074[17].vy +
                                 (*(u16*)&dirB->vy - *(u16*)&D_dryfield_night_saloon_g_r_80185074[17].vy) * 4;
        *(u16*)&block->tipB.vz = *(u16*)&D_dryfield_night_saloon_g_r_80185074[17].vz +
                                 (*(u16*)&dirB->vz - *(u16*)&D_dryfield_night_saloon_g_r_80185074[17].vz) * 4;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((_DryfieldNightSaloonGRShaftScratch*)(head - 0x24))->tipB);
        gte_rtv0_real();
        gte_stsv(&((_DryfieldNightSaloonGRShaftScratch*)(head - 0x24))->tipB);
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
        gte_ldv3(&block->rootB, &((_DryfieldNightSaloonGRShaftScratch*)(head - 0x24))->tipA,
                 &((_DryfieldNightSaloonGRShaftScratch*)(head - 0x24))->tipB);
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

/// Draws a tapered beam between two points of `coord`'s local space. `arg1`
/// and `arg2` are rotated by `coord->workm` and offset by its translation,
/// then projected through `GsWSMATRIX`; nothing is drawn unless the far end's
/// `otz` is at least 0x11. Unlike `Room_Draw24`, the near end's `otz` is not
/// clamped. The two ends get screen radii `(s16)arg3 * 64 / otz`.
///
/// Each quarter-turn step of an angle running 0..0x800 queues three
/// `POLY_G4`s: a wedge around the near end, a quad joining the two ends, and a
/// wedge around the far end walked backwards from 0x1000. The centre vertices
/// take a grey of 0x20 or 0x30 depending on the parity of
/// `gDisplayState.animFrame`, the rim vertices are black. Each primitive goes
/// into the OT bucket of its own end's `otz` with a `Gp_AddTpageShift` tpage.
void func_dryfield_night_saloon_g_r_8017F0A4(GsCOORDINATE2* coord, SVECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    u8*                head;
    RoomDraw24Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                rgb;
    s32                extent;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x28;
        *scratch = tmp;
        block    = (RoomDraw24Scratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(arg1);
    gte_rtv0_real();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    *(u16*)&block->vec0.vx = *(u16*)&block->vec0.vx + *(u16*)&coord->workm.t[0];
    *(u16*)&block->vec0.vy = *(u16*)&block->vec0.vy + *(u16*)&coord->workm.t[1];
    *(u16*)&block->vec0.vz = *(u16*)&block->vec0.vz + *(u16*)&coord->workm.t[2];

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(arg2);
    gte_rtv0_real();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    *(u16*)&block->vec1.vx = *(u16*)&block->vec1.vx + *(u16*)&coord->workm.t[0];
    *(u16*)&block->vec1.vy = *(u16*)&block->vec1.vy + *(u16*)&coord->workm.t[1];
    *(u16*)&block->vec1.vz = *(u16*)&block->vec1.vz + *(u16*)&coord->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx1);
    gte_stszotz(&((RoomDraw24Scratch*)(head - 0x28))->otz1);
    if (block->otz1 >= 0x11) {
        extent    = (s16)arg3 * 64;
        ang       = 0;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 16) | 0x20;
        block->r0 = extent / ((RoomDraw24Scratch*)(head - 0x28))->otz0;
        block->r1 = extent / block->otz1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
            prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang * 2)) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang * 2)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(ang * 2)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(ang * 2)) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(0x1000 - ang)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(0x1000 - ang)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(0xE00 - ang)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(0xE00 - ang)) >> 12);
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            prim->x3 = block->sx1 + ((block->r1 * rsin(0xC00 - ang)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(0xC00 - ang)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x28;
}
