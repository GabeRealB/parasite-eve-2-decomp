/* Part of the glow drawing library; see glow.h. */

/// Draws a pulsing grey light prism from the eight corners at `firstCorner` of the package's `gGlowPrismCorners`,
/// shaped as `_glowDrawPrism`'s.
static void _glowDrawGreyPrism(const GfxCoord* coord, s16 firstCorner)
{
    enum {
        GLOW_GREY_PRISM_RING_CORNERS      = 4,
        GLOW_GREY_PRISM_PULSE_PHASE_SHIFT = 10,
        GLOW_GREY_PRISM_PULSE_TRIG_SHIFT  = 11,
        GLOW_GREY_PRISM_BRIGHTNESS_BASE   = 24,
    };

    EffectQuadCornersScratch* block;
    POLY_G4*                  prim;
    s32                       cornerIndex;
    s32                       nextCorner;
    s32                       farCorner;
    s32                       farNextCorner;
    u8                        brightness;

    SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    block = SCRATCH_STACK_CURSOR(EffectQuadCornersScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    brightness = (rsin(gDisplayState.animFrame << GLOW_GREY_PRISM_PULSE_PHASE_SHIFT) >> GLOW_GREY_PRISM_PULSE_TRIG_SHIFT) + GLOW_GREY_PRISM_BRIGHTNESS_BASE;
    /// Moves a prism corner into world space through `coord`, whose rotation the caller has loaded, into `block`'s
    /// vertex `destinationIndex`.
#define GLOW_GREY_PRISM_TRANSFORM_CORNER(source, destinationIndex)                                                     \
    (void)({                                                                                                           \
        gte_ldv0((source));                                                                                            \
        gte_rtv0();                                                                                                    \
        gte_stsv(&block->vertices[(destinationIndex)]);                                                                \
        block->vertices[(destinationIndex)].vx = (u16)block->vertices[(destinationIndex)].vx + (u16)coord->workm.t[0]; \
        block->vertices[(destinationIndex)].vy = (u16)block->vertices[(destinationIndex)].vy + (u16)coord->workm.t[1]; \
        block->vertices[(destinationIndex)].vz = (u16)block->vertices[(destinationIndex)].vz + (u16)coord->workm.t[2]; \
    })

    // Fade the four sides from the lit ring to the dark rim.
    for (cornerIndex = 0; cornerIndex < GLOW_GREY_PRISM_RING_CORNERS; cornerIndex++) {
        gte_SetRotMatrix(&coord->workm);
        GLOW_GREY_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner + cornerIndex], 0);
        gte_SetRotMatrix(&coord->workm);
        nextCorner = (cornerIndex + 1) & (GLOW_GREY_PRISM_RING_CORNERS - 1);
        GLOW_GREY_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner + nextCorner], 1);
        gte_SetRotMatrix(&coord->workm);
        farCorner = cornerIndex + GLOW_GREY_PRISM_RING_CORNERS;
        GLOW_GREY_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner + farCorner], 2);
        gte_SetRotMatrix(&coord->workm);
        farNextCorner = nextCorner + GLOW_GREY_PRISM_RING_CORNERS;
        GLOW_GREY_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner + farNextCorner], 3);
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
        setRGB0(prim, brightness, brightness, brightness);
        setRGB1(prim, brightness, brightness, brightness);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
    }
    gte_SetRotMatrix(&coord->workm);
    // Close the lit end in quad vertex order 0, 1, 3, 2.
    GLOW_GREY_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner], 0);
    gte_SetRotMatrix(&coord->workm);
    GLOW_GREY_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner + 1], 1);
    gte_SetRotMatrix(&coord->workm);
    GLOW_GREY_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner + 3], 2);
    gte_SetRotMatrix(&coord->workm);
    GLOW_GREY_PRISM_TRANSFORM_CORNER(&gGlowPrismCorners[firstCorner + 2], 3);
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
    setRGB0(prim, brightness, brightness, brightness);
    setRGB1(prim, brightness, brightness, brightness);
    setRGB2(prim, brightness, brightness, brightness);
    setRGB3(prim, brightness, brightness, brightness);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth), prim);
    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
#undef GLOW_GREY_PRISM_TRANSFORM_CORNER
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
}
