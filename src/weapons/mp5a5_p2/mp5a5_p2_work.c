#include "common.h"

#include "weapons/mp5a5_p2.h"

/// Zeroed work area at the very end of the package, so it is its own unit:
/// splat lists an object in the linker script at its first subsegment, and
/// this has to link after the trailing data.
s16 D_mp5a5_p2_8012B508[4] = { 0, 0, 0, 0 };
