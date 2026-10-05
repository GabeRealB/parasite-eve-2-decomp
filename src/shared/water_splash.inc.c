/* Part of the water effects library; see water_effects.h. */

/// Projects four prepared splash corners to screen pixels.
///
/// `quadScratch` is a borrowed, live, word-aligned block. All four vertices'
/// XYZ components must be initialized as signed 16-bit coordinates in
/// `GsWSMATRIX`'s input space, in GPU quad strip order. The caller must load
/// that matrix's translation and configure the GTE projection beforehand;
/// this helper loads its rotation.
///
/// Writes every `screenCorners` entry and the RTPT `projectionFlags`, even
/// when projection fails. Corner 0's RTPS flags are discarded; the caller
/// tests the final FLAG for rejection. Vertices and the `depth` member are
/// untouched. Corner 3's depth remains in GTE SZ3 for the caller to read
/// before another depth-changing GTE command. GTE state is not restored;
/// the block is neither reserved nor released here, and no pointer is retained.
static inline void _waterProjectSplashCorners(EffectQuadScratch* quadScratch)
{
    gte_SetRotMatrix(&GsWSMATRIX);
    // Save corner 0 before the triple transform replaces the screen FIFO.
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
}

/// Draws one additive, brightness-modulated water splash in a coordinate's X/Z plane.
///
/// `coord` is borrowed for this call, with `workm` already composed for
/// projection. `halfSize` is the local half-side in game coordinate units;
/// running ripple tasks pass 32..5119. Sign products must fit s32 before
/// narrowing. Corner products and translated positions retain their low
/// 16 bits as signed GTE coordinates. `brightness` supplies
/// its low byte to all three colour channels (0x80 is neutral modulation);
/// running ripple tasks draw with values 2..64.
///
/// A nonnegative final GTE FLAG emits one `POLY_FT4`, ordered by the last
/// corner's SZ3 / 4. The frame arena must have space for the complete packet
/// and remain live through GPU drawing. One `EffectQuadScratch` is reserved
/// on the initialized scratch stack and released on every path. No pointer
/// is retained; GTE state is overwritten.
static void _waterDrawSplash(const GfxCoord* coord, s32 halfSize, s32 brightness)
{
    enum { TEXTURE_LEFT          = 0,
           TEXTURE_TOP           = 56,
           TEXTURE_WIDTH_TEXELS  = 56,
           TEXTURE_HEIGHT_TEXELS = 56 };

    EffectQuadScratch*          quadScratch;
    SVECTOR*                    cornerVertex;
    s32                         cornerIndex;
    const EffectUnitQuadCorner* unitCorner;
    POLY_FT4*                   splashQuad;
    s32                         scaledX;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Transform the local square into the composed coordinate space.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_80111E38); cornerIndex++) {
        unitCorner       = &D_80111E38[cornerIndex];
        cornerVertex     = &quadScratch->vertices[cornerIndex];
        scaledX          = (u16)unitCorner->axis0Sign * halfSize;
        cornerVertex->vy = 0;
        cornerVertex->vx = scaledX;
        cornerVertex->vz = (u16)unitCorner->axis1Sign * halfSize;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(cornerVertex);
        gte_rtv0();
        gte_stsv(cornerVertex);
        cornerVertex->vx += coord->workm.t[0];
        cornerVertex->vy += coord->workm.t[1];
        cornerVertex->vz += coord->workm.t[2];
    }

    _waterProjectSplashCorners(quadScratch);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        splashQuad     = gGpuPrimCursor;
        gGpuPrimCursor = splashQuad + 1;
        setPolyFT4(splashQuad);
        splashQuad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
        splashQuad->clut  = getClut(272, 271);
        splashQuad->v0    = TEXTURE_TOP;
        splashQuad->v1    = TEXTURE_TOP;
        setRGB0(splashQuad, brightness, brightness, brightness);
        splashQuad->u0 = TEXTURE_LEFT;
        splashQuad->u1 = TEXTURE_LEFT + TEXTURE_WIDTH_TEXELS - 1;
        splashQuad->u2 = TEXTURE_LEFT;
        splashQuad->v2 = TEXTURE_TOP + TEXTURE_HEIGHT_TEXELS - 1;
        splashQuad->u3 = TEXTURE_LEFT + TEXTURE_WIDTH_TEXELS - 1;
        splashQuad->v3 = TEXTURE_TOP + TEXTURE_HEIGHT_TEXELS - 1;
        setSemiTrans(splashQuad, 1);
        splashQuad->x0 = quadScratch->screenCorners[0].vx;
        splashQuad->y0 = quadScratch->screenCorners[0].vy;
        splashQuad->x1 = quadScratch->screenCorners[1].vx;
        splashQuad->y1 = quadScratch->screenCorners[1].vy;
        splashQuad->x2 = quadScratch->screenCorners[2].vx;
        splashQuad->y2 = quadScratch->screenCorners[2].vy;
        splashQuad->x3 = quadScratch->screenCorners[3].vx;
        splashQuad->y3 = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                splashQuad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}
