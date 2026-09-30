#include "mp5a5_private.h"

#include "types.h"
#include "../../shared/muzzle_flash.h"

/// Zeroed work area at the very end of the package, so it is its own unit:
/// splat lists an object in the linker script at its first subsegment, and
/// this has to link after the trailing data.
s16 gMuzzleFlashAngles[4] = { 0, 0, 0, 0 };
