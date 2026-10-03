/* The muzzle flash of the MP5A5 and the P229: a short-lived task that lights
 * the room from the muzzle and draws a spinning textured core, a full-screen
 * fade and four Gouraud streaks fanning out around the barrel.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The task is an entry point gameplay's effect table names by
 * address, so each package keeps its own name for it and calls the inline
 * body. The flash's data belongs to the package, which defines it at its own
 * positions under these names:
 *
 *   SVECTOR _gMuzzleOffset         the muzzle in the firing hand's frame
 *   s16     gMuzzleFlashAngles[4]  the streak angles rolled on the first frame
 */

#ifndef SRC_SHARED_MUZZLE_FLASH_H
#define SRC_SHARED_MUZZLE_FLASH_H

#include <psyq/sys/types.h>

#include "common.h"

#include "gameplay/effects.h"

#include "main/coord.h"
#include "main/task_types.h"

void muzzleFlashDrawCore(GfxCoord* arg0, s16 arg1, s16 arg2);
void muzzleFlashDrawStreak(GfxCoord* arg0, s16 arg1, s16 arg2);

#endif /* SRC_SHARED_MUZZLE_FLASH_H */
