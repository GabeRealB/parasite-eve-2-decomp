/* Small helpers that set where an actor's model sits: one sets the root
 * coordinate from a matrix times a scale (uniform or Y only), and one is a
 * child task's setup state that hangs its model off a part of the parent's
 * model and takes the parent's lighting and visibility.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_MODEL_PLACEMENT_H
#define SRC_SHARED_MODEL_PLACEMENT_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "main/task_types.h"

void modelPlacementAttachChild(Task* task);
void modelPlacementSetScaled(Task* arg0, MATRIX* arg1, s16 arg2, s32 arg3);
void modelPlacementAttachPart(Task* childTask);
void modelPlacementMirrorParent(Task* task);

#endif /* SRC_SHARED_MODEL_PLACEMENT_H */
