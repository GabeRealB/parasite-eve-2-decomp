#ifndef GAMEPLAY_EFFECT_TASKS_H
#define GAMEPLAY_EFFECT_TASKS_H

#include "types.h"

#include "gameplay/effects.h"

#include "main/coord.h"
#include "main/task_types.h"

// Effect task entry points and shared drawing data.

extern GpEffUv8 D_80111E48[];

/// Unit quad corners `(-1, 1)`, `(1, 1)`, `(-1, -1)`, `(1, -1)`.
extern GpQuadCorner D_80111E38[4];

/// Draws the ground shadow under an object: a flat textured quad whose corners
/// are the unit corner table scaled by `size` and rotated by `gGfxViewCoord.workm`,
/// centred on `pos`, and drawn with subtractive blending. `shade` is the
/// vertex colour, with 0 drawing the texture unmodulated and a negative value
/// drawing nothing; nothing is drawn either once `Gp_State1C->eventState`
/// reaches 2.
void Gp_DrawEffGroundQuad(VECTOR3* pos, s32 size, s16 shade);

void Gp_DrawEffSprite7C(GpCoord* arg0, s32 arg1, u32 arg2);

extern TaskDesc D_80114B34[6];

void Gp_EffAttachTask37(Task* arg0);

#endif // GAMEPLAY_EFFECT_TASKS_H
