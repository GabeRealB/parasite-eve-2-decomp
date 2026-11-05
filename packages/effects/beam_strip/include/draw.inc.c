/* Part of the beam strip library; see beam_strip.h. */

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include <core/pe2.h>
#include <math/gte.h>

#include <engine/coord.h>
#include <engine/display.h>
#include <engine/display_types.h>
#include <engine/scratch.h>

#include "beam_strip.h"

#ifndef BEAM_STRIP_INCREMENT_DEPTH
/// Nonzero (the default) adds one to the start's depth after each end is projected, two in all, before sizing and
/// sorting.
#define BEAM_STRIP_INCREMENT_DEPTH 1
#endif

/// Scratch block for one beam: its start point, both projected ends and the current corner offset.
typedef struct {
    SVECTOR worldStart;      // Start in world space
    s32     depth;           // Start's SZ3 / 4: the width's divisor and the sorting depth
    s32     projectionFlags; // GTE flag word of the latest projection; negative drops the beam
    s32     cornerOffsetX;   // Corner offset in pixels: the scaled width times the angle's sine
    s32     cornerOffsetY;   // The scaled width times the angle's cosine
    DVECTOR screenStart;     // Projected start
    DVECTOR screenEnd;       // Projected end
} _BeamStripScratch;

/// Draws an additive textured beam from the coordinate's translation to `worldEnd`, sorted at the start's depth; either
/// end failing to project drops it. `textureFrame` cycles its four cells, `widthScale` sets its width; its short edges
/// lie at 45 degrees to the beam.
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

/// Projects `worldPoint` to `screenPoint`, storing the GTE flag word in `projectionFlags`.
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
#if BEAM_STRIP_INCREMENT_DEPTH
        scratch->depth++;
#endif
        BEAM_STRIP_PROJECT_POINT(worldEnd, &scratch->screenEnd, &scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
#if BEAM_STRIP_INCREMENT_DEPTH
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
            // Sort by the start depth after the configured unit increments.
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_BeamStripScratch);

#undef BEAM_STRIP_PROJECT_POINT
}

#undef BEAM_STRIP_INCREMENT_DEPTH
