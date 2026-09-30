/* The NPC walker with footsteps: a 19-slot animated character that cutscene
 * scripts walk around. Its spawn state publishes a singleton work block,
 * lights and binds the model and installs the message table; the per-frame
 * update reseeds the rig in states 1 and 2, then in state 3 walks forward at
 * the mode's speed while the walk clip has travel left, queues the idle clip
 * with a 10-frame blend when it runs out, turns while the turn clip has frames
 * left and plays a panned step sound on each foot cue. The 'walk to' message
 * (0x7DD) turns the model to face its target and divides the planar distance
 * into the mode's step count. The exit callback, footstepWalkExit, is the
 * package's own.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The walker's state belongs to the package, which defines it at
 * its own positions under these names:
 *
 *   Actor151000Work*  gFootstepWalkWork         the published work block
 *   Task*             gFootstepWalkTask         the walker's task
 *   s16               gFootstepWalkMode         the mode of the last walk
 *   s16               gFootstepWalkBlendFrames  the blend the next reseed uses
 *   the animation stream and message table the spawn state installs, as
 *   gFootstepWalkAnims and gFootstepWalkMsgTable
 */

#ifndef SRC_SHARED_FOOTSTEP_WALK_H
#define SRC_SHARED_FOOTSTEP_WALK_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/enemy.h"

#include "main/task_types.h"

void footstepWalkExit(Task* task);
void footstepWalkSpawn(GpEnemy* enemy, Task* task);
void footstepWalkUpdate(Task* task);
void footstepWalkPlaySteps(Task* task);
void footstepWalkTickAnim(void);
void footstepWalkResetAnim(void);
void footstepWalkBlendAnim(void);
s32  footstepWalkTo(Task* task, s32 arg1, VECTOR* target, s32 mode);

#endif /* SRC_SHARED_FOOTSTEP_WALK_H */
