/* Part of the jet cone library; see jet_cone.h. */

/// Projects one cone segment through the currently installed GTE matrices.
///
/// Requires a live, word-aligned scratch block with both rings initialized
/// in the installed matrices' input frame, and `segmentIndex` in
/// 0..`EFFECT_BAND_SEGMENT_COUNT`-1. Wraps the next index, writes corners
/// 0/1 from the rim and 2/3 from the hub, and retains only the final RTPT
/// flags. Rings and `otz` stay intact. Leaves SZ3 at the next hub vertex for
/// depth capture by the caller; retains no pointer.
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

/// Rotates one cone vertex in place through a borrowed Q12 basis.
///
/// Statement macro: rotation is evaluated once and vertex twice, so both
/// arguments must be stable, side-effect-free pointers valid for the whole
/// sequence. Changes GTE rotation and narrows the result to s16; the caller
/// adds translation afterward. Captures no locals and is undefined below.
#define JET_CONE_ROTATE_VERTEX(rotation, vertex) \
    {                                            \
        gte_SetRotMatrix((rotation));            \
        gte_ldv0((vertex));                      \
        gte_rtv0();                              \
        gte_stsv((vertex));                      \
    }

/// Draws a sixteen-quad textured jet from two local XY rings along negative Z.
///
/// Requires the coordinate's composed matrix and translation, plus a carrier
/// frame-offset table of sixteen s16 elements. baseLength is in local game
/// units and ageFrames in update frames. With extendedTail zero, the rim is
/// baseLength + 16*ageFrames behind the 64-unit hub; otherwise it is
/// 2*baseLength + 256*ageFrames behind the 128-unit hub. Distance narrows to
/// s16, as do rotated/transformed vertices. Callers draw both modes together.
/// Six 40x40 cells animate by (frame offset + ageFrames) modulo six; callers
/// must keep that sum nonnegative. Rejected segments emit no packet. Borrows
/// scratch during the call and needs arena capacity for up to sixteen FT4s.
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
        selectedRimRadius = JET_CONE_RIM_SHORT;
    } else {
        tailDistance      = baseLength + (ageFrames << JET_CONE_BASE_TAIL_GROWTH_SHIFT);
        selectedHubRadius = JET_CONE_BASE_HUB_RADIUS;
        selectedRimRadius = JET_CONE_RIM_LONG;
    }
    /// Builds the two local rings into the coordinate's composed frame.
    ///
    /// Block statement macro reading coneScratch, coord, tailDistance,
    /// selectedRimRadius and selectedHubRadius. Uses this function's ring
    /// constants and writes segmentIndex, rimRadius, hubRadius, ringAngle,
    /// hubVertex and rotation. Inputs must be stable live values. Writes both
    /// rings and changes GTE matrices; coordinates/radii use game units and
    /// all vertex stores narrow to s16. Undefined immediately after its call.
#define JET_CONE_BUILD_RINGS()                                                                                                        \
    {                                                                                                                                 \
        gte_SetTransMatrix(&GsWSMATRIX);                                                                                              \
        segmentIndex = 0;                                                                                                             \
        rimRadius    = selectedRimRadius;                                                                                             \
        rotation     = &coord->workm;                                                                                                 \
        hubRadius    = selectedHubRadius;                                                                                             \
        /* Rotate the rim and hub into the composed frame, preserving s16 narrowing. */                                               \
        for (; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {                                                            \
            ringAngle                             = segmentIndex << JET_CONE_RING_ANGLE_SHIFT;                                        \
            coneScratch->topRing[segmentIndex].vx = (rsin(ringAngle) * rimRadius) >> JET_CONE_TRIG_FRACTION_BITS;                     \
            coneScratch->topRing[segmentIndex].vy = (rcos(ringAngle) * rimRadius) >> JET_CONE_TRIG_FRACTION_BITS;                     \
            coneScratch->topRing[segmentIndex].vz = -tailDistance;                                                                    \
            JET_CONE_ROTATE_VERTEX(rotation, &coneScratch->topRing[segmentIndex]);                                                    \
            coneScratch->topRing[segmentIndex].vx   += (u16)coord->workm.t[0];                                                        \
            coneScratch->topRing[segmentIndex].vy   += (u16)coord->workm.t[1];                                                        \
            coneScratch->topRing[segmentIndex].vz   += (u16)coord->workm.t[2];                                                        \
            coneScratch->bottomRing[segmentIndex].vx = (rsin(ringAngle) * hubRadius) >> JET_CONE_TRIG_FRACTION_BITS;                  \
            /* Keep the vertex address within the complete scratch object's byte view. */                                             \
            hubVertex     = (SVECTOR*)((u8*)coneScratch + segmentIndex * sizeof(SVECTOR) + OFFSET_OF(EffectBandScratch, bottomRing)); \
            hubVertex->vy = (rcos(ringAngle) * hubRadius) >> JET_CONE_TRIG_FRACTION_BITS;                                             \
            hubVertex->vz = 0;                                                                                                        \
            JET_CONE_ROTATE_VERTEX(rotation, &coneScratch->bottomRing[segmentIndex]);                                                 \
            coneScratch->bottomRing[segmentIndex].vx += (u16)coord->workm.t[0];                                                       \
            hubVertex->vy                            += (u16)coord->workm.t[1];                                                       \
            hubVertex->vz                            += (u16)coord->workm.t[2];                                                       \
        }                                                                                                                             \
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
            textureU = (s16)((JET_CONE_FRAME_JITTER[segmentIndex] + ageFrames) % JET_CONE_TEXTURE_FRAME_COUNT) * JET_CONE_TEXTURE_CELL_SIZE;
            setUV4(prim, textureU, JET_CONE_TEXTURE_V_TOP, textureU + JET_CONE_TEXTURE_UV_SPAN, JET_CONE_TEXTURE_V_TOP, textureU, JET_CONE_TEXTURE_V_TOP + JET_CONE_TEXTURE_UV_SPAN, textureU + JET_CONE_TEXTURE_UV_SPAN, JET_CONE_TEXTURE_V_TOP + JET_CONE_TEXTURE_UV_SPAN);
            setXY4(prim, coneScratch->sxy0.vx, coneScratch->sxy0.vy, coneScratch->sxy1.vx, coneScratch->sxy1.vy, coneScratch->sxy2.vx, coneScratch->sxy2.vy, coneScratch->sxy3.vx,
                   coneScratch->sxy3.vy);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)coneScratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}

#undef JET_CONE_ROTATE_VERTEX
