/* Reusable room dialogue task: repeat a CAP command until its exit choice,
 * then restore the scripted player control held by the spawner.
 *
 * Include this header in the prologue and the fragment at its function's
 * position. Each carrier keeps a static callback instance.
 */

#ifndef SRC_SHARED_CAP_DIALOGUE_H
#define SRC_SHARED_CAP_DIALOGUE_H

#include "types.h"

#include "main/task_types.h"

static void _capDialogueLoopTask(Task* task);

#endif /* SRC_SHARED_CAP_DIALOGUE_H */
