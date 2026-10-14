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
#ifndef SRC_SHARED_ACROPOLIS_GLOWS_H
#define SRC_SHARED_ACROPOLIS_GLOWS_H

#include "common.h"
#include "main/task_types.h"

#endif /* SRC_SHARED_ACROPOLIS_GLOWS_H */
