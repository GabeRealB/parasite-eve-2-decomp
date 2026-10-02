/* Part of the glow drawing library; see glow_draw.h. */

/// Draws a glowing bar between the world points `arg0[0]` and `arg0[1]`,
/// projected through `gGfxViewCoord.workm`; nothing is drawn when either
/// projection flags an error. Each end gets a half-disc of gouraud wedges of
/// radius `(s16)arg1 * 64` over its depth, joined by quads across the bar. The
/// lit vertices take the colour packed in `arg2`, one nibble per channel in
/// the high nibble, with bit 3 following the animation frame. A unit that
/// defines GLOW_DRAW_CAPSULE_PULL draws each end beyond depth 0x50 that much
/// nearer, over what it lights; GLOW_DRAW_CAPSULE_SHIFTED_FLICKER adds the
/// frame bit shifted by the colour's top nibble instead of setting bit 3.
void glowDrawCapsule(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      side;
#ifdef GLOW_DRAW_CAPSULE_PULL
    s32 otz;
#endif
    u8 r;
    u8 g;
    u8 b;

    p1    = arg0 + 1;
    block = SCRATCH_STACK_RESERVE_BLOCK(OverlayPointPairScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
#ifdef GLOW_DRAW_CAPSULE_PULL
        otz = block->otz0;
        if (otz > 0x50) {
            block->otz0 = otz - GLOW_DRAW_CAPSULE_PULL;
        }
#endif
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
#ifdef GLOW_DRAW_CAPSULE_PULL
            otz = block->otz1;
            if (otz > 0x50) {
                block->otz1 = otz - GLOW_DRAW_CAPSULE_PULL;
            }
#endif
            scaled         = (s16)arg1 * 64;
            block->radius0 = scaled / block->otz0;
            block->radius1 = scaled / block->otz1;
            ang            = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds             = &gDisplayState;
#ifdef GLOW_DRAW_CAPSULE_SHIFTED_FLICKER
            /* the flicker bit, shifted by the colour word's top nibble, is
               added to each channel */
            blend  = ds->animFrame;
            packed = arg2 << 16;
            blend  = blend & 1;
            ang    = (s16)ang;
            blend  = blend << (packed >> 28);
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend + tr;
            g      = blend + tg;
            b      = blend + ((arg2 & 0xF) << 4);
#else
            ang    = (s16)ang;
            blend  = ((u8)ds->animFrame & 1) * 8;
            packed = arg2 << 16;
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend | tr;
            g      = blend | tg;
            b      = blend | ((arg2 & 0xF) << 4);
#endif
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->radius0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->radius0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->radius0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->radius0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->radius0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->radius0 * rcos(t2)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz0);

                    prim           = gGpuPrimCursor;
                    side           = angStart + (ang - angStart) * 2;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->radius0 * rsin(side)) >> 12);
                    prim->y0 = block->sy0 + ((block->radius0 * rcos(side)) >> 12);
                    prim->x1 = block->sx1 + ((block->radius1 * rsin(side)) >> 12);
                    prim->y1 = block->sy1 + ((block->radius1 * rcos(side)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, (block->otz1 + block->otz0) / 2);
                    t3             = ang + 0x800;
                    prim           = gGpuPrimCursor;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->radius1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->radius1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->radius1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->radius1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->radius1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->radius1 * rcos(t)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayPointPairScratch);
}

#undef GLOW_DRAW_CAPSULE_PULL
#undef GLOW_DRAW_CAPSULE_SHIFTED_FLICKER
