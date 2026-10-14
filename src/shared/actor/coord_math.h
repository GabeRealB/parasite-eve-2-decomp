/* Actor-render point transform with a static instance in each carrier.
 *
 * Include this header in the prologue and coord_math_local_to_world.inc.c at
 * the function's position. The uncalled yaw-scale fragment needs no prototype.
 */

#ifndef SRC_SHARED_COORD_MATH_H
#define SRC_SHARED_COORD_MATH_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "main/coord.h"

static s32 _actorRenderTransformPointToWorld(const GfxCoord* startCoord, SVECTOR* point);

#endif /* SRC_SHARED_COORD_MATH_H */
