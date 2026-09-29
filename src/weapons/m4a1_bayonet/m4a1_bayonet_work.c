#include "m4a1_bayonet_private.h"

#include "main/coord.h"

/// Zeroed work area at the very end of the package, so it is its own unit:
/// splat lists an object in the linker script at its first subsegment, and
/// this has to link after the trailing data.
GpCoord D_m4a1_bayonet_8012D398[8] = { 0 };
GpCoord D_m4a1_bayonet_8012D618[8] = { 0 };
