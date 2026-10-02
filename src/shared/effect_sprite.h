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

/// Advances an animated drifting sprite, then releases its work and task at the last cell.
///
/// `task` must be a live coordinate-body effect with an `EffectWork` in
/// `spawnArg2.pointer`, normally initialized by `Gp_SpawnEff` with state 0.
/// The first running update initializes without drawing. Later running updates
/// draw before moving, accelerating and advancing the cell (12 banked cells or
/// 10 alternate cells). Any nonzero `RoomEffectState::effectControl` redraws
/// without advancing; values >= 4 release the effect after that final draw.
/// Hidden control values still draw. Drawing needs a composed coordinate,
/// initialized scratch stack, and primitive-packet capacity.
///
/// `spawnArg1.value` packs perspective size in bits 0..11, period in bits
/// 12..14, speed in bits 16..23 (zero means 64 coordinate units per update),
/// movement kind in bits 24..27, palette bank in bits 28..30, and the alternate
/// drawer in bit 31. Period defaults to 1 only when the entire bits-12..15
/// nibble is zero; a nonzero nibble must encode a nonzero period in bits 12..14.
/// Bit 15 alone produces zero and is invalid for later modulo updates.
///
/// The initial spin uses 4096 units per turn. An existing nonzero `move` is
/// retained; otherwise kinds 1/2/3/6 roll upward/all-axis/narrow-upward/planar
/// directions and normalize to the encoded speed. Kind 0 disables movement.
/// Kind 5 copies `pos`, whose X has already been replaced by palette bits.
/// Moving updates subtract 2 or 1 from the banked/alternate Y velocity;
/// kind 7 instead adds age/10. Positions use coordinate units and signed s16
/// velocity components wrap on assignment. No work pointer survives release.
void effectSpriteDriftTask(Task* task);
void effectSpriteDriftTaskAimed(Task* task);
void effectSpriteDebrisTask(Task* task);

void effectSpriteRiseTask(Task* task);

#endif /* SRC_SHARED_EFFECT_SPRITE_H */
