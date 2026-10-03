#ifndef GAMEPLAY_PRIVATE_GEOMETRY_H
#define GAMEPLAY_PRIVATE_GEOMETRY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// Scratch-stack workspace for a 3D point and its screen projection.
///
/// The caller supplies the GTE rotation and translation matrices. The screen
/// position may then be clamped or eased for presentation. Reserve the complete
/// record on the scratch stack; none of its members survive the matching pop.
typedef struct {
    SVECTOR point;           // Input point in the current GTE matrices' coordinate space
    s32     depthCue;        // GTE IR0 depth-cue coefficient, with 12 fractional bits
    s32     projectionFlags; // GTE FLAG bits; negative when the summary error bit is set
    s32     orderingDepth;   // Quarter camera-space depth from SZ3 (0..16383)
    DVECTOR screen;          // Signed screen pixels; X/Y are written together by the GTE
} WorldCoordProjectionScratch;
STATIC_ASSERT_SIZEOF(WorldCoordProjectionScratch, 0x18);

#endif // GAMEPLAY_PRIVATE_GEOMETRY_H
