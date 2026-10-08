/* Part of the ground glow library; see ground_glow.h. */

/// Builds a world-ground square around a composed view-space centre.
///
/// Borrows all four scratch vertices and the coordinate during this call.
/// Each offset narrows to s16 before view rotation and again after translation.
/// Overwrites GTE rotation/translation; projection follows in the caller.
static inline void _groundGlowBuildCorners(EffectGroundQuadScratch* quadScratch, const GfxCoord* ground, s32 halfExtent)
{
    s32 cornerIndex;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_80111E38); cornerIndex++) {
        quadScratch->vertices[cornerIndex].vx = (u16)D_80111E38[cornerIndex].axis0Sign * halfExtent;
        quadScratch->vertices[cornerIndex].vy = 0;
        quadScratch->vertices[cornerIndex].vz = (u16)D_80111E38[cornerIndex].axis1Sign * halfExtent;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&quadScratch->vertices[cornerIndex]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[cornerIndex]);
        quadScratch->vertices[cornerIndex].vx += ground->workm.t[0];
        quadScratch->vertices[cornerIndex].vy += ground->workm.t[1];
        quadScratch->vertices[cornerIndex].vz += ground->workm.t[2];
    }
}

/// Draws the effect's flickering additive floor glow at a composed ground point.
///
/// Requires `ground->workm.t` in the composed view frame and `halfExtent` in
/// world-coordinate units. Alternates two 32x32 cells by display-frame parity.
/// Only the final RTPT flags are tested; an accepted quad is sorted at SZ3/4+1.
/// Borrows one scratch block and reserves at most one POLY_FT4 from the current
/// GPU arena. The carrier supplies GROUND_GLOW_R/G/B and GROUND_GLOW_CLUT.
static void _groundGlowDraw(const GfxCoord* ground, s32 halfExtent)
{
    enum { GROUND_GLOW_FRAME_SHIFT = 5,
           GROUND_GLOW_U_LEFT      = 192,
           GROUND_GLOW_U_RIGHT     = 223,
           GROUND_GLOW_V_TOP       = 56,
           GROUND_GLOW_V_BOTTOM    = 87 };
    EffectGroundQuadScratch* quadScratch;
    POLY_FT4*                prim;
    s32                      depth;
    s32                      projectionFlags;
    s32                      textureU;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectGroundQuadScratch);
    _groundGlowBuildCorners(quadScratch, ground, halfExtent);

    // Project one corner, then the remaining three; retain the final flags.
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&projectionFlags);
    if (projectionFlags >= 0) {
        gte_stszotz(&depth);
        depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        prim->r0    = GROUND_GLOW_R;
        prim->g0    = GROUND_GLOW_G;
        prim->b0    = GROUND_GLOW_B;
        prim->tpage = getTPage(0, GPU_BLEND_ADD, 512, 0);
        prim->clut  = GROUND_GLOW_CLUT;
        // Latch U before its byte store so the frame arithmetic stays full-width.
        textureU = ((gDisplayState.animFrame & 1) << GROUND_GLOW_FRAME_SHIFT) + GROUND_GLOW_U_LEFT;
        prim->v0 = GROUND_GLOW_V_TOP;
        prim->u0 = textureU;
        textureU = ((gDisplayState.animFrame & 1) << GROUND_GLOW_FRAME_SHIFT) + GROUND_GLOW_U_RIGHT;
        prim->v1 = GROUND_GLOW_V_TOP;
        prim->u1 = textureU;
        textureU = ((gDisplayState.animFrame & 1) << GROUND_GLOW_FRAME_SHIFT) + GROUND_GLOW_U_LEFT;
        prim->v2 = GROUND_GLOW_V_BOTTOM;
        prim->u2 = textureU;
        textureU = ((gDisplayState.animFrame & 1) << GROUND_GLOW_FRAME_SHIFT) + GROUND_GLOW_U_RIGHT;
        prim->v3 = GROUND_GLOW_V_BOTTOM;
        prim->u3 = textureU;
        prim->x0 = quadScratch->screenCorners[0].vx;
        prim->y0 = quadScratch->screenCorners[0].vy;
        prim->x1 = quadScratch->screenCorners[1].vx;
        prim->y1 = quadScratch->screenCorners[1].vy;
        prim->x2 = quadScratch->screenCorners[2].vx;
        prim->y2 = quadScratch->screenCorners[2].vy;
        prim->x3 = quadScratch->screenCorners[3].vx;
        prim->y3 = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectGroundQuadScratch);
}

#undef GROUND_GLOW_R
#undef GROUND_GLOW_G
#undef GROUND_GLOW_B
#undef GROUND_GLOW_CLUT
