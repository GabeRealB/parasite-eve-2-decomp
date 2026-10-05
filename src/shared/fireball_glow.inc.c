#include "main/random.h"

/* Part of the fireball library; see fireball.h. */

/// Lights `gWorldCoordTransientPointLights[2]` at `coord` with a randomly flickering
/// intensity, projects `coord` and draws two `POLY_FT4` glow billboards around
/// it, the outer one half again as large as `size`; when
/// `gRoomEffectState->groundTraceEnabled` is set, traces the ground below and draws the
/// ground quad there at twice the outer size.
void fireballDrawGlow(GfxCoord* coord, s16 size)
{
    GfxCoord                       ground;
    POLY_FT4*                      prim;
    s16                            intensity;
    s16                            outerLeft;
    s16                            outerRight;
    s16                            outerTop;
    s16                            outerBottom;
    s16                            left;
    s16                            right;
    s16                            top;
    s16                            bottom;
    s32                            outerSize;
    u32                            random;
    WorldCoordTransientPointLight* slot;
    WorldCoordPointLight*          light;
    EffectCentreScratch*           sc;

    slot                                          = &gWorldCoordTransientPointLights[2];
    slot->framesLeft                              = 2;
    light                                         = &slot->light;
    light->inner                                  = 0x300;
    light->outer                                  = 0x3000;
    random                                        = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState                               = random;
    intensity                                     = ((random >> 0x10) & 0x700) + 0x800;
    light->head.color.r                           = intensity;
    light->head.color.g                           = intensity >> 1;
    light->head.color.b                           = intensity >> 2;
    light->head.transform.lighting.local.t[0]     = (s32)coord->coord.t[0];
    light->head.transform.lighting.local.t[1]     = (s32)coord->coord.t[1];
    light->head.transform.lighting.local.t[2]     = coord->coord.t[2];
    slot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    sc                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    sc->worldPoint.vx = coord->workm.t[0];
    sc->worldPoint.vy = coord->workm.t[1];
    sc->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->worldPoint);
    gte_rtps();
    gte_stsxy(&sc->screenX);
    gte_stflg(&sc->projectionFlags);
    if (sc->projectionFlags >= 0) {
        gte_stszotz(&sc->depth);
        prim           = gGpuPrimCursor;
        sc->depth      = (s32)(sc->depth + 1);
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
            prim->code = (u8)(prim->code | 1);
        }
        sc->screenExtent = ((s16)size * 0x37) / sc->depth;
        left             = sc->screenX - sc->screenExtent;
        prim->x2         = left;
        prim->x0         = left;
        right            = sc->screenX + sc->screenExtent;
        prim->x3         = right;
        prim->x1         = right;
        top              = sc->screenY - sc->screenExtent;
        prim->y1         = top;
        prim->y0         = top;
        bottom           = sc->screenY + sc->screenExtent;
        prim->y3         = bottom;
        prim->y2         = bottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)sc->depth << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (s16)(((u32)(((gDisplayState.animFrame & 1) * 0x10) + 0x120) >> 4) |
                  0x4300);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize        = (s16)((s16)size * 3 / 2);
        sc->screenExtent = (outerSize * 0x37) / sc->depth;
        outerLeft        = sc->screenX - sc->screenExtent;
        prim->x2         = outerLeft;
        prim->x0         = outerLeft;
        outerRight       = sc->screenX + sc->screenExtent;
        prim->x3         = outerRight;
        prim->x1         = outerRight;
        outerTop         = sc->screenY - sc->screenExtent;
        prim->y1         = outerTop;
        prim->y0         = outerTop;
        outerBottom      = sc->screenY + sc->screenExtent;
        prim->y3         = outerBottom;
        prim->y2         = outerBottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)sc->depth << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        if (gRoomEffectState->groundTraceEnabled != 0) {
            if (worldCollisionProjectGroundCoord(coord, &ground) == 1) {
                fireballDrawGroundGlow(&ground, (s32)(s16)(outerSize * 2));
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
