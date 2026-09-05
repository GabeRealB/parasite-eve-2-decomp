#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "weapons/gunblade.h"

/// Zeroed work area at the very end of the package, so it is its own unit:
/// splat lists an object in the linker script at its first subsegment, and
/// this has to link after the trailing data.

/// The running beam task and its `GpEffWork`, cached on entry to state 0 so
/// `func_gunblade_8011E008` can reach them from outside the task.
Task*      D_gunblade_8012E244 = NULL;
GpEffWork* D_gunblade_8012E248 = NULL;

/// Nothing reads the two words after the work pointer; they keep the offset of
/// the trails that follow.
static s32 s_unused_8012E24C[2] = { 0, 0 };

/// The eight-segment beam trails, one array per end of the blade.
GsCOORDINATE2 D_gunblade_8012E254[8] = { 0 };
GsCOORDINATE2 D_gunblade_8012E4D4[8] = { 0 };
