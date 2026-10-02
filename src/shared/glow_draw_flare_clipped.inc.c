/* Part of the glow drawing library; see glow_draw.h. */

/// Projects the world point `arg0` through `gGfxViewCoord.workm` and, when its OTZ
/// is above 0x10, queues one semi-transparent `POLY_FT4` sprite centred on it:
/// tpage 0x2B, clut `(arg1 & 0x3F) | 0x4380`, and the 40-texel-wide UV cell
/// `(s16)arg1` selects. `(s16)arg2` is the half-extent; the on-screen radius is
/// `arg2 * 39 / otz`. All three colour channels take the flickering
/// `((animFrame & 1) * 16) + 0x20`.
void glowDrawFlareClipped(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    GlowCentreRadiusScratch* block;
    POLY_FT4*                prim;
    s32                      u;
    s32                      blend;
    s32                      idx;
    u8                       frame;

    block = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreRadiusScratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    gte_stsxy(&block->sx);
    gte_stszotz(&block->otz);
    if (block->otz > 0x10) {
        idx         = (s16)arg1;
        frame       = gDisplayState.animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u           = idx * 40;
        setUV4(prim, u, 0, u + 39, 0, u, 39, u + 39, 39);
        blend = ((frame & 1) << 4) + 0x20;
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->radius;
        prim->x1 = prim->x3 = block->sx + block->radius;
        prim->y0 = prim->y1 = block->sy - block->radius;
        prim->y2 = prim->y3 = block->sy + block->radius;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreRadiusScratch);
}
