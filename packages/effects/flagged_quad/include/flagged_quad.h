/* A quad drawn from four projected corners whose transforms are checked: its scratch block. */
#ifndef EFFECTS_FLAGGED_QUAD_FLAGGED_QUAD_H
#define EFFECTS_FLAGGED_QUAD_FLAGGED_QUAD_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <core/pe2.h>

/// Scratch block for a quad whose four corners are projected straight into its packet, keeping the GTE flag word to
/// drop the quad when a transform fails.
typedef struct {
    s32     otz;        // Last projected corner's SZ3 / 4, with the drawer's bias
    s32     flag;       // GTE flag word; negative when a transform failed
    SVECTOR corners[4]; // Corners as local offsets, then world positions to project
} FlaggedQuadScratch;

#endif // EFFECTS_FLAGGED_QUAD_FLAGGED_QUAD_H
