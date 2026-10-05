/* Part of the glow drawing library; see glow_draw.h. */

/// Draws an additive grey light prism from an eight-corner local-space block.
///
/// `firstCorner` is an element index into this package's `gGlowPrismCorners`;
/// all eight elements starting there must exist. The first four form the lit
/// ring and the next four its dark rim. `coord->workm` must already map the
/// corners into world space. Borrows `coord`; transformed components wrap to
/// signed 16 bits before view projection through `GsWSMATRIX`.
///
/// Four fading sides and a lit cap use grey `24 + (rsin(animFrame * 1024)
/// >> 11)`. Each sorts at its last projected corner's camera Z / 4, without
/// clipping or testing projection flags. Queues five quads and additive blend
/// commands in the current frame packet arena.
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
    // Place one local corner with coord rotation already loaded. Captures coord and
    // block; source is evaluated once. destinationIndex is repeated and must be
    // a side-effect-free index 0..3. Use only as statements in braced bodies.
    // This binding is confined to this function and is undefined before return.
#define GLOW_GREY_PRISM_TRANSFORM_CORNER(source, destinationIndex)                                             \
    gte_ldv0((source));                                                                                        \
    gte_rtv0();                                                                                                \
    gte_stsv(&block->vertices[destinationIndex]);                                                              \
    block->vertices[destinationIndex].vx = (u16)block->vertices[destinationIndex].vx + (u16)coord->workm.t[0]; \
    block->vertices[destinationIndex].vy = (u16)block->vertices[destinationIndex].vy + (u16)coord->workm.t[1]; \
    block->vertices[destinationIndex].vz = (u16)block->vertices[destinationIndex].vz + (u16)coord->workm.t[2];

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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
#undef GLOW_GREY_PRISM_TRANSFORM_CORNER
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
}
