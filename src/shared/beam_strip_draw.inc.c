/* Part of the beam strip library; see beam_strip.h. */

#ifndef BEAM_STRIP_OTZ_BIAS
#define BEAM_STRIP_OTZ_BIAS 1
#endif

/// Scratch-stack workspace for one beam strip.
///
/// The drawer copies the effect coordinate's world translation into
/// `worldStart`, narrowed to signed 16-bit coordinate units, and projects it
/// and then the caller's end point through `GsWSMATRIX`, one perspective
/// transform each. Each transform writes its point's screen position and
/// replaces the GTE flag word; a negative flag word after either one drops
/// the strip. `depth` is taken from the start point alone, raised by
/// `BEAM_STRIP_OTZ_BIAS` for each projection that survives, and is both the
/// divisor that scales the caller's width onto the screen and the
/// ordering-table depth the strip is queued at.
///
/// The corner offset is that scaled width resolved against a screen angle. It
/// is filled twice: at the strip's own angle for the first and last corners of
/// the quad, then a quarter turn further for the middle two. The corners are
/// 16-bit, so only the low half of each offset word is read back.
///
/// Reserve one complete block and release it after drawing; nothing outlives
/// the call.
typedef struct {
    SVECTOR worldStart;      // Strip start in world space, each component narrowed to s16; projection input
    s32     depth;           // Start point's SZ3 / 4 plus the bias per surviving projection; width divisor and ordering-table depth
    s32     projectionFlags; // GTE FLAG word of the latest projection; bit 31 set rejects the strip
    s32     cornerOffsetX;   // Scaled width times the sine of the current corner angle, in pixels
    s32     cornerOffsetY;   // Scaled width times the cosine of the current corner angle, in pixels
    DVECTOR screenStart;     // Projected screen position of the start point
    DVECTOR screenEnd;       // Projected screen position of the caller's end point
} _BeamStripScratch;
STATIC_ASSERT_SIZEOF(_BeamStripScratch, 0x20);

