/* Part of the water effects library; see water_effects.h. */

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent shade-tex
/// `POLY_FT4` (tpage 0x2B, clut 0x43D2). `arg1` picks one of eight 56-texel
/// tiles, four across and two down from v=0x70. The on-screen radius is
/// `arg2 * 55 / otz`; the quad is 2*radius on a side, shifted up so the
/// projected point sits at three-quarters height.
void waterDrawTile(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    GpRingScratch* block;
    POLY_FT4*      prim;

    SCRATCH_STACK_RESERVE_BLOCK(GpRingScratch);
    block         = SCRATCH_STACK_CURSOR(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
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
        block->step = (arg2 * 0x37) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->step;
        prim->x1 = prim->x3 = block->sx + block->step;
        prim->y0 = prim->y1 = block->sy - block->step - (block->step >> 1);
        prim->y2 = prim->y3 = block->sy + (block->step >> 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpRingScratch);
}
