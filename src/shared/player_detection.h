/* Enemy tests of whether the player can be engaged. A line-of-sight test
 * checks for a wall between the actor's and the player's head-height points. A
 * reach test checks whether the player is outside the actor's facing arc or
 * too far from a point ahead of it. A segment-versus-wall test is carried as a
 * package-private copy of gameplay's func_800E0308. Packages include only the
 * tests they carry.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_PLAYER_DETECTION_H
#define SRC_SHARED_PLAYER_DETECTION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "main/coord.h"
#include "main/task_types.h"

s32 detectSightBlocked(Task* arg0);
s32 detectPlayerOutOfReach(GfxCoord* coord, s16 range, s16 offset);
s32 detectSegmentHitsWall(SVECTOR* arg0, SVECTOR* arg1);

#endif /* SRC_SHARED_PLAYER_DETECTION_H */
