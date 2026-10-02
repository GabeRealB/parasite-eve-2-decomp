/* Part of the effect sprite library; see effect_sprite.h. */

/// Draws a textured billboard at the world position of `arg0`, projected
/// through `GsWSMATRIX`: one semi-transparent axis-aligned `POLY_FT4`, a
/// square of half-side `(s16)arg2 * 55` over the depth, raised so the
/// projected point sits three quarters of the way down it. `arg1` picks the
/// frame, a 56x56 cell in a four-by-two grid of the texture page. Nothing is
/// drawn when the projection flags an error.
void effectSpriteDrawBillboard(GfxCoord* arg0, s32 arg1, s32 arg2)
{
    EffectCentreScratch* block;
    POLY_FT4*            prim;
    u16                  idx;
    u32                  cell;
    s32                  row;
    u8                   u0;
    u8                   u1;
    u8                   v0;
    u8                   v1;

    idx                  = arg1;
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
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
        prim->clut  = 0x4393;
        cell        = idx;
        u0          = (cell & 3) * 0x38;
        row         = ((cell & 7) >> 2) * 0x38;
        v0          = row;
        v1          = row + 0x37;
        u1          = u0 + 0x37;
        setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
        block->screenExtent = ((s16)arg2 * 55) / block->depth;
        prim->x0 = prim->x2 = block->screenX - block->screenExtent;
        prim->x1 = prim->x3 = block->screenX + block->screenExtent;
        prim->y0 = prim->y1 = block->screenY - block->screenExtent - (block->screenExtent >> 1);
        prim->y2 = prim->y3 = block->screenY + (block->screenExtent >> 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
