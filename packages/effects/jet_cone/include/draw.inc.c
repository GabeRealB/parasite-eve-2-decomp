/* Part of the jet cone library; see jet_cone.h. */

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include <core/pe2.h>
#include <math/gte.h>

#include <gameplay/effects.h>

#include <engine/coord.h>
#include <engine/display.h>
#include <engine/display_types.h>
#include <engine/scratch.h>

#include "jet_cone.h"

STATIC_ASSERT(ARRAY_SIZE(JET_CONE_TEXTURE_FRAME_OFFSETS) >= EFFECT_BAND_SEGMENT_COUNT, jet_cone_texture_frame_offsets_cover_segments);
STATIC_ASSERT(JET_CONE_EXTENDED_TAIL_RIM_RADIUS >= 0 && JET_CONE_EXTENDED_TAIL_RIM_RADIUS <= 0x7FFF, jet_cone_extended_tail_rim_radius_fits_s16);
STATIC_ASSERT(JET_CONE_BASE_TAIL_RIM_RADIUS >= 0 && JET_CONE_BASE_TAIL_RIM_RADIUS <= 0x7FFF, jet_cone_base_tail_rim_radius_fits_s16);

/// Projects one segment's quad: two rim points, then the two hub points beneath them.
static inline void _jetConeProjectSegment(EffectBandScratch* scratch, s32 segmentIndex)
{
    s32 nextSegmentIndex;
    gte_ldv0(&scratch->topRing[segmentIndex]);
    gte_rtps();
    // Preserve corner 0 before the three-corner projection advances the FIFO.
    gte_stsxy(&scratch->sxy0);
    nextSegmentIndex = (segmentIndex + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);
    gte_ldv3(&scratch->topRing[nextSegmentIndex], &scratch->bottomRing[segmentIndex], &scratch->bottomRing[nextSegmentIndex]);
    gte_rtpt();
    gte_stsxy3(&scratch->sxy1, &scratch->sxy2, &scratch->sxy3);
    gte_stflg(&scratch->projectionFlags);
}

/// Rotates `vertex` in place by `rotation`.
#define JET_CONE_ROTATE_VERTEX(rotation, vertex) \
    {                                            \
        gte_SetRotMatrix((rotation));            \
        gte_ldv0((vertex));                      \
        gte_rtv0();                              \
        gte_stsv((vertex));                      \
    }

