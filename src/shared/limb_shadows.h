/* Horizontal limb shadows drawn from pairs of model parts. Each carrier
 * includes its own private drawer.
 *
 * Include this header in the prologue and the segment fragment at its
 * function's position.
 */

#ifndef SRC_SHARED_LIMB_SHADOWS_H
#define SRC_SHARED_LIMB_SHADOWS_H

#include "types.h"

#include "main/task_types.h"

static void _limbShadowDrawSegment(Task* actor, s16 firstJoint, s16 secondJoint, s16 halfWidth, s16 worldY, u8 shade);

#endif /* SRC_SHARED_LIMB_SHADOWS_H */
