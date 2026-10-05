/* Part of the falling leaves library; see falling_leaves.h. */

/// Places one corner of the leaf's local XZ square in its composed coordinate space.
///
/// `cornerIndex` is 0..3 in GPU quad strip order; `halfSize` is the half-side
/// in coordinate units (the leaf task supplies 32). `coord->workm` must be
/// composed, and `quadScratch` must be a live, word-aligned scratch block.
/// Local products and translated components narrow to signed 16 bits; the
/// intervening rotation stores the GTE's signed IR results. Reading the signs
/// as `u16` preserves the same low product bits for negative corners.
/// Updates only the selected vertex's X/Y/Z, leaving its fourth halfword
/// intact. Loads the GTE rotation matrix and V0 and overwrites the IR results;
/// it does not project or load GTE translation. Both pointers are borrowed
/// for this call, with no allocation or retention.
static inline void _leafTransformCorner(EffectQuadCornersScratch* quadScratch, s32 cornerIndex, s32 halfSize, const GfxCoord* coord)
{
    SVECTOR* corner;
    quadScratch->vertices[cornerIndex].vx = (u16)D_80111E38[cornerIndex].axis0Sign * halfSize;
    // This aliases vertices[cornerIndex]; the byte view preserves separate store/GTE addresses.
    corner     = (SVECTOR*)((u8*)quadScratch + cornerIndex * sizeof(SVECTOR) + OFFSET_OF(EffectQuadCornersScratch, vertices));
    corner->vy = 0;
    corner->vz = (u16)D_80111E38[cornerIndex].axis1Sign * halfSize;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&quadScratch->vertices[cornerIndex]);
    gte_rtv0();
    gte_stsv(&quadScratch->vertices[cornerIndex]);
    quadScratch->vertices[cornerIndex].vx += coord->workm.t[0];
    corner->vy                            += coord->workm.t[1];
    corner->vz                            += coord->workm.t[2];
}

/// Draws an Acropolis leaf as a textured square in the coordinate's local XZ plane.
///
/// `coord->workm` must already be composed. `halfSize` is in coordinate units;
/// transformed corners narrow to signed 16-bit coordinates. Brightness zero
/// selects raw opaque texturing; nonzero brightness uses its low byte for
/// grey modulation with additive blending (128 is neutral modulation).
/// Uses an 8x8 cell at texture UV (0, 232) and rejects depths below 17.
/// Consumes one frame-arena quad even when rejected, and releases its scratch
/// block before returning. The caller provides primitive and scratch capacity.
static void _leafDraw(const GfxCoord* coord, s32 halfSize, s16 brightness)
{
    enum {
        LEAF_MIN_DRAW_DEPTH     = 17,
        LEAF_TEXTURE_PAGE       = getTPage(0, GPU_BLEND_ADD, 704, 0),
        LEAF_TEXTURE_CLUT       = getClut(256, 270),
        LEAF_TEXTURE_V          = 232,
        LEAF_TEXTURE_LAST_TEXEL = 7,
    };
    EffectQuadCornersScratch* quadScratch;
    POLY_FT4*                 primitive;
    s32                       cornerIndex;

    // Build the square in the leaf's frame, retaining 16-bit corner arithmetic.
    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_80111E38); cornerIndex++) {
        _leafTransformCorner(quadScratch, cornerIndex, halfSize, coord);
    }
    // Project directly into a packet; rejected leaves still consume that packet.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    primitive      = gGpuPrimCursor;
    gGpuPrimCursor = primitive + 1;
    setPolyFT4(primitive);
    gte_stsxy(&primitive->x0);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    setUV4(primitive, 0, LEAF_TEXTURE_V, LEAF_TEXTURE_LAST_TEXEL, LEAF_TEXTURE_V,
           0, LEAF_TEXTURE_V + LEAF_TEXTURE_LAST_TEXEL, LEAF_TEXTURE_LAST_TEXEL, LEAF_TEXTURE_V + LEAF_TEXTURE_LAST_TEXEL);
    gte_stsxy3(&primitive->x1, &primitive->x2, &primitive->x3);
    gte_stszotz(&quadScratch->depth);
    if (quadScratch->depth >= LEAF_MIN_DRAW_DEPTH) {
        if (brightness != LEAF_BRIGHTNESS_RAW_TEXTURE) {
            setRGB0(primitive, brightness, brightness, brightness);
            setSemiTrans(primitive, 1);
        } else {
            setShadeTex(primitive, 1);
        }
        primitive->tpage = LEAF_TEXTURE_PAGE;
        primitive->clut  = LEAF_TEXTURE_CLUT;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                primitive);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
}
