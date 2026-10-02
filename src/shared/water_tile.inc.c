/* Part of the water effects library; see water_effects.h. */

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent shade-tex
/// `POLY_FT4` (tpage 0x2B, clut 0x43D2). `arg1` picks one of eight 56-texel
/// tiles, four across and two down from v=0x70. The on-screen radius is
/// `arg2 * 55 / depth`; the quad is 2*radius on a side, shifted up so the
/// projected point sits at three-quarters height.
void waterDrawTile(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    EffectCentreScratch* block;
    POLY_FT4*            prim;

    SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    block                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        setUV4(prim, (arg1 % 4) * 0x38, (arg1 % 8) / 4 * 0x38 + 0x70,
               (arg1 % 4) * 0x38 + 0x37, (arg1 % 8) / 4 * 0x38 + 0x70,
               (arg1 % 4) * 0x38, (arg1 % 8) / 4 * 0x38 + 0x70 + 0x37,
               (arg1 % 4) * 0x38 + 0x37, (arg1 % 8) / 4 * 0x38 + 0x70 + 0x37);
        block->screenExtent = (arg2 * 0x37) / block->depth;
        prim->x0 = prim->x2 = block->screenX - block->screenExtent;
        prim->x1 = prim->x3 = block->screenX + block->screenExtent;
        prim->y0 = prim->y1 = block->screenY - block->screenExtent - (block->screenExtent >> 1);
        prim->y2 = prim->y3 = block->screenY + (block->screenExtent >> 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
