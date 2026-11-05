/* Part of the glow drawing library; see glow.h. */

/// The carrier's array of local-space prism corners, `gGlowPrismCorners` unless the carrier defines another first.
#ifndef GLOW_PRISM_CORNERS
#define GLOW_PRISM_CORNERS gGlowPrismCorners
#endif

/// Draws a pulsing light prism from the eight corners at `firstCorner`, in `coord`'s space: four sides fading from the
/// lit ring of the first four to the black rim of the next four, and a cap over the ring. Red is three quarters of
/// green and blue.
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
    /// Moves a prism corner into world space through `coord`, whose rotation the caller has loaded, into `block`'s
    /// vertex `destinationIndex`.
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
        GLOW_PRISM_TRANSFORM_CORNER(&GLOW_PRISM_CORNERS[firstCorner + cornerIndex], 0);
        gte_SetRotMatrix(&coord->workm);
        nextCorner = (cornerIndex + 1) & (GLOW_PRISM_RING_CORNERS - 1);
        GLOW_PRISM_TRANSFORM_CORNER(&GLOW_PRISM_CORNERS[firstCorner + nextCorner], 1);
        gte_SetRotMatrix(&coord->workm);
        farCorner = cornerIndex + GLOW_PRISM_RING_CORNERS;
        GLOW_PRISM_TRANSFORM_CORNER(&GLOW_PRISM_CORNERS[firstCorner + farCorner], 2);
        gte_SetRotMatrix(&coord->workm);
        farNextCorner = nextCorner + GLOW_PRISM_RING_CORNERS;
        GLOW_PRISM_TRANSFORM_CORNER(&GLOW_PRISM_CORNERS[firstCorner + farNextCorner], 3);
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
    }
    gte_SetRotMatrix(&coord->workm);
    // Close the lit end in quad vertex order 0, 1, 3, 2.
    GLOW_PRISM_TRANSFORM_CORNER(&GLOW_PRISM_CORNERS[firstCorner], 0);
    gte_SetRotMatrix(&coord->workm);
    GLOW_PRISM_TRANSFORM_CORNER(&GLOW_PRISM_CORNERS[firstCorner + 1], 1);
    gte_SetRotMatrix(&coord->workm);
    GLOW_PRISM_TRANSFORM_CORNER(&GLOW_PRISM_CORNERS[firstCorner + 3], 2);
    gte_SetRotMatrix(&coord->workm);
    GLOW_PRISM_TRANSFORM_CORNER(&GLOW_PRISM_CORNERS[firstCorner + 2], 3);
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
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
            prim);
    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
#undef GLOW_PRISM_TRANSFORM_CORNER
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
}

#undef GLOW_PRISM_CORNERS