/// Draws a jet of sixteen textured quads from a hub ring at the coordinate to a rim ring along its local -Z. The rim
/// lies `baseLength` + 16 * `ageFrames` behind a 64-unit hub, or with `extendedTail` 2 * `baseLength` + 256 *
/// `ageFrames` behind a 128-unit hub. Each segment cycles six 40-texel cells from its own frame offset.
static void _jetConeDraw(const GfxCoord* coord, s16 ageFrames, s16 baseLength, s32 extendedTail)
{
    enum { JET_CONE_TRIG_FRACTION_BITS         = 12,
           JET_CONE_RING_ANGLE_SHIFT           = 8,
           JET_CONE_BASE_TAIL_GROWTH_SHIFT     = 4,
           JET_CONE_EXTENDED_TAIL_GROWTH_SHIFT = 8,
           JET_CONE_BASE_HUB_RADIUS            = 64,
           JET_CONE_EXTENDED_HUB_RADIUS        = 128,
           JET_CONE_TEXTURE_FRAME_COUNT        = 6,
           JET_CONE_TEXTURE_CELL_SIZE          = 40,
           JET_CONE_TEXTURE_V_TOP              = 96,
           JET_CONE_TEXTURE_UV_SPAN            = JET_CONE_TEXTURE_CELL_SIZE - 1,
           JET_CONE_TEXTURE_INTENSITY          = 48 };
    EffectBandScratch* coneScratch;
    POLY_FT4*          prim;
    SVECTOR*           hubVertex;
    s32                rimRadius;
    s32                hubRadius;
    s32                selectedRimRadius;
    s32                selectedHubRadius;
    s32                segmentIndex;
    s32                ringAngle;
    s32                textureU;
    s16                tailDistance;
    const MATRIX*      rotation;

    coneScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    if (extendedTail != 0) {
        tailDistance      = (baseLength << 1) + (ageFrames << JET_CONE_EXTENDED_TAIL_GROWTH_SHIFT);
        selectedHubRadius = JET_CONE_EXTENDED_HUB_RADIUS;
        selectedRimRadius = JET_CONE_EXTENDED_TAIL_RIM_RADIUS;
    } else {
        tailDistance      = baseLength + (ageFrames << JET_CONE_BASE_TAIL_GROWTH_SHIFT);
        selectedHubRadius = JET_CONE_BASE_HUB_RADIUS;
        selectedRimRadius = JET_CONE_BASE_TAIL_RIM_RADIUS;
    }
/// Builds the rim and hub rings in the coordinate's frame.
#define JET_CONE_BUILD_RINGS()                                                                                                                                   \
    {                                                                                                                                                            \
        gte_SetTransMatrix(&GsWSMATRIX);                                                                                                                         \
        segmentIndex = 0;                                                                                                                                        \
        rimRadius    = selectedRimRadius;                                                                                                                        \
        rotation     = &coord->workm;                                                                                                                            \
        hubRadius    = selectedHubRadius;                                                                                                                        \
        /* Rotate the rim and hub into the composed frame, preserving s16 narrowing. */                                                                          \
        for (; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {                                                                                       \
            ringAngle                             = segmentIndex << JET_CONE_RING_ANGLE_SHIFT;                                                                   \
            coneScratch->topRing[segmentIndex].vx = (rsin(ringAngle) * rimRadius) >> JET_CONE_TRIG_FRACTION_BITS;                                                \
            coneScratch->topRing[segmentIndex].vy = (rcos(ringAngle) * rimRadius) >> JET_CONE_TRIG_FRACTION_BITS;                                                \
            coneScratch->topRing[segmentIndex].vz = -tailDistance;                                                                                               \
            JET_CONE_ROTATE_VERTEX(rotation, &coneScratch->topRing[segmentIndex]);                                                                               \
            coneScratch->topRing[segmentIndex].vx   += (u16)coord->workm.t[0];                                                                                   \
            coneScratch->topRing[segmentIndex].vy   += (u16)coord->workm.t[1];                                                                                   \
            coneScratch->topRing[segmentIndex].vz   += (u16)coord->workm.t[2];                                                                                   \
            coneScratch->bottomRing[segmentIndex].vx = (rsin(ringAngle) * hubRadius) >> JET_CONE_TRIG_FRACTION_BITS;                                             \
            hubVertex                                = (SVECTOR*)((u8*)coneScratch + segmentIndex * sizeof(SVECTOR) + OFFSET_OF(EffectBandScratch, bottomRing)); \
            hubVertex->vy                            = (rcos(ringAngle) * hubRadius) >> JET_CONE_TRIG_FRACTION_BITS;                                             \
            hubVertex->vz                            = 0;                                                                                                        \
            JET_CONE_ROTATE_VERTEX(rotation, &coneScratch->bottomRing[segmentIndex]);                                                                            \
            coneScratch->bottomRing[segmentIndex].vx += (u16)coord->workm.t[0];                                                                                  \
            hubVertex->vy                            += (u16)coord->workm.t[1];                                                                                  \
            hubVertex->vz                            += (u16)coord->workm.t[2];                                                                                  \
        }                                                                                                                                                        \
    }
    JET_CONE_BUILD_RINGS();
#undef JET_CONE_BUILD_RINGS
    // Project and sort each accepted segment independently.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        _jetConeProjectSegment(coneScratch, segmentIndex);
        if (coneScratch->projectionFlags >= 0) {
            gte_stszotz(&coneScratch->otz);
            coneScratch->otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            prim->tpage = getTPage(0, GPU_BLEND_ADD, 640, 0);
            prim->clut  = JET_CONE_CLUT;
            setRGB0(prim, JET_CONE_TEXTURE_INTENSITY, JET_CONE_TEXTURE_INTENSITY, JET_CONE_TEXTURE_INTENSITY);
            setSemiTrans(prim, 1);
            textureU = (s16)((JET_CONE_TEXTURE_FRAME_OFFSETS[segmentIndex] + ageFrames) % JET_CONE_TEXTURE_FRAME_COUNT) * JET_CONE_TEXTURE_CELL_SIZE;
            setUV4(prim, textureU, JET_CONE_TEXTURE_V_TOP, textureU + JET_CONE_TEXTURE_UV_SPAN, JET_CONE_TEXTURE_V_TOP, textureU, JET_CONE_TEXTURE_V_TOP + JET_CONE_TEXTURE_UV_SPAN, textureU + JET_CONE_TEXTURE_UV_SPAN, JET_CONE_TEXTURE_V_TOP + JET_CONE_TEXTURE_UV_SPAN);
            setXY4(prim, coneScratch->sxy0.vx, coneScratch->sxy0.vy, coneScratch->sxy1.vx, coneScratch->sxy1.vy, coneScratch->sxy2.vx, coneScratch->sxy2.vy, coneScratch->sxy3.vx,
                   coneScratch->sxy3.vy);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(coneScratch->otz), prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}

#undef JET_CONE_ROTATE_VERTEX
#undef JET_CONE_CLUT
#undef JET_CONE_EXTENDED_TAIL_RIM_RADIUS
#undef JET_CONE_BASE_TAIL_RIM_RADIUS
#undef JET_CONE_TEXTURE_FRAME_OFFSETS
