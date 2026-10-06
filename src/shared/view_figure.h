/* A scripted figure with a 20-slot rig whose root is parented to the camera
 * (gGfxViewCoord), so it stays placed relative to the view. It carries a
 * second model hung on its part 8 by a helper task. It publishes its work
 * block as a singleton, is not lockable, plays clips on the 0x7D3 message
 * (reseeding every slot) and queues footstep-like cues from animation frames.
 * The slot tick, reset and reseed have the same shape as scripted_walk's, with
 * a fixed reseed blend of 8, but they are not byte-identical to it, so this is
 * a separate library.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_VIEW_FIGURE_H
#define SRC_SHARED_VIEW_FIGURE_H

#include "types.h"

#include "actors/actor.h"

#include "main/task_types.h"

/// Work block of a view figure's task.
///
/// The spawn state allocates it zeroed at its full size, keeps it at
/// `Task::work` and publishes it through the package's work pointer, which
/// the library reaches it by from then on. The figure is placed by its
/// parent coordinate and never walks, so of `st` only the animation request
/// is used: the heading and the walk stay zero.
///
/// Nothing in the library or its packages touches the bytes after `st`, so
/// what the allocation's tail was laid out to hold is unproven.
typedef struct {
    ActorAnimRig20  rig;               // Playback storage of the twenty-part body model; every slot is seeded and ticked
    ActorEnemyState st;                // Animation request, and the record a sound was last cued for in the package that cues footsteps
    byte            unknown_4AC[0xB0]; // Allocated zeroed and never accessed; role unproven
} ViewFigureWork;
STATIC_ASSERT_SIZEOF(ViewFigureWork, 0x55C);

void viewFigureSpawnState(Enemy* enemy, Task* task);
void viewFigureStepAnim(Task* task);
void viewFigureResetAnim(void);
void viewFigureReseedAnim(void);
s32  viewFigurePlayMessage(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3);

static void _viewFigureExit(Task* task);
static void _viewFigureTickAnim(void);

#endif /* SRC_SHARED_VIEW_FIGURE_H */
