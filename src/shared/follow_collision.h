/* Included room-local actor-following collision obstacle.
 *
 * Include this header in the prologue and the rebuild fragment at its original
 * function position. Each carrier supplies gFollowCollisionSource, the
 * obstacle's local geometry, and binds FOLLOW_COLLISION_ROOM_GRID to its
 * mutable WorldCollisionGrid object before including the rebuild fragment.
 * That grid contains the whole room, with leading faces, normals and vertices
 * reserved for the obstacle. The carrier undefines the binding afterwards.
 */

#ifndef SRC_SHARED_FOLLOW_COLLISION_H
#define SRC_SHARED_FOLLOW_COLLISION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "main/coord.h"

static void _followCollisionRebuildObstacle(const GfxCoord* modelRoot, const SVECTOR* roomOffset);

#endif /* SRC_SHARED_FOLLOW_COLLISION_H */
