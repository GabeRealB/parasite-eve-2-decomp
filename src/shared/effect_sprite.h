/* Drawing of the Shelter rooms' animated effect sprites as camera-facing
 * textured quads at a coordinate's world position, rotated and scaled by depth.
 * effectSpriteRiseTask is the plainer rising sprite of the pod rooms: eight
 * cells drawn through gameplay's Gp_DrawFxQuad in one of six random CLUTs.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_EFFECT_SPRITE_H
#define SRC_SHARED_EFFECT_SPRITE_H

#include "types.h"

#include "main/coord.h"

void effectSpriteDrawChip(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3);

// Select the carrier's billboard signature before the first inclusion: either
// halfword (u16, s16) or word (s32, s32) arguments. Drift-only rooms define neither.
#ifdef EFFECT_SPRITE_BILLBOARD_HALFWORD_ARGUMENTS
void effectSpriteDrawBillboard(GfxCoord* coord, u16 frame, s16 size);
#elif defined(EFFECT_SPRITE_BILLBOARD_WORD_ARGUMENTS)
void effectSpriteDrawBillboard(GfxCoord* arg0, s32 arg1, s32 arg2);
#endif

void effectSpriteDriftTaskAimed(Task* task);
void effectSpriteDebrisTask(Task* task);

void effectSpriteRiseTask(Task* task);

#endif /* SRC_SHARED_EFFECT_SPRITE_H */
