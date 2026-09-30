/* Small GfxCoord helpers actors carry privately: one rebuilds a coordinate's
 * rotation as its bare yaw at a uniform scale, the other carries a point from
 * a coordinate's local space up its parent chain into world space.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_COORD_MATH_H
#define SRC_SHARED_COORD_MATH_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "main/coord.h"

void coordSetYawScale(GfxCoord* coord, s16 scale);
s32  coordLocalToWorld(GfxCoord* coord, SVECTOR* pos);

#endif /* SRC_SHARED_COORD_MATH_H */
