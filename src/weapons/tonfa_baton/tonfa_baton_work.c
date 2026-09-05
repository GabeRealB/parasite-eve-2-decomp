#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "weapons/tonfa_baton.h"

/// Zeroed work area at the very end of the package, so it is its own unit:
/// splat lists an object in the linker script at its first subsegment, and
/// this has to link after the trailing data.
GsCOORDINATE2 D_tonfa_baton_8012BBEC[8] = { 0 };
GsCOORDINATE2 D_tonfa_baton_8012BE6C[8] = { 0 };
