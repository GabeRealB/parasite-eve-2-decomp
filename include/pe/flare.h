#ifndef PE_FLARE_H
#define PE_FLARE_H

#include "common.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>
#include "gameplay/3CD8.h"

/// Draws one frame of a spark's sprite at a world position.
///
/// The position is projected to the screen and the quad dropped if it lands
/// behind the camera. `arg1` picks one of the eight texture frames, `arg3`
/// spins the quad and `arg2` sizes it, with the size divided by depth so the
/// sprite shrinks into the distance.
void flareDrawSparkQuad(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);

#endif /* PE_FLARE_H */
