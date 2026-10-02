/* Part of the Diver library; see diver.h. */

/// Links one frame of the rotating impact-spark billboard at `arg0`'s world
/// position, projected through `GsWSMATRIX` by a single `RTPS`; a negative
/// projection flag drops the quad.  `arg1` picks one of six 0x27-square frames
/// along row 0x38 of tpage 0x2A, `arg2` sizes the quad and `arg3` spins it: the
/// corners sit `arg2 * 0x27 / depth` from the projected centre along `arg3` and
/// `arg3 + 0x400`, so the spark shrinks with depth.
void diverDrawSpark(GfxCoord* arg0, u16 arg1, u16 arg2, s32 arg3)
{
    void**                  scratch;
    EffectBillboardScratch* scratchHead;
    EffectBillboardScratch* block;
    s32*                    depthOutput;
    POLY_FT4*               prim;
    s32                     ang;
    u16                     frame;
    s32                     u;

    scratch                        = SCRATCH_HEAD_ADDR;
    scratchHead                    = SCRATCH_HEAD_AT(scratch, EffectBillboardScratch);
    block                          = scratchHead - 1;
    depthOutput                    = &block->depth;
    block->worldPoint.vx           = (u16)arg0->workm.t[0];
    block->worldPoint.vy           = (u16)arg0->workm.t[1];
    block->worldPoint.vz           = (u16)arg0->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(depthOutput);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x2A;
        prim->clut  = 0x4293;
        frame       = arg1 % 6;
        u           = frame * 0x28;
        setUV4(prim, u, 0x38, u + 0x27, 0x38, u, 0x5F, u + 0x27, 0x5F);
        ang                  = (s16)arg3;
        block->cornerOffsetX = (((arg2 * 0x27) / block->depth) * rsin(ang)) >> 12;
        block->cornerOffsetY = (((arg2 * 0x27) / block->depth) * rcos(ang)) >> 12;
        prim->x0             = block->screenX + (u16)block->cornerOffsetX;
        prim->x3             = block->screenX - (u16)block->cornerOffsetX;
        prim->y0             = block->screenY - (u16)block->cornerOffsetY;
        prim->y3             = block->screenY + (u16)block->cornerOffsetY;
        ang                  = ang + 0x400;
        block->cornerOffsetX = (((arg2 * 0x27) / block->depth) * rsin(ang)) >> 12;
        block->cornerOffsetY = (((arg2 * 0x27) / block->depth) * rcos(ang)) >> 12;
        prim->x1             = block->screenX + (u16)block->cornerOffsetX;
        prim->x2             = block->screenX - (u16)block->cornerOffsetX;
        prim->y1             = block->screenY - (u16)block->cornerOffsetY;
        prim->y2             = block->screenY + (u16)block->cornerOffsetY;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, sizeof(*block));
}
