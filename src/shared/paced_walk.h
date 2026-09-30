/* The walk of the NPCs that cutscene scripts move around, in the version that
 * keeps its work block in Task::work. The 'walk to' message (0x7DD) aims the
 * model's root coordinate at a target and records the distance in twelfths;
 * the per-frame body reseeds the 20-slot rig in states 1 and 2, then in state
 * 3 steps the root 12 units forward per frame while the walk clip has travel
 * left, switches to the idle clip with a 10-frame blend when it runs out, and
 * ticks the rig.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. A file with several walkers, as actor_460200 has, includes the
 * fragments again for each further walker with the library names defined to
 * that walker's own.
 */

#ifndef SRC_SHARED_PACED_WALK_H
#define SRC_SHARED_PACED_WALK_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

void pacedWalkUpdate(Task* task);
void pacedWalkTickAnim(Task* task);
void pacedWalkResetAnim(Task* task);
void pacedWalkBlendAnim(Task* task);
s32  pacedWalkTo(Task* task, s32 arg1, ActorTransform* target);

s32 pacedWalkPlace(Task* task, s32 arg1, ActorTransform* placement);

#endif /* SRC_SHARED_PACED_WALK_H */
