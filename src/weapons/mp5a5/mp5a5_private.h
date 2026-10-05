#ifndef SRC_WEAPONS_MP5A5_MP5A5_PRIVATE_H
#define SRC_WEAPONS_MP5A5_MP5A5_PRIVATE_H

#include "types.h"

/// The four flash angles rolled on the frame the shot goes off, one per
/// `_muzzleFlashDrawStreak` quad. Each is a fixed quadrant (`i << 10`) plus a
/// 10-bit LCG jitter, so the four quads always fan out around the muzzle.
extern s16 gMuzzleFlashAngles[4];

#endif // SRC_WEAPONS_MP5A5_MP5A5_PRIVATE_H
