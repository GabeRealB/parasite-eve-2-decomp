/* Included room-local actor-following collision obstacle.
 *
 * Include this header in the prologue and the rebuild fragment at its original
 * function position. Each carrier supplies gFollowCollisionSource and
 * gFollowCollisionGrid before including the fragment: the former holds the
 * obstacle's local geometry, the latter the room grid whose leading entries
 * reserve space for that obstacle.
 */

#ifndef SRC_SHARED_FOLLOW_COLLISION_H
#define SRC_SHARED_FOLLOW_COLLISION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "main/coord.h"

static void _followCollisionRebuildObstacle(const GfxCoord* modelRoot, const SVECTOR* roomOffset);

#endif /* SRC_SHARED_FOLLOW_COLLISION_H */
