/* Part of the water effects library; see water.h. */

/// Projects the four splash corners to the screen and stores the GTE flags; corner 3's depth stays in the GTE for the
/// caller.
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

/// Draws an additive splash square in the coordinate's X/Z plane, `halfSize` units from center to edge, modulated by
/// gray `brightness` (0x80 neutral).
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
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(gEffectUnitQuadCorners); cornerIndex++) {
        unitCorner       = &gEffectUnitQuadCorners[cornerIndex];
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(quadScratch->depth),
                splashQuad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}
