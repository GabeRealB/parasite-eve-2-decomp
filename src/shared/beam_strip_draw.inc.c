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

static void beamStripDraw(GfxCoord* coord, SVECTOR* end, s32 cell, s16 width)
{
    _BeamStripScratch* head;
    _BeamStripScratch* scratch;
    _BeamStripScratch* projectionInput;
    POLY_FT4*          prim;
    s16                ang;
    u16                vz;

    // Reserve the block below the cursor and fill the start point on the way.
    head                                    = SCRATCH_STACK_CURSOR(_BeamStripScratch);
    head[-1].worldStart.vx                  = (u16)coord->workm.t[0];
    scratch                                 = head - 1;
    scratch->worldStart.vy                  = (u16)coord->workm.t[1];
    vz                                      = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(_BeamStripScratch) = scratch;
    scratch->worldStart.vz                  = vz;
    projectionInput                         = scratch;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projectionInput->worldStart);
    gte_rtps();
    gte_stsxy(&head[-1].screenStart);
    gte_stflg(&head[-1].projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&head[-1].depth);
#if BEAM_STRIP_OTZ_BIAS
        scratch->depth++;
#endif
        gte_ldv0(end);
        gte_rtps();
        gte_stsxy(&head[-1].screenEnd);
        gte_stflg(&head[-1].projectionFlags);
        if (scratch->projectionFlags >= 0) {
#if BEAM_STRIP_OTZ_BIAS
            scratch->depth++;
#endif
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage            = 0x28;
            prim->clut             = 0x4287;
            prim->u0               = (cell & 1) << 7;
            prim->v0               = ((u32)(cell & 3) >> 1) * 24 - 0x30;
            prim->u1               = ((cell & 1) << 7) + 0x7F;
            prim->v1               = ((u32)(cell & 3) >> 1) * 24 - 0x30;
            prim->u2               = (cell & 1) << 7;
            prim->v2               = ((u32)(cell & 3) >> 1) * 24 - 0x19;
            prim->u3               = ((cell & 1) << 7) + 0x7F;
            prim->v3               = ((u32)(cell & 3) >> 1) * 24 - 0x19;
            ang                    = ratan2(scratch->screenEnd.vy - scratch->screenStart.vy, scratch->screenEnd.vx - scratch->screenStart.vx);
            scratch->cornerOffsetX = (((width * 23) / scratch->depth) * rsin(ang)) >> 12;
            scratch->cornerOffsetY = (((width * 23) / scratch->depth) * rcos(ang)) >> 12;
            prim->x0               = (u16)scratch->screenStart.vx + (u16)scratch->cornerOffsetX;
            prim->x3               = (u16)scratch->screenEnd.vx - (u16)scratch->cornerOffsetX;
            prim->y0               = (u16)scratch->screenStart.vy - (u16)scratch->cornerOffsetY;
            prim->y3               = (u16)scratch->screenEnd.vy + (u16)scratch->cornerOffsetY;
            scratch->cornerOffsetX = (((width * 23) / scratch->depth) * rsin(ang + 0x400)) >> 12;
            scratch->cornerOffsetY = (((width * 23) / scratch->depth) * rcos(ang + 0x400)) >> 12;
            prim->x1               = (u16)scratch->screenEnd.vx + (u16)scratch->cornerOffsetX;
            prim->x2               = (u16)scratch->screenStart.vx - (u16)scratch->cornerOffsetX;
            prim->y1               = (u16)scratch->screenEnd.vy - (u16)scratch->cornerOffsetY;
            prim->y2               = (u16)scratch->screenStart.vy + (u16)scratch->cornerOffsetY;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_BeamStripScratch);
}

#undef BEAM_STRIP_OTZ_BIAS
