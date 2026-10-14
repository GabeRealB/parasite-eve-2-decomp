/* Private interface for the fixed-shade walker shadow instances.
 *
 * Include this header in each fixed-shadow carrier's prologue. Bind
 * ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW to a declared static void(Task*)
 * function before including walker_shadow.inc.c or a walker frame fragment,
 * then undefine it. The frame fragments call that function once with their
 * task; it may draw either a fixed-shade or a room-shaded shadow.
 *
 * walker_shadow.inc.c defines the fixed-shade instance at its text position.
 * Further copies need their own static prototypes in the carrier's prologue.
 * Room-shaded instances use actor_render_walker_shadow.inc.c and its separate
 * ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW definition binding.
 * walker_frame.inc.c also requires ACTOR_RENDER_WALKER_FRAME and
 * ACTOR_RENDER_UPDATE_WALKER. Bind the latter to the declared static void(Task*)
 * motion/animation update for that frame's task and work block, then undefine
 * it after inclusion. The frame calls it once between lighting and shadow
 * drawing; the identifier alias captures no locals and constructs no tokens.
 */

#ifndef SRC_SHARED_WALKER_H
#define SRC_SHARED_WALKER_H

#include "gameplay/actor_render_shadow_types.h"

#include "main/task_types.h"

static void _actorRenderDrawFixedWalkerGroundShadow(Task* task);

#endif /* SRC_SHARED_WALKER_H */
