/* Part of the glow drawing library; see glow_draw.h. */

/// Draws the flame ring: `arg0`'s origin is projected once through
/// `GsWSMATRIX` and eight `POLY_G4` blades are swept around it, each spanning
/// a 0x200 arc of radius `(arg1 * 64) / depth`. Only the third vertex carries
/// colour, the rest of the blade fading to black, and that colour is the
/// `arg2` ramp `(arg2, arg2 >> 1, arg2 >> 2)` - a red-biased fire tint. A
/// negative `gte_stflg` drops the whole ring.
void glowDrawFlameStar(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    EffectCentreScratch* block;
    POLY_G4*             prim;
    s32                  ang;

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
        block->depth++;
        block->screenExtent = (arg1 * 64) / block->depth;
        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2, arg2 >> 1, arg2 >> 2);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->screenExtent * rsin(ang)) >> 12);
            prim->y0 = block->screenY + ((block->screenExtent * rcos(ang)) >> 12);
            prim->x1 = block->screenX + ((block->screenExtent * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->screenY + ((block->screenExtent * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->screenExtent * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->screenY + ((block->screenExtent * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
