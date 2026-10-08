/* The flying fireball some enemies throw. It lights the room with a flickering
 * orange point light and draws two additive glow billboards that alternate
 * textures each frame. When ground tracing is on, it also draws a glow quad on
 * the floor below. An ember spawner that drifts sparks outward is kept even
 * though nothing calls it.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_FIREBALL_H
#define SRC_SHARED_FIREBALL_H

#include "types.h"

#include "main/coord.h"

static void _fireballDrawGlow(const GfxCoord* coord, s16 halfExtent);
static void _fireballDrawGroundGlow(const GfxCoord* coord, s32 halfExtent);
static void _fireballSpawnEmber(GfxCoord* coord, s32 spawnArgBits);

#endif /* SRC_SHARED_FIREBALL_H */
