#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgte.h>

/// `rtps`, with the two leading nops the original emits.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
/// `rtv0`. The `inline_c.h` macro of that name assembles to a different word.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
/// `rtpt`, with the two leading nops the original emits.
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")

/// 0x2C-byte scratch block `func_shelter_b2_pod_bottom_8018101C` takes from
/// `G_SCRATCH_HEAD`: the coordinate's world position, the tip point offset from
/// it, and both points' projections. `otz0`/`sx0`/`sy0` belong to `base`,
/// `otz1`/`sx1`/`sy1` to `tip`; `r0`/`r1` are the wedge radii at each end.
typedef struct {
    SVECTOR base;
    SVECTOR tip;
    s32     otz0;
    s32     otz1;
    s32     flag;
    s32     r0;
    s32     r1;
    u16     sx0;
    u16     sy0;
    u16     sx1;
    u16     sy1;
} _ShelterB2PodBottomBeamScratch;

/// 0x120-byte scratch block `func_shelter_b2_pod_bottom_80180A4C` takes from
/// `G_SCRATCH_HEAD`: the 32 rotated ring points and the disc centre, the
/// quad's depth and GTE flag, and the four projected corners of one quad.
typedef struct {
    SVECTOR pts[32];
    SVECTOR center;
    s32     otz;
    s32     flag;
    DVECTOR sxy0;
    DVECTOR sxy1;
    DVECTOR sxy2;
    DVECTOR sxy3;
} _ShelterB2PodBottomRingScratch;

extern u32 Gp_LcgState;

void func_shelter_b2_pod_bottom_8017E788(GsCOORDINATE2* coord, s16 arg1, s16 arg2);
void func_shelter_b2_pod_bottom_8018101C(GsCOORDINATE2* coord, s16 size, u16 color, u16 scale);

/// Draws a flat white disc of radius `radius` in `coord`'s local XY plane,
/// centred on `center` (the frame's origin when it is NULL). 32 rim points are
/// rotated by `coord->workm`, the centre is projected through the full
/// `workm`, and the disc is queued as 16 `POLY_F4` fans, each joining the
/// centre to three consecutive rim points; any piece with a negative GTE flag
/// is dropped.
void func_shelter_b2_pod_bottom_80180A4C(GsCOORDINATE2* coord, s16 radius, SVECTOR* center)
{
    void**                          scratch;
    _ShelterB2PodBottomRingScratch* block;
    POLY_F4*                        prim;
    s32                             i;
    s32                             ang;
    u8*                             head;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x120;
    *scratch = head;
    block    = (_ShelterB2PodBottomRingScratch*)head;
    gte_SetTransMatrix(&coord->workm);
    if (center != NULL) {
        for (i = 0; i < 32; i++) {
            block->pts[i].vx = *(u16*)&center->vx + ((rsin(i << 7) * radius) >> 12);
            block->pts[i].vy = *(u16*)&center->vy + ((rcos(i << 7) * radius) >> 12);
            block->pts[i].vz = center->vz;
            gte_SetRotMatrix(&coord->workm);
            gte_ldv0(&block->pts[i]);
            gte_rtv0_real();
            gte_stsv(&block->pts[i]);
        }
        block->center.vx = center->vx;
        block->center.vy = center->vy;
        block->center.vz = center->vz;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->center);
        gte_rtv0_real();
        gte_stsv(&block->center);
    } else {
        for (i = 0; i < 32; i++) {
            ang              = i << 7;
            block->pts[i].vx = (rsin(ang) * radius) >> 12;
            block->pts[i].vy = (rcos(ang) * radius) >> 12;
            block->pts[i].vz = 0;
            gte_SetRotMatrix(&coord->workm);
            gte_ldv0(&block->pts[i]);
            gte_rtv0_real();
            gte_stsv(&block->pts[i]);
        }
        block->center.vx = 0;
        block->center.vy = 0;
        block->center.vz = 0;
    }
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&block->center);
    gte_rtps_real();
    gte_stsxy(&block->sxy0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        for (i = 0; i < 32; i += 2) {
            gte_ldv3(&block->pts[i], &block->pts[(i + 2) & 0x1F], &block->pts[i + 1]);
            gte_rtpt_real();
            gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
            gte_stflg(&block->flag);
            if (block->flag >= 0) {
                gte_stszotz(&block->otz);
                prim           = (POLY_F4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyF4(prim);
                setRGB0(prim, 0xFF, 0xFF, 0xFF);
                prim->x0 = *(u16*)&block->sxy0.vx;
                prim->y0 = *(u16*)&block->sxy0.vy;
                prim->x1 = *(u16*)&block->sxy1.vx;
                prim->y1 = *(u16*)&block->sxy1.vy;
                prim->x2 = *(u16*)&block->sxy2.vx;
                prim->y2 = *(u16*)&block->sxy2.vy;
                prim->x3 = *(u16*)&block->sxy3.vx;
                prim->y3 = *(u16*)&block->sxy3.vy;
                addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x120;
}

void func_shelter_b2_pod_bottom_80180F10(Task* arg0)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    u32            rnd;

    work  = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            rnd         = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rnd;
            func_shelter_b2_pod_bottom_8018101C(coord, 0x100, (rnd >> 16) & 0x777, 0x10);
            return;
        }
        Gp_ReleaseState1CMem(work, arg0);
        return;
    }
    work->field_22++;
    coord->coord.t[1] += arg0->spawnArg1;
    coord->flg         = 0;
    if ((s16)work->field_22 < 8) {
        func_shelter_b2_pod_bottom_8018101C(coord, 0x100, 0xCCC, 0x10);
        return;
    }
    func_shelter_b2_pod_bottom_8018101C(coord, 0x100, 0xCCC, (u16)((0x10 - (s16)work->field_22) * 2));
    if ((s16)work->field_22 >= 0x10) {
        Gp_ReleaseState1CMem(work, arg0);
    }
}

