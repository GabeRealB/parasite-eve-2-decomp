/* Model morph: blends a TMD model between its rest shape and a target shape
 * described by an `OverlayMorphTarget` (include/overlay.h). The Dilapidated
 * House's model and actor_323300's blend of the Dryfield toilet's.
 */
#ifndef SRC_SHARED_MODEL_MORPH_H
#define SRC_SHARED_MODEL_MORPH_H

#include "overlay.h"

static void modelMorphBlend(Task* task, OverlayMorphTarget* morph, s32 ramp);

#endif /* SRC_SHARED_MODEL_MORPH_H */
