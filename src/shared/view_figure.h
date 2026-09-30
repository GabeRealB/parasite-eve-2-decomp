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

#include "main/task_types.h"

void viewFigureSpawnState(GpEnemy* enemy, Task* task);
void viewFigureStepAnim(Task* task);
void viewFigureResetAnim(void);
void viewFigureReseedAnim(void);
s32  viewFigurePlayMessage(Task* task, s32 arg1, AnimationPlayRequest* args);

/* Defined by each package. */
void viewFigureExit(Task* arg0);
void viewFigureTickAnim(void);

#endif /* SRC_SHARED_VIEW_FIGURE_H */
