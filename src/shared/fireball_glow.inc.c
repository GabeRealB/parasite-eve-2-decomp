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
    randomBits                                              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
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
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)projection->depth << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)projection->depth << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
