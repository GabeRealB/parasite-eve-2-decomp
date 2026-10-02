/* Part of the glow drawing library; see glow_draw.h. */

/// Draws one wedge of the drain funnel as a Gouraud triangle. `arg0`'s origin
/// is projected once through `GsWSMATRIX`; the two outer corners sit `arg1`
/// screen units away at `arg2 - 0x20` and `arg2 + 0x20`, so the wedge is a
/// 0x40-wide fan blade about `arg2`. Only the apex carries `rgb`, the rim
/// fading to black. A negative `gte_stflg` drops the wedge.
void glowDrawWedge(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    u8*                  head;
    EffectCentreScratch* block;
    SVECTOR*             vec;
    POLY_G3*             prim;
    s32                  ang;
    s32                  ang2;
    u16                  vz;

    head                                                                        = SCRATCH_STACK_CURSOR(u8);
    ((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->worldPoint.vx = (u16)arg0->workm.t[0];
    block                                                                       = (EffectCentreScratch*)(head - sizeof(EffectCentreScratch));
    block->worldPoint.vy                                                        = (u16)arg0->workm.t[1];
    vz                                                                          = (u16)arg0->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectCentreScratch)                                   = block;
    block->worldPoint.vz                                                        = vz;
    vec                                                                         = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->screenX);
    gte_stflg(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG3(prim);
        setRGB0(prim, rgb[0], rgb[1], rgb[2]);
        setRGB1(prim, 0, 0, 0);
        setRGB2(prim, 0, 0, 0);
        block->screenExtent = ((s16)arg1 * 128) / block->depth;
        ang                 = (s16)arg2;
        ang2                = ang - 0x20;
        prim->x0            = block->screenX;
        prim->y0            = block->screenY;
        prim->x1            = block->screenX + ((block->screenExtent * rsin(ang2)) >> 12);
        prim->y1            = block->screenY + ((block->screenExtent * rcos(ang2)) >> 12);
        ang                += 0x20;
        prim->x2            = block->screenX + ((block->screenExtent * rsin(ang)) >> 12);
        prim->y2            = block->screenY + ((block->screenExtent * rcos(ang)) >> 12);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
