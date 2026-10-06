/* The NPC walker that carries a second model on one of its parts, as in
 * actor_150400 and the packages built alongside it: the per-frame update that
 * reseeds the 19-slot rig and walks while the walk clip has travel left, the
 * slot tick, reset and reseed it runs, the play-animation, placement and
 * visibility messages (visibility applies to both models), and the sub-model
 * task that hangs the second model off part 7 under the walker's lighting.
 * pairWalkUpdate comes in two versions: pair_walk_update.inc.c, and
 * pair_walk_update_model.inc.c, which walks the model 12 units a frame through
 * _actorMovementStepModelForward. The packages with the second version also share the
 * spawn state (pair_walk_spawn.inc.c) and the walk-to message
 * (pair_walk_to.inc.c), and define pairWalkExit themselves.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_PAIR_WALK_H
#define SRC_SHARED_PAIR_WALK_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "actors/actor.h"

#include "gameplay/enemy.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// Work block of a pair walker, allocated zeroed at its full size by the
/// walker's spawn state and kept at `Task::work`.
///
/// The model object borrows `light` and `color` for as long as the block
/// lives, and the second model the walker carries is lit through the same
/// two.
typedef struct {
    MATRIX          light;       // Light-direction matrix lent to the model object
    MATRIX          color;       // Light-colour matrix lent to the model object
    ActorAnimRig19  rig;         // Playback storage of the nineteen-part model; slots 1 to 18 are driven
    ActorEnemyState st;          // Animation request, heading last given the root and frames of walk left
    s16             blendFrames; // Whole frames the next blended reseed takes to reach the requested clip
    Task*           pairTask;    // Task of the second model the walker carries, shown and hidden with it
    Enemy*          enemy;       // Enemy the walker's task belongs to; recorded at spawn, never read
} PairWalkWork;
STATIC_ASSERT_SIZEOF(PairWalkWork, 0x4C0);

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
