/* Part of the fireball library; see fireball.h. */

/// Builds four view-space corners around the fireball's ground point.
///
/// `coord->workm.t` must already contain the centre in the view frame;
/// `halfExtent` is a signed-halfword extent in world units, promoted to s32.
/// Borrows all four scratch vertices and overwrites GTE rotation/translation.
/// Each corner narrows before rotation and again after adding the centre.
/// The scratch block must remain live until its caller finishes projection.
static inline void _fireballBuildGroundQuad(EffectGroundQuadScratch* quadScratch, const GfxCoord* coord, s32 halfExtent)
{
    s32 cornerIndex;
    gte_SetTransMatrix(&GsWSMATRIX);
    // Rotate the flat world axes, then translate within the composed view frame.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_80111E38); cornerIndex++) {
        quadScratch->vertices[cornerIndex].vx = (u16)D_80111E38[cornerIndex].axis0Sign * halfExtent;
        quadScratch->vertices[cornerIndex].vy = 0;
        quadScratch->vertices[cornerIndex].vz = (u16)D_80111E38[cornerIndex].axis1Sign * halfExtent;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&quadScratch->vertices[cornerIndex]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[cornerIndex]);
        quadScratch->vertices[cornerIndex].vx += coord->workm.t[0];
        quadScratch->vertices[cornerIndex].vy += coord->workm.t[1];
        quadScratch->vertices[cornerIndex].vz += coord->workm.t[2];
    }
}

/// Draws the fireball's additive textured quad on the ground at a composed point.
///
/// `coord->workm.t` must contain the point in the composed view frame;
/// `halfExtent` is a signed-halfword corner extent in world units, promoted to
/// s32 by the caller. Ground-axis corners
/// are rotated by the view matrix and narrow to halfwords before translation
/// and projection. Invalid projections emit no packet. Alternates two 32x32
/// texture cells by display-frame parity and queues at projected depth + 1.
/// Borrows one scratch block during the call. The current arena needs room
/// for one POLY_FT4; it and the ordering table stay live until GPU consumption.
static void _fireballDrawGroundGlow(const GfxCoord* coord, s32 halfExtent)
{
    enum { FIREBALL_GROUND_GLOW_FRAME_SHIFT = 5,
           FIREBALL_GROUND_GLOW_U_LEFT      = 0xC0,
           FIREBALL_GROUND_GLOW_U_RIGHT     = 0xDF,
           FIREBALL_GROUND_GLOW_V_TOP       = 0x38,
           FIREBALL_GROUND_GLOW_V_BOTTOM    = 0x57 };
    EffectGroundQuadScratch* quadScratch;
    POLY_FT4*                prim;
    s32                      depth;
    s32                      projectionFlags;
    s32                      textureU;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectGroundQuadScratch);
    _fireballBuildGroundQuad(quadScratch, coord, halfExtent);

    // Project one corner first, then the remaining three through the GTE FIFO.
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_stflg(&projectionFlags);
    if (projectionFlags >= 0) {
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

            prim->r0    = 0x30;
            prim->g0    = 0x20;
            prim->b0    = 0x20;
            prim->tpage = getTPage(0, GPU_BLEND_ADD, 512, 0);
            prim->clut  = getClut(192, 266);
            setSemiTrans(prim, 1);
            // Latch U before each byte store to preserve the full-width frame load.
            textureU = ((gDisplayState.animFrame & 1) << FIREBALL_GROUND_GLOW_FRAME_SHIFT) + FIREBALL_GROUND_GLOW_U_LEFT;
            prim->v0 = FIREBALL_GROUND_GLOW_V_TOP;
            prim->u0 = textureU;
            textureU = ((gDisplayState.animFrame & 1) << FIREBALL_GROUND_GLOW_FRAME_SHIFT) + FIREBALL_GROUND_GLOW_U_RIGHT;
            prim->v1 = FIREBALL_GROUND_GLOW_V_TOP;
            prim->u1 = textureU;
            textureU = ((gDisplayState.animFrame & 1) << FIREBALL_GROUND_GLOW_FRAME_SHIFT) + FIREBALL_GROUND_GLOW_U_LEFT;
            prim->v2 = FIREBALL_GROUND_GLOW_V_BOTTOM;
            prim->u2 = textureU;
            textureU = ((gDisplayState.animFrame & 1) << FIREBALL_GROUND_GLOW_FRAME_SHIFT) + FIREBALL_GROUND_GLOW_U_RIGHT;
            prim->v3 = FIREBALL_GROUND_GLOW_V_BOTTOM;
            prim->u3 = textureU;
            prim->x0 = quadScratch->screenCorners[0].vx;
            prim->y0 = quadScratch->screenCorners[0].vy;
            prim->x1 = quadScratch->screenCorners[1].vx;
            prim->y1 = quadScratch->screenCorners[1].vy;
            prim->x2 = quadScratch->screenCorners[2].vx;
            prim->y2 = quadScratch->screenCorners[2].vy;
            prim->x3 = quadScratch->screenCorners[3].vx;
            prim->y3 = quadScratch->screenCorners[3].vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectGroundQuadScratch);
}
