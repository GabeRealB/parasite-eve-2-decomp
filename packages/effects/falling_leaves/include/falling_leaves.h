/* Falling leaves for the Acropolis and Neo Ark rooms: each leaf tumbles down,
 * comes to rest and fades. Include this header in the prologue,
 * task.inc.c before the room's exported wrapper, and draw.inc.c (Acropolis) or
 * draw_neo_ark.inc.c (Neo Ark) at the drawer's position.
 */

#ifndef EFFECTS_FALLING_LEAVES_FALLING_LEAVES_H
#define EFFECTS_FALLING_LEAVES_FALLING_LEAVES_H

#include <core/pe2.h>

#include <engine/coord.h>

/// Draws the leaf opaque with its texture's own colors, instead of fading it by a brightness.
enum { LEAF_BRIGHTNESS_RAW_TEXTURE = 0 };

static void _leafDraw(const GfxCoord* coord, s32 halfSize, s16 brightness);

#endif /* EFFECTS_FALLING_LEAVES_FALLING_LEAVES_H */
