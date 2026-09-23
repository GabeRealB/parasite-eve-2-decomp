#ifndef ROOMS_DRYFIELD_TOILET_H
#define ROOMS_DRYFIELD_TOILET_H

#include "common.h"

#include <psyq/libgte.h>

/// Per-frame scratch the room's animated billboard task reserves off
/// `G_SCRATCH_HEAD`. `vec` is the task coordinate's world translation, projected
/// through `GsWSMATRIX` into `sxy` with `otz` as the resulting depth. `dx` / `dy`
/// are the rotated half-extents that offset `sxy` into the quad's corners, so
/// the sprite shrinks with distance. The same layout appears in other rooms'
/// billboard tasks; whether they are one type is unsettled.
typedef struct DryfieldToiletSpriteScratch {
    s32     otz;
    s32     dx;
    s32     dy;
    SVECTOR vec;
    DVECTOR sxy;
} DryfieldToiletSpriteScratch;
STATIC_ASSERT_SIZEOF(DryfieldToiletSpriteScratch, 0x18);

#endif // ROOMS_DRYFIELD_TOILET_H
