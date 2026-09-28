#ifndef M4A1_HAMMER_H
#define M4A1_HAMMER_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include "main/coord.h"

#include "main/task.h"

/// Parent task the hammer effect re-attaches itself to each time it restarts.
extern Task* D_m4a1_hammer_8012D660;
/// Offset vector handed to the `func_m4a1_hammer_8011E29C` sprite draw.
extern SVECTOR D_m4a1_hammer_8012D668;

/// Jitter table for the eight sparks the charged hammer throws: `[0..7]` are
/// the spin angles, `[8..15]` the heights and `[16..23]` the radii. Reseeded
/// from `Gp_LcgState` on the first charge frame and walked every other frame.
extern s16 D_m4a1_hammer_8012D630[24];

/// 0x20-byte scratch block `func_m4a1_hammer_8011E29C` carves off
/// `G_SCRATCH_HEAD` for the hammer's shock trail.
///
/// `vec` is the effect coordinate's world position (`workm.t`) truncated to
/// s16; it and the caller's endpoint `SVECTOR` are projected by one `RTPS`
/// each, filling `sxy0` / `sxy1` through `gte_stsxy`. `flag` is `gte_stflg` of
/// whichever projection just ran - both are tested, so an off-screen endpoint
/// drops the whole strip - and `otz` is `gte_stszotz` of the first point,
/// bumped once per surviving projection so it serves as both the divisor of
/// the strip's half-width and the OT index the primitive is queued at. `dx` /
/// `dy` are that half-width rotated by `(size * 23 / otz) * rsin|rcos(angle)
/// >> 12`, applied once at the strip's own screen angle and once at 90 degrees
/// to it to give the `POLY_FT4` its four corners.
typedef struct _M4a1HammerTrailScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ DVECTOR sxy0;
    /* 0x1C */ DVECTOR sxy1;
} M4a1HammerTrailScratch;
STATIC_ASSERT_SIZEOF(M4a1HammerTrailScratch, 0x20);

#endif
