/* Falling leaves for the Acropolis gardens and the Neo Ark forest. Each leaf
 * is a room-effect task that falls with a swaying drift and a random tumble
 * about X and Z until it reaches the ground plane, lies there briefly, then
 * fades out and frees its work block. It is drawn as a small textured quad by
 * leafDraw, which comes in two versions: falling_leaves_draw.inc.c for the
 * Acropolis rooms and falling_leaves_draw_neo_ark.inc.c, with another texture
 * cell, for the Neo Ark rooms. The task is named by gameplay's effect table, so each room keeps
 * its own name for it and calls the inline body.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_FALLING_LEAVES_H
#define SRC_SHARED_FALLING_LEAVES_H

#include "types.h"

#include "main/coord.h"

void leafDraw(GfxCoord* coord, s32 arg1, s16 arg2);

#endif /* SRC_SHARED_FALLING_LEAVES_H */
