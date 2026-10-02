/* Part of the glow drawing library; see glow_draw.h. */

/// Draws a halo around `coord`'s world position: sixteen additive gouraud
/// `POLY_G4`s forming a ring `width` wide outside a radius of `inner`, both
/// divided by the projected depth. The inner edge is black and the outer rim
/// `rgb`, so the ring brightens outward. Each quad sorts at the centre's depth -
/// one slot behind it, or pulled GLOW_DRAW_HALO_PULL toward the eye when the
/// including unit defines that. A negative `gte_stflg` drops the halo.
void glowDrawHalo(GfxCoord* coord, s32 inner, s32 width, u8* rgb)
{
    EffectShapeScratch* block;
    POLY_G4*            prim;
    s32                 ang;
    s32                 next;
    s32                 outer;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    outer                = inner + width;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
#ifdef GLOW_DRAW_HALO_PULL
        /* drawn over what it surrounds: pulled toward the eye, kept off the near plane */
        block->depth -= GLOW_DRAW_HALO_PULL;
        if (block->depth < 0x10) {
            block->depth = 0x10;
        }
#else
        block->depth++;
#endif
        block->extent.ring.inner = ((s16)inner * 64) / block->depth;
        block->extent.ring.outer = ((s16)outer * 64) / block->depth;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->screenX + ((block->extent.ring.inner * rsin(ang)) >> 12);
            prim->y0 = block->screenY + ((block->extent.ring.inner * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->screenX + ((block->extent.ring.inner * rsin(next)) >> 12);
            prim->y1 = block->screenY + ((block->extent.ring.inner * rcos(next)) >> 12);
            prim->x2 = block->screenX + ((block->extent.ring.outer * rsin(ang)) >> 12);
            prim->y2 = block->screenY + ((block->extent.ring.outer * rcos(ang)) >> 12);
            prim->x3 = block->screenX + ((block->extent.ring.outer * rsin(next)) >> 12);
            prim->y3 = block->screenY + ((block->extent.ring.outer * rcos(next)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

#undef GLOW_DRAW_HALO_PULL
