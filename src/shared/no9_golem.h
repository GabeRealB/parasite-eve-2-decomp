/* The No. 9 GOLEM, carried by actor_510900 (Akropolis) and actor_521100
 * (Dryfield). Most of the enemy is implemented separately in each package; the
 * shared part is the end of its per-frame update: model part 4 is turned to
 * face the player, its offset rotated into the root frame and clamped to a
 * forward cone, and a ground square with a 768-unit half-side is drawn under
 * part 1 at the root's height.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_NO9_GOLEM_H
#define SRC_SHARED_NO9_GOLEM_H

#include "types.h"

#include "main/task_types.h"

void no9GolemAimHead(Task* arg0);

#endif /* SRC_SHARED_NO9_GOLEM_H */
