/* The sprites of the M4A1's Pyke flamethrower attachment, also carried by
 * actor_800100's copy of the weapon. One is the flame at the nozzle: a six-
 * cell animated strip billboarded at a world point. The other is the flying
 * flame: a spinning, widening billboard that the fireball task draws each
 * frame as it flies, falls and ricochets.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_PYKE_FLAME_H
#define SRC_SHARED_PYKE_FLAME_H

#include "types.h"

void pykeFlameDrawNozzle(VECTOR3* pos, u16 frame, s32 brightness);
void pykeFlameDrawBlob(VECTOR3* pos, u16 frame, u16 width, s16 ang);

#endif /* SRC_SHARED_PYKE_FLAME_H */
