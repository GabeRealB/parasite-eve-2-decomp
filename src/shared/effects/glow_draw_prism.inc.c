/* Part of the glow drawing library; see glow_draw.h. */

/// Selects the carrier's local-space corner array for the tinted prism drawer.
///
/// An override must name a declared `SVECTOR` array before this fragment is
/// included. Each corner must be word-aligned and fully readable: the GTE
/// loads both words, although only signed-halfword XYZ supplies the position.
/// Coordinates use the drawing coordinate's local game units. Each call's
/// nonnegative `firstCorner` is an element index selecting eight elements:
/// four lit-end corners in cyclic order, then four corresponding dark-rim
/// corners. Other regions of the array may hold unrelated geometry.
///
/// The default selects independent `gGlowPrismCorners[16]` arrays in the night
/// warehouse (prism at index 8) and night souvenir shop (prisms at 0 and 8).
/// The night dilapidated house overrides it with its three-prism array.
/// The drawer only reads the selected array and retains no pointer; its
/// carrier must remain loaded during the call. This parameterless identifier
/// binding has no side effects, token pasting or stringification. It is used
/// for every corner load and undefined at the end, including an override.
#ifndef GLOW_PRISM_CORNERS
#define GLOW_PRISM_CORNERS gGlowPrismCorners
#endif

/// Draws an additive tinted light prism from an eight-corner local-space block.
///
/// `firstCorner` is an element index into the carrier's `GLOW_PRISM_CORNERS`;
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
    /// Rotates a local prism corner and adds the composed coordinate's translation.
    ///
    /// Requires a readable, word-aligned `SVECTOR*` source and the coordinate's
    /// Q12 rotation already loaded in the GTE. Captures the readable `GfxCoord*`
    /// `coord` and writable `EffectQuadCornersScratch*` `block`. Stores signed
    /// GTE IR components, then narrows each translated sum back to s16; the
    /// vector's fourth halfword stays intact. An in-place source is permitted.
    /// `source` is evaluated once and `destinationIndex` four times; the index
    /// must be side-effect-free and in 0..3. Clobbers GTE V0, MAC/IR and FLAG;
    /// leaves translation registers intact. Use this statement list only inside
    /// a braced body. Undefined before leaving the drawer.
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
