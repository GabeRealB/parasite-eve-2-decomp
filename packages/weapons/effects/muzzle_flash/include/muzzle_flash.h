/* The muzzle flash of the MP5A5, its upgrades and the P229: a spinning textured core, an additive screen tint, four
 * Gouraud streaks and a brief white point light.
 *
 * task.inc.c holds the flash's task and its draws; gameplay's effect table names the task by address, so each carrier
 * exports `_muzzleFlashTask` under its own name with DEFINE_ALIAS. Before including it the carrier defines:
 *
 *   static SVECTOR _gMuzzleFlashOffset, the flash's position from the weapon's root, in its model units;
 *   static s16 _gMuzzleFlashAngles[MUZZLE_FLASH_STREAK_COUNT], the streaks' directions, one per quadrant in
 *   4096 units per turn, drawn anew as each flash starts.
 */

#ifndef WEAPONS_MUZZLE_FLASH_H
#define WEAPONS_MUZZLE_FLASH_H

#include <engine/coord.h>

enum {
    MUZZLE_FLASH_STREAK_COUNT       = 4,    // One per quadrant; a power of two, as indices are masked by count - 1
    MUZZLE_FLASH_MIN_DEPTH          = 0x11, // Nearest depth (SZ3 / 4) either drawer draws at
    MUZZLE_FLASH_TRIG_FRACTION_BITS = 12,   // rsin/rcos return 12 fractional bits
    MUZZLE_FLASH_ANGLE_QUARTER_TURN = 0x400,
    MUZZLE_FLASH_ANGLE_MASK         = 0xFFF // 4096 units per turn
};

static void _muzzleFlashDrawCore(const GfxCoord* muzzleCoord, s16 worldSize, s16 spinAngle);
static void _muzzleFlashDrawStreak(const GfxCoord* muzzleCoord, s16 directionAngle, s16 brightness);

#endif /* WEAPONS_MUZZLE_FLASH_H */
