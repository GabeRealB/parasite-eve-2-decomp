/* Part of the Pyke flame library; see pyke_flame.h. */

/// Draws the animated, additive flame at the weapon nozzle as a billboard.
///
/// Borrows the three s32 world coordinates for this call, narrowing to s16.
/// `animationFrame` wraps modulo six 32-texel cells. `sizeScale` controls the
/// screen half-side in pixels as sizeScale * 31 / (SZ3 / 4 + 1); it does not
/// modulate colour. A negative GTE FLAG rejects the projection. Requires space
/// for one scratch block and one POLY_FT4 in the frame arena; the packet must
/// remain live through GPU drawing.
static void _pykeFlameDrawNozzle(const VECTOR3* worldPosition, u16 animationFrame, u16 sizeScale)
{
    enum { FRAME_COUNT = 6,
           CELL_SHIFT  = 5,
           UV_SPAN     = (1 << CELL_SHIFT) - 1,
           TEXTURE_TOP = 0x98 };

    EffectCentreScratch* scratchEnd;
    EffectCentreScratch* scratch;
    POLY_FT4*            quad;
    SVECTOR*             projectionPoint;
    s16                  screenX;
    s16                  screenY;
    u16                  textureFrameIndex;
    s32                  textureLeft;
    s32                  textureRight;
    u16                  worldZ;

    // Project the nozzle centre, then size the square from its biased depth.
    scratchEnd                                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    (scratchEnd - 1)->worldPoint.vx           = (u16)worldPosition->vx;
    scratch                                   = scratchEnd - 1;
    scratch->worldPoint.vy                    = (u16)worldPosition->vy;
    worldZ                                    = (u16)worldPosition->vz;
    SCRATCH_STACK_CURSOR(EffectCentreScratch) = scratch;
    scratch->worldPoint.vz                    = worldZ;
    projectionPoint                           = &scratch->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(projectionPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        scratch->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->tpage = EFFECT_SPRITE_ATLAS_TEXTURE_PAGE;
        quad->clut  = getClut(208, 268);
        quad->v0    = TEXTURE_TOP;
        quad->v1    = TEXTURE_TOP;
        quad->v2    = TEXTURE_TOP + UV_SPAN;
        quad->v3    = TEXTURE_TOP + UV_SPAN;
        // Keep the wrapped frame in a halfword before forming byte UVs.
        textureFrameIndex     = animationFrame % FRAME_COUNT;
        textureLeft           = textureFrameIndex << CELL_SHIFT;
        textureRight          = textureLeft + UV_SPAN;
        quad->u0              = textureLeft;
        quad->u1              = textureRight;
        quad->u2              = textureLeft;
        quad->u3              = textureRight;
        scratch->screenExtent = (sizeScale * UV_SPAN) / scratch->depth;
        screenX               = scratch->screenX - (u16)scratch->screenExtent;
        quad->x2              = screenX;
        quad->x0              = screenX;
        screenX               = scratch->screenX + (u16)scratch->screenExtent;
        quad->x3              = screenX;
        quad->x1              = screenX;
        screenY               = scratch->screenY - (u16)scratch->screenExtent;
        quad->y1              = screenY;
        quad->y0              = screenY;
        screenY               = scratch->screenY + (u16)scratch->screenExtent;
        quad->y3              = screenY;
        quad->y2              = screenY;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
