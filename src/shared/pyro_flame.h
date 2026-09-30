/* The flame sprite of the fire PE spells (Pyrokinesis, Combustion): one
 * animation frame of a textured, semi-transparent flame quad at a coordinate's
 * world position, spun and scaled with depth.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_PYRO_FLAME_H
#define SRC_SHARED_PYRO_FLAME_H

#include "types.h"

#include "main/coord.h"

void pyroFlameDrawSprite(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);

#endif /* SRC_SHARED_PYRO_FLAME_H */
