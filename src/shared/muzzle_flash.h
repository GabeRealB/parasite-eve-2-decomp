/* Included muzzle-flash implementation for the MP5A5 variants and P229:
 * a spinning textured core, an additive screen tint, four Gouraud streaks
 * and a transient white point light.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The task is an entry point gameplay's effect table names by
 * address, so each package keeps its own name for it and calls the inline
 * body. All shared functions have static per-instance linkage. The flash's
 * data belongs to the package, which defines it at its own
 * positions under these names:
 *
 *   SVECTOR _gMuzzleOffset         the muzzle in the spawning coordinate's frame
 *   s16     gMuzzleFlashAngles[4]  the streak angles rolled on the first frame
 */

#ifndef SRC_SHARED_MUZZLE_FLASH_H
#define SRC_SHARED_MUZZLE_FLASH_H

#include "main/coord.h"

enum {
    MUZZLE_FLASH_MIN_DEPTH          = 0x11, // Minimum GTE SZ3 / 4 accepted by both drawers
    MUZZLE_FLASH_TRIG_FRACTION_BITS = 12,   // rsin/rcos amplitudes have 12 fractional bits
    MUZZLE_FLASH_ANGLE_QUARTER_TURN = 0x400,
    MUZZLE_FLASH_ANGLE_MASK         = 0xFFF // Angles use 4096 units per turn
};

static void _muzzleFlashDrawCore(const GfxCoord* muzzleCoord, s16 worldSize, s16 spinAngle);
static void _muzzleFlashDrawStreak(const GfxCoord* muzzleCoord, s16 directionAngle, s16 brightness);

#endif /* SRC_SHARED_MUZZLE_FLASH_H */
