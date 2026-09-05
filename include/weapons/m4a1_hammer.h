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
/// Handwritten GTE routine. Draws a trailing sprite offset from `arg0` by the
/// vector `arg1`; `arg2` is the animation frame and `arg3` the radius.
void func_m4a1_hammer_8011E29C(GsCOORDINATE2* arg0, SVECTOR* arg1, s16 arg2, s32 arg3);

#endif
