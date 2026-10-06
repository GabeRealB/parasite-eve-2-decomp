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

/// Axis choices for root scaling; the s32 selector treats every nonzero value as uniform.
enum {
    MODEL_PLACEMENT_SCALE_Y_ONLY  = 0,
    MODEL_PLACEMENT_SCALE_UNIFORM = 1
};

static void _modelPlacementAttachChild(Task* childTask);
static void _modelPlacementSetScaled(Task* modelTask, const MATRIX* unscaledMatrix, s16 scale, s32 uniformScale);

#endif /* SRC_SHARED_MODEL_PLACEMENT_H */
