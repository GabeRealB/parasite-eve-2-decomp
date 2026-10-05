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
static inline void _leafTransformCorner(EffectQuadScratch* quadScratch, s32 cornerIndex, s32 halfSize, const GfxCoord* coord)
{
    quadScratch->vertices[cornerIndex].vx = (u16)D_80111E38[cornerIndex].axis0Sign * halfSize;
    quadScratch->vertices[cornerIndex].vy = 0;
    quadScratch->vertices[cornerIndex].vz = (u16)D_80111E38[cornerIndex].axis1Sign * halfSize;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&quadScratch->vertices[cornerIndex]);
    gte_rtv0();
    gte_stsv(&quadScratch->vertices[cornerIndex]);
    quadScratch->vertices[cornerIndex].vx += coord->workm.t[0];
    quadScratch->vertices[cornerIndex].vy += coord->workm.t[1];
    quadScratch->vertices[cornerIndex].vz += coord->workm.t[2];
}

/// Draws a Neo Ark leaf as a textured square in the coordinate's local XZ plane.
///
/// `coord->workm` must already be composed. `halfSize` is in coordinate units;
/// transformed corners narrow to signed 16-bit coordinates. Brightness zero
/// selects raw opaque texturing; nonzero brightness uses its low byte for
/// grey modulation with additive blending (128 is neutral modulation).
/// Uses an 8x8 cell at texture UV (0, 40); a negative GTE FLAG rejects the
/// projection before allocating a frame-arena quad. Releases its scratch
/// block before returning. The caller provides primitive and scratch capacity.
static void _leafDraw(const GfxCoord* coord, s32 halfSize, s16 brightness)
{
    enum {
        LEAF_TEXTURE_PAGE       = getTPage(0, GPU_BLEND_ADD, 704, 0),
        LEAF_TEXTURE_CLUT       = getClut(256, 270),
        LEAF_TEXTURE_V          = 40,
        LEAF_TEXTURE_LAST_TEXEL = 7,
    };
    EffectQuadScratch* quadScratch;
    s32                cornerIndex;
    POLY_FT4*          primitive;

    // Build the square in the leaf's frame, retaining 16-bit corner arithmetic.
    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_80111E38); cornerIndex++) {
        _leafTransformCorner(quadScratch, cornerIndex, halfSize, coord);
    }

    // Keep projected corners in scratch until the GTE accepts the quad.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        primitive      = gGpuPrimCursor;
        gGpuPrimCursor = primitive + 1;
        setPolyFT4(primitive);
        if (brightness != LEAF_BRIGHTNESS_RAW_TEXTURE) {
            setSemiTrans(primitive, 1);
            setRGB0(primitive, brightness, brightness, brightness);
        } else {
            setShadeTex(primitive, 1);
        }
        primitive->tpage = LEAF_TEXTURE_PAGE;
        primitive->clut  = LEAF_TEXTURE_CLUT;
        primitive->v0    = LEAF_TEXTURE_V;
        primitive->v1    = LEAF_TEXTURE_V;
        primitive->u0    = 0;
        primitive->u1    = LEAF_TEXTURE_LAST_TEXEL;
        primitive->u2    = 0;
        primitive->v2    = LEAF_TEXTURE_V + LEAF_TEXTURE_LAST_TEXEL;
        primitive->u3    = LEAF_TEXTURE_LAST_TEXEL;
        primitive->v3    = LEAF_TEXTURE_V + LEAF_TEXTURE_LAST_TEXEL;
        primitive->x0    = quadScratch->screenCorners[0].vx;
        primitive->y0    = quadScratch->screenCorners[0].vy;
        primitive->x1    = quadScratch->screenCorners[1].vx;
        primitive->y1    = quadScratch->screenCorners[1].vy;
        primitive->x2    = quadScratch->screenCorners[2].vx;
        primitive->y2    = quadScratch->screenCorners[2].vy;
        primitive->x3    = quadScratch->screenCorners[3].vx;
        primitive->y3    = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                primitive);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}
