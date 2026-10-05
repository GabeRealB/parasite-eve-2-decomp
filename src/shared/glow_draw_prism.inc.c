/* Part of the glow drawing library; see glow_draw.h. */

/// Draws an additive tinted light prism from an eight-corner local-space block.
///
/// `firstCorner` is an element index into this package's `gGlowPrismCorners`;
/// all eight elements starting there must exist. The first four form the lit
/// ring and the next four its dark rim. `coord->workm` must already map the
/// corners into world space. Borrows `coord`; transformed components narrow
/// to signed 16 bits before view projection through `GsWSMATRIX`.
///
/// Four fading sides and a lit cap use green/blue `16 + (rsin(animFrame *
/// 1024) >> 12)` and red three quarters of that value. Each sorts at its last
/// projected corner's camera Z / 4, without clipping or testing projection
/// flags. Queues five quads and additive blend commands in the current frame.
static void _glowDrawPrism(const GfxCoord* coord, s16 firstCorner)
{
    enum {
        GLOW_PRISM_RING_CORNERS      = 4,
        GLOW_PRISM_PULSE_PHASE_SHIFT = 10,
        GLOW_PRISM_PULSE_TRIG_SHIFT  = 12,
        GLOW_PRISM_BRIGHTNESS_BASE   = 16,
    };

    EffectQuadCornersScratch* block;
    POLY_G4*                  prim;
    s32                       cornerIndex;
    s32                       nextCorner;
    s32                       farCorner;
    s32                       farNextCorner;
    s16                       brightness;
    s16                       red;
    s16                       blue;
    s16                       green;

    brightness = (rsin(gDisplayState.animFrame << GLOW_PRISM_PULSE_PHASE_SHIFT) >> GLOW_PRISM_PULSE_TRIG_SHIFT) + GLOW_PRISM_BRIGHTNESS_BASE;
    SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    block = SCRATCH_STACK_CURSOR(EffectQuadCornersScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    red   = brightness * 3 / 4;
    green = brightness;
    blue  = brightness;
    // Place one local corner with coord rotation already loaded. Captures coord and
    // block; source is evaluated once. destinationIndex is repeated and must be
    // a side-effect-free index 0..3. Use only as statements in braced bodies.
    // This binding is confined to this function and is undefined before return.
#define GLOW_PRISM_TRANSFORM_CORNER(source, destinationIndex)  \
    gte_ldv0((source));                                        \
    gte_rtv0();                                                \
    gte_stsv(&block->vertices[destinationIndex]);              \
    block->vertices[destinationIndex].vx += coord->workm.t[0]; \
    block->vertices[destinationIndex].vy += coord->workm.t[1]; \
    block->vertices[destinationIndex].vz += coord->workm.t[2];

    // Fade the four sides from the lit ring to the dark rim.
    for (cornerIndex = 0; cornerIndex < GLOW_PRISM_RING_CORNERS; cornerIndex++) {
        gte_SetRotMatrix(&coord->workm);
        GLOW_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner + cornerIndex], 0);
        gte_SetRotMatrix(&coord->workm);
        nextCorner = (cornerIndex + 1) & (GLOW_PRISM_RING_CORNERS - 1);
        GLOW_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner + nextCorner], 1);
        gte_SetRotMatrix(&coord->workm);
        farCorner = cornerIndex + GLOW_PRISM_RING_CORNERS;
        GLOW_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner + farCorner], 2);
        gte_SetRotMatrix(&coord->workm);
        farNextCorner = nextCorner + GLOW_PRISM_RING_CORNERS;
        GLOW_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner + farNextCorner], 3);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vertices[0]);
        gte_rtps();
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&block->vertices[1], &block->vertices[2], &block->vertices[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&block->depth);
        setRGB0(prim, red, green, blue);
        setRGB1(prim, red, green, blue);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
    }
    gte_SetRotMatrix(&coord->workm);
    // Close the lit end in quad vertex order 0, 1, 3, 2.
    GLOW_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner], 0);
    gte_SetRotMatrix(&coord->workm);
    GLOW_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner + 1], 1);
    gte_SetRotMatrix(&coord->workm);
    GLOW_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner + 3], 2);
    gte_SetRotMatrix(&coord->workm);
    GLOW_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner + 2], 3);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vertices[0]);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&block->vertices[1], &block->vertices[2], &block->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&block->depth);
    setRGB0(prim, red, green, blue);
    setRGB1(prim, red, green, blue);
    setRGB2(prim, red, green, blue);
    setRGB3(prim, red, green, blue);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
#undef GLOW_PRISM_TRANSFORM_CORNER
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
}