/// Queues a beam of gouraud `POLY_G4` wedges along `coord`'s local up axis:
/// the tip sits `size * 16` units above the coordinate's world position, and
/// both ends are projected. Two passes widen the wedge radii (`size * 64` and
/// `size * 128` over each end's depth) and draw a fan at each end plus a
/// connecting quad. `color` packs `[r][g][b]` nibbles scaled by `scale`, with
/// `gDisplayState.animFrame & 1` adding a 16-unit flicker to every channel.
void func_shelter_b2_pod_bottom_8018101C(GsCOORDINATE2* coord, s16 size, u16 color, u16 scale)
{
    void**                          scratch;
    u8*                             head;
    _ShelterB2PodBottomBeamScratch* block;
    POLY_G4*                        prim;
    s32                             pass;
    u8                              r;
    u8                              g;
    u8                              b;
    s32                             limit;
    s32                             angStart;
    s32                             scaled;
    s32                             ang;
    s32                             next;
    s32                             mid;
    s32                             blend;

    scratch       = (void**)G_SCRATCH_HEAD;
    head          = *scratch;
    block         = (_ShelterB2PodBottomBeamScratch*)(*scratch = head - 0x2C);
    block->tip.vy = -(size << 4);
    block->tip.vx = 0;
    block->tip.vz = 0;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&block->tip);
    gte_rtv0_real();
    gte_stsv(&block->tip);
    block->base.vx = coord->workm.t[0];
    block->base.vy = coord->workm.t[1];
    block->base.vz = coord->workm.t[2];
    block->tip.vx += block->base.vx;
    block->tip.vy += block->base.vy;
    block->tip.vz += block->base.vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->base);
    gte_rtps_real();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(&block->tip);
        gte_rtps_real();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        gte_stszotz(&block->otz1);
        if (block->flag >= 0) {
            blend = (*(u8*)&gDisplayState.animFrame & 1) << 4;
            r     = scale * ((color >> 8) & 0xF) + blend;
            g     = scale * ((color >> 4) & 0xF) + blend;
            b     = scale * (color & 0xF) + blend;
            for (pass = 1; pass < 3; pass++) {
                scaled    = size * (pass << 6);
                block->r0 = scaled / block->otz0;
                block->r1 = scaled / block->otz1;
                ang       = (s16)ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
                if (ang < ang + 0x800) {
                    angStart = ang;
                    limit    = ang + 0x800;
                    do {
                        prim           = (POLY_G4*)gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = block->sx1 + ((block->r1 * rsin(ang + 0x800)) >> 12);
                        prim->y0 = block->sy1 + ((block->r1 * rcos(ang + 0x800)) >> 12);
                        prim->x1 = block->sx1 + ((block->r1 * rsin(ang + 0xA00)) >> 12);
                        prim->y1 = block->sy1 + ((block->r1 * rcos(ang + 0xA00)) >> 12);
                        prim->x2 = block->sx1;
                        prim->y2 = block->sy1;
                        prim->x3 = block->sx1 + ((block->r1 * rsin(ang + 0xC00)) >> 12);
                        prim->y3 = block->sy1 + ((block->r1 * rcos(ang + 0xC00)) >> 12);
                        addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);

                        prim           = (POLY_G4*)gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                        prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                        prim->x1 = block->sx0 + ((block->r0 * rsin(ang + 0x200)) >> 12);
                        prim->y1 = block->sy0 + ((block->r0 * rcos(ang + 0x200)) >> 12);
                        next     = ang + 0x400;
                        prim->x2 = block->sx0;
                        prim->y2 = block->sy0;
                        prim->x3 = block->sx0 + ((block->r0 * rsin(next)) >> 12);
                        prim->y3 = block->sy0 + ((block->r0 * rcos(next)) >> 12);
                        addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                        prim           = (POLY_G4*)gGpuPrimCursor;
                        mid            = angStart + (ang - angStart) * 2;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, r, g, b);
                        prim->x0 = block->sx0 + ((block->r0 * rsin(mid)) >> 12);
                        prim->y0 = block->sy0 + ((block->r0 * rcos(mid)) >> 12);
                        prim->x1 = block->sx1 + ((block->r1 * rsin(mid)) >> 12);
                        prim->y1 = block->sy1 + ((block->r1 * rcos(mid)) >> 12);
                        prim->x2 = block->sx0;
                        prim->y2 = block->sy0;
                        prim->x3 = block->sx1;
                        prim->y3 = block->sy1;
                        addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                        ang = next;
                    } while (ang < limit);
                }
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x2C;
}

