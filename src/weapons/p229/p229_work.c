#include "common.h"

#include "weapons/p229.h"

/// The four flash angles rolled on the frame the shot goes off, one per
/// `WeaponsShared8011d864` quad. Each is a fixed quadrant (`i << 10`) plus a
/// 10-bit LCG jitter, so the four quads always fan out around the muzzle.
///
/// Zeroed work area at the very end of the package, so it is its own unit:
/// splat lists an object in the linker script at its first subsegment, and
/// this has to link after the trailing data.
s16 D_p229_8012B658[4] = { 0, 0, 0, 0 };
