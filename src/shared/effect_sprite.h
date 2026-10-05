/* Room sprite effects: animated debris, drifting sprites and rising sprites,
 * with camera-facing chip and upright-billboard drawers. Included fragments
 * provide each carrier's local implementations and texture layouts.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. Select the billboard signature before the first inclusion.
 */

#ifndef SRC_SHARED_EFFECT_SPRITE_H
#define SRC_SHARED_EFFECT_SPRITE_H

#include "types.h"

#include "main/coord.h"

// Select the carrier's billboard signature before the first inclusion: either
// halfword (u16, s16) or word (s32, s32) arguments. Drift-only rooms define neither.
#if defined(EFFECT_SPRITE_BILLBOARD_HALFWORD_ARGUMENTS) || defined(EFFECT_SPRITE_BILLBOARD_WORD_ARGUMENTS)
static void _effectSpriteDrawChip(const GfxCoord* coord, u16 frame, s16 size, s16 angle);
#endif
#ifdef EFFECT_SPRITE_BILLBOARD_HALFWORD_ARGUMENTS
static void _effectSpriteDrawBillboard(const GfxCoord* coord, u16 frame, s16 size);
#elif defined(EFFECT_SPRITE_BILLBOARD_WORD_ARGUMENTS)
static void _effectSpriteDrawBillboard(const GfxCoord* coord, s32 frame, s32 size);
#endif

void effectSpriteDriftTaskAimed(Task* task);
void effectSpriteDebrisTask(Task* task);

void effectSpriteRiseTask(Task* task);

#endif /* SRC_SHARED_EFFECT_SPRITE_H */
