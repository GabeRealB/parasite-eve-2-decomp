/* Part of the model morph library; see model_morph.h. */

/// Applies a vertex morph and an optional normal blend to the task's TMD model.
///
/// `ramp` is an unclamped Q12 weight in 0..`ONE`: zero restores the saved
/// position, and `ONE` adds the full vertex delta and selects the target normal.
/// Only XYZ components change; the fourth halfword of each vector stays intact.
/// Normals use the GTE's weighted blend without renormalization.
///
/// The rest snapshot must be initialized from this model. `deltaCount` must be
/// positive, `firstVertex` nonnegative, and their sum must not exceed
/// `savedVertexCount` or the model's vertex extent. `vertexDeltas` supplies
/// `deltaCount` entries. When `targetNormals` is present, `normalCount` must be
/// nonnegative and the model, saved and target normal arrays must each provide
/// that many entries, starting at zero regardless of
/// `firstVertex`. All storage is borrowed and must stay live during the call;
/// input arrays must be separate from the model arrays modified in place.
static void _modelMorphBlend(Task* task, const ModelMorph* morph, s32 ramp)
{
    s32            elementIndex;
    s32            elementCount;
    s32            firstVertex;
    TmdSource*     modelSource;
    SVECTOR*       vertices;
    const SVECTOR* savedVertices;
    s16*           vertexZ;
    const s16*     savedVertexZ;
    SVECTOR*       normals;
    const SVECTOR* targetNormals;
    const SVECTOR* savedNormals;
    SVECTOR*       normal;
    s32            targetWeight;
    s32            savedWeight;

    // Restore the rest position so repeated calls do not accumulate displacement.
    elementIndex  = 0;
    elementCount  = morph->deltaCount;
    modelSource   = task->extra.tmd->source;
    firstVertex   = morph->firstVertex;
    savedVertices = &morph->savedVertices[firstVertex];
    normals       = modelSource->normals;
    vertices      = &modelSource->verts[firstVertex];
    if (elementCount > 0) {
        // Y/Z cursors stride over whole vectors, leaving each fourth halfword intact.
        savedVertexZ = &savedVertices->vz;
        vertexZ      = &vertices->vz;
        do {
            vertices->vx = savedVertices->vx;
            savedVertices++;
            elementIndex++;
            vertices++;
            vertexZ[-1]   = savedVertexZ[-1];
            vertexZ[0]    = savedVertexZ[0];
            savedVertexZ += sizeof(SVECTOR) / sizeof(*savedVertexZ);
            vertexZ      += sizeof(SVECTOR) / sizeof(*vertexZ);
        } while (elementIndex < elementCount);
    }
    // Apply displacement to the restored vertices; normals blend independently from index zero.
    targetWeight = ramp;
    savedWeight  = ONE - targetWeight;
    gteMIMefunc(modelSource->verts + morph->firstVertex, morph->vertexDeltas, morph->deltaCount, targetWeight);
    targetNormals = morph->targetNormals;
    if (targetNormals != NULL) {
        elementCount = morph->normalCount;
        savedNormals = morph->savedNormals;
        elementIndex = 0;
        if (elementCount > 0) {
            do {
                gte_lddp(targetWeight);
                gte_ldsv(targetNormals);
                gte_gpf12();
                normal = normals + elementIndex;
                gte_lddp(savedWeight);
                gte_ldsv(savedNormals);
                gte_gpl12();
                savedNormals++;
                elementIndex++;
                targetNormals++;
                gte_stsv(normal);
            } while (elementIndex < elementCount);
        }
    }
}
