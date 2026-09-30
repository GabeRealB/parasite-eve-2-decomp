/* Part of the glow drawing library; see glow_draw.h. */

/// Draws the pyrokinesis flame band: two 16-vertex rings of radius `arg1`
/// and `arg1 + arg2` are built in the XZ plane by `rsin` / `rcos`, rotated by
/// `arg0`'s `workm` and offset by its translation, then each of the 16
/// segments is projected through `GsWSMATRIX` as one `POLY_G4`. The inner
/// edge carries the `arg3` ramp `(arg3, arg3 >> 1, arg3 >> 2)` and the outer
/// edge fades to black; a negative `gte_stflg` drops the segment.
void glowDrawFlameRing(GfxCoord* arg0, s16 arg1, s32 arg2, s16 arg3)
{
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    s32            i;
    s32            next;
    s32            ang;
    s16            r0;
    s16            r1;

    r1    = arg1 + arg2;
    block = SCRATCH_STACK_RESERVE_BLOCK(GpBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = 0;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx += arg0->workm.t[0];
        block->inner[i].vy += arg0->workm.t[1];
        block->inner[i].vz += arg0->workm.t[2];
        block->outer[i].vx  = (rsin(ang) * r1) >> 12;
        op                  = &block->inner[i] + 16;
        op->vy              = 0;
        op->vz              = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx += arg0->workm.t[0];
        op->vy             += arg0->workm.t[1];
        op->vz             += arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, arg3, arg3 >> 1, arg3 >> 2);
            setRGB1(prim, arg3, arg3 >> 1, arg3 >> 2);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sxy0.vx;
            prim->y0 = block->sxy0.vy;
            prim->x1 = block->sxy1.vx;
            prim->y1 = block->sxy1.vy;
            prim->x2 = block->sxy2.vx;
            prim->y2 = block->sxy2.vy;
            prim->x3 = block->sxy3.vx;
            prim->y3 = block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpBandScratch);
}
