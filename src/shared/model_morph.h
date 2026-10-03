/* Model morph: deforms a TMD model from its rest shape by a ramp, adding scaled
 * vertex deltas and interpolating normals toward a target set, as described by
 * a `ModelMorph` (include/overlay.h). The Dilapidated House morphs a model of
 * its own; actor_323300 morphs its model with the Dryfield toilet's record.
 */
#ifndef SRC_SHARED_MODEL_MORPH_H
#define SRC_SHARED_MODEL_MORPH_H

#include "overlay.h"

static void modelMorphBlend(Task* task, ModelMorph* morph, s32 ramp);

#endif /* SRC_SHARED_MODEL_MORPH_H */