/// Queues an additive textured beam parallelogram between two world-space points.
///
/// `startCoord->workm.t` must already contain the start in world-coordinate
/// units; its low 16 bits and the signed `worldEnd` are projected through
/// `GsWSMATRIX`. Both inputs are borrowed read-only until return. `worldEnd`
/// must be word-aligned with a complete readable `SVECTOR` for the GTE loads.
/// A negative GTE FLAG after either projection drops the beam without a packet.
///
/// `textureFrame` repeats modulo four: bit 0 selects a 128-texel column and
/// bit 1 a 24-texel row. `widthScale` is a signed sizing input; the corner
/// offset scale is `widthScale * 23 / depth` pixels, truncated
/// before the Q12 sine/cosine products. Depth is the start's SZ3 / 4, plus
/// one per successful projection when `BEAM_STRIP_OTZ_BIAS` is enabled (default
/// 1 for Hammer, 0 in the gallery; undefined after this fragment). Depth
/// must be nonzero when drawing. The endpoint contributes no ordering depth.
///
/// The start corners use a perpendicular and a backwards longitudinal offset;
/// the end corners use their opposites. Thus the short edges lie at 45 degrees
/// to the projected segment, before integer rounding. GPU XY fields retain
/// only the low 16 bits of each sum. Requires an initialized scratch stack with
/// one free `_BeamStripScratch` block, a current ordering table and space for
/// one `POLY_FT4` at `gGpuPrimCursor`. Scratch storage is released on every
/// path; an emitted packet stays in the frame's primitive arena for GPU use.
/// Overwrites the GTE rotation, translation, vector and projection registers.
static void _beamStripDraw(const GfxCoord* startCoord, const SVECTOR* worldEnd, s32 textureFrame, s16 widthScale)
{
    enum {
        BEAM_STRIP_CELL_WIDTH   = 128,
        BEAM_STRIP_CELL_HEIGHT  = 24,
        BEAM_STRIP_COLUMN_MASK  = 1,
        BEAM_STRIP_FRAME_MASK   = 3,
        BEAM_STRIP_TOP_V        = 208,   // First row's top texel; the second starts at 232
        BEAM_STRIP_QUARTER_TURN = 0x400, // 4096 angle units per turn
        BEAM_STRIP_TRIG_SHIFT   = 12     // rsin/rcos: 4096 is 1.0
    };
    _BeamStripScratch* scratch;
    POLY_FT4*          quad;
    s16                screenAngle;

    /// Projects one word-aligned `SVECTOR` with the GTE matrices already loaded.
    ///
    /// Writes screen pixels and the complete FLAG word; a negative flag rejects
    /// the point. Leaves SZ3 available for the caller's depth read. Each pointer
    /// argument is evaluated once, in order, and no caller identifiers are
    /// captured. The outputs must be writable word-aligned storage. Expands to
    /// several statements: invoke only within a braced block, as below. It is
    /// defined only around this drawer and is undefined before it ends.
#define BEAM_STRIP_PROJECT_POINT(worldPoint, screenPoint, projectionFlags) \
    gte_ldv0((worldPoint));                                                \
    gte_rtps();                                                            \
    gte_stsxy((screenPoint));                                              \
    gte_stflg((projectionFlags))

    // Stage the cached world translation's low halves and project the start.
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(_BeamStripScratch);
    scratch->worldStart.vx = startCoord->workm.t[0];
    scratch->worldStart.vy = startCoord->workm.t[1];
    scratch->worldStart.vz = startCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    BEAM_STRIP_PROJECT_POINT(&scratch->worldStart, &scratch->screenStart, &scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
#if BEAM_STRIP_OTZ_BIAS
        scratch->depth++;
#endif
        BEAM_STRIP_PROJECT_POINT(worldEnd, &scratch->screenEnd, &scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
#if BEAM_STRIP_OTZ_BIAS
            scratch->depth++;
#endif
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            // Use raw texture colour; only texture colours with bit 15 blend.
            setPolyFT4(quad);
            setSemiTrans(quad, 1);
            setShadeTex(quad, 1);
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 512, 0);
            quad->clut  = getClut(112, 266);
            // Subtract wrapped V origins; packet bytes retain the inclusive UVs.
            quad->u0 = (textureFrame & BEAM_STRIP_COLUMN_MASK) * BEAM_STRIP_CELL_WIDTH;
            quad->v0 = ((u32)(textureFrame & BEAM_STRIP_FRAME_MASK) >> 1) * BEAM_STRIP_CELL_HEIGHT - (256 - BEAM_STRIP_TOP_V);
            quad->u1 = ((textureFrame & BEAM_STRIP_COLUMN_MASK) * BEAM_STRIP_CELL_WIDTH) + (BEAM_STRIP_CELL_WIDTH - 1);
            quad->v1 = ((u32)(textureFrame & BEAM_STRIP_FRAME_MASK) >> 1) * BEAM_STRIP_CELL_HEIGHT - (256 - BEAM_STRIP_TOP_V);
            quad->u2 = (textureFrame & BEAM_STRIP_COLUMN_MASK) * BEAM_STRIP_CELL_WIDTH;
            quad->v2 = ((u32)(textureFrame & BEAM_STRIP_FRAME_MASK) >> 1) * BEAM_STRIP_CELL_HEIGHT - (256 - BEAM_STRIP_TOP_V - (BEAM_STRIP_CELL_HEIGHT - 1));
            quad->u3 = ((textureFrame & BEAM_STRIP_COLUMN_MASK) * BEAM_STRIP_CELL_WIDTH) + (BEAM_STRIP_CELL_WIDTH - 1);
            quad->v3 = ((u32)(textureFrame & BEAM_STRIP_FRAME_MASK) >> 1) * BEAM_STRIP_CELL_HEIGHT - (256 - BEAM_STRIP_TOP_V - (BEAM_STRIP_CELL_HEIGHT - 1));
            // Opposite corners use perpendicular offsets, then longitudinal ones.
            screenAngle            = ratan2(scratch->screenEnd.vy - scratch->screenStart.vy, scratch->screenEnd.vx - scratch->screenStart.vx);
            scratch->cornerOffsetX = (((widthScale * (BEAM_STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rsin(screenAngle)) >> BEAM_STRIP_TRIG_SHIFT;
            scratch->cornerOffsetY = (((widthScale * (BEAM_STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rcos(screenAngle)) >> BEAM_STRIP_TRIG_SHIFT;
            quad->x0               = (u16)scratch->screenStart.vx + (u16)scratch->cornerOffsetX;
            quad->x3               = (u16)scratch->screenEnd.vx - (u16)scratch->cornerOffsetX;
            quad->y0               = (u16)scratch->screenStart.vy - (u16)scratch->cornerOffsetY;
            quad->y3               = (u16)scratch->screenEnd.vy + (u16)scratch->cornerOffsetY;
            scratch->cornerOffsetX = (((widthScale * (BEAM_STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rsin(screenAngle + BEAM_STRIP_QUARTER_TURN)) >> BEAM_STRIP_TRIG_SHIFT;
            scratch->cornerOffsetY = (((widthScale * (BEAM_STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rcos(screenAngle + BEAM_STRIP_QUARTER_TURN)) >> BEAM_STRIP_TRIG_SHIFT;
            quad->x1               = (u16)scratch->screenEnd.vx + (u16)scratch->cornerOffsetX;
            quad->x2               = (u16)scratch->screenStart.vx - (u16)scratch->cornerOffsetX;
            quad->y1               = (u16)scratch->screenEnd.vy - (u16)scratch->cornerOffsetY;
            quad->y2               = (u16)scratch->screenStart.vy + (u16)scratch->cornerOffsetY;
            // Sort by the start depth, including the carrier's projection bias.
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_BeamStripScratch);

#undef BEAM_STRIP_PROJECT_POINT
}

#undef BEAM_STRIP_OTZ_BIAS
