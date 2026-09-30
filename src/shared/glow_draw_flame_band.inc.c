/* Part of the glow drawing library; see glow_draw.h. */

/// Draws the pyrokinesis flame cone: a 16-vertex inner ring of radius `arg1`
/// at depth `0x100` and an outer ring of radius `arg1 + 0x100` at depth 0 are
/// built in the XY plane by `rsin` / `rcos`, rotated by `arg0`'s `workm` and
/// offset by its translation, then each of the 16 segments is projected
/// through `GsWSMATRIX` as one `POLY_G4`. The inner edge carries the `arg2`
/// ramp `(arg2, arg2 >> 1, arg2 >> 2)` and the outer edge fades to black; a
/// negative `gte_stflg` drops the segment.
void glowDrawFlameBand(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    s32            i;
    s32            next;
    s32            ang;
    s16            r0;
    s16            r1;
    u32            ramp;
    u8             red;
    u8             grn;
    u8             blu;

    /* The ramp halves are unsigned: writing them as `(u16)arg2 >> 1` folds the
     * widening into an `andi`, where the ROM shifts the value up and back. */
    ramp  = (u32)arg2 << 16;
    red   = arg2;
    grn   = ramp >> 17;
    blu   = ramp >> 18;
    r1    = arg1 + 0x100;
    block = SCRATCH_STACK_RESERVE_BLOCK(GpBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = (rcos(ang) * r0) >> 12;
        block->inner[i].vz = 0x100;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = (u16)block->inner[i].vx + (u16)arg0->workm.t[0];
        block->inner[i].vy = (u16)block->inner[i].vy + (u16)arg0->workm.t[1];
        block->inner[i].vz = (u16)block->inner[i].vz + (u16)arg0->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = (rcos(ang) * r1) >> 12;
        op->vz             = 0;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = (u16)block->outer[i].vx + (u16)arg0->workm.t[0];
        op->vy             = (u16)op->vy + (u16)arg0->workm.t[1];
        op->vz             = (u16)op->vz + (u16)arg0->workm.t[2];
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
            setRGB0(prim, red, grn, blu);
            setRGB1(prim, red, grn, blu);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = (u16)block->sxy0.vx;
            prim->y0 = (u16)block->sxy0.vy;
            prim->x1 = (u16)block->sxy1.vx;
            prim->y1 = (u16)block->sxy1.vy;
            prim->x2 = (u16)block->sxy2.vx;
            prim->y2 = (u16)block->sxy2.vy;
            prim->x3 = (u16)block->sxy3.vx;
            prim->y3 = (u16)block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpBandScratch);
}