void func_shelter_b2_pod_bottom_80181940(Task* arg0)
{
    GsCOORDINATE2* coord;
    u32            rnd;

    if (Gp_State1C->eventState == 0) {
        rnd         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rnd;
        coord       = &((TmdObject*)arg0->extra)->coords[(u16)((rnd >> 16) % 18) + 2];
        Gp_SpawnEff(0x600E0, coord, 0x10300, 0);
        rnd         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rnd;
        if ((rnd >> 16) & 1) {
            Gp_SpawnEff(0x600E1, coord, 0x10300, 0);
        }
    }
}

void func_shelter_b2_pod_bottom_80181A48(Task* arg0)
{
    GsCOORDINATE2* coord;
    u32            rnd;

    if (Gp_State1C->eventState == 0) {
        rnd         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rnd;
        coord       = &((TmdObject*)arg0->extra)->coords[(u16)((rnd >> 16) % 18) + 2];
        Gp_SpawnEff(0x600F4, coord, 0x8600, 0);
        rnd         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rnd;
        if (!((rnd >> 16) & 1)) {
            Gp_SpawnEff(0x600F4, coord, 0x8600, 0);
        }
    }
}

void func_shelter_b2_pod_bottom_80181B48(Task* arg0)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    s16            y;

    work  = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState >= 4) {
        Gp_ReleaseState1CMem(work, arg0);
        return;
    }
    work->field_22++;
    switch (arg0->state) {
        case 0:
            Gfx_RotMatrixX(&coord->coord, arg0->spawnArg1, 0);
            work->field_24 = 0xC0;
            work->field_26 = 0x180;
            arg0->state    = 1;
        case 1:
            func_shelter_b2_pod_bottom_8017E788(coord, (s16)work->field_26, (s16)work->field_24);
            if (Gp_State1C->eventState == 0) {
                work->field_26 += 0x60;
                y               = work->field_24 - 0x18;
                work->field_24  = y;
                if (y < 0x18) {
                    Gp_ReleaseState1CMem(work, arg0);
                }
            } else {
                work->field_22--;
            }
            break;
    }
}
