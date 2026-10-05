/* Part of the Pyke flame library; see pyke_flame.h. */

#ifndef PYKE_FLAME_SPLASH_DEAD_BIAS
#define PYKE_FLAME_SPLASH_DEAD_BIAS 0
#endif

/// Draws an additive textured ground splash below a flying flame.
///
/// `halfSize` is a signed 16-bit local half-side in game coordinate units,
/// promoted by the caller to s32. Rotates its X/Z corners by the view
/// coordinate's work matrix, then adds the three
/// borrowed s32 coordinates of `worldPosition`, narrowing each result to s16.
/// The position need only live for this call. The final projection's negative
/// GTE FLAG rejects the quad. Requires one scratch block and one POLY_FT4 in
/// the frame arena, whose packet must remain live through GPU drawing.
static void _pykeFlameDrawSplash(const VECTOR3* worldPosition, s32 halfSize)
{
    enum { TEXTURE_LEFT  = 0xE0,
           TEXTURE_TOP   = 0xC8,
           UV_SPAN       = 31,
           TEXTURE_SHADE = 0x40 }; // Half-intensity modulation (0x80 is neutral).

    EffectGroundQuadScratch* scratchHead;
    EffectGroundQuadScratch* scratch;
    POLY_FT4*                quad;
    EffectUnitQuadCorner*    unitCorners;
    s32                      cornerIndex;
    s32                      projectionFlags;
    s32                      orderingDepth;

    scratchHead                                   = SCRATCH_STACK_CURSOR(EffectGroundQuadScratch) - 1;
    SCRATCH_STACK_CURSOR(EffectGroundQuadScratch) = scratchHead;
    scratch                                       = scratchHead;
    gte_SetTransMatrix(&GsWSMATRIX);
    cornerIndex = 0;
    unitCorners = D_80111E38;
    do {
        scratch->vertices[cornerIndex].vx = (u16)unitCorners[cornerIndex].axis0Sign * halfSize;
        scratch->vertices[cornerIndex].vy = 0;
        scratch->vertices[cornerIndex].vz = (u16)unitCorners[cornerIndex].axis1Sign * halfSize;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&scratch->vertices[cornerIndex]);
        gte_rtv0();
        gte_stsv(&scratch->vertices[cornerIndex]);
        (u16) scratch->vertices[cornerIndex].vx = (u16)scratch->vertices[cornerIndex].vx + (u16)worldPosition->vx;
        (u16) scratch->vertices[cornerIndex].vy = (u16)scratch->vertices[cornerIndex].vy + (u16)worldPosition->vy;
        (u16) scratch->vertices[cornerIndex].vz = (u16)scratch->vertices[cornerIndex].vz + (u16)worldPosition->vz;
        cornerIndex++;
    } while (cornerIndex < ARRAY_SIZE(D_80111E38));

    // Project the first corner separately, then the remaining three together.
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&scratch->screenCorners[0]);
    gte_ldv3(&scratch->vertices[1], &scratch->vertices[2], &scratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&scratch->screenCorners[1], &scratch->screenCorners[2], &scratch->screenCorners[3]);
    gte_stflg(&projectionFlags);
    if (projectionFlags >= 0) {
#if PYKE_FLAME_SPLASH_DEAD_BIAS
        // This carrier applies the bias before the GTE store overwrites it.
        orderingDepth++;
        gte_stszotz(&orderingDepth);
#else
        gte_stszotz(&orderingDepth);
        orderingDepth++;
#endif
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setRGB0(quad, TEXTURE_SHADE, TEXTURE_SHADE, TEXTURE_SHADE);
        quad->tpage = EFFECT_SPRITE_ATLAS_TEXTURE_PAGE;
        quad->clut  = getClut(240, 268);
        setUV4(quad, TEXTURE_LEFT, TEXTURE_TOP, TEXTURE_LEFT + UV_SPAN, TEXTURE_TOP,
               TEXTURE_LEFT, TEXTURE_TOP + UV_SPAN, TEXTURE_LEFT + UV_SPAN, TEXTURE_TOP + UV_SPAN);
        quad->x0 = scratch->screenCorners[0].vx;
        quad->y0 = scratch->screenCorners[0].vy;
        quad->x1 = scratch->screenCorners[1].vx;
        quad->y1 = scratch->screenCorners[1].vy;
        quad->x2 = scratch->screenCorners[2].vx;
        quad->y2 = scratch->screenCorners[2].vy;
        quad->x3 = scratch->screenCorners[3].vx;
        quad->y3 = scratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)orderingDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectGroundQuadScratch);
}

#undef PYKE_FLAME_SPLASH_DEAD_BIAS
