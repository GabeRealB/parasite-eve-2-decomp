#include "main/random.h"

/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// Draws a layered burst with a ground glow and a flickering orange point light.
///
/// `coord` must have a composed world matrix. The inner square's `halfExtent`,
/// in world units, is scaled by 55 / (SZ3 / 4); the outer square and ground quad
/// use a signed 16-bit half-extent of `halfExtent * 3 / 2`. Negative projection
/// flags suppress the quads, while transient light slot 2 is refreshed for two
/// frames regardless of projection. Both textures alternate each display frame.
static void _roomVisualEffectsDrawHaloBurstGlow(const GfxCoord* coord, s16 halfExtent)
{
    GfxCoord                       ground;
    POLY_FT4*                      quad;
    s16                            outerLeft;
    s16                            outerRight;
    s16                            outerTop;
    s16                            outerBottom;
    s16                            intensity;
    s16                            left;
    s16                            right;
    s16                            top;
    s16                            bottom;
    s32                            outerHalfExtent;
    s32                            packedIntensity;
    u32                            randomState;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          light;
    EffectCentreScratch*           projection;

    // Refresh the shared transient light even when the glow is off screen.
    lightSlot                                          = &gWorldCoordTransientPointLights[ROOM_VISUAL_EFFECTS_BURST_LIGHT_SLOT];
    lightSlot->framesLeft                              = ROOM_VISUAL_EFFECTS_BURST_LIGHT_LIFETIME_FRAMES;
    light                                              = &lightSlot->light;
    light->inner                                       = ROOM_VISUAL_EFFECTS_BURST_LIGHT_INNER_RADIUS;
    light->outer                                       = ROOM_VISUAL_EFFECTS_BURST_LIGHT_OUTER_RADIUS;
    randomState                                        = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    intensity                                          = ((randomState >> 0x10) & ROOM_VISUAL_EFFECTS_BURST_LIGHT_RANDOM_INTENSITY_MASK) + ROOM_VISUAL_EFFECTS_BURST_LIGHT_BASE_INTENSITY;
    light->head.color.r                                = intensity;
    packedIntensity                                    = intensity << 0x10;
    light->head.color.g                                = packedIntensity >> 0x11;
    light->head.color.b                                = packedIntensity >> 0x12;
    light->head.transform.lighting.local.t[0]          = coord->coord.t[0];
    light->head.transform.lighting.local.t[1]          = coord->coord.t[1];
    light->head.transform.lighting.local.t[2]          = coord->coord.t[2];
    lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    gRandomLcgState                                    = randomState;
    projection                                         = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    projection->worldPoint.vx                          = coord->workm.t[0];
    projection->worldPoint.vy                          = coord->workm.t[1];
    projection->worldPoint.vz                          = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        quad->code  = ROOM_VISUAL_EFFECTS_TEXTURED_QUAD_BLEND;
        quad->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
            quad->r0   = 0xA0;
            quad->g0   = 0x80;
            quad->b0   = 0x60;
            quad->clut = 0x428B;
            setUV4(quad, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            quad->clut = 0x428C;
            setUV4(quad, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            quad->code |= ROOM_VISUAL_EFFECTS_RAW_TEXTURE_FLAG;
        }
        projection->screenExtent = halfExtent * ROOM_VISUAL_EFFECTS_GLOW_PROJECTION_SCALE / projection->depth;
        left                     = projection->screenX - projection->screenExtent;
        quad->x2                 = left;
        quad->x0                 = left;
        right                    = projection->screenX + projection->screenExtent;
        quad->x3                 = right;
        quad->x1                 = right;
        top                      = projection->screenY - projection->screenExtent;
        quad->y1                 = top;
        quad->y0                 = top;
        bottom                   = projection->screenY + projection->screenExtent;
        quad->y3                 = bottom;
        quad->y2                 = bottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)projection->depth << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            quad);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        quad->code  = ROOM_VISUAL_EFFECTS_TEXTURED_QUAD_RAW_BLEND;
        quad->tpage = 0x29;
        quad->clut =
            (((gDisplayState.animFrame & 1) * 0x10 + 0x120) >> 4) | 0x4300;
        setUV4(quad, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerHalfExtent          = (s16)(halfExtent * 3 / 2);
        projection->screenExtent = outerHalfExtent * ROOM_VISUAL_EFFECTS_GLOW_PROJECTION_SCALE / projection->depth;
        outerLeft                = projection->screenX - projection->screenExtent;
        quad->x2                 = outerLeft;
        quad->x0                 = outerLeft;
        outerRight               = projection->screenX + projection->screenExtent;
        quad->x3                 = outerRight;
        quad->x1                 = outerRight;
        outerTop                 = projection->screenY - projection->screenExtent;
        quad->y1                 = outerTop;
        quad->y0                 = outerTop;
        outerBottom              = projection->screenY + projection->screenExtent;
        quad->y3                 = outerBottom;
        quad->y2                 = outerBottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)projection->depth << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            quad);
        if (worldCollisionProjectGroundCoord(coord, &ground) == 1) {
            _roomVisualEffectsDrawHaloBurstGroundQuad(&ground, outerHalfExtent);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Draws the animated ground glow quad at a composed ground-hit coordinate.
///
/// `halfExtent` scales the four unit corners in world units. Their XZ-plane
/// offsets are rotated by the view coordinate's world matrix before translation
/// to `coord` and projection. A negative flag on the three-corner transform
/// suppresses drawing. The texture alternates between two 32-texel cells.
static void _roomVisualEffectsDrawHaloBurstGroundQuad(const GfxCoord* coord, s32 halfExtent)
{
    EffectQuadScratch*    quadScratch;
    SVECTOR*              vertex;
    s32                   cornerIndex;
    EffectUnitQuadCorner* corner;
    POLY_FT4*             quad;
    s32                   scaledX;
    s32                   textureU;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_80111E38); cornerIndex++) {
        vertex     = &quadScratch->vertices[cornerIndex];
        corner     = &D_80111E38[cornerIndex];
        scaledX    = (u16)corner->axis0Sign * halfExtent;
        vertex->vy = 0;
        vertex->vx = scaledX;
        vertex->vz = (u16)corner->axis1Sign * halfExtent;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(vertex);
        gte_rtv0();
        gte_stsv(vertex);
        vertex->vx += coord->workm.t[0];
        vertex->vy += coord->workm.t[1];
        vertex->vz += coord->workm.t[2];
    }

    // Project one corner and then the remaining three; only the latter flags are tested.
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
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        setcode(quad, ROOM_VISUAL_EFFECTS_TEXTURED_QUAD_BLEND);
        setRGB0(quad, 0x30, 0x20, 0x20);
        quad->tpage = 0x28;
        quad->clut  = 0x428C;
        textureU    = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        quad->v0    = 0x38;
        quad->u0    = textureU;
        textureU    = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        quad->v1    = 0x38;
        quad->u1    = textureU;
        textureU    = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        quad->v2    = 0x57;
        quad->u2    = textureU;
        textureU    = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        quad->v3    = 0x57;
        quad->u3    = textureU;
        quad->x0    = quadScratch->screenCorners[0].vx;
        quad->y0    = quadScratch->screenCorners[0].vy;
        quad->x1    = quadScratch->screenCorners[1].vx;
        quad->y1    = quadScratch->screenCorners[1].vy;
        quad->x2    = quadScratch->screenCorners[2].vx;
        quad->y2    = quadScratch->screenCorners[2].vy;
        quad->x3    = quadScratch->screenCorners[3].vx;
        quad->y3    = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}
