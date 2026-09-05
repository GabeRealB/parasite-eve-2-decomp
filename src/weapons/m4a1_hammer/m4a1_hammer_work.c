#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "weapons/m4a1_hammer.h"

/// Zeroed work area at the very end of the package, so it is its own unit:
/// splat lists an object in the linker script at its first subsegment, and
/// this has to link after the trailing data.

/// Jitter table for the eight sparks the charged hammer throws: `[0..7]` are
/// the spin angles, `[8..15]` the heights and `[16..23]` the radii.
s16 D_m4a1_hammer_8012D630[24] = { 0 };

/// Parent task the hammer effect re-attaches itself to each time it restarts.
Task* D_m4a1_hammer_8012D660 = NULL;

/// Nothing reads the word after the task pointer; it keeps the offset of the
/// vector that follows.
static s32 s_unused_8012D664 = 0;

/// Offset vector handed to the `func_m4a1_hammer_8011E29C` sprite draw.
SVECTOR D_m4a1_hammer_8012D668 = { 0, 0, 0, 0 };
