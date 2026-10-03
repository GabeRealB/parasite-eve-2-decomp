/* The NPC walker that carries a second model on one of its parts, as in
 * actor_150400 and the packages built alongside it: the per-frame update that
 * reseeds the 19-slot rig and walks while the walk clip has travel left, the
 * slot tick, reset and reseed it runs, the play-animation, placement and
 * visibility messages (visibility applies to both models), and the sub-model
 * task that hangs the second model off part 7 under the walker's lighting.
 * pairWalkUpdate comes in two versions: pair_walk_update.inc.c, and
 * pair_walk_update_model.inc.c, which walks the model 12 units a frame through
 * actorMoveModelForward. The packages with the second version also share the
 * spawn state (pair_walk_spawn.inc.c) and the walk-to message
 * (pair_walk_to.inc.c), and define pairWalkExit themselves.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_PAIR_WALK_H
#define SRC_SHARED_PAIR_WALK_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

void pairWalkUpdate(Task* task);
void pairWalkTickAnim(Task* task);
void pairWalkResetAnim(Task* task);
void pairWalkReseedAnim(Task* task);
s32  pairWalkPlay(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3);
s32  pairWalkSetVisibility(Task* task, s32 arg1, s32 flags, s32 arg3);
s32  pairWalkPlace(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);
void pairWalkSubModelTask(Task* task);

void pairWalkSpawn(Enemy* enemy, Task* task);
s32  pairWalkTo(Task* task, s32 arg1, VECTOR* target, s32 arg3);

/* Defined by each package. */
void pairWalkExit(Task* task);

#endif /* SRC_SHARED_PAIR_WALK_H */
