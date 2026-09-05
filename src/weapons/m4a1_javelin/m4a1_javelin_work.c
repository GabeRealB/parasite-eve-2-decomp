#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "weapons/m4a1_javelin.h"

/// Zeroed work area at the very end of the package, so it is its own unit:
/// splat lists an object in the linker script at its first subsegment, and
/// this has to link after the trailing data.
u16     D_m4a1_javelin_8012EB60 = 0;
u16     D_m4a1_javelin_8012EB62 = 0;
s16     D_m4a1_javelin_8012EB64 = 0;
s16     D_m4a1_javelin_8012EB66 = 0;
SVECTOR D_m4a1_javelin_8012EB68 = { 0, 0, 0, 0 };
s32     D_m4a1_javelin_8012EB70 = 0;
