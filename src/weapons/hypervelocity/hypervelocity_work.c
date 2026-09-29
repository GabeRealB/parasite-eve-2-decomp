#include "hypervelocity_private.h"

#include "types.h"

/// Zeroed work area at the very end of the package, so it is its own unit:
/// splat lists an object in the linker script at its first subsegment, and
/// this has to link after the trailing data.
s16 D_hypervelocity_8012EF0C[16] = { 0 };
