#include "main/random.h"

/* Part of the fireball library; see fireball.h. */

/// Refreshes the fireball's shared orange point-light slot for two gameplay frames.
///
/// Copies local XYZ translation into transient slot 2; the last fireball drawn
/// owns that frame's contribution. Consumes one shared LCG draw for Q12 colour
/// intensity (red 2048..3840, green half, blue quarter), with falloff radii
/// 768 and 12288 world units. Requires the transient slot's view parent already
/// initialized and the input translation in that parent's world frame. Only
/// the position is copied; no input pointer is retained. Expiry advances while
/// room effects are unpaused.
static inline void _fireballRefreshGlowLight(const GfxCoord* coord)
{
    enum {
        FIREBALL_LIGHT_SLOT              = 2,
        FIREBALL_LIGHT_UPDATES           = 2,
        FIREBALL_LIGHT_INNER_RADIUS      = 768,
        FIREBALL_LIGHT_OUTER_RADIUS      = 12288,
        FIREBALL_LIGHT_RANDOM_MASK_Q12   = 0x700,
        FIREBALL_LIGHT_MIN_INTENSITY_Q12 = 0x800
    };
    s16                            intensity;
    u32                            randomBits;
    WorldCoordTransientPointLight* pointLightSlot;
    WorldCoordPointLight*          pointLight;

    pointLightSlot                                          = &gWorldCoordTransientPointLights[FIREBALL_LIGHT_SLOT];
    pointLightSlot->framesLeft                              = FIREBALL_LIGHT_UPDATES;
    pointLight                                              = &pointLightSlot->light;
    pointLight->inner                                       = FIREBALL_LIGHT_INNER_RADIUS;
    pointLight->outer                                       = FIREBALL_LIGHT_OUTER_RADIUS;
    randomBits                                              = RANDOM_LCG_NEXT(gRandomLcgState);
    gRandomLcgState                                         = randomBits;
    intensity                                               = ((randomBits >> 16) & FIREBALL_LIGHT_RANDOM_MASK_Q12) + FIREBALL_LIGHT_MIN_INTENSITY_Q12;
    pointLight->head.color.r                                = intensity;
    pointLight->head.color.g                                = intensity >> 1;
    pointLight->head.color.b                                = intensity >> 2;
    pointLight->head.transform.lighting.local.t[0]          = coord->coord.t[0];
    pointLight->head.transform.lighting.local.t[1]          = coord->coord.t[1];
    pointLight->head.transform.lighting.local.t[2]          = coord->coord.t[2];
    pointLightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Draws two flickering fireball billboards and an optional glow on the ground below.
///
/// coord's local translation supplies the transient light, while cached workm
/// XYZ is narrowed to s16 for projection. The cache and active view/GTE matrices
/// must already be composed. halfExtent is a signed game-unit half-extent;
/// pixel extent is halfExtent*55/(SZ3/4+1). The outer extent is 3/2 of it,
/// narrowed to s16, and the ground extent is twice that, narrowed again.
/// Updates the shared light even on rejected projections. Valid projections
/// consume two POLY_FT4 packets, plus a ground packet when the enabled probe hits.
/// Requires live room-effect/display/light state, sufficient primitive space
/// and a word-aligned scratch stack for projection and nested ground queries.
/// No input pointer is retained; the transient light is copied and renewed.
static void _fireballDrawGlow(const GfxCoord* coord, s16 halfExtent)
{
    enum {
        FIREBALL_GLOW_PROJECTION_SCALE    = 55,
        FIREBALL_GLOW_MODULATED_FT4       = 0x2E,
        FIREBALL_GLOW_RAW_FT4             = 0x2F,
        FIREBALL_GLOW_RAW_TEXTURE_BIT     = 1,
        FIREBALL_GLOW_TEXTURE_PAGE        = 0x29,
        FIREBALL_GLOW_INNER_ODD_CLUT      = 0x428B,
        FIREBALL_GLOW_INNER_EVEN_CLUT     = 0x428C,
        FIREBALL_GLOW_OUTER_CLUT_X        = 0x120,
        FIREBALL_GLOW_OUTER_CLUT_ROW_BITS = 0x4300
    };
    GfxCoord             groundCoord;
    POLY_FT4*            glowQuad;
    s16                  outerLeft;
    s16                  outerRight;
    s16                  outerTop;
    s16                  outerBottom;
    s16                  left;
    s16                  right;
    s16                  top;
    s16                  bottom;
    s32                  outerHalfExtent;
    EffectCentreScratch* projection;

    _fireballRefreshGlowLight(coord);
    // Reject invalid projection flags before reserving GPU packets.
    SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    projection                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    projection->worldPoint.vx = coord->workm.t[0];
    projection->worldPoint.vy = coord->workm.t[1];
    projection->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        glowQuad          = gGpuPrimCursor;
        projection->depth = (s32)(projection->depth + 1);
        gGpuPrimCursor    = glowQuad + 1;
        setlen(glowQuad, sizeof(*glowQuad) / sizeof(u32) - 1);
        glowQuad->code  = FIREBALL_GLOW_MODULATED_FT4;
        glowQuad->tpage = FIREBALL_GLOW_TEXTURE_PAGE;
        if (gDisplayState.animFrame & 1) {
            glowQuad->r0   = 0xA0;
            glowQuad->g0   = 0x80;
            glowQuad->b0   = 0x60;
            glowQuad->clut = FIREBALL_GLOW_INNER_ODD_CLUT;
            setUV4(glowQuad, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            glowQuad->clut = FIREBALL_GLOW_INNER_EVEN_CLUT;
            setUV4(glowQuad, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            glowQuad->code = glowQuad->code | FIREBALL_GLOW_RAW_TEXTURE_BIT;
        }
        projection->screenExtent = (halfExtent * FIREBALL_GLOW_PROJECTION_SCALE) / projection->depth;
        left                     = projection->screenX - projection->screenExtent;
        glowQuad->x2             = left;
        glowQuad->x0             = left;
        right                    = projection->screenX + projection->screenExtent;
        glowQuad->x3             = right;
        glowQuad->x1             = right;
        top                      = projection->screenY - projection->screenExtent;
        glowQuad->y1             = top;
        glowQuad->y0             = top;
        bottom                   = projection->screenY + projection->screenExtent;
        glowQuad->y3             = bottom;
        glowQuad->y2             = bottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth),
            glowQuad);
        glowQuad       = gGpuPrimCursor;
        gGpuPrimCursor = glowQuad + 1;
        setlen(glowQuad, sizeof(*glowQuad) / sizeof(u32) - 1);
        glowQuad->code  = FIREBALL_GLOW_RAW_FT4;
        glowQuad->tpage = FIREBALL_GLOW_TEXTURE_PAGE;
        glowQuad->clut =
            (s16)(((u32)(((gDisplayState.animFrame & 1) * 0x10) + FIREBALL_GLOW_OUTER_CLUT_X) >> 4) |
                  FIREBALL_GLOW_OUTER_CLUT_ROW_BITS);
        setUV4(glowQuad, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerHalfExtent          = (s16)(halfExtent * 3 / 2);
        projection->screenExtent = (outerHalfExtent * FIREBALL_GLOW_PROJECTION_SCALE) / projection->depth;
        outerLeft                = projection->screenX - projection->screenExtent;
        glowQuad->x2             = outerLeft;
        glowQuad->x0             = outerLeft;
        outerRight               = projection->screenX + projection->screenExtent;
        glowQuad->x3             = outerRight;
        glowQuad->x1             = outerRight;
        outerTop                 = projection->screenY - projection->screenExtent;
        glowQuad->y1             = outerTop;
        glowQuad->y0             = outerTop;
        outerBottom              = projection->screenY + projection->screenExtent;
        glowQuad->y3             = outerBottom;
        glowQuad->y2             = outerBottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth),
            glowQuad);
        // The optional floor quad shares this call's still-reserved projection block.
        if (gRoomEffectState->groundTraceEnabled != 0) {
            if (worldCollisionProjectGroundCoord(coord, &groundCoord) == 1) {
                _fireballDrawGroundGlow(&groundCoord, (s32)(s16)(outerHalfExtent * 2));
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

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
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(gEffectUnitQuadCorners); cornerIndex++) {
        quadScratch->vertices[cornerIndex].vx = (u16)gEffectUnitQuadCorners[cornerIndex].axis0Sign * halfExtent;
        quadScratch->vertices[cornerIndex].vy = 0;
        quadScratch->vertices[cornerIndex].vz = (u16)gEffectUnitQuadCorners[cornerIndex].axis1Sign * halfExtent;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(depth),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectGroundQuadScratch);
}
