#include "main/random.h"

/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// Draws a glow at the coordinate: two camera-facing textured squares, an
/// inner one of half-extent `size` and an outer one of `size * 3 / 2`
/// (each scaled by 0x37 / depth), plus a flat quad on the ground beneath it.
/// It also points the `gWorldCoordTransientPointLights[2]` light at the
/// coordinate with a randomly flickering intensity. Nothing is drawn when the
/// GTE flags the projection.
static void RoomFx_DrawBurst2Glow(GfxCoord* coord, s16 size)
{
    GfxCoord                       ground;
    POLY_FT4*                      prim;
    s16                            outerLeft;
    s16                            outerRight;
    s16                            outerTop;
    s16                            outerBottom;
    s16                            intensity;
    s16                            left;
    s16                            right;
    s16                            top;
    s16                            bottom;
    s32                            outerSize;
    s32                            shifted;
    u32                            random;
    WorldCoordTransientPointLight* slot;
    WorldCoordPointLight*          light;
    EffectCentreScratch*           block;

    slot                                          = &gWorldCoordTransientPointLights[2];
    slot->framesLeft                              = 2;
    light                                         = &slot->light;
    light->inner                                  = 0x300;
    light->outer                                  = 0x3000;
    random                                        = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    intensity                                     = ((random >> 0x10) & 0x700) + 0x800;
    light->head.color.r                           = intensity;
    shifted                                       = intensity << 0x10;
    light->head.color.g                           = shifted >> 0x11;
    light->head.color.b                           = shifted >> 0x12;
    light->head.transform.lighting.local.t[0]     = coord->coord.t[0];
    light->head.transform.lighting.local.t[1]     = coord->coord.t[1];
    light->head.transform.lighting.local.t[2]     = coord->coord.t[2];
    slot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    gRandomLcgState                               = random;
    block                                         = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    block->worldPoint.vx                          = coord->workm.t[0];
    block->worldPoint.vy                          = coord->workm.t[1];
    block->worldPoint.vz                          = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code |= 1;
        }
        block->screenExtent = size * 0x37 / block->depth;
        left                = block->screenX - block->screenExtent;
        prim->x2            = left;
        prim->x0            = left;
        right               = block->screenX + block->screenExtent;
        prim->x3            = right;
        prim->x1            = right;
        top                 = block->screenY - block->screenExtent;
        prim->y1            = top;
        prim->y0            = top;
        bottom              = block->screenY + block->screenExtent;
        prim->y3            = bottom;
        prim->y2            = bottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)block->depth << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (((gDisplayState.animFrame & 1) * 0x10 + 0x120) >> 4) | 0x4300;
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize           = (s16)(size * 3 / 2);
        block->screenExtent = outerSize * 0x37 / block->depth;
        outerLeft           = block->screenX - block->screenExtent;
        prim->x2            = outerLeft;
        prim->x0            = outerLeft;
        outerRight          = block->screenX + block->screenExtent;
        prim->x3            = outerRight;
        prim->x1            = outerRight;
        outerTop            = block->screenY - block->screenExtent;
        prim->y1            = outerTop;
        prim->y0            = outerTop;
        outerBottom         = block->screenY + block->screenExtent;
        prim->y3            = outerBottom;
        prim->y2            = outerBottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)block->depth << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        if (worldCollisionProjectGroundCoord(coord, &ground) == 1) {
            RoomFx_DrawGround2Quad(&ground, outerSize);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void RoomFx_DrawGround2Quad(GfxCoord* arg0, s32 arg1)
{
    EffectQuadScratch*    quadScratch;
    SVECTOR*              v;
    s32                   i;
    EffectUnitQuadCorner* corners;
    POLY_FT4*             prim;
    s32                   prod;
    s32                   u;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < ARRAY_SIZE(D_80111E38); i++) {
        v       = &quadScratch->vertices[i];
        corners = &D_80111E38[i];
        prod    = (u16)corners->axis0Sign * arg1;
        v->vy   = 0;
        v->vx   = prod;
        v->vz   = (u16)corners->axis1Sign * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += arg0->workm.t[0];
        v->vy += arg0->workm.t[1];
        v->vz += arg0->workm.t[2];
    }

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
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = quadScratch->screenCorners[0].vx;
        prim->y0    = quadScratch->screenCorners[0].vy;
        prim->x1    = quadScratch->screenCorners[1].vx;
        prim->y1    = quadScratch->screenCorners[1].vy;
        prim->x2    = quadScratch->screenCorners[2].vx;
        prim->y2    = quadScratch->screenCorners[2].vy;
        prim->x3    = quadScratch->screenCorners[3].vx;
        prim->y3    = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}
