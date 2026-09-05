#ifndef M4A1_HAMMER_H
#define M4A1_HAMMER_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// Parent task the hammer effect re-attaches itself to each time it restarts.
extern Task* D_m4a1_hammer_8012D660;
/// Offset vector handed to the `func_m4a1_hammer_8011E29C` sprite draw.
extern SVECTOR D_m4a1_hammer_8012D668;

/// Fixed offset from the parent coordinate that the hammer effect starts at.
extern SVECTOR D_m4a1_hammer_8011EB60;
/// Jitter table for the eight sparks the charged hammer throws: `[0..7]` are
/// the spin angles, `[8..15]` the heights and `[16..23]` the radii. Reseeded
/// from `Gp_LcgState` on the first charge frame and walked every other frame.
extern s16 D_m4a1_hammer_8012D630[24];

/// Handwritten GTE routine. Draws the charging flare around the world position
/// `arg0` (a `GsCOORDINATE2::workm` translation); `arg1` is the animation
/// frame, `arg2` the radius and `arg3` the spin angle.
void func_m4a1_hammer_8011D904(s32* arg0, u16 arg1, u16 arg2, s16 arg3);
/// Handwritten GTE routine. Draws the hammer's expanding billboard at `arg0`'s
/// projected position: `arg1` is the animation frame, `arg2` the radius and
/// `arg3` the spin angle.
void func_m4a1_hammer_8011DE60(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
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

/// Handwritten GTE routine. Draws one semi-transparent `POLY_FT4` stretched
/// between `coord`'s world position and `arg1`, the offset endpoint the hammer
/// effect keeps in its data. Both points are projected with their own `RTPS`
/// and the quad is given a half-width of `arg3 * 23 / otz`, rotated onto the
/// strip's own screen-space angle so it stays perpendicular to it. `arg2`
/// selects the strip out of the texture page: bit 0 picks the left or right
/// half and bit 1 the upper or lower row. Nothing is drawn if either endpoint
/// projects off-screen.
void func_m4a1_hammer_8011E29C(GsCOORDINATE2* coord, SVECTOR* arg1, s32 arg2, s16 arg3);

#endif
