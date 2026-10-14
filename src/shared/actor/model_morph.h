/* Included TMD model morphing. Each carrier has a static implementation;
 * ModelMorph records and their snapshot storage can be owned by another package.
 */
#ifndef SRC_SHARED_MODEL_MORPH_H
#define SRC_SHARED_MODEL_MORPH_H

#include "overlay.h"

static void _modelMorphBlend(Task* task, const ModelMorph* morph, s32 ramp);

#endif /* SRC_SHARED_MODEL_MORPH_H */
