/* A small collision grid (4 faces, 8 corners) that follows a character's
 * model. The room's handler takes task slot 0xA, falling back to the player
 * (slot 3), and rebuilds the live grid from its pristine copy under that
 * model's root coordinate. When a game-flag nibble says the obstacle is
 * inactive, it pushes the grid 10000 units up and out of reach.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_FOLLOW_COLLISION_H
#define SRC_SHARED_FOLLOW_COLLISION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "main/coord.h"

void followCollisionRebuild(GfxCoord* coord, SVECTOR* offset);

#endif /* SRC_SHARED_FOLLOW_COLLISION_H */
