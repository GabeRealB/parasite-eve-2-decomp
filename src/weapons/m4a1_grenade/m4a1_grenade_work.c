#include "common.h"

#include "weapons/m4a1_grenade.h"

/// Sits in the middle of this package's trailing data, so it is its own unit:
/// splat lists an object in the linker script at its first subsegment, and
/// this has to link between the two runs of split data around it.
u16 D_m4a1_grenade_8012E08C[4] = { 0x1F4, 0x4B0, 0x7D0, 0 };
