/* Glows the Acropolis Promenade and Bridge both carry, as one-frame room-effect
 * tasks: a flickering star (an animated core quad and a rotating flare of
 * random grey) and a lamp sprite (three texture variants, flickering between two
 * grey levels). Each releases its work block once queued, so the room
 * respawns it every frame it wants the glow.
 *
 * Gameplay's room-effect table names each room's task, so the room defines
 * GLOW_STAR_TASK / GLOW_LAMP_TASK to its entry name
 * before including the fragment at the function's position.
 */
#ifndef EFFECTS_ACROPOLIS_GLOWS_ACROPOLIS_GLOWS_H
#define EFFECTS_ACROPOLIS_GLOWS_ACROPOLIS_GLOWS_H

#include <core/pe2.h>
#include <engine/task_types.h>
#include <psyq/sys/types.h>
#include <psyq/libgpu.h>
#include <effects/glow/glow.h>

/// Sets a square quad's corners around its projected center, `halfExtent` pixels out.
static inline void _acropolisGlowSetSpriteBounds(POLY_FT4* quad, const RoomGlowSpriteScratch* projection)
{
    s16 left;
    s16 right;
    s16 top;
    s16 bottom;

    left     = projection->screenPos.vx - projection->halfExtent;
    quad->x2 = left;
    quad->x0 = left;
    right    = projection->screenPos.vx + projection->halfExtent;
    quad->x3 = right;
    quad->x1 = right;
    top      = projection->screenPos.vy - projection->halfExtent;
    quad->y1 = top;
    quad->y0 = top;
    bottom   = projection->screenPos.vy + projection->halfExtent;
    quad->y3 = bottom;
    quad->y2 = bottom;
}

#endif /* EFFECTS_ACROPOLIS_GLOWS_ACROPOLIS_GLOWS_H */
