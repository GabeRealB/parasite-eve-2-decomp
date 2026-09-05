#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "weapons/m4a1_bayonet.h"

/// Zeroed work area at the very end of the package, so it is its own unit:
/// splat lists an object in the linker script at its first subsegment, and
/// this has to link after the trailing data.
GsCOORDINATE2 D_m4a1_bayonet_8012D398[8] = { 0 };
GsCOORDINATE2 D_m4a1_bayonet_8012D618[8] = { 0 };
