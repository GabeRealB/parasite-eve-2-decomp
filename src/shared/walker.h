/* What the scripted NPC walkers share whatever their kind: a ground shadow
 * at a fixed shade and the per-frame state that relights the model from a
 * point 0x320 above its root, runs the walker's update and draws its shadow.
 * Room-shaded shadows use actor_render_walker_shadow.inc.c with a private
 * declaration in each carrier's prologue.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The shadows are library functions; a file with more than one copy
 * includes the fragment again under the further copy's name. The frame state
 * is written around the walker's own update and shadow: every file includes it
 * with walkerFrame, walkerUpdate and walkerDrawShadow defined to its own names.
 */

#ifndef SRC_SHARED_WALKER_H
#define SRC_SHARED_WALKER_H

#include "types.h"

#include "main/task_types.h"

#include "main/task_types.h"

void walkerDrawShadow(Task* task);

#endif /* SRC_SHARED_WALKER_H */
