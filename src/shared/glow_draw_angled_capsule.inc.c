/* Part of the glow drawing library; see glow_draw.h. */

/// Draws a capsule-shaped glow between the world point `arg0` and the one
/// after it: a half-disc of gouraud wedges around each end and a band joining
/// them, lit along the centre line and black at the rim. Nothing is drawn
/// unless the second point's OTZ is at least 0x11. `arg1` is the half-extent
/// (the on-screen radius is `(s16)arg1 * 64 / otz`), `arg2` the capsule's
/// angle, and `arg3` the colour: a red byte at bits 8-15 and two-bit green
/// and blue at bits 4 and 0, each scaled by a blend that flickers with the
/// frame counter.
void glowDrawAngledCapsule(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GlowPointPairScratch* block;
    POLY_G4*              prim;
    POLY_G4*              p;
    SVECTOR*              p1;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   t3;
    s32                   packed;
    s32                   extent;
    s32                   r0;
    s32                   r1;
    s32                   base;
    u8                    blend;
    u8                    r;
    u8                    g;
    u8                    b;

    p1    = arg0 + 1;
    block = SCRATCH_STACK_RESERVE_BLOCK(GlowPointPairScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&block->sx1);
    gte_stszotz(&block->otz1);
    if (block->otz1 >= 0x11) {
        if (block->otz0 < 0x10) {
            block->otz0 = 0x10;
        }
        extent         = (s16)arg1 * 64;
        r0             = extent / block->otz0;
        r1             = extent / block->otz1;
        packed         = arg3 << 16;
        blend          = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        r              = blend * (packed >> 24);
        g              = blend * ((packed >> 20) & 3);
        base           = (s16)arg2;
        b              = blend * (arg3 & 3);
        ang            = 0;
        block->radius0 = r0;
        block->radius1 = r1;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = r;
            p->g2    = g;
            prim->b2 = b;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->radius0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->radius0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->radius0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->radius0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->radius0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->radius0 * rcos(base + t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, r, g, b);
            prim->x0 = block->sx0 + ((block->radius0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->radius0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->radius1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->radius1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz0);

            t3             = ang - 0x1000;
            prim           = gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->radius1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->radius1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->radius1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->radius1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->radius1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->radius1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowPointPairScratch);
}
