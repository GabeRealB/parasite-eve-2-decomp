/* Included TMD model morphing. Each carrier has a static implementation;
 * ModelMorph records and their snapshot storage can be owned by another package.
 */
#ifndef SRC_SHARED_MODEL_MORPH_H
#define SRC_SHARED_MODEL_MORPH_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "engine/task_types.h"

/// One morph of a TMD model: what the included `modelMorph` code needs to
/// snapshot the model's rest shape and to deform it by a 0..`ONE` ramp.
///
/// A vertex morphs additively - its rest position plus its delta scaled by
/// the ramp - while a normal is interpolated between its rest value and a
/// target. The snapshot covers the model's first `savedVertexCount` vertices
/// and `normalCount` normals; the deltas apply to the `deltaCount` vertices
/// from `firstVertex`, which need not be all of them. The package owning the
/// model's data defines the record and both snapshot buffers.
typedef struct {
    SVECTOR* vertexDeltas;     // displacement of each morphed vertex at a full ramp, `deltaCount` entries
    SVECTOR* targetNormals;    // normals at a full ramp, `normalCount` entries; NULL leaves the model's normals alone
    SVECTOR* savedVertices;    // snapshot buffer for the rest vertices, at least `savedVertexCount` entries
    SVECTOR* savedNormals;     // snapshot buffer for the rest normals, at least `normalCount` entries; unused without `targetNormals`
    s16      savedVertexCount; // vertices snapshotted, from the model's first
    s16      normalCount;      // normals snapshotted and interpolated, from the model's first
    s16      firstVertex;      // model vertex the first delta applies to
    s16      deltaCount;       // vertices restored from the snapshot and displaced on each blend
} ModelMorph;
STATIC_ASSERT_SIZEOF(ModelMorph, 0x18);

static void _modelMorphBlend(Task* task, const ModelMorph* morph, s32 ramp);

#endif /* SRC_SHARED_MODEL_MORPH_H */
