#ifndef GAMEPLAY_3E9C_H
#define GAMEPLAY_3E9C_H

#include "common.h"

/// Draws the ground shadow under an object: a flat textured quad whose corners
/// are the unit corner table scaled by `size` and rotated by `Gfx_ViewWorldMtx`,
/// centred on `pos`, and drawn with subtractive blending. `shade` is the
/// vertex colour, with 0 drawing the texture unmodulated and a negative value
/// drawing nothing; nothing is drawn either once `Gp_State1C->eventState`
/// reaches 2.
void Gp_DrawEffGroundQuad(VECTOR3* pos, s32 size, s16 shade);

#endif
