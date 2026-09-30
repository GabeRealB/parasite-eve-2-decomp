/* Drawing of the Shelter rooms' animated effect sprites: one frame of a ten-
 * cell 48x48 sprite sheet, drawn as a textured quad at a coordinate's world
 * position, rotated and scaled by depth.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_EFFECT_SPRITE_H
#define SRC_SHARED_EFFECT_SPRITE_H

#include "types.h"

#include "main/coord.h"

void effectSpriteDrawRotated(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3);

void effectSpriteDrawBanked(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3);
/* A room whose debris draws with its own chip and billboard builds defines
   EFFECT_SPRITE_OWN_DRAWERS and declares its own. The shared billboard is a
   different build (its frame and size are full words) and is declared only
   for a room that defines EFFECT_SPRITE_SHARED_BILLBOARD and includes
   effect_sprite_draw_billboard.inc.c. A room that includes this header for
   the drift drawers does not draw one, and publishing the prototype there
   would make the two builds one imported declaration. */
#ifndef EFFECT_SPRITE_OWN_DRAWERS
void effectSpriteDrawChip(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3);
#ifdef EFFECT_SPRITE_SHARED_BILLBOARD
void effectSpriteDrawBillboard(GfxCoord* arg0, s32 arg1, s32 arg2);
#endif
#endif
void effectSpriteDriftTask(Task* task);
void effectSpriteDriftTaskAimed(Task* task);
void effectSpriteDebrisTask(Task* task);

#endif /* SRC_SHARED_EFFECT_SPRITE_H */
